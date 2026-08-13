/*
 //
 // DEKAF(tm): Lighter, Faster, Smarter (tm)
 //
 // Copyright (c) 2026, Ridgeware, Inc.
 //
 // +-------------------------------------------------------------------------+
 // | /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\|
 // |/+---------------------------------------------------------------------+/|
 // |/|                                                                     |/|
 // |\|  ** THIS NOTICE MUST NOT BE REMOVED FROM THE SOURCE CODE MODULE **  |\|
 // |/|                                                                     |/|
 // |\|   OPEN SOURCE LICENSE                                               |\|
 // |/|                                                                     |/|
 // |\|   Permission is hereby granted, free of charge, to any person       |\|
 // |/|   obtaining a copy of this software and associated                  |/|
 // |\|   documentation files (the "Software"), to deal in the              |\|
 // |/|   Software without restriction, including without limitation        |/|
 // |\|   the rights to use, copy, modify, merge, publish,                  |\|
 // |/|   distribute, sublicense, and/or sell copies of the Software,       |/|
 // |\|   and to permit persons to whom the Software is furnished to        |\|
 // |/|   do so, subject to the following conditions:                       |/|
 // |\|                                                                     |\|
 // |/|   The above copyright notice and this permission notice shall       |/|
 // |\|   be included in all copies or substantial portions of the          |\|
 // |/|   Software.                                                         |/|
 // |\|                                                                     |\|
 // |/|   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY         |/|
 // |\|   KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE        |\|
 // |/|   WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR           |/|
 // |\|   PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS        |\|
 // |/|   OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR          |/|
 // |\|   OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR        |\|
 // |/|   OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE         |/|
 // |\|   SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.            |\|
 // |/|                                                                     |/|
 // |/+---------------------------------------------------------------------+/|
 // |\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/ |
 // +-------------------------------------------------------------------------+
 */

// ktunnel_test - an end to end integrity test for KTunnel.
//
// It builds the full topology of a real deployment inside one process:
//
//   driver(s) >>--TCP-->> forward listener      tunnel link      exposed side
//                              |            (TCP or TLS)             |
//                         KTunnel A  <<================>>  KTunnel B >>--TCP-->> echo target
//                        (waits for login)                (logs in)
//
// Every driver pushes a deterministic byte stream through its own multiplexed
// channel and compares what comes back byte for byte - so it catches lost,
// duplicated, reordered or truncated payload, not just a broken connection.
// Run it with enough connections and volume to keep the tunnel saturated: that
// is where an unfair or starving event loop shows up.
//
// The drivers are also a worked example of the duplex pattern a socket stream
// requires: one thread per connection, sending and receiving interleaved over
// one poll, with partial writes - never a reader thread plus a writer thread on
// the same stream.

#include <dekaf2/net/util/ktunnel.h>
#include <dekaf2/net/tcp/ktcpserver.h>
#include <dekaf2/net/tcp/ktcpstream.h>
#include <dekaf2/net/tls/ktlsstream.h>
#include <dekaf2/core/init/dekaf2.h>
#include <dekaf2/core/errors/kexception.h>
#include <dekaf2/core/format/kformat.h>
#include <dekaf2/io/readwrite/kwriter.h>
#include <dekaf2/system/os/ksystem.h>
#include <dekaf2/util/cli/koptions.h>

#include <array>
#include <atomic>
#include <thread>
#include <vector>

using namespace dekaf2;

namespace {

constexpr uint16_t     s_iTunnelPort  { 7801 };
constexpr uint16_t     s_iForwardPort { 7802 };
constexpr uint16_t     s_iEchoPort    { 7803 };
constexpr KStringViewZ s_sSecret      { "ktunnel_test" };
constexpr KStringViewZ s_sNode        { "testnode"     };

/// how much we hand to the stream per attempt
constexpr std::size_t  s_iChunk       { 32 * 1024 };

/// set while we tear the test down - both tunnel sides end with an exception
/// then, which is expected and not worth reporting
std::atomic<bool>      s_bStopping    { false };

//-----------------------------------------------------------------------------
/// A deterministic payload: a pseudo random block that is rotated by one byte
/// on every repetition, so neither a dropped nor a reordered nor a duplicated
/// chunk can pass unnoticed.
KString MakePayload(std::size_t iSeed, std::size_t iSize)
//-----------------------------------------------------------------------------
{
	KString sBlock;
	sBlock.reserve(4096);

	// xorshift64 - we only need reproducible bytes, not crypto
	uint64_t iState = 0x9E3779B97F4A7C15ULL + iSeed * 0x2545F4914F6CDD1DULL;

	for (std::size_t i = 0; i < 4096; ++i)
	{
		iState ^= iState << 13;
		iState ^= iState >> 7;
		iState ^= iState << 17;
		sBlock += static_cast<char>(iState & 0xFF);
	}

	KString sPayload;
	sPayload.reserve(iSize);

	for (std::size_t iRound = 0; sPayload.size() < iSize; ++iRound)
	{
		auto iOffset = iRound % sBlock.size();
		sPayload += KStringView(sBlock).substr(iOffset);
		sPayload += KStringView(sBlock).substr(0, iOffset);
	}

	sPayload.resize(iSize);

	return sPayload;

} // MakePayload

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// the target at the far end of the tunnel - it echoes everything back
class KEchoServer : public KTCPServer
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

public:

	using KTCPServer::KTCPServer;

protected:

	virtual void Session(std::unique_ptr<KIOStreamSocket>& Stream) override
	{
		std::array<char, s_iChunk> Buffer;

		for (;;)
		{
			auto iRead = Stream->direct_read_some(Buffer.data(), Buffer.size());

			if (iRead <= 0) break;

			if (!Stream->Write(Buffer.data(), iRead).Flush().Good()) break;
		}
	}

}; // KEchoServer

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// the side of the tunnel that waits to be logged into ("exposed host")
class KTunnelServer : public KTCPServer
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

public:

	using KTCPServer::KTCPServer;

	/// the running tunnel, or nullptr while none is established
	KTunnel* GetTunnel() const { return m_pTunnel.load(std::memory_order_acquire); }

protected:

	virtual void Session(std::unique_ptr<KIOStreamSocket>& Stream) override
	{
		KTunnel::Config Config;
		Config.Secrets.insert(s_sSecret);
		Config.iMaxTunneledConnections = 200;

		// no node/secret given - this side waits for the peer to log in
		KTunnel Tunnel(Config, std::move(Stream));

		// publish it for the forward listener. The test tears down in order
		// (drivers, then the tunnel), so the pointer cannot go stale under a
		// forward session
		m_pTunnel.store(&Tunnel, std::memory_order_release);

		try
		{
			Tunnel.Run();
		}
		catch (const std::exception& ex)
		{
			if (!s_bStopping) kPrintLine(KErr, ">> exposed side ended: {}", ex.what());
		}

		m_pTunnel.store(nullptr, std::memory_order_release);
	}

	std::atomic<KTunnel*> m_pTunnel { nullptr };

}; // KTunnelServer

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// takes the driver connections and hands each of them to the tunnel, which
/// opens a channel and asks the far side to connect to the echo target
class KForwardServer : public KTCPServer
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

public:

	KForwardServer(uint16_t iPort, const KTunnelServer& TunnelServer)
	: KTCPServer(iPort, false, 100)
	, m_TunnelServer(TunnelServer)
	{
	}

protected:

	virtual void Session(std::unique_ptr<KIOStreamSocket>& Stream) override
	{
		auto* pTunnel = m_TunnelServer.GetTunnel();

		if (!pTunnel)
		{
			kPrintLine(KErr, ">> no tunnel for a forwarded connection");
			return;
		}

		try
		{
			// returns when either end closed the channel
			pTunnel->Connect(Stream.get(), KTCPEndPoint(kFormat("127.0.0.1:{}", s_iEchoPort)));
		}
		catch (const std::exception& ex)
		{
			kPrintLine(KErr, ">> forward channel ended: {}", ex.what());
		}
	}

	const KTunnelServer& m_TunnelServer;

}; // KForwardServer

//-----------------------------------------------------------------------------
/// One driver: sends its payload through the tunnel and verifies the echo.
/// Single threaded duplex - sending and receiving interleave over one poll, and
/// a partial write is a normal result. @return an empty string on success, else
/// the failure
KString RunOneDriver(std::size_t iIndex, std::size_t iSize, KDuration StallTimeout)
//-----------------------------------------------------------------------------
{
	KTCPStream Stream(KTCPEndPoint(kFormat("127.0.0.1:{}", s_iForwardPort)),
	                  KStreamOptions(chrono::seconds(30)));

	if (!Stream.Good())
	{
		return kFormat("cannot connect: {}", Stream.GetLastError());
	}

	auto sPayload = MakePayload(iIndex, iSize);

	KString sEchoed;
	sEchoed.resize(iSize);

	std::size_t iSent { 0 };
	std::size_t iGot  { 0 };
	KStopTime   Stall;

	while (iGot < iSize)
	{
		bool bDidIO { false };

		if (iSent < iSize && Stream.IsWriteReady(KDuration(), false))
		{
			auto iWrote = Stream.direct_write_some(sPayload.data() + iSent,
			                                      std::min(s_iChunk, iSize - iSent));

			if (iWrote < 0) return kFormat("write failed after {} bytes", iSent);

			if (iWrote > 0)
			{
				iSent  += static_cast<std::size_t>(iWrote);
				bDidIO  = true;
			}
		}

		if (Stream.IsReadReady(KDuration(), false))
		{
			auto iRead = Stream.direct_read_some(&sEchoed[iGot], iSize - iGot);

			if (iRead <= 0)
			{
				return kFormat("connection closed after {} of {} bytes echoed", iGot, iSize);
			}

			iGot   += static_cast<std::size_t>(iRead);
			bDidIO  = true;
		}

		if (bDidIO)
		{
			Stall.clear();
			continue;
		}

		if (Stall.elapsed() >= StallTimeout)
		{
			return kFormat("stalled with {} sent and {} of {} echoed", iSent, iGot, iSize);
		}

		// nothing to do right now - wait for either direction
		int iWhat = POLLIN;

		if (iSent < iSize) iWhat |= POLLOUT;

		Stream.CheckIfReady(iWhat, chrono::milliseconds(100), false);
	}

	if (sEchoed != sPayload)
	{
		for (std::size_t i = 0; i < iSize; ++i)
		{
			if (sEchoed[i] != sPayload[i])
			{
				return kFormat("payload differs at byte {} of {}", i, iSize);
			}
		}
	}

	return KString{};

} // RunOneDriver

} // end of anonymous namespace

//-----------------------------------------------------------------------------
int main(int argc, char** argv)
//-----------------------------------------------------------------------------
{
	try
	{
		KInit(false);

		KOptions Options(false, argc, argv, KLog::STDOUT, /*bThrow*/true);

		Options.SetBriefDescription("end to end integrity test for KTunnel");

		uint16_t iConnections = Options("c,connections <count> : parallel tunneled connections", 8);
		uint32_t iMegaBytes   = Options("m,mb <count>          : payload per connection in MB", 2);
		uint16_t iStallSecs   = Options("s,stall <seconds>     : fail a connection after this long without progress", 30);
		bool     bTLS         = Options("tls                   : run the tunnel link over TLS (self signed cert)", false);

		if (!Options.Check()) return 1;

		std::size_t iSize = static_cast<std::size_t>(iMegaBytes) * 1024 * 1024;

		kPrintLine("ktunnel_test: {} connections, {} MB each, tunnel link over {}",
		           iConnections, iMegaBytes, bTLS ? "TLS" : "TCP");

		// ---- the far end of the tunnel ---------------------------------------

		KEchoServer Echo(s_iEchoPort, false, 100);

		if (!Echo.Start(chrono::seconds(30), false))
		{
			throw KError(kFormat("cannot start the echo target on port {}", s_iEchoPort));
		}

		// ---- the side that waits to be logged into ---------------------------

		KTunnelServer TunnelServer(s_iTunnelPort, bTLS, 4);

		if (!TunnelServer.Start(chrono::seconds(30), false))
		{
			throw KError(kFormat("cannot start the tunnel listener on port {}", s_iTunnelPort));
		}

		// ---- the side that logs in -------------------------------------------

		KTCPEndPoint LinkEndPoint(kFormat("127.0.0.1:{}", s_iTunnelPort));
		KStreamOptions LinkOptions(chrono::seconds(15));

		std::unique_ptr<KIOStreamSocket> Link;

		if (bTLS) Link = std::make_unique<KTLSStream>(LinkEndPoint, LinkOptions);
		else      Link = std::make_unique<KTCPStream>(LinkEndPoint, LinkOptions);

		if (!Link->Good())
		{
			throw KError(kFormat("cannot connect to the tunnel listener: {}", Link->GetLastError()));
		}

		KTunnel::Config Config;
		Config.Secrets.insert(s_sSecret);
		Config.iMaxTunneledConnections = 200;

		// the ctor logs in and only returns once the peer acknowledged, so both
		// tunnel sides are established when we get here
		KTunnel Client(Config, std::move(Link), s_sNode, s_sSecret);

		std::thread ClientRunner([&Client]()
		{
			try
			{
				Client.Run();
			}
			catch (const std::exception& ex)
			{
				if (!s_bStopping) kPrintLine(KErr, ">> protected side ended: {}", ex.what());
			}
		});

		// ---- the listener that feeds connections into the tunnel -------------

		KForwardServer Forward(s_iForwardPort, TunnelServer);

		if (!Forward.Start(chrono::seconds(30), false))
		{
			throw KError(kFormat("cannot start the forward listener on port {}", s_iForwardPort));
		}

		// ---- run the drivers -------------------------------------------------

		std::vector<KString> Results(iConnections);
		std::vector<std::thread> Drivers;

		KStopTime Took;

		for (uint16_t iDriver = 0; iDriver < iConnections; ++iDriver)
		{
			Drivers.push_back(std::thread([&Results, iDriver, iSize, iStallSecs]()
			{
				try
				{
					Results[iDriver] = RunOneDriver(iDriver, iSize, chrono::seconds(iStallSecs));
				}
				catch (const std::exception& ex)
				{
					Results[iDriver] = ex.what();
				}
			}));
		}

		for (auto& Driver : Drivers)
		{
			Driver.join();
		}

		auto Duration = Took.elapsed();

		// ---- report ----------------------------------------------------------

		std::size_t iFailed { 0 };

		for (uint16_t iDriver = 0; iDriver < iConnections; ++iDriver)
		{
			if (!Results[iDriver].empty())
			{
				++iFailed;
				kPrintLine(">> connection {}: {}", iDriver, Results[iDriver]);
			}
		}

		// each byte travels through the tunnel twice, there and back
		auto iTotal  = iSize * iConnections * 2;
		auto iMillis = static_cast<std::size_t>(Duration.milliseconds().count());

		kPrintLine("{} of {} connections passed, {} in {} ({}/s roundtrip)",
		           iConnections - iFailed, iConnections,
		           kFormBytes(iTotal), Duration,
		           kFormBytes(iMillis ? iTotal * 1000 / iMillis : iTotal));

		// ---- tear down in order ----------------------------------------------

		s_bStopping = true;

		Forward.Stop();
		Client.Stop();

		if (ClientRunner.joinable()) ClientRunner.join();

		TunnelServer.Stop();
		Echo.Stop();

		if (iFailed)
		{
			kPrintLine(KErr, ">> FAILED");
			return 1;
		}

		kPrintLine("ALL OK");

		return 0;
	}
	catch (const std::exception& ex)
	{
		kPrintLine(KErr, ">> {}", ex.what());
	}

	return 1;

} // main
