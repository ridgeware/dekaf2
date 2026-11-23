#include "catch.hpp"

#include <dekaf2/http/server/kserversenteventserver.h>
#include <dekaf2/rest/framework/krest.h>
#include <dekaf2/rest/framework/krestserver.h>
#include <dekaf2/http/server/khttperror.h>
#include <dekaf2/http/client/khttpclient.h>
#include <dekaf2/http/protocol/kserversentevent.h>
#include <dekaf2/web/url/kmime.h>
#include <dekaf2/net/tcp/ktcpstream.h>
#include <dekaf2/net/util/kstreamoptions.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/format/kformat.h>
#include <dekaf2/system/os/ksystem.h>
#include <atomic>
#include <memory>
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

			bMissedEvents     = HTTP.GetEventServer().HasMissedEvents(iHandle);
			bSentBeforeReturn = HTTP.GetEventServer().Send(iHandle, { KString::to_string(iHandle), "hello" });
		});

		// two topics for one connection
		Routes.AddRoute("/both").Get([this](KRESTServer& HTTP)
		{
			auto iHandle = HTTP.SubscribeToEvents("a", "b");

			bMissedEvents = HTTP.GetEventServer().HasMissedEvents(iHandle);

			HTTP.GetEventServer().Send(iHandle, { KString::to_string(iHandle), "hello" });
		});

		Routes.AddRoute("/fail").Get([](KRESTServer& HTTP)
		{
			HTTP.SubscribeToEvents("news");

			throw KHTTPError { KHTTPError::H5xx_ERROR, "failed after subscribing" };
		});

		// events from the worker thread: the second one only after the client has the first
		Routes.AddRoute("/writer").Get([this](KRESTServer& HTTP)
		{
			auto Events = HTTP.EventStream();

			Events.Send({ "first", "", "1" });

			for (std::size_t i = 0; i < 500 && !bClientHasFirst; ++i)
			{
				kSleep(chrono::milliseconds(10));
			}

			bWaitedInVain = !bClientHasFirst;

			Events.Send({ "second", "", "2" });
		});

		Routes.AddRoute("/json").Get([](KRESTServer& HTTP)
		{
			HTTP.json.tx["hello"] = "world";
		});

		Rest.GetEventServer().SetOptions(std::move(EventOptions));

		Rest.GetEventServer().SetCloseCallback([this](KServerSentEventServer::Handle iHandle)
		{
			if (bPublishOnClose)
			{
				// an application may tell the others who left
				Rest.GetEventServer().Publish("left", { KString::to_string(iHandle) });
			}

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

	// waits until the close callback was called for iHandle, for five seconds at most -
	// the connection leaves the count of the server before the callback is called
	bool WasClosed(KServerSentEventServer::Handle iHandle)
	{
		for (std::size_t i = 0; i < 500; ++i)
		{
			{
				std::lock_guard<std::mutex> Lock(Mutex);

				for (auto iClosed : Closed)
				{
					if (iClosed == iHandle)
					{
						return true;
					}
				}
			}

			kSleep(chrono::milliseconds(10));
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
	std::atomic<bool>                            bMissedEvents     { false };
	std::atomic<bool>                            bPublishOnClose   { false };
	std::atomic<bool>                            bClientHasFirst   { false };
	std::atomic<bool>                            bWaitedInVain     { false };
	KString                                      sEndPoint;
	KREST                                        Rest;

}; // EventTestServer

//-----------------------------------------------------------------------------
/// a client that requests an event stream, and reads its events
struct EventClient
//-----------------------------------------------------------------------------
{
	// with sLastEventID the client connects again, after it got the event with this id
	EventClient(KStringView sEndPoint, KStringView sPath, KStringView sLastEventID = KStringView{})
	: Stream(KTCPEndPoint(sEndPoint), KStreamOptions(chrono::seconds(5)))
	{
		REQUIRE ( Stream.is_open() );

		Stream.Write(kFormat("GET {} HTTP/1.1\r\nHost: a\r\n", sPath));

		if (!sLastEventID.empty())
		{
			Stream.Write(kFormat("Last-Event-ID: {}\r\n", sLastEventID));
		}

		Stream.Write("\r\n").Flush();

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

//-----------------------------------------------------------------------------
/// a client that reads an event stream with KHTTPClient and KServerSentEventReader
struct ReaderClient
//-----------------------------------------------------------------------------
{
	ReaderClient(KStringView sEndPoint, KStringView sPath, KStringView sLastEventID = KStringView{})
	{
		KURL URL(kFormat("http://{}{}", sEndPoint, sPath));

		HTTP.SetTimeout(chrono::seconds(5));
		HTTP.RequestCompression(false);

		REQUIRE ( HTTP.Connect(URL) );
		REQUIRE ( HTTP.Resource(URL) );

		HTTP.AddHeader(KHTTPHeader::ACCEPT, KMIME::EVENT_STREAM);

		if (!sLastEventID.empty())
		{
			HTTP.AddHeader(KHTTPHeader::LAST_EVENT_ID, sLastEventID);
		}

		REQUIRE ( HTTP.SendRequest() );

		iStatus = HTTP.Response.GetStatusCode();

		Reader = std::make_unique<KServerSentEventReader>(HTTP.Response.FilteredStream());
	}

	bool Next(KServerSentEvent& Event)
	{
		return Reader->Next(Event);
	}

	KHTTPClient                             HTTP;
	std::unique_ptr<KServerSentEventReader> Reader;
	uint16_t                                iStatus { 0 };

}; // ReaderClient

// options without comment lines, which would come between the events of a test
KServerSentEventServer::Options QuietOptions(std::size_t iHistory = 0)
{
	KServerSentEventServer::Options Options;
	Options.KeepAliveInterval = KDuration::zero();
	Options.iHistory          = iHistory;
	return Options;
}

// the count in an id of the event server, 0 if the id has none
uint64_t CountOfID(KStringView sID)
{
	auto iDash = sID.rfind('-');

	return iDash == KStringView::npos ? 0 : sID.substr(iDash + 1).UInt64();
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

	SECTION("without a history the server sets no ids")
	{
		EventTestServer Server(QuietOptions());
		EventClient     Client(Server.sEndPoint, "/events/news", "some-id");

		auto iHandle = Client.Hello();

		CHECK_FALSE ( Server.bMissedEvents );
		CHECK_FALSE ( Server.Events().HasMissedEvents(iHandle) );
		CHECK ( Server.Events().Publish("news", { "first" }) == 1 );

		KServerSentEvent Event;

		REQUIRE ( Client.Next(Event) );
		CHECK ( Event.sData == "first" );
		CHECK ( Event.sID   == "" );
	}

	SECTION("with a history a client that connects again gets the events it missed")
	{
		EventTestServer Server(QuietOptions(10));

		KString          sFirstID;
		KString          sThirdID;
		KServerSentEvent Event;

		{
			EventClient Client(Server.sEndPoint, "/events/news");

			Client.Hello();
			CHECK_FALSE ( Server.bMissedEvents );

			// an id of the application is replaced
			CHECK ( Server.Events().Publish("news", { "first", "", "my own id" }) == 1 );
			CHECK ( Server.Events().Publish("news", { "second", "headline" }) == 1 );
			CHECK ( Server.Events().Publish("other", { "for another topic" }) == 0 );
			CHECK ( Server.Events().Publish("news", { "third" }) == 1 );

			REQUIRE ( Client.Next(Event) );
			CHECK ( Event.sData == "first" );
			sFirstID = Event.sID;
			CHECK ( CountOfID(sFirstID) == 1 );
			CHECK ( sFirstID != "my own id" );

			REQUIRE ( Client.Next(Event) );
			CHECK ( Event.sData == "second" );
			CHECK ( CountOfID(Event.sID) == 2 );

			REQUIRE ( Client.Next(Event) );
			CHECK ( Event.sData == "third" );
			sThirdID = Event.sID;
			// the ids count over all topics
			CHECK ( CountOfID(sThirdID) == 4 );
			CHECK ( sThirdID.starts_with(sFirstID.substr(0, sFirstID.rfind('-') + 1)) );
		}

		CHECK ( Server.WaitForSize(0) );

		// events without a connection are kept, events without a topic are not
		CHECK ( Server.Events().Publish("news", { "fourth" }) == 0 );
		CHECK ( Server.Events().Broadcast({ "not kept" }) == 0 );

		{
			// the client got the first event only
			EventClient Client(Server.sEndPoint, "/events/news", sFirstID);

			REQUIRE ( Client.Next(Event) );
			CHECK ( Event.sData  == "second" );
			CHECK ( Event.sEvent == "headline" );
			CHECK ( CountOfID(Event.sID) == 2 );

			REQUIRE ( Client.Next(Event) );
			CHECK ( Event.sData == "third" );
			CHECK ( Event.sID   == sThirdID );

			REQUIRE ( Client.Next(Event) );
			CHECK ( Event.sData == "fourth" );

			// what the route sends follows the events that are sent again
			auto iHandle = Client.Hello();

			CHECK_FALSE ( Server.bMissedEvents );
			CHECK_FALSE ( Server.Events().HasMissedEvents(iHandle) );

			CHECK ( Server.Events().Publish("news", { "fifth" }) == 1 );

			REQUIRE ( Client.Next(Event) );
			CHECK ( Event.sData == "fifth" );
			CHECK ( CountOfID(Event.sID) == 6 );
		}

		CHECK ( Server.WaitForSize(0) );

		{
			// a client that got all events gets none again
			EventClient Client(Server.sEndPoint, "/events/news", Event.sID);

			Client.Hello();
			CHECK_FALSE ( Server.bMissedEvents );
		}
	}

	SECTION("a history that does not reach back far enough sends nothing again")
	{
		EventTestServer Server(QuietOptions(2));

		std::vector<KString> IDs;
		KServerSentEvent     Event;

		{
			EventClient Client(Server.sEndPoint, "/events/news");

			Client.Hello();

			for (KStringView sData : { "first", "second", "third", "fourth" })
			{
				CHECK ( Server.Events().Publish("news", { sData }) == 1 );
				REQUIRE ( Client.Next(Event) );
				CHECK ( Event.sData == sData );
				IDs.push_back(Event.sID);
			}
		}

		CHECK ( Server.WaitForSize(0) );

		{
			// the second event has left the history, and the client has not seen it
			EventClient Client(Server.sEndPoint, "/events/news", IDs[0]);

			auto iHandle = Client.Hello();

			CHECK ( Server.bMissedEvents );
			CHECK ( Server.Events().HasMissedEvents(iHandle) );

			// the connection gets the new events
			CHECK ( Server.Events().Publish("news", { "fifth" }) == 1 );
			REQUIRE ( Client.Next(Event) );
			CHECK ( Event.sData == "fifth" );
		}

		CHECK ( Server.WaitForSize(0) );

		{
			// the history holds all events after the third: the fourth and the fifth
			EventClient Client(Server.sEndPoint, "/events/news", IDs[2]);

			REQUIRE ( Client.Next(Event) );
			CHECK ( Event.sData == "fourth" );
			REQUIRE ( Client.Next(Event) );
			CHECK ( Event.sData == "fifth" );

			Client.Hello();
			CHECK_FALSE ( Server.bMissedEvents );
		}
	}

	SECTION("an id that is not of this server sends nothing again")
	{
		EventTestServer Server(QuietOptions(10));

		CHECK ( Server.Events().Publish("news", { "first" }) == 0 );

		for (KStringView sLastEventID : { "0-1", "no id of the server", "-1" })
		{
			INFO ( sLastEventID );

			EventClient Client(Server.sEndPoint, "/events/news", sLastEventID);

			auto iHandle = Client.Hello();

			CHECK ( Server.bMissedEvents );
			CHECK ( Server.Events().HasMissedEvents(iHandle) );
		}

		CHECK_FALSE ( Server.Events().HasMissedEvents(1000) );
	}

	SECTION("the events of two topics are sent again in the order of their ids")
	{
		EventTestServer Server(QuietOptions(10));

		KString          sFirstID;
		KServerSentEvent Event;

		{
			EventClient Client(Server.sEndPoint, "/both");

			Client.Hello();

			CHECK ( Server.Events().size("a") == 1 );
			CHECK ( Server.Events().size("b") == 1 );
			CHECK ( Server.Events().Publish("a", { "a1" }) == 1 );

			REQUIRE ( Client.Next(Event) );
			CHECK ( Event.sData == "a1" );
			sFirstID = Event.sID;
		}

		CHECK ( Server.WaitForSize(0) );

		CHECK ( Server.Events().Publish("b", { "b1" }) == 0 );
		CHECK ( Server.Events().Publish("a", { "a2" }) == 0 );
		CHECK ( Server.Events().Publish("c", { "c1" }) == 0 );
		CHECK ( Server.Events().Publish("b", { "b2" }) == 0 );
		CHECK ( Server.Events().Publish("a", { "a3" }) == 0 );

		EventClient Client(Server.sEndPoint, "/both", sFirstID);

		uint64_t iLastCount { CountOfID(sFirstID) };

		for (KStringView sData : { "b1", "a2", "b2", "a3" })
		{
			REQUIRE ( Client.Next(Event) );
			CHECK ( Event.sData == sData );
			CHECK ( CountOfID(Event.sID) > iLastCount );
			iLastCount = CountOfID(Event.sID);
		}

		Client.Hello();
		CHECK_FALSE ( Server.bMissedEvents );
	}

	SECTION("the close callback may publish, also when a full queue closes a connection")
	{
		auto Options = QuietOptions(10);

		Options.iMaxQueueBytes = 1000;

		EventTestServer Server(Options);

		Server.bPublishOnClose = true;

		EventClient Watcher(Server.sEndPoint, "/events/left");
		EventClient Client (Server.sEndPoint, "/events/news");

		Watcher.Hello();

		auto iHandle = Client.Hello();

		// the queue of the client is full with this event
		CHECK ( Server.Events().Publish("news", { KString(2000, 'x') }) == 0 );
		CHECK ( Server.WasClosed(iHandle) );

		KServerSentEvent Event;

		REQUIRE ( Watcher.Next(Event) );
		CHECK ( Event.sData.UInt64() == iHandle );
	}
}

TEST_CASE("KServerSentEventServer: limits for the count of connections")
{
	SECTION("connections over the limit are answered with 503")
	{
		auto Options = QuietOptions();

		Options.iMaxConnections = 2;

		EventTestServer Server(Options);

		auto ClientA = std::make_unique<EventClient>(Server.sEndPoint, "/events/news");
		EventClient ClientB(Server.sEndPoint, "/events/news");

		auto iHandleA = ClientA->Hello();
		ClientB.Hello();

		CHECK ( Server.Events().size() == 2 );

		{
			EventClient Refused(Server.sEndPoint, "/events/news");

			INFO  ( Refused.sHeader );
			CHECK ( Refused.sHeader.starts_with("HTTP/1.1 503 ") );
			CHECK_FALSE ( Refused.sHeader.contains("text/event-stream") );
		}

		CHECK ( Server.Events().size()       == 2 );
		CHECK ( Server.Events().size("news") == 2 );

		// a connection that ends makes room for another one
		ClientA.reset();
		CHECK ( Server.WasClosed(iHandleA) );

		EventClient ClientC(Server.sEndPoint, "/events/news");

		INFO  ( ClientC.sHeader );
		CHECK ( ClientC.sHeader.starts_with("HTTP/1.1 200 ") );
		CHECK ( ClientC.Hello() > 0 );
		CHECK ( Server.Events().size() == 2 );
	}

	SECTION("connections of one client over the limit are answered with 429")
	{
		auto Options = QuietOptions();

		Options.iMaxConnectionsPerClient = 1;

		EventTestServer Server(Options);

		auto ClientA = std::make_unique<EventClient>(Server.sEndPoint, "/events/news");
		auto iHandleA = ClientA->Hello();

		{
			EventClient Refused(Server.sEndPoint, "/events/news");

			INFO  ( Refused.sHeader );
			CHECK ( Refused.sHeader.starts_with("HTTP/1.1 429 ") );
		}

		CHECK ( Server.Events().size() == 1 );

		ClientA.reset();
		CHECK ( Server.WasClosed(iHandleA) );

		EventClient ClientB(Server.sEndPoint, "/events/news");

		INFO  ( ClientB.sHeader );
		CHECK ( ClientB.sHeader.starts_with("HTTP/1.1 200 ") );
		CHECK ( ClientB.Hello() > 0 );
	}

	SECTION("a HEAD request does not count")
	{
		auto Options = QuietOptions();

		Options.iMaxConnections = 1;

		EventTestServer Server(Options);
		EventClient     Client(Server.sEndPoint, "/events/news");

		Client.Hello();

		KTCPStream Stream(KTCPEndPoint(Server.sEndPoint), KStreamOptions(chrono::seconds(5)));
		REQUIRE ( Stream.is_open() );

		Stream.Write("HEAD /events/news HTTP/1.1\r\nHost: a\r\nConnection: close\r\n\r\n").Flush();

		KString sResponse;
		Stream.ReadRemaining(sResponse);

		INFO  ( sResponse );
		CHECK ( sResponse.starts_with("HTTP/1.1 200 ") );
		CHECK ( sResponse.contains("content-type: text/event-stream\r\n") );
	}
}

TEST_CASE("KServerSentEventReader with KHTTPClient")
{
	SECTION("events from the worker thread arrive while the route runs")
	{
		EventTestServer Server(QuietOptions());
		ReaderClient    Client(Server.sEndPoint, "/writer");

		CHECK ( Client.iStatus == 200 );

		KServerSentEvent Event;

		REQUIRE ( Client.Next(Event) );
		CHECK ( Event.sData == "first" );
		Server.bClientHasFirst = true;

		REQUIRE ( Client.Next(Event) );
		CHECK ( Event.sData == "second" );
		CHECK ( Client.Reader->GetLastEventID() == "2" );

		// the stream ends with the route
		CHECK_FALSE ( Client.Next(Event) );
		CHECK_FALSE ( Client.Reader->HasError() );
		CHECK_FALSE ( Server.bWaitedInVain );
	}

	SECTION("events from the event server, and the events that a client missed")
	{
		EventTestServer  Server(QuietOptions(10));
		KServerSentEvent Event;
		KString          sLastEventID;

		{
			ReaderClient Client(Server.sEndPoint, "/events/news");

			CHECK ( Client.iStatus == 200 );

			REQUIRE ( Client.Next(Event) );
			CHECK ( Event.sEvent == "hello" );

			CHECK ( Server.Events().Publish("news", { "first" }) == 1 );

			REQUIRE ( Client.Next(Event) );
			CHECK ( Event.sData == "first" );

			sLastEventID = Client.Reader->GetLastEventID();
			CHECK ( CountOfID(sLastEventID) == 1 );
		}

		CHECK ( Server.WaitForSize(0) );
		CHECK ( Server.Events().Publish("news", { "second" }) == 0 );
		CHECK ( Server.Events().Publish("news", { "third" })  == 0 );

		ReaderClient Client(Server.sEndPoint, "/events/news", sLastEventID);

		REQUIRE ( Client.Next(Event) );
		CHECK ( Event.sData == "second" );
		REQUIRE ( Client.Next(Event) );
		CHECK ( Event.sData == "third" );
		REQUIRE ( Client.Next(Event) );
		CHECK ( Event.sEvent == "hello" );
		CHECK ( CountOfID(Client.Reader->GetLastEventID()) == 3 );
		CHECK_FALSE ( Server.bMissedEvents );
	}
}
