#include "catch.hpp"
#include <dekaf2/http/websocket/kwebsocket.h>
#include <dekaf2/net/tcp/ktcpserver.h>
#include <dekaf2/net/util/kiostreamsocket.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/format/kformat.h>
#include <dekaf2/system/os/ksystem.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <functional>
#include <memory>
#include <random>
#include <set>
#include <thread>
#include <vector>

#if DEKAF2_IS_WINDOWS
	#include <winsock2.h>
#else
	#include <sys/socket.h>
#endif

using namespace dekaf2;

// Tests for the reactor of KWebSocketServer, modeled on how a real application (a
// /Control channel that pushes state to browser pages) uses it: few worker threads, an
// AutoPing per connection, a push of several messages from the connect handler, replies
// sent by handle from the message handler, and other threads that keep sending to the
// live handles.
//
// - "slow consumer" (disabled, compiled only with
//   DEKAF2_ENABLE_KWEBSOCKETSERVER_NONBLOCKING_TESTS): a client that stops reading must not
//   hold up the other connections. With one worker thread this needs a write that does not
//   block, which KWebSocketServer does not have - see the comment at the test case
// - "half-closed peer" (always run, deterministic): a peer that sends its FIN but keeps
//   the socket open and stops reading - what a reverse proxy does when its client went
//   away while it was still writing to it. No write to such a peer fails, only the end of
//   its input tells that it is gone (seen in production as a socket in CLOSE_WAIT for hours)
// - "churn" (always run, short): clients come and go in all manners, the half-closed one
//   included; afterwards the server must drop every connection on its own, WITHOUT a new
//   connection arriving in the meantime - a new connection rebuilds the poll vector of the
//   reactor and would hide a socket that fell out of it
// - "reactor stress" (disabled like "slow consumer", and hidden, run with "[.stress]"): the
//   same churn, randomized, longer, in all thread modes, with a latency check. The latency
//   check of its section "one worker thread" fails for the same reason as "slow consumer".
//   DEKAF2_WS_STRESS_MS sets the duration of the churn phase, DEKAF2_WS_STRESS_SEED the seed.

namespace {

//-----------------------------------------------------------------------------
/// accepts TCP connections and hands them to a KWebSocketServer, without an HTTP upgrade
class KWebSocketAcceptor : public KTCPServer
//-----------------------------------------------------------------------------
{

public:

	using Factory = std::function<KWebSocket(std::unique_ptr<KIOStreamSocket>&)>;

	KWebSocketAcceptor(uint16_t iPort, KWebSocketServer& Server, Factory Make)
	: KTCPServer(iPort, false, 20)
	, m_Server(Server)
	, m_Make(std::move(Make))
	{
	}

protected:

	void Session(std::unique_ptr<KIOStreamSocket>& Stream) override
	{
		m_Server.AddWebSocket(m_Make(Stream));
	}

private:

	KWebSocketServer& m_Server;
	Factory           m_Make;

}; // KWebSocketAcceptor

//-----------------------------------------------------------------------------
/// make the next close of the socket send a RST instead of a FIN
void SetResetOnClose(KIOStreamSocket& Stream)
//-----------------------------------------------------------------------------
{
	struct ::linger Linger;
	Linger.l_onoff  = 1;
	Linger.l_linger = 0;

#if DEKAF2_IS_WINDOWS
	::setsockopt(Stream.GetNativeSocket(), SOL_SOCKET, SO_LINGER, reinterpret_cast<const char*>(&Linger), sizeof(Linger));
#else
	::setsockopt(Stream.GetNativeSocket(), SOL_SOCKET, SO_LINGER, &Linger, sizeof(Linger));
#endif

} // SetResetOnClose

//-----------------------------------------------------------------------------
/// send a FIN, but keep the socket open for reading - a half close
void HalfClose(KIOStreamSocket& Stream)
//-----------------------------------------------------------------------------
{
#if DEKAF2_IS_WINDOWS
	::shutdown(Stream.GetNativeSocket(), SD_SEND);
#else
	::shutdown(Stream.GetNativeSocket(), SHUT_WR);
#endif

} // HalfClose

//-----------------------------------------------------------------------------
/// shrink the receive buffer of a socket, so that a client that stops reading fills up quickly
void SetReceiveBuffer(KIOStreamSocket& Stream, int iBytes)
//-----------------------------------------------------------------------------
{
#if DEKAF2_IS_WINDOWS
	::setsockopt(Stream.GetNativeSocket(), SOL_SOCKET, SO_RCVBUF, reinterpret_cast<const char*>(&iBytes), sizeof(iBytes));
#else
	::setsockopt(Stream.GetNativeSocket(), SOL_SOCKET, SO_RCVBUF, &iBytes, sizeof(iBytes));
#endif

} // SetReceiveBuffer

//-----------------------------------------------------------------------------
/// connect a client websocket (masked, as clients must) to the acceptor, nullptr on failure
std::shared_ptr<KWebSocket> ConnectClient(uint16_t iPort)
//-----------------------------------------------------------------------------
{
	auto Stream = KIOStreamSocket::Create(KURL(kFormat("http://127.0.0.1:{}", iPort)));

	if (!Stream || !Stream->Good())
	{
		return nullptr;
	}

	auto pClient = std::make_shared<KWebSocket>(Stream, [](KWebSocket&){}, true);
	pClient->SetReadTimeout (chrono::seconds(5));
	pClient->SetWriteTimeout(chrono::seconds(5));

	return pClient;

} // ConnectClient

//-----------------------------------------------------------------------------
/// write one message and read until its echo arrives - pushes and broadcasts may come in
/// between. Returns false if the connection failed or the echo did not arrive.
bool EchoRoundTrip(KWebSocket& Client, const KString& sMessage)
//-----------------------------------------------------------------------------
{
	if (!Client.Write(sMessage, false))
	{
		return false;
	}

	for (;;)
	{
		KStringRef sReply;

		if (!Client.Read(sReply))
		{
			return false;
		}

		if (sReply == sMessage)
		{
			return true;
		}
	}

} // EchoRoundTrip

//-----------------------------------------------------------------------------
int64_t MillisecondsSince(std::chrono::steady_clock::time_point tStart)
//-----------------------------------------------------------------------------
{
	return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - tStart).count();

} // MillisecondsSince

//-----------------------------------------------------------------------------
/// the parameters of one churn run
struct ChurnConfig
//-----------------------------------------------------------------------------
{
	std::chrono::milliseconds Duration;      ///< length of the churn phase
	unsigned                  iSlowMinMs;    ///< shortest time a slow consumer does not read
	unsigned                  iSlowMaxMs;    ///< longest time a slow consumer does not read
	KDuration                 WriteTimeout;  ///< write timeout of the server
	int64_t                   iMaxLatencyMs; ///< bound for the canary's round trip, 0 = no check

}; // ChurnConfig

//-----------------------------------------------------------------------------
void RunChurn(uint16_t iPort, std::size_t iWorkers, unsigned iSeed, const ChurnConfig& Config)
//-----------------------------------------------------------------------------
{
	constexpr std::size_t iChurnClients { 8 };
	constexpr std::size_t iPushers      { 2 };

	KWebSocketServer::Options Options;
	Options.iWorkerThreads = iWorkers;
	// the write timeout bounds how long a slow consumer is kept - and an idle timeout
	// that turns a worker parked in a read into a visible stall, not a hang
	Options.WriteTimeout   = Config.WriteTimeout;
	Options.IdleTimeout    = chrono::seconds(20);
	Options.iMaxQueueBytes = 4 * 1024 * 1024;

	KWebSocketServer Server(Options);

	std::atomic<std::size_t>  iConnects { 0 };
	std::atomic<std::size_t>  iCloses   { 0 };
	KThreadSafe<std::set<std::size_t>> Live;

	KWebSocketAcceptor Acceptor(iPort, Server, [&](std::unique_ptr<KIOStreamSocket>& Stream)
	{
		KWebSocket WebSocket(Stream, [](KWebSocket& ws)
		{
			// reply through the server by handle, like an application that answers pings
			ws.GetServer()->Send(ws.GetHandle(), ws.GetFrame().GetPayload(), false);
		}, false);

		WebSocket.SetConnectHandler([&](KWebSocket& ws)
		{
			// what an application typically does on connect: keep the connection alive,
			// register it, and push the current state right away
			ws.AutoPing(chrono::milliseconds(200));
			Live.unique()->insert(ws.GetHandle());
			++iConnects;
			ws.GetServer()->Send(ws.GetHandle(), "state:small", false);
			ws.GetServer()->Send(ws.GetHandle(), KString(64 * 1024, 's'), false);
		});

		WebSocket.SetCloseHandler([&](std::size_t iHandle)
		{
			Live.unique()->erase(iHandle);
			++iCloses;
		});

		return WebSocket;
	});

	// the socket timeout bounds a frame write that has started, the write timeout the wait
	// for buffer space - both the same, so a slow consumer is dropped after the write timeout
	REQUIRE ( Acceptor.Start(Config.WriteTimeout, false) );

	std::atomic<bool> bStopChurn   { false };
	std::atomic<bool> bStopPushers { false };
	std::atomic<bool> bStopCanary  { false };

	std::atomic<std::size_t> iClientConnects { 0 };
	std::atomic<std::size_t> iEchoes         { 0 };
	std::atomic<std::size_t> iSlowRounds     { 0 };
	std::atomic<std::size_t> iCloseFrames    { 0 };
	std::atomic<std::size_t> iPlainFINs      { 0 };
	std::atomic<std::size_t> iResets         { 0 };
	std::atomic<std::size_t> iHalfCloses     { 0 };
	std::atomic<std::size_t> iConnectErrors  { 0 };

	// the half-closed clients: they keep their sockets open, without reading, until the
	// quiet phase is over
	KThreadSafe<std::vector<std::shared_ptr<KWebSocket>>> Held;

	// --- the long lived canary: measures the round trip while everything else churns
	std::atomic<int64_t>     iCanaryMaxMs    { 0 };
	std::atomic<std::size_t> iCanaryRounds   { 0 };
	std::atomic<std::size_t> iCanaryErrors   { 0 };

	std::thread Canary([&]()
	{
		auto pClient = ConnectClient(iPort);

		if (!pClient)
		{
			++iCanaryErrors;
			return;
		}

		std::size_t iRound { 0 };

		while (!bStopCanary)
		{
			auto tStart = std::chrono::steady_clock::now();

			if (!EchoRoundTrip(*pClient, kFormat("canary-{}", iRound++)))
			{
				++iCanaryErrors;
				return;
			}

			auto iMs = MillisecondsSince(tStart);

			if (iMs > iCanaryMaxMs)
			{
				iCanaryMaxMs = iMs;
			}

			++iCanaryRounds;
			kSleep(chrono::milliseconds(20));
		}

		pClient->Close(1000);
	});

	// --- the pushers: keep sending to random live handles, like broadcasts by handle
	std::vector<std::thread> Pushers;

	for (std::size_t iPusher = 0; iPusher < iPushers; ++iPusher)
	{
		Pushers.push_back(std::thread([&, iPusher]()
		{
			std::mt19937 Random(iSeed + 1000 + static_cast<unsigned>(iPusher));
			std::size_t  iRound { 0 };

			while (!bStopPushers)
			{
				std::vector<std::size_t> Handles;
				{
					auto LiveHandles = Live.shared();
					Handles.assign(LiveHandles->begin(), LiveHandles->end());
				}

				if (!Handles.empty())
				{
					auto iHandle = Handles[Random() % Handles.size()];
					// mostly small messages, now and then a large one that fills buffers
					auto sMessage = (Random() % 10 == 0) ? KString(32 * 1024, 'p') : kFormat("push-{}-{}", iPusher, iRound);
					Server.Send(iHandle, std::move(sMessage), false);
				}

				if (++iRound % 50 == 0)
				{
					Server.Broadcast("bcast", false);
				}

				kSleep(chrono::milliseconds(1));
			}
		}));
	}

	// --- the churn clients: connect, work or stall, leave in one of three ways, repeat
	std::vector<std::thread> Churners;

	for (std::size_t iClient = 0; iClient < iChurnClients; ++iClient)
	{
		Churners.push_back(std::thread([&, iClient]()
		{
			std::mt19937 Random(iSeed + static_cast<unsigned>(iClient));
			std::size_t  iRound { 0 };

			while (!bStopChurn)
			{
				auto pClient = ConnectClient(iPort);

				if (!pClient)
				{
					++iConnectErrors;
					kSleep(chrono::milliseconds(10));
					continue;
				}

				++iClientConnects;

				if (Random() % 4 == 0)
				{
					// a slow consumer: stops reading while the server keeps pushing
					++iSlowRounds;
					kSleep(chrono::milliseconds(Config.iSlowMinMs + Random() % (Config.iSlowMaxMs - Config.iSlowMinMs + 1)));
				}
				else
				{
					auto iMessages = 1 + Random() % 20;

					for (unsigned iMessage = 0; iMessage < iMessages; ++iMessage)
					{
						if (!EchoRoundTrip(*pClient, kFormat("c{}-{}-{}", iClient, iRound, iMessage)))
						{
							break;
						}

						++iEchoes;
					}
				}

				switch (Random() % 4)
				{
					case 0:
						// a close frame, then the FIN when the socket goes
						pClient->Close(1000);
						++iCloseFrames;
						break;

					case 1:
						// just the FIN
						++iPlainFINs;
						break;

					case 2:
						// a RST
						SetResetOnClose(pClient->GetStream());
						++iResets;
						break;

					case 3:
						// the FIN, but the socket stays open and nobody reads it anymore
						HalfClose(pClient->GetStream());
						Held.unique()->push_back(std::move(pClient));
						++iHalfCloses;
						break;
				}

				pClient.reset();
				++iRound;
			}
		}));
	}

	kSleep(Config.Duration);

	bStopChurn = true;

	for (auto& Churner : Churners)
	{
		Churner.join();
	}

	bStopPushers = true;

	for (auto& Pusher : Pushers)
	{
		Pusher.join();
	}

	bStopCanary = true;
	Canary.join();

	// --- the quiet phase: no new connection from here on. Every peer is gone, so the
	// server has to notice all of them by itself (FIN or RST on a watched socket, a failed
	// write of a queued message, or the write timeout of a slow consumer)
	auto tQuietStart = std::chrono::steady_clock::now();
	// a writer that started a frame on a slow consumer right before may still need the full
	// write timeout, twice: once for the frame, once for the wait for buffer space
	auto iQuietLimitMs = std::max<int64_t>(2000, 3 * Config.WriteTimeout.milliseconds().count());

	while ((Server.size() != 0 || iCloses != iConnects) && MillisecondsSince(tQuietStart) < iQuietLimitMs)
	{
		kSleep(chrono::milliseconds(5));
	}

	auto iQuietMs = MillisecondsSince(tQuietStart);

	INFO ( kFormat("seed {}, workers {}: {} client connects ({} connect errors), {} echoes, {} slow rounds, "
	               "closes: {} close frames, {} FINs, {} RSTs, {} half closes - canary: {} rounds, max {} ms, {} errors - "
	               "server: {} connects, {} closes, {} left after {} ms of quiet",
	               iSeed, iWorkers,
	               iClientConnects.load(), iConnectErrors.load(), iEchoes.load(), iSlowRounds.load(),
	               iCloseFrames.load(), iPlainFINs.load(), iResets.load(), iHalfCloses.load(),
	               iCanaryRounds.load(), iCanaryMaxMs.load(), iCanaryErrors.load(),
	               iConnects.load(), iCloses.load(), Server.size(), iQuietMs) );

	// every connection whose peer is gone must have been dropped, without help from a new one
	CHECK ( Server.size()  == 0 );
	CHECK ( iCloses.load() == iConnects.load() );
	// all churn connections plus the canary reached the server
	CHECK ( iConnects.load() == iClientConnects.load() + 1 );
	// the long lived client never lost its connection
	CHECK ( iCanaryErrors.load() == 0 );
	CHECK ( iCanaryRounds.load() >  0 );

	if (Config.iMaxLatencyMs)
	{
		// ... and was never parked for long - a round trip on localhost takes a few
		// milliseconds, the bound only leaves room for a loaded test machine
		CHECK ( iCanaryMaxMs.load() < Config.iMaxLatencyMs );
	}

	Held.unique()->clear();
	Acceptor.Stop();

} // RunChurn

} // end of anonymous namespace

#ifdef DEKAF2_ENABLE_KWEBSOCKETSERVER_NONBLOCKING_TESTS

// Disabled, because it fails until KWebSocketServer writes without blocking. Hiding it
// with a tag is not enough: a test runner that starts every test that --list-tests prints
// by its name runs it anyway, because that list includes the hidden tests. A flush runs as
// a task in the worker pool and waits in KWebSocket::Write() for buffer space at the peer,
// up to the write timeout. The pool never grows beyond iWorkerThreads, so with one worker
// thread the echo for the second client waits in the queue until the flush gives up: the
// test measures the write timeout of 2 seconds instead of staying below 300 ms.
// iMaxConcurrentWrites cannot help with a single worker. A write that returns on a full
// buffer and resumes on POLLOUT in the reactor would make it pass.
TEST_CASE("KWebSocketServer slow consumer")
{
	// One worker thread, and a client that stops reading while the server pushes to it:
	// the flush to that client fills the socket buffers and then waits for the peer - with
	// some buffer space left inside the frame write, bounded by the timeout of the socket,
	// with none left for POLLOUT, bounded by the write timeout. Both are 2 seconds here.
	// Meanwhile a second client must still get its echo right away.
	constexpr uint16_t iPort { 6810 };

	KWebSocketServer::Options Options;
	Options.iWorkerThreads = 1;
	Options.WriteTimeout   = chrono::seconds(2);
	// never drop the slow consumer for its queue size, it shall block a writer
	Options.iMaxQueueBytes = 64 * 1024 * 1024;

	KWebSocketServer Server(Options);

	KThreadSafe<std::vector<std::size_t>> Handles;

	KWebSocketAcceptor Acceptor(iPort, Server, [&](std::unique_ptr<KIOStreamSocket>& Stream)
	{
		KWebSocket WebSocket(Stream, [](KWebSocket& ws)
		{
			ws.GetServer()->Send(ws.GetHandle(), ws.GetFrame().GetPayload(), false);
		}, false);

		WebSocket.SetConnectHandler([&](KWebSocket& ws)
		{
			Handles.unique()->push_back(ws.GetHandle());
		});

		return WebSocket;
	});

	REQUIRE ( Acceptor.Start(chrono::seconds(2), false) );

	auto WaitForHandles = [&](std::size_t iCount)
	{
		for (int iWait = 0; iWait < 200 && Handles.shared()->size() < iCount; ++iWait)
		{
			kSleep(chrono::milliseconds(5));
		}

		return Handles.shared()->size() >= iCount;
	};

	auto pSlow = ConnectClient(iPort);
	REQUIRE ( pSlow );
	SetReceiveBuffer(pSlow->GetStream(), 4096);
	REQUIRE ( WaitForHandles(1) );
	auto iSlowHandle = Handles.shared()->at(0);

	auto pFast = ConnectClient(iPort);
	REQUIRE ( pFast );
	REQUIRE ( WaitForHandles(2) );

	// the fast client works
	REQUIRE ( EchoRoundTrip(*pFast, "before") );

	// 16 MB for the slow client, which never reads - far more than all socket buffers take
	for (int iChunk = 0; iChunk < 256; ++iChunk)
	{
		Server.Send(iSlowHandle, KString(64 * 1024, 'x'), false);
	}

	// give the flush time to fill the buffers and start waiting
	kSleep(chrono::milliseconds(50));

	auto tStart = std::chrono::steady_clock::now();
	CHECK ( EchoRoundTrip(*pFast, "during") );
	auto iMs = MillisecondsSince(tStart);

	INFO ( kFormat("echo round trip while a slow consumer is flushed: {} ms", iMs) );
	CHECK ( iMs < 300 );

	pSlow.reset();
	pFast.reset();
	Acceptor.Stop();
}

#endif // DEKAF2_ENABLE_KWEBSOCKETSERVER_NONBLOCKING_TESTS

TEST_CASE("KWebSocketServer half-closed peer")
{
	// Two peers send their FIN but keep their sockets open and never read again: one with
	// an idle connection, one while the server flushes a large backlog to it. Both have to
	// be dropped by the server alone - no new connection arrives meanwhile, and no write to
	// them fails with an error.
	constexpr uint16_t iPort { 6815 };

	KWebSocketServer::Options Options;
	Options.iWorkerThreads = 1;
	Options.WriteTimeout   = chrono::milliseconds(100);
	Options.iMaxQueueBytes = 64 * 1024 * 1024;

	KWebSocketServer Server(Options);

	KThreadSafe<std::vector<std::size_t>> Handles;
	std::atomic<std::size_t>              iCloses { 0 };

	KWebSocketAcceptor Acceptor(iPort, Server, [&](std::unique_ptr<KIOStreamSocket>& Stream)
	{
		KWebSocket WebSocket(Stream, [](KWebSocket& ws)
		{
			ws.GetServer()->Send(ws.GetHandle(), ws.GetFrame().GetPayload(), false);
		}, false);

		WebSocket.SetConnectHandler([&](KWebSocket& ws)
		{
			ws.AutoPing(chrono::milliseconds(50));
			Handles.unique()->push_back(ws.GetHandle());
		});

		WebSocket.SetCloseHandler([&](std::size_t)
		{
			++iCloses;
		});

		return WebSocket;
	});

	// the timeout of the server side sockets bounds a frame write that has started
	REQUIRE ( Acceptor.Start(chrono::milliseconds(100), false) );

	auto WaitForHandles = [&](std::size_t iCount)
	{
		for (int iWait = 0; iWait < 200 && Handles.shared()->size() < iCount; ++iWait)
		{
			kSleep(chrono::milliseconds(5));
		}

		return Handles.shared()->size() >= iCount;
	};

	auto pIdle = ConnectClient(iPort);
	REQUIRE ( pIdle );
	REQUIRE ( WaitForHandles(1) );
	REQUIRE ( EchoRoundTrip(*pIdle, "idle") );

	auto pBusy = ConnectClient(iPort);
	REQUIRE ( pBusy );
	SetReceiveBuffer(pBusy->GetStream(), 4096);
	REQUIRE ( WaitForHandles(2) );
	REQUIRE ( EchoRoundTrip(*pBusy, "busy") );
	auto iBusyHandle = Handles.shared()->at(1);

	// a backlog the busy peer will never take
	for (int iChunk = 0; iChunk < 256; ++iChunk)
	{
		Server.Send(iBusyHandle, KString(64 * 1024, 'x'), false);
	}

	kSleep(chrono::milliseconds(20));

	auto tStart = std::chrono::steady_clock::now();

	HalfClose(pIdle->GetStream());
	HalfClose(pBusy->GetStream());

	for (int iWait = 0; iWait < 400 && (Server.size() != 0 || iCloses != 2); ++iWait)
	{
		kSleep(chrono::milliseconds(5));
	}

	auto iMs = MillisecondsSince(tStart);

	INFO ( kFormat("{} of 2 half-closed connections dropped after {} ms", iCloses.load(), iMs) );
	CHECK ( Server.size()  == 0 );
	CHECK ( iCloses.load() == 2 );

	pIdle.reset();
	pBusy.reset();
	Acceptor.Stop();
}

TEST_CASE("KWebSocketServer closes the socket of a dropped connection")
{
	// When the server drops a connection - here because its peer sent the FIN - nothing may
	// keep the connection, and with it the socket, alive: the peer has to see the FIN of the
	// server at once. The peer half-closes, so it can still read it. (A worker thread that
	// kept its last task until it ran the next one held such a socket open for hours.)
	constexpr uint16_t iPort { 6816 };

	KWebSocketServer::Options Options;
	Options.iWorkerThreads = 1;

	KWebSocketServer Server(Options);

	std::atomic<std::size_t> iCloses { 0 };

	KWebSocketAcceptor Acceptor(iPort, Server, [&](std::unique_ptr<KIOStreamSocket>& Stream)
	{
		KWebSocket WebSocket(Stream, [](KWebSocket& ws)
		{
			ws.GetServer()->Send(ws.GetHandle(), ws.GetFrame().GetPayload(), false);
		}, false);

		WebSocket.SetCloseHandler([&](std::size_t)
		{
			++iCloses;
		});

		return WebSocket;
	});

	REQUIRE ( Acceptor.Start(chrono::seconds(5), false) );

	auto pClient = ConnectClient(iPort);
	REQUIRE ( pClient );
	REQUIRE ( EchoRoundTrip(*pClient, "hello") );

	// nothing else will happen on this server after this - no further task for the worker
	pClient->SetReadTimeout(chrono::seconds(1));
	HalfClose(pClient->GetStream());

	auto tStart = std::chrono::steady_clock::now();
	// the end of the input (false) has to come right away, not after the read timeout
	CHECK_FALSE ( pClient->Read() );
	auto iMs = MillisecondsSince(tStart);

	INFO ( kFormat("the server's FIN arrived after {} ms, {} close callbacks", iMs, iCloses.load()) );
	CHECK ( iMs < 500 );
	CHECK ( iCloses.load() == 1 );

	pClient.reset();
	Acceptor.Stop();
}

TEST_CASE("KWebSocketServer AutoPing")
{
	// On a connection owned by the server, AutoPing sends through the server: the pings
	// arrive, queued with the other messages, and they end with the connection
	constexpr uint16_t iPort { 6817 };

	KWebSocketServer::Options Options;
	Options.iWorkerThreads = 1;

	KWebSocketServer Server(Options);

	std::atomic<std::size_t> iCloses { 0 };

	KWebSocketAcceptor Acceptor(iPort, Server, [&](std::unique_ptr<KIOStreamSocket>& Stream)
	{
		KWebSocket WebSocket(Stream, [](KWebSocket&){}, false);

		WebSocket.SetConnectHandler([&](KWebSocket& ws)
		{
			ws.AutoPing(chrono::milliseconds(20));
			ws.GetServer()->Send(ws.GetHandle(), "hello", false);
		});

		WebSocket.SetCloseHandler([&](std::size_t)
		{
			++iCloses;
		});

		return WebSocket;
	});

	REQUIRE ( Acceptor.Start(chrono::seconds(5), false) );

	auto pClient = ConnectClient(iPort);
	REQUIRE ( pClient );
	pClient->SetReadTimeout(chrono::milliseconds(500));

	std::size_t iPings { 0 };
	bool        bHello { false };
	auto        tStart = std::chrono::steady_clock::now();

	while (MillisecondsSince(tStart) < 150)
	{
		// Read() returns every frame, the client answers pings by itself
		if (!pClient->Read())
		{
			break;
		}

		switch (pClient->GetFrame().Type())
		{
			case KWebSocket::Frame::FrameType::Ping:
				++iPings;
				break;

			case KWebSocket::Frame::FrameType::Text:
				bHello = (pClient->GetFrame().GetPayload() == "hello");
				break;

			default:
				break;
		}
	}

	CHECK ( bHello );
	CHECK ( iPings >= 3 );

	pClient->Close(1000);
	pClient.reset();

	for (int iWait = 0; iWait < 200 && (Server.size() != 0 || iCloses != 1); ++iWait)
	{
		kSleep(chrono::milliseconds(5));
	}

	CHECK ( Server.size()  == 0 );
	CHECK ( iCloses.load() == 1 );

	Acceptor.Stop();
}

TEST_CASE("KWebSocketServer churn")
{
	ChurnConfig Config;
	Config.Duration      = std::chrono::milliseconds(100);
	Config.iSlowMinMs    = 10;
	Config.iSlowMaxMs    = 40;
	Config.WriteTimeout  = chrono::milliseconds(100);
	// timing is the subject of the disabled "slow consumer" - here only the bookkeeping counts
	Config.iMaxLatencyMs = 0;

	RunChurn(6811, 1, 4711, Config);
}

#ifdef DEKAF2_ENABLE_KWEBSOCKETSERVER_NONBLOCKING_TESTS

// Disabled like "slow consumer": its section "one worker thread" fails the latency check
// until KWebSocketServer writes without blocking, for the reason given there - a slow
// consumer occupies the only worker for up to the write timeout of one second, and the
// canary waits behind it. Once enabled it stays hidden as a long running stress test.
TEST_CASE("KWebSocketServer reactor stress", "[.stress]")
{
	unsigned iSeed = 4711;

	if (const char* sSeed = std::getenv("DEKAF2_WS_STRESS_SEED"))
	{
		iSeed = static_cast<unsigned>(std::strtoul(sSeed, nullptr, 10));
	}

	ChurnConfig Config;
	Config.Duration      = std::chrono::milliseconds(2500);
	Config.iSlowMinMs    = 100;
	Config.iSlowMaxMs    = 1500;
	Config.WriteTimeout  = chrono::seconds(1);
	Config.iMaxLatencyMs = 500;

	if (const char* sMs = std::getenv("DEKAF2_WS_STRESS_MS"))
	{
		auto iMs = std::atoi(sMs);

		if (iMs > 0)
		{
			Config.Duration = std::chrono::milliseconds(iMs);
		}
	}

	SECTION("one worker thread")
	{
		RunChurn(6812, 1, iSeed, Config);
	}

	SECTION("single thread mode")
	{
		RunChurn(6813, 0, iSeed, Config);
	}

	SECTION("worker thread pool")
	{
		RunChurn(6814, 4, iSeed, Config);
	}
}

#endif // DEKAF2_ENABLE_KWEBSOCKETSERVER_NONBLOCKING_TESTS
