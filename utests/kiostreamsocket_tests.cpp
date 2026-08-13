#include "catch.hpp"

#include <dekaf2/net/tcp/ktcpserver.h>
#include <dekaf2/net/tcp/ktcpstream.h>
#include <dekaf2/net/util/kstreamoptions.h>
#include <dekaf2/system/os/ksystem.h>

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
}
