#include "catch.hpp"

#include <dekaf2/http/server/kserversenteventserver.h>
#include <dekaf2/rest/framework/krest.h>
#include <dekaf2/rest/framework/krestserver.h>
#include <dekaf2/http/server/khttperror.h>
#include <dekaf2/net/tcp/ktcpstream.h>
#include <dekaf2/net/util/kstreamoptions.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/format/kformat.h>
#include <dekaf2/system/os/ksystem.h>
#include <atomic>
#include <mutex>
#include <vector>

using namespace dekaf2;

namespace {

//-----------------------------------------------------------------------------
/// a REST server with routes that give their connections to the event server
struct EventTestServer
//-----------------------------------------------------------------------------
{
	explicit EventTestServer(KServerSentEventServer::Options EventOptions, uint16_t iMaxConnections = 20)
	{
		// each connection gets a first event with its handle, sent before the route returned
		Routes.AddRoute("/events/:TOPIC").Get([this](KRESTServer& HTTP)
		{
			auto iHandle = HTTP.SubscribeToEvents(HTTP.GetQueryParm(":TOPIC"));

			bSentBeforeReturn = HTTP.GetEventServer().Send(iHandle, { KString::to_string(iHandle), "hello" });
		});

		Routes.AddRoute("/fail").Get([](KRESTServer& HTTP)
		{
			HTTP.SubscribeToEvents("news");

			throw KHTTPError { KHTTPError::H5xx_ERROR, "failed after subscribing" };
		});

		Routes.AddRoute("/json").Get([](KRESTServer& HTTP)
		{
			HTTP.json.tx["hello"] = "world";
		});

		Rest.GetEventServer().SetOptions(std::move(EventOptions));

		Rest.GetEventServer().SetCloseCallback([this](KServerSentEventServer::Handle iHandle)
		{
			std::lock_guard<std::mutex> Lock(Mutex);
			Closed.push_back(iHandle);
		});

		Options.Type                 = KREST::HTTP;
		Options.iPort                = 0;
		Options.sBindAddress         = "127.0.0.1";
		Options.Timeout              = chrono::seconds(2);
		Options.iMaxConnections      = iMaxConnections;
		Options.bBlocking            = false;
		Options.bCreateEphemeralCert = false;

		REQUIRE ( Rest.Execute(Options, Routes) );

		sEndPoint = kFormat("127.0.0.1:{}", Rest.GetPort());
	}

	KServerSentEventServer& Events()
	{
		return Rest.GetEventServer();
	}

	// waits until the event server has iCount connections, for five seconds at most
	bool WaitForSize(std::size_t iCount)
	{
		for (std::size_t i = 0; i < 500 && Events().size() != iCount; ++i)
		{
			kSleep(chrono::milliseconds(10));
		}

		return Events().size() == iCount;
	}

	bool WasClosed(KServerSentEventServer::Handle iHandle)
	{
		std::lock_guard<std::mutex> Lock(Mutex);

		for (auto iClosed : Closed)
		{
			if (iClosed == iHandle)
			{
				return true;
			}
		}

		return false;
	}

	// the server keeps references to its options and routes, and calls
	// the close callback until it is destroyed
	KREST::Options                               Options;
	KRESTRoutes                                  Routes;
	std::mutex                                   Mutex;
	std::vector<KServerSentEventServer::Handle>  Closed;
	std::atomic<bool>                            bSentBeforeReturn { false };
	KString                                      sEndPoint;
	KREST                                        Rest;

}; // EventTestServer

//-----------------------------------------------------------------------------
/// a client that requests an event stream, and reads its events
struct EventClient
//-----------------------------------------------------------------------------
{
	EventClient(KStringView sEndPoint, KStringView sPath)
	: Stream(KTCPEndPoint(sEndPoint), KStreamOptions(chrono::seconds(5)))
	{
		REQUIRE ( Stream.is_open() );

		Stream.Write(kFormat("GET {} HTTP/1.1\r\nHost: a\r\n\r\n", sPath)).Flush();

		KString sLine;

		while (Stream.ReadLine(sLine) && !sLine.empty())
		{
			sHeader += sLine;
			sHeader += '\n';
		}
	}

	// reads the next line of the stream, false at its end
	bool NextLine(KString& sLine)
	{
		if (!Stream.ReadLine(sLine))
		{
			return false;
		}

		Parser.Add(sLine);
		Parser.Add("\n");

		return true;
	}

	// reads until an event is complete, false at the end of the stream
	bool Next(KServerSentEvent& Event)
	{
		KString sLine;

		while (!Parser.Next(Event))
		{
			if (!NextLine(sLine))
			{
				return false;
			}
		}

		return true;
	}

	// reads the first event of the routes, which has the handle of the connection
	KServerSentEventServer::Handle Hello()
	{
		KServerSentEvent Event;

		REQUIRE ( Next(Event) );
		REQUIRE ( Event.sEvent == "hello" );

		return Event.sData.UInt64();
	}

	KTCPStream             Stream;
	KString                sHeader;
	KServerSentEventParser Parser;

}; // EventClient

// options without comment lines, which would come between the events of a test
KServerSentEventServer::Options QuietOptions()
{
	KServerSentEventServer::Options Options;
	Options.KeepAliveInterval = KDuration::zero();
	return Options;
}

} // end of anonymous namespace

TEST_CASE("KServerSentEventServer")
{
	SECTION("events for a topic, for a handle, and for all")
	{
		EventTestServer Server(QuietOptions());
		EventClient     Client(Server.sEndPoint, "/events/news");

		INFO  ( Client.sHeader );
		CHECK ( Client.sHeader.starts_with("HTTP/1.1 200 ") );
		CHECK ( Client.sHeader.contains("content-type: text/event-stream\n") );
		CHECK ( Client.sHeader.contains("cache-control: no-cache\n") );
		CHECK ( Client.sHeader.contains("connection: close\n") );
		CHECK_FALSE ( Client.sHeader.contains("transfer-encoding") );
		CHECK_FALSE ( Client.sHeader.contains("content-length") );

		auto iHandle = Client.Hello();

		CHECK ( iHandle > 0 );
		CHECK ( Server.bSentBeforeReturn );
		CHECK ( Server.Events().size()        == 1 );
		CHECK ( Server.Events().size("news")  == 1 );
		CHECK ( Server.Events().size("other") == 0 );

		CHECK ( Server.Events().Publish("other", { "not sent" }) == 0 );
		CHECK ( Server.Events().Publish("news", KServerSentEvent{}) == 0 );
		CHECK ( Server.Events().Publish("news", { "first", "headline", "7" }) == 1 );
		CHECK ( Server.Events().Send(iHandle, { "second" }) );
		CHECK ( Server.Events().Broadcast({ "third\nline" }) == 1 );
		CHECK_FALSE ( Server.Events().Send(iHandle + 100, { "not sent" }) );

		KServerSentEvent Event;

		REQUIRE ( Client.Next(Event) );
		CHECK ( Event.sData  == "first" );
		CHECK ( Event.sEvent == "headline" );
		CHECK ( Event.sID    == "7" );

		REQUIRE ( Client.Next(Event) );
		CHECK ( Event.sData  == "second" );
		CHECK ( Event.sEvent == "" );

		REQUIRE ( Client.Next(Event) );
		CHECK ( Event.sData  == "third\nline" );
	}

	SECTION("topics")
	{
		EventTestServer Server(QuietOptions());
		EventClient     ClientA(Server.sEndPoint, "/events/a");
		EventClient     ClientB(Server.sEndPoint, "/events/b");

		auto iHandleA = ClientA.Hello();
		auto iHandleB = ClientB.Hello();

		CHECK ( iHandleA != iHandleB );
		CHECK ( Server.Events().size() == 2 );

		// a connection may have more than one topic
		CHECK ( Server.Events().Subscribe(iHandleB, "a") );
		CHECK ( Server.Events().Subscribe(iHandleB, "a") );
		CHECK_FALSE ( Server.Events().Subscribe(iHandleB + 100, "a") );
		CHECK ( Server.Events().size("a") == 2 );
		CHECK ( Server.Events().size("b") == 1 );

		CHECK ( Server.Events().Publish("a", { "for a" }) == 2 );
		CHECK ( Server.Events().Publish("b", { "for b" }) == 1 );

		KServerSentEvent Event;

		REQUIRE ( ClientA.Next(Event) );
		CHECK ( Event.sData == "for a" );
		REQUIRE ( ClientB.Next(Event) );
		CHECK ( Event.sData == "for a" );
		REQUIRE ( ClientB.Next(Event) );
		CHECK ( Event.sData == "for b" );

		CHECK ( Server.Events().Unsubscribe(iHandleB, "a") );
		CHECK_FALSE ( Server.Events().Unsubscribe(iHandleB, "a") );
		CHECK_FALSE ( Server.Events().Unsubscribe(iHandleB, "c") );
		CHECK ( Server.Events().size("a") == 1 );

		CHECK ( Server.Events().Publish("a", { "again for a" }) == 1 );
		CHECK ( Server.Events().Publish("b", { "again for b" }) == 1 );

		REQUIRE ( ClientA.Next(Event) );
		CHECK ( Event.sData == "again for a" );
		REQUIRE ( ClientB.Next(Event) );
		CHECK ( Event.sData == "again for b" );
	}

	SECTION("a closed connection gets the events that wait, and then ends")
	{
		EventTestServer Server(QuietOptions());
		EventClient     Client(Server.sEndPoint, "/events/news");

		auto iHandle = Client.Hello();

		CHECK ( Server.Events().Send(iHandle, { "last" }) );
		CHECK ( Server.Events().Close(iHandle) );
		CHECK_FALSE ( Server.Events().Send(iHandle, { "not sent" }) );
		CHECK_FALSE ( Server.Events().Close(iHandle) );

		KServerSentEvent Event;

		REQUIRE ( Client.Next(Event) );
		CHECK ( Event.sData == "last" );
		CHECK_FALSE ( Client.Next(Event) );

		CHECK ( Server.WaitForSize(0) );
		CHECK ( Server.Events().size("news") == 0 );
		CHECK ( Server.WasClosed(iHandle) );
	}

	SECTION("a client that leaves is removed")
	{
		EventTestServer Server(QuietOptions());

		KServerSentEventServer::Handle iHandle { 0 };

		{
			EventClient Client(Server.sEndPoint, "/events/news");
			iHandle = Client.Hello();
			CHECK ( Server.Events().size() == 1 );
		}

		// no write tells here that the client is gone
		CHECK ( Server.WaitForSize(0) );
		CHECK ( Server.Events().size("news") == 0 );
		CHECK ( Server.WasClosed(iHandle) );
		CHECK ( Server.Events().Publish("news", { "not sent" }) == 0 );
	}

	SECTION("comment lines without events, and the time to wait")
	{
		KServerSentEventServer::Options Options;

		Options.KeepAliveInterval = chrono::milliseconds(50);
		Options.Retry             = chrono::seconds(3);

		EventTestServer Server(Options);
		EventClient     Client(Server.sEndPoint, "/events/news");

		KString sLine;

		// the time to wait comes before the events that were queued in the route
		REQUIRE ( Client.NextLine(sLine) );
		CHECK ( sLine == "retry: 3000" );

		auto iHandle = Client.Hello();

		CHECK ( Client.Parser.HasRetry() );
		CHECK ( Client.Parser.GetRetry() == chrono::seconds(3) );

		// the empty line that ended the first event, then comment lines
		bool bHadComment { false };

		for (std::size_t i = 0; i < 5 && !bHadComment && Client.NextLine(sLine); ++i)
		{
			bHadComment = sLine == ":";
		}

		CHECK ( bHadComment );

		CHECK ( Server.Events().Send(iHandle, { "after the comment" }) );

		KServerSentEvent Event;

		REQUIRE ( Client.Next(Event) );
		CHECK ( Event.sData == "after the comment" );
	}

	SECTION("a HEAD request gets the headers, and stays with the worker")
	{
		EventTestServer Server(QuietOptions());

		KTCPStream Stream(KTCPEndPoint(Server.sEndPoint), KStreamOptions(chrono::seconds(5)));
		REQUIRE ( Stream.is_open() );

		Stream.Write("HEAD /events/news HTTP/1.1\r\nHost: a\r\n\r\n"
		             "GET /json HTTP/1.1\r\nHost: a\r\nConnection: close\r\n\r\n").Flush();

		KString sResponse;
		Stream.ReadRemaining(sResponse);

		INFO  ( sResponse );
		CHECK ( sResponse.contains("content-type: text/event-stream\r\n") );
		CHECK ( sResponse.contains("connection: keep-alive\r\n") );
		CHECK ( sResponse.contains("\r\n\r\nHTTP/1.1 200 ") );
		CHECK ( sResponse.contains("\"hello\"") );
		CHECK_FALSE ( Server.bSentBeforeReturn );
		CHECK ( Server.Events().size() == 0 );
	}

	SECTION("a route that fails after subscribing gives no connection to the event server")
	{
		EventTestServer Server(QuietOptions());
		EventClient     Client(Server.sEndPoint, "/fail");

		INFO  ( Client.sHeader );
		CHECK ( Client.sHeader.contains("content-type: text/event-stream\n") );

		KServerSentEvent Event;

		CHECK_FALSE ( Client.Next(Event) );
		CHECK ( Server.Events().size()       == 0 );
		CHECK ( Server.Events().size("news") == 0 );
		CHECK ( Server.WasClosed(1) );
	}

	SECTION("a connection whose queue is full is closed")
	{
		auto Options = QuietOptions();

		Options.iMaxQueueBytes = 1000;

		EventTestServer Server(Options);
		EventClient     Client(Server.sEndPoint, "/events/news");

		auto iHandle = Client.Hello();

		CHECK_FALSE ( Server.Events().Send(iHandle, { KString(2000, 'x') }) );
		CHECK ( Server.WaitForSize(0) );
		CHECK ( Server.WasClosed(iHandle) );

		KServerSentEvent Event;

		CHECK_FALSE ( Client.Next(Event) );
	}

	SECTION("the connections do not occupy the worker threads")
	{
		// two worker threads, and more event streams than that
		EventTestServer Server(QuietOptions(), 2);

		std::vector<std::unique_ptr<EventClient>> Clients;

		for (std::size_t i = 0; i < 6; ++i)
		{
			Clients.push_back(std::make_unique<EventClient>(Server.sEndPoint, "/events/news"));
			CHECK ( Clients.back()->Hello() > 0 );
		}

		CHECK ( Server.Events().size() == 6 );
		CHECK ( Server.Events().Publish("news", { "for all" }) == 6 );

		for (auto& Client : Clients)
		{
			KServerSentEvent Event;

			REQUIRE ( Client->Next(Event) );
			CHECK ( Event.sData == "for all" );
		}

		// a request without events is still served
		KTCPStream Stream(KTCPEndPoint(Server.sEndPoint), KStreamOptions(chrono::seconds(5)));
		REQUIRE ( Stream.is_open() );

		Stream.Write("GET /json HTTP/1.1\r\nHost: a\r\nConnection: close\r\n\r\n").Flush();

		KString sResponse;
		Stream.ReadRemaining(sResponse);

		CHECK ( sResponse.contains("\"hello\"") );
	}
}
