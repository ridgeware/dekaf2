#include "catch.hpp"

#include <dekaf2/net/tcp/ktcpserver.h>
#include <dekaf2/net/tcp/ktcpstream.h>
#include <dekaf2/net/util/kstreamoptions.h>
#include <dekaf2/system/os/ksystem.h>
#include <dekaf2/core/format/kformat.h>

#include <atomic>

using namespace dekaf2;

namespace {

class KPingPongServer : public KTCPServer
{

public:

	using KTCPServer::KTCPServer;

protected:

	virtual bool Accepted(std::unique_ptr<KIOStreamSocket>& stream) override
	{
		stream->SetReaderRightTrim("\r\n");
		stream->SetWriterEndOfLine("\r\n");
		return true;
	}

	virtual KString Request(KStringRef& sLine, Parameters& parameters) override
	{
		// one line in, two lines out - the second line ends up in the
		// client's streambuf, where IsReadReady() has to spot it
		if (sLine == "ping") return "pong\r\nextra";
		return {};
	}

}; // KPingPongServer

/// accepts a connection and then never reads from it, so the sending side
/// fills up its buffers
class KSilentServer : public KTCPServer
{

public:

	using KTCPServer::KTCPServer;

protected:

	virtual void Session(std::unique_ptr<KIOStreamSocket>& stream) override
	{
		kSleep(chrono::seconds(3));
	}

}; // KSilentServer

/// notes the local end of the connection it accepted
class KLocalEndServer : public KTCPServer
{

public:

	using KTCPServer::KTCPServer;

	std::atomic<uint16_t> iAcceptedAtPort { 0 };
	std::atomic<bool>     bAcceptedAtLoopback { false };

protected:

	virtual void Session(std::unique_ptr<KIOStreamSocket>& stream) override
	{
		auto Local = stream->GetLocalEndPoint();
		// a listener for both families reports an IPv4 peer's connection
		// in the mapped form, ::ffff:127.0.0.1
		bAcceptedAtLoopback = (Local.Domain.get() == "127.0.0.1" || Local.Domain.get().contains("::ffff:127.0.0.1"));
		iAcceptedAtPort     = Local.Port.get();
		kSleep(chrono::milliseconds(500));
	}

}; // KLocalEndServer

} // end of anonymous namespace

TEST_CASE("KIOStreamSocket")
{
	SECTION("CancelOnTimeout makes read timeouts repeatable")
	{
		KPingPongServer Server(7611, false, 2);
		Server.Start(chrono::seconds(5), false);

		KTCPStream Stream(KTCPEndPoint("127.0.0.1:7611"),
		                  KStreamOptions(KStreamOptions::CancelOnTimeout, chrono::milliseconds(200)));

		CHECK ( Stream.Good()    == true );
		CHECK ( Stream.is_open() == true );

		Stream.SetReaderRightTrim("\r\n");
		Stream.SetWriterEndOfLine("\r\n");

		KString sLine;

		// two timed out reads on the quiet connection - each returns empty,
		// but the connection survives and stays usable
		CHECK ( Stream.ReadLine(sLine) == false );
		CHECK ( Stream.is_open()       == true  );
		CHECK ( Stream.Good()          == true  );

		static_cast<std::iostream&>(Stream).clear();

		CHECK ( Stream.ReadLine(sLine) == false );
		CHECK ( Stream.is_open()       == true  );

		static_cast<std::iostream&>(Stream).clear();

		// the eofbit from the timed out reads must not block the write side:
		// Flush() delivers past the ostream sentry
		Stream.WriteLine("ping").Flush();

		CHECK ( Stream.ReadLine(sLine) == true );
		CHECK ( sLine == "pong" );

		// "extra" already sits in the streambuf - IsReadReady() has to see
		// it without touching the socket
		CHECK ( Stream.IsReadReady(chrono::milliseconds(0)) == true );

		CHECK ( Stream.ReadLine(sLine) == true );
		CHECK ( sLine == "extra" );

		// nothing left on any layer - a zero timeout poll returns false
		CHECK ( Stream.IsReadReady(chrono::milliseconds(0), false) == false );
	}

	SECTION("direct_write_some reports partial writes and keeps the stream")
	{
		KSilentServer Server(7614, false, 2);
		Server.Start(chrono::seconds(10), false);

		KTCPStream Stream(KTCPEndPoint("127.0.0.1:7614"),
		                  KStreamOptions(KStreamOptions::CancelOnTimeout, chrono::milliseconds(300)));

		REQUIRE ( Stream.Good() == true );

		KString sBlob(64 * 1024, 'x');

		std::size_t     iTotal { 0 };
		std::streamsize iWrote { 0 };
		bool            bShortWrite { false };

		// the peer never reads, so the buffers fill up and the writes get
		// short - which is a normal result here, not a stream error
		for (uint16_t iRound = 0; iRound < 2000; ++iRound)
		{
			iWrote = Stream.direct_write_some(sBlob.data(), sBlob.size());

			if (iWrote < 0) break;

			iTotal += static_cast<std::size_t>(iWrote);

			if (static_cast<std::size_t>(iWrote) < sBlob.size())
			{
				bShortWrite = true;
				break;
			}
		}

		CHECK ( iTotal      >  0    );
		CHECK ( bShortWrite == true );

		// and the connection is still alive and usable afterwards - this is
		// what a Write()/kWrite() would have broken with a badbit
		CHECK ( Stream.is_open()                                   == true  );
		CHECK ( Stream.Good()                                      == true  );
		CHECK ( Stream.IsReadReady(chrono::milliseconds(0), false) == false );
	}

	SECTION("GetLocalEndPoint")
	{
		KLocalEndServer Server(7616, false, 2);
		Server.Start(chrono::seconds(5), false);

		{
			// never connected: no local end
			KTCPStream Idle;
			CHECK ( Idle.GetLocalEndPoint().empty() == true );
		}

		KTCPStream Stream(KTCPEndPoint("127.0.0.1:7616"), KStreamOptions(chrono::seconds(2)));

		CHECK ( Stream.Good() == true );

		// the connecting end: the loopback address, and a port of the
		// system's choosing - not the one we connected to
		auto Local = Stream.GetLocalEndPoint();

		CHECK ( Local.empty()      == false       );
		CHECK ( Local.Domain.get() == "127.0.0.1" );
		CHECK ( Local.Port.get()   != 0           );
		CHECK ( Local.Port.get()   != 7616        );
		CHECK ( Local.Serialize()  == kFormat("127.0.0.1:{}", Local.Port.get()) );

		// the same through the base class, as a protocol implementation holds it
		KIOStreamSocket& Base = Stream;
		CHECK ( Base.GetLocalEndPoint().Serialize() == Local.Serialize() );

		// the accepting end: the address and port it listens at
		for (int i = 0; i < 100 && Server.iAcceptedAtPort == 0; ++i) kSleep(chrono::milliseconds(10));

		CHECK ( Server.iAcceptedAtPort     == 7616 );
		CHECK ( Server.bAcceptedAtLoopback == true );

		// gone with the connection
		Stream.Disconnect();
		CHECK ( Stream.GetLocalEndPoint().empty() == true );
	}
}
