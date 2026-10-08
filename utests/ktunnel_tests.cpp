#include "catch.hpp"

#include <dekaf2/net/util/ktunnel.h>
#include <dekaf2/net/tcp/ktcpserver.h>
#include <dekaf2/net/tcp/ktcpstream.h>
#include <dekaf2/net/tls/ktlsstream.h>
#include <dekaf2/crypto/rsa/krsakey.h>
#include <dekaf2/crypto/rsa/krsacert.h>
#include <dekaf2/core/format/kformat.h>
#include <dekaf2/time/duration/kduration.h>
#include <array>
#include <atomic>
#include <mutex>
#include <thread>
#include <vector>

using namespace dekaf2;

TEST_CASE("KTunnel")
{
	SECTION("Connection disconnect reason")
	{
		auto Connection = std::make_shared<KTunnel::Connection>(
			1, [](KTunnel::Message&&){});

		CHECK ( Connection->GetDisconnectReason().empty() );

		// a Disconnect frame without payload leaves the reason empty
		Connection->SendData(KTunnel::Message(KTunnel::Message::Disconnect, 1));
		CHECK ( Connection->GetDisconnectReason().empty() );

		// a Disconnect frame with payload carries the peer's error
		Connection->SendData(KTunnel::Message(KTunnel::Message::Disconnect, 1,
		                                      "cannot connect to target host:1: refused"));
		CHECK ( Connection->GetDisconnectReason() == "cannot connect to target host:1: refused" );
	}

	SECTION("Connection wait for late disconnect reason")
	{
		auto Connection = std::make_shared<KTunnel::Connection>(
			2, [](KTunnel::Message&&){});

		// the reason arrives while another thread already waits for it -
		// the exact situation after a downstream that closed first
		std::thread Sender([Connection]()
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
			Connection->SendData(KTunnel::Message(KTunnel::Message::Disconnect, 2, "late error"));
		});

		auto sReason = Connection->WaitForDisconnectReason(chrono::seconds(2));
		CHECK ( sReason == "late error" );

		Sender.join();
	}

	SECTION("Connection wait times out without reason")
	{
		KTunnel::Connection Connection(3, [](KTunnel::Message&&){});

		auto sReason = Connection.WaitForDisconnectReason(chrono::milliseconds(20));
		CHECK ( sReason.empty() );
	}

	SECTION("Connection target and peer diagnostics")
	{
		KTunnel::Connection Connection(4, [](KTunnel::Message&&){});

		CHECK ( Connection.GetTarget().empty() );
		CHECK ( Connection.GetPeer().empty()   );

		Connection.SetTarget(KTCPEndPoint("db.example.com", 3306));
		Connection.SetPeer("10.0.0.1:54321");

		CHECK ( Connection.GetTarget().Serialize() == "db.example.com:3306" );
		CHECK ( Connection.GetPeer()               == "10.0.0.1:54321"      );
		CHECK ( Connection.GetStartTime() > KUnixTime() );
		CHECK ( Connection.GetBytesToDirect()   == 0 );
		CHECK ( Connection.GetBytesFromDirect() == 0 );
	}
}

TEST_CASE("KTunnel inlet relay")
{
	SECTION("a tunnel name round-trips through KTCPEndPoint")
	{
		// the inlet encodes the tunnel name in the Connect frame's
		// endpoint - the relay reads it back from the domain part
		KTCPEndPoint Named("sqlserver", 0);
		KTCPEndPoint Parsed(Named.Serialize());

		CHECK ( KString(Parsed.Domain.get()) == "sqlserver" );
	}

	SECTION("Connection duplex is what the relay bridges with")
	{
		// the relay pumps InletChannel <-> OutletChannel purely through
		// ReadData()/WriteData(), so verify that pairing in isolation
		std::vector<KTunnel::Message> Sent;

		auto Channel = std::make_shared<KTunnel::Connection>(
			7, [&Sent](KTunnel::Message&& msg) { Sent.push_back(std::move(msg)); });

		// outbound: WriteData turns into a Data frame on our channel
		Channel->WriteData("select 1");

		REQUIRE ( Sent.size() == 1 );
		CHECK   ( Sent[0].GetType()    == KTunnel::Message::Data );
		CHECK   ( Sent[0].GetChannel() == 7                      );
		CHECK   ( Sent[0].GetMessage() == "select 1"             );

		// inbound: a Data frame handed in is what ReadData() returns
		Channel->SendData(KTunnel::Message(KTunnel::Message::Data, 7, "one row"));

		KString sIn;
		REQUIRE ( Channel->ReadData(sIn) );
		CHECK   ( sIn == "one row"       );

		// a Disconnect closes the channel for the reader
		Channel->SendData(KTunnel::Message(KTunnel::Message::Disconnect, 7));
		CHECK ( !Channel->ReadData(sIn) );
	}
}

namespace {

constexpr KStringViewZ s_sTunnelSecret { "ktunnel_utest" };
constexpr KStringViewZ s_sTunnelNode   { "testnode"      };
constexpr std::size_t  s_iTunnelChunk  { 16 * 1024 };

//-----------------------------------------------------------------------------
/// what the threads of an end to end run report, and whether it is being torn down
struct EndToEndState
//-----------------------------------------------------------------------------
{
	std::atomic<bool> bStopping { false };
	std::mutex        Mutex;
	KString           sErrors;

	void AddError(KStringView sError)
	{
		// both tunnel sides end with an exception at the tear down - that is expected
		if (bStopping)
		{
			return;
		}

		std::lock_guard<std::mutex> Lock(Mutex);
		sErrors += sError;
		sErrors += '\n';
	}

}; // EndToEndState

//-----------------------------------------------------------------------------
/// a deterministic payload that does not repeat, so that neither a lost nor a
/// duplicated nor a reordered chunk passes unnoticed
KString MakeTunnelPayload(std::size_t iSeed, std::size_t iSize)
//-----------------------------------------------------------------------------
{
	KString sPayload;
	sPayload.reserve(iSize);

	// xorshift64 - reproducible bytes, not crypto
	uint64_t iState = 0x9E3779B97F4A7C15ULL + iSeed * 0x2545F4914F6CDD1DULL;

	while (sPayload.size() < iSize)
	{
		iState ^= iState << 13;
		iState ^= iState >> 7;
		iState ^= iState << 17;
		sPayload += static_cast<char>(iState & 0xFF);
	}

	return sPayload;

} // MakeTunnelPayload

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// the target at the far end of the tunnel - it echoes everything back
class TunnelEchoServer : public KTCPServer
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

public:

	TunnelEchoServer() : KTCPServer(0, false, 10) {}

protected:

	virtual void Session(std::unique_ptr<KIOStreamSocket>& Stream) override
	{
		std::array<char, s_iTunnelChunk> Buffer;

		for (;;)
		{
			auto iRead = Stream->direct_read_some(Buffer.data(), Buffer.size());

			if (iRead <= 0)
			{
				break;
			}

			if (!Stream->Write(Buffer.data(), iRead).Flush().Good())
			{
				break;
			}
		}
	}

}; // TunnelEchoServer

//-----------------------------------------------------------------------------
/// a certificate for localhost and its key, made once for the test run - with
/// none, a TLS server creates one with a 4096 bit key at every start, which
/// takes up to a second
const std::pair<KString, KString>& TunnelTestCertificate()
//-----------------------------------------------------------------------------
{
	static const auto s_CertAndKey = []()
	{
		KRSAKey  Key(2048);
		KRSACert Cert(Key, "localhost", "US");
		return std::make_pair(Cert.GetPEM(), Key.GetPEM(true));
	}();

	return s_CertAndKey;

} // TunnelTestCertificate

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// the side of the tunnel that waits for the login
class TunnelExposedServer : public KTCPServer
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

public:

	TunnelExposedServer(bool bTLS, EndToEndState& State)
	: KTCPServer(0, bTLS, 4, /*bStoreNewCerts=*/false)
	, m_State(State)
	{
		if (bTLS)
		{
			SetTLSCertificates(TunnelTestCertificate().first, TunnelTestCertificate().second);
		}
	}

	/// the running tunnel, or nullptr while none is established
	KTunnel* GetTunnel() const { return m_pTunnel.load(std::memory_order_acquire); }

protected:

	virtual void Session(std::unique_ptr<KIOStreamSocket>& Stream) override
	{
		KTunnel::Config Config;
		Config.Secrets.insert(s_sTunnelSecret);

		// neither node nor secret - this side waits for the peer to log in
		KTunnel Tunnel(Config, std::move(Stream));

		m_pTunnel.store(&Tunnel, std::memory_order_release);

		try
		{
			Tunnel.Run();
		}
		catch (const std::exception& ex)
		{
			m_State.AddError(kFormat("exposed side: {}", ex.what()));
		}

		m_pTunnel.store(nullptr, std::memory_order_release);
	}

	EndToEndState&        m_State;
	std::atomic<KTunnel*> m_pTunnel { nullptr };

}; // TunnelExposedServer

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// hands each connection to the tunnel, which asks the far side to connect to the echo target
class TunnelForwardServer : public KTCPServer
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

public:

	TunnelForwardServer(const TunnelExposedServer& Exposed, uint16_t iEchoPort, EndToEndState& State)
	: KTCPServer(0, false, 10)
	, m_Exposed(Exposed)
	, m_iEchoPort(iEchoPort)
	, m_State(State)
	{
	}

protected:

	virtual void Session(std::unique_ptr<KIOStreamSocket>& Stream) override
	{
		auto* pTunnel = m_Exposed.GetTunnel();

		if (!pTunnel)
		{
			m_State.AddError("no tunnel for a forwarded connection");
			return;
		}

		try
		{
			// returns when either end closed the channel
			pTunnel->Connect(Stream.get(), KTCPEndPoint(kFormat("127.0.0.1:{}", m_iEchoPort)));
		}
		catch (const std::exception& ex)
		{
			m_State.AddError(kFormat("forward channel: {}", ex.what()));
		}
	}

	const TunnelExposedServer& m_Exposed;
	uint16_t                   m_iEchoPort;
	EndToEndState&             m_State;

}; // TunnelForwardServer

//-----------------------------------------------------------------------------
/// sends a payload through the tunnel and compares the echo - in one thread, with
/// sending and receiving interleaved over one poll, as a socket stream requires.
/// @return empty on success, else the failure
KString RunTunnelDriver(uint16_t iForwardPort, std::size_t iIndex, std::size_t iSize)
//-----------------------------------------------------------------------------
{
	KTCPStream Stream(KTCPEndPoint(kFormat("127.0.0.1:{}", iForwardPort)), KStreamOptions(chrono::seconds(5)));

	if (!Stream.Good())
	{
		return kFormat("cannot connect: {}", Stream.GetLastError());
	}

	auto    sPayload = MakeTunnelPayload(iIndex, iSize);
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
			auto iWrote = Stream.direct_write_some(sPayload.data() + iSent, std::min(s_iTunnelChunk, iSize - iSent));

			if (iWrote < 0)
			{
				return kFormat("write failed after {} bytes", iSent);
			}

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

		if (Stall.elapsed() >= chrono::seconds(5))
		{
			return kFormat("stalled with {} sent and {} of {} echoed", iSent, iGot, iSize);
		}

		int iWhat = POLLIN;

		if (iSent < iSize)
		{
			iWhat |= POLLOUT;
		}

		Stream.CheckIfReady(iWhat, chrono::milliseconds(100), false);
	}

	if (sEchoed != sPayload)
	{
		return kFormat("the echo of {} bytes differs from the payload", iSize);
	}

	return KString{};

} // RunTunnelDriver

//-----------------------------------------------------------------------------
/// the topology of a deployment in one process: drivers send through a forward
/// listener into the exposed tunnel side, the tunnel link carries it to the side
/// that logged in, which connects to the echo target.
/// @return empty on success, else the failures
KString RunTunnelEndToEnd(bool bTLS, std::size_t iConnections, std::size_t iSize)
//-----------------------------------------------------------------------------
{
	EndToEndState State;

	TunnelEchoServer Echo;

	if (!Echo.Start(chrono::seconds(5), false))
	{
		return "cannot start the echo target";
	}

	TunnelExposedServer Exposed(bTLS, State);

	if (!Exposed.Start(chrono::seconds(5), false))
	{
		return "cannot start the tunnel listener";
	}

	TunnelForwardServer Forward(Exposed, Echo.GetPort(), State);

	if (!Forward.Start(chrono::seconds(5), false))
	{
		return "cannot start the forward listener";
	}

	KTCPEndPoint   LinkEndPoint(kFormat("127.0.0.1:{}", Exposed.GetPort()));
	KStreamOptions LinkOptions(chrono::seconds(5));

	std::unique_ptr<KIOStreamSocket> Link;

	if (bTLS)
	{
		Link = std::make_unique<KTLSStream>(LinkEndPoint, LinkOptions);
	}
	else
	{
		Link = std::make_unique<KTCPStream>(LinkEndPoint, LinkOptions);
	}

	if (!Link->Good())
	{
		return kFormat("cannot connect to the tunnel listener: {}", Link->GetLastError());
	}

	KTunnel::Config Config;
	Config.Secrets.insert(s_sTunnelSecret);

	// the constructor logs in and returns once the peer acknowledged - both
	// tunnel sides are established afterwards
	KTunnel Client(Config, std::move(Link), s_sTunnelNode, s_sTunnelSecret);

	std::thread ClientRunner([&Client, &State]()
	{
		try
		{
			Client.Run();
		}
		catch (const std::exception& ex)
		{
			State.AddError(kFormat("logged in side: {}", ex.what()));
		}
	});

	std::vector<KString>     Results(iConnections);
	std::vector<std::thread> Drivers;

	for (std::size_t iDriver = 0; iDriver < iConnections; ++iDriver)
	{
		Drivers.push_back(std::thread([&Results, &Forward, iDriver, iSize]()
		{
			try
			{
				Results[iDriver] = RunTunnelDriver(Forward.GetPort(), iDriver, iSize);
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

	// tear down in order
	State.bStopping = true;

	Forward.Stop();
	Client.Stop();
	ClientRunner.join();
	Exposed.Stop();
	Echo.Stop();

	KString sFailures = State.sErrors;

	for (std::size_t iDriver = 0; iDriver < iConnections; ++iDriver)
	{
		if (!Results[iDriver].empty())
		{
			sFailures += kFormat("connection {}: {}\n", iDriver, Results[iDriver]);
		}
	}

	return sFailures;

} // RunTunnelEndToEnd

} // end of anonymous namespace

TEST_CASE("KTunnel end to end")
{
	// two connections of 256 KB through the tunnel and back, compared byte for
	// byte - a short version of the load test in samples/ktunnel_test.cpp
	constexpr std::size_t iConnections { 2 };
	constexpr std::size_t iSize        { 256 * 1024 };

	SECTION("tunnel link over TCP")
	{
		CHECK ( RunTunnelEndToEnd(false, iConnections, iSize) == "" );
	}

	SECTION("tunnel link over TLS")
	{
		CHECK ( RunTunnelEndToEnd(true, iConnections, iSize) == "" );
	}
}
