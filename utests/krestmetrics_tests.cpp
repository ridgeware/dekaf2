#include "catch.hpp"
#include <dekaf2/rest/framework/krest.h>
#include <dekaf2/rest/framework/krestmetrics.h>
#include <dekaf2/rest/framework/krestroute.h>
#include <dekaf2/rest/framework/krestserver.h>
#include <dekaf2/http/client/kwebclient.h>
#include <dekaf2/http/server/khttperror.h>
#include <dekaf2/util/metrics/kmetrics.h>
#include <dekaf2/system/os/ksystem.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/time/duration/kduration.h>
#include <memory>

using namespace dekaf2;

namespace {

constexpr uint16_t iPort = 44371;

/// @return how many lines of the text start with this text
std::size_t CountLines(KStringView sText, KStringView sStart)
{
	std::size_t iCount { 0 };

	for (auto sLine : sText.Split("\n"))
	{
		if (sLine.starts_with(sStart))
		{
			++iCount;
		}
	}

	return iCount;
}

/// The server records a request after it sent the response, so the client can be
/// faster than the count: wait until the server has finished this many requests.
bool WaitForFinished(KMetrics& Metrics, uint64_t iRequests)
{
	auto& Finished = Metrics.AddCounter("http_requests_finished_total", "").Get();

	for (int i = 0; i < 5000; ++i)
	{
		if (Finished.Get() >= iRequests)
		{
			return Finished.Get() == iRequests;
		}

		kSleep(chrono::milliseconds(1));
	}

	return false;
}

KRESTRoutes TestRoutes()
{
	KRESTRoutes Routes;

	Routes.AddRoute("/hello").Get([](KRESTServer& HTTP)
	{
		HTTP.json.tx["hello"] = "world";
	});

	Routes.AddRoute("/user/:NAME").Get([](KRESTServer& HTTP)
	{
		HTTP.json.tx["user"] = true;
	});

	Routes.AddRoute("/echo").Post([](KRESTServer& HTTP)
	{
		HTTP.SetRawOutput(HTTP.GetRequestBody());
	}).Parse(KRESTRoute::PLAIN);

	Routes.AddRoute("/fail").Get([](KRESTServer&)
	{
		throw KHTTPError { KHTTPError::H5xx_ERROR, "failed on purpose" };
	});

	Routes.AddRoute("/forbidden").Get([](KRESTServer&)
	{
		throw KHTTPError { KHTTPError::H4xx_FORBIDDEN, "forbidden on purpose" };
	});

	Routes.AddRoute("/slow").Get([](KRESTServer& HTTP)
	{
		kSleep(chrono::milliseconds(30));
		HTTP.json.tx["slow"] = true;
	});

	Routes.AddRoute("/metrics").Get([](KRESTServer& HTTP)
	{
		HTTP.ServeMetrics();
	});

	return Routes;
}

KREST::Options TestOptions()
{
	KREST::Options Options;

	Options.Type                 = KREST::HTTP;
	Options.iPort                = iPort;
	Options.Timeout              = chrono::seconds(2);
	Options.bBlocking            = false;
	Options.bCreateEphemeralCert = false;

	return Options;
}

} // end of anonymous namespace

TEST_CASE("KRESTMetrics")
{
	auto sBase = kFormat("http://localhost:{}", iPort);

	SECTION("requests by route, method and status")
	{
		auto Registry = std::make_shared<KMetrics>();
		auto Options  = TestOptions();
		auto Routes   = TestRoutes();

		Options.SetMetrics(Registry);

		REQUIRE ( Options.Metrics != nullptr );
		CHECK   ( &Options.Metrics->GetMetrics() == Registry.get() );

		{
			KREST Server;
			REQUIRE ( Server.Execute(Options, Routes) );

			KWebClient Client;

			CHECK ( Client.Get(kFormat("{}/hello", sBase)).contains("world") );
			CHECK ( Client.GetStatusCode() == 200 );
			Client.Get(kFormat("{}/hello", sBase));
			Client.Get(kFormat("{}/hello", sBase));

			// two requests of one route with a parameter
			Client.Get(kFormat("{}/user/tom", sBase));
			CHECK ( Client.GetStatusCode() == 200 );
			Client.Get(kFormat("{}/user/jerry", sBase));

			KString sBody(1000, 'x');

			CHECK ( Client.Post(kFormat("{}/echo", sBase), sBody, KMIME::TEXT_PLAIN) == sBody );

			Client.Get(kFormat("{}/forbidden", sBase));
			CHECK ( Client.GetStatusCode() == 403 );

			Client.Get(kFormat("{}/fail", sBase));
			CHECK ( Client.GetStatusCode() == 500 );

			Client.Get(kFormat("{}/slow", sBase));
			CHECK ( Client.GetStatusCode() == 200 );

			// no route: on the connection that just used a route, and with another method
			Client.Get(kFormat("{}/nowhere", sBase));
			CHECK ( Client.GetStatusCode() == 404 );
			Client.Get(kFormat("{}/nowhere/else", sBase));
			Client.Post(kFormat("{}/nowhere", sBase), "body", KMIME::TEXT_PLAIN);

			REQUIRE ( WaitForFinished(*Registry, 12) );

			auto sText = Registry->Serialize();

			INFO ( sText );

			CHECK ( sText.contains("http_requests_total{route=\"/hello\",method=\"GET\",status=\"2xx\"} 3\n")     );
			CHECK ( sText.contains("http_requests_total{route=\"/user/:NAME\",method=\"GET\",status=\"2xx\"} 2\n") );
			CHECK ( sText.contains("http_requests_total{route=\"/echo\",method=\"POST\",status=\"2xx\"} 1\n")     );
			CHECK ( sText.contains("http_requests_total{route=\"/forbidden\",method=\"GET\",status=\"4xx\"} 1\n") );
			CHECK ( sText.contains("http_requests_total{route=\"/forbidden\",method=\"GET\",status=\"2xx\"} 0\n") );
			CHECK ( sText.contains("http_requests_total{route=\"/fail\",method=\"GET\",status=\"5xx\"} 1\n")      );
			CHECK ( sText.contains("http_requests_total{route=\"/slow\",method=\"GET\",status=\"2xx\"} 1\n")      );
			// requests without a route are not counted for the route of the request before
			CHECK ( sText.contains("http_requests_total{route=\"unmatched\",method=\"GET\",status=\"4xx\"} 2\n")  );
			CHECK ( sText.contains("http_requests_total{route=\"unmatched\",method=\"POST\",status=\"4xx\"} 1\n") );
			CHECK ( sText.contains("http_requests_total{route=\"/slow\",method=\"GET\",status=\"4xx\"} 0\n")      );
			// the path of a request is no route
			CHECK ( sText.contains("tom")     == false );
			CHECK ( sText.contains("nowhere") == false );

			// the bodies
			CHECK ( sText.contains("http_request_bytes_total{route=\"/echo\",method=\"POST\"} 1000\n")  );
			CHECK ( sText.contains("http_response_bytes_total{route=\"/echo\",method=\"POST\"} 1000\n") );
			CHECK ( sText.contains("http_request_bytes_total{route=\"/hello\",method=\"GET\"} 0\n")     );

			CHECK ( sText.contains("http_requests_started_total 12\n")  );
			CHECK ( sText.contains("http_requests_finished_total 12\n") );
			CHECK ( sText.contains("http_requests_in_flight 0\n")       );

			// the durations: one per request, and the slow one took its time
			auto& Durations = Registry->AddHistogram("http_request_duration_seconds", "", { "route", "method" });

			CHECK ( Durations.Get({ "/hello", "GET" }).GetCount() == 3 );
			CHECK ( Durations.Get({ "/slow" , "GET" }).GetCount() == 1 );
			CHECK ( Durations.Get({ "/slow" , "GET" }).GetSum()   >= 30000000ull );
			CHECK ( Durations.Get({ "/hello", "GET" }).GetSum()   >  0 );
			CHECK ( sText.contains("http_request_duration_seconds_count{route=\"/echo\",method=\"POST\"} 1\n") );

			// the thread pool of the server, under its thread name
			CHECK ( CountLines(sText, "threadpool_threads{pool=\"rest\"")     == 2 );
			CHECK ( CountLines(sText, "threadpool_threads_max{pool=\"rest\"") == 1 );
		}

		// the server is gone: its thread pool is no more asked, the counts stay
		auto sText = Registry->Serialize();

		CHECK ( sText.contains("pool=\"rest\"") == false );
		CHECK ( sText.contains("http_requests_total{route=\"/hello\",method=\"GET\",status=\"2xx\"} 3\n") );
	}

	SECTION("the metrics endpoint")
	{
		auto Registry = std::make_shared<KMetrics>();
		auto Options  = TestOptions();
		auto Routes   = TestRoutes();

		Options.SetMetrics(Registry);

		KREST Server;
		REQUIRE ( Server.Execute(Options, Routes) );

		KWebClient Client;

		Client.Get(kFormat("{}/hello", sBase));
		REQUIRE ( WaitForFinished(*Registry, 1) );

		auto sText = Client.Get(kFormat("{}/metrics", sBase));

		CHECK ( Client.GetStatusCode() == 200 );
		CHECK ( Client.Response.Headers.Get(KHTTPHeader::CONTENT_TYPE) == KMetrics::ContentType );
		CHECK ( sText.contains("# TYPE http_requests_total counter\n") );
		CHECK ( sText.contains("# TYPE http_request_duration_seconds histogram\n") );
		CHECK ( sText.contains("http_requests_total{route=\"/hello\",method=\"GET\",status=\"2xx\"} 1\n") );
		// the request for the metrics is in flight while it is answered
		CHECK ( sText.contains("http_requests_started_total 2\n")  );
		CHECK ( sText.contains("http_requests_finished_total 1\n") );
		CHECK ( sText.contains("http_requests_in_flight 1\n")      );

		REQUIRE ( WaitForFinished(*Registry, 2) );

		// as JSON
		KJSON jMetrics;

		REQUIRE ( jMetrics.Parse(Client.Get(kFormat("{}/metrics?format=json", sBase))) );
		CHECK   ( Client.Response.Headers.Get(KHTTPHeader::CONTENT_TYPE).starts_with("application/json") );
		REQUIRE ( jMetrics.is_array() );

		bool bFound { false };

		for (auto& jMetric : jMetrics)
		{
			if (jMetric("name").String() == "http_requests_total")
			{
				bFound = true;
				CHECK ( jMetric("type").String() == "counter" );
				CHECK ( jMetric("values").size() >= 6         );
			}
		}

		CHECK ( bFound );
	}

	SECTION("durations with the timers of the phases")
	{
		// with a timer header the server measures the phases of a request, and the
		// metrics take the duration from them instead of from a start time of their own
		auto Registry = std::make_shared<KMetrics>();
		auto Options  = TestOptions();
		auto Routes   = TestRoutes();

		Options.SetMetrics(Registry);
		Options.TimerHeader = "x-milliseconds";

		KREST Server;
		REQUIRE ( Server.Execute(Options, Routes) );

		KWebClient Client;

		Client.Get(kFormat("{}/slow", sBase));
		CHECK ( Client.Response.Headers.Get("x-milliseconds").empty() == false );
		Client.Get(kFormat("{}/hello", sBase));

		REQUIRE ( WaitForFinished(*Registry, 2) );

		auto& Durations = Registry->AddHistogram("http_request_duration_seconds", "", { "route", "method" });

		CHECK ( Durations.Get({ "/slow" , "GET" }).GetCount() == 1 );
		CHECK ( Durations.Get({ "/slow" , "GET" }).GetSum()   >= 30000000ull );
		CHECK ( Durations.Get({ "/hello", "GET" }).GetCount() == 1 );
		CHECK ( Durations.Get({ "/hello", "GET" }).GetSum()   >  0 );
	}

	SECTION("phases on request")
	{
		auto Registry = std::make_shared<KMetrics>();
		auto Options  = TestOptions();
		auto Routes   = TestRoutes();

		Options.SetMetrics(Registry, true);

		REQUIRE ( Options.Metrics != nullptr );
		CHECK   ( Options.Metrics->HasPhases() );

		KREST Server;
		REQUIRE ( Server.Execute(Options, Routes) );

		KWebClient Client;

		Client.Get(kFormat("{}/slow", sBase));
		Client.Get(kFormat("{}/slow", sBase));
		// no timer header was asked for
		CHECK ( Client.Response.Headers.Get("x-milliseconds").empty() );

		REQUIRE ( WaitForFinished(*Registry, 2) );

		auto sText = Registry->Serialize();

		INFO ( sText );

		CHECK ( sText.contains("# TYPE http_request_phase_seconds histogram\n") );

		// sum and count for each phase, without buckets
		for (std::size_t iPhase = 0; iPhase < KRESTMetrics::iPhases; ++iPhase)
		{
			auto sLabels = kFormat("{{route=\"/slow\",method=\"GET\",phase=\"{}\"}}", KRESTMetrics::PhaseName(iPhase));

			CHECK ( sText.contains(kFormat("http_request_phase_seconds_count{} 2\n", sLabels)) );
			CHECK ( CountLines(sText, kFormat("http_request_phase_seconds_sum{} ", sLabels)) == 1 );
		}

		CHECK ( CountLines(sText, "http_request_phase_seconds_bucket{route=\"/slow\"") == KRESTMetrics::iPhases );

		// the handler sleeps: that is the phase "process"
		auto& Phases = Registry->AddHistogram("http_request_phase_seconds", "", { "route", "method", "phase" });

		CHECK ( Phases.Get({ "/slow", "GET", "process" }).GetSum()   >= 60000000ull );
		CHECK ( Phases.Get({ "/slow", "GET", "process" }).GetCount() == 2           );
		CHECK ( Phases.Get({ "/slow", "GET", "route"   }).GetSum()   <  60000000ull );

		// the names of the phases are those of the timers of the server
		for (auto& Timer : KRESTServer::Timers)
		{
			CHECK ( KRESTMetrics::PhaseName(Timer.Value) == Timer.sLabel );
		}

		CHECK ( KRESTMetrics::PhaseName(KRESTMetrics::iPhases) == "" );
	}

	SECTION("no phases without the request for them")
	{
		// also not when the server measures the phases for its timer header
		auto Registry = std::make_shared<KMetrics>();
		auto Options  = TestOptions();
		auto Routes   = TestRoutes();

		Options.SetMetrics(Registry);
		Options.TimerHeader = "x-milliseconds";

		CHECK ( Options.Metrics->HasPhases() == false );

		KREST Server;
		REQUIRE ( Server.Execute(Options, Routes) );

		KWebClient Client;

		Client.Get(kFormat("{}/hello", sBase));

		REQUIRE ( WaitForFinished(*Registry, 1) );

		CHECK ( Registry->Serialize().contains("http_request_phase_seconds") == false );
		CHECK ( Registry->Serialize().contains("http_request_duration_seconds_count{route=\"/hello\",method=\"GET\"} 1\n") );
	}

	SECTION("route statistics from the metrics")
	{
		auto Registry = std::make_shared<KMetrics>();
		auto Options  = TestOptions();
		auto Routes   = TestRoutes();

		Options.SetMetrics(Registry, true);

		// no request yet
		CHECK ( Routes.GetRouterStats().is_array() );
		CHECK ( Routes.GetRouterStats().empty()    );

		KREST Server;
		REQUIRE ( Server.Execute(Options, Routes) );

		KWebClient Client;

		Client.Get(kFormat("{}/hello", sBase));
		Client.Get(kFormat("{}/hello", sBase));
		Client.Post(kFormat("{}/echo", sBase), KString(500, 'x'), KMIME::TEXT_PLAIN);
		Client.Get(kFormat("{}/forbidden", sBase));
		Client.Get(kFormat("{}/slow", sBase));
		// a request without a route is in no route
		Client.Get(kFormat("{}/nowhere", sBase));

		REQUIRE ( WaitForFinished(*Registry, 6) );

		auto jStats = Routes.GetRouterStats();

		INFO ( jStats.dump(1, '\t') );

		// one entry per route and method that had a request, in the order of the routes
		REQUIRE ( jStats.size() == 4 );

		auto& jHello = jStats(0);

		CHECK ( jHello("route").String()            == "/hello" );
		CHECK ( jHello("method").String()           == "GET"    );
		CHECK ( jHello("requests").UInt64()         == 2        );
		CHECK ( jHello("status")("2xx").UInt64()    == 2        );
		CHECK ( jHello("status")("4xx").UInt64()    == 0        );
		CHECK ( jHello("status")("other").UInt64()  == 0        );
		CHECK ( jHello("bytes")("total")("rx").UInt64() == 0    );
		CHECK ( jHello("bytes")("total")("tx").UInt64() >  0    );
		CHECK ( jHello("bytes")("avg")("tx").UInt64()   == jHello("bytes")("total")("tx").UInt64() / 2 );
		CHECK ( jHello("usecs")("total").is_number()            );
		CHECK ( jHello("usecs")("p50").is_number()              );
		CHECK ( jHello("usecs")("p99").is_number()              );
		// the averages of the phases
		CHECK ( jHello("usecs")("rx").is_number()               );
		CHECK ( jHello("usecs")("process").is_number()          );
		CHECK ( jHello("usecs")("tx").is_number()               );

		auto& jEcho = jStats(1);

		CHECK ( jEcho("route").String()                == "/echo" );
		CHECK ( jEcho("method").String()               == "POST"  );
		CHECK ( jEcho("bytes")("total")("rx").UInt64() == 500     );
		CHECK ( jEcho("bytes")("avg")("rx").UInt64()   == 500     );
		CHECK ( jEcho("bytes")("total")("tx").UInt64() == 500     );

		auto& jForbidden = jStats(2);

		CHECK ( jForbidden("route").String()         == "/forbidden" );
		CHECK ( jForbidden("requests").UInt64()      == 1            );
		CHECK ( jForbidden("status")("4xx").UInt64() == 1            );
		CHECK ( jForbidden("status")("2xx").UInt64() == 0            );

		auto& jSlow = jStats(3);

		CHECK ( jSlow("route").String()             == "/slow" );
		// one request of 30 milliseconds and more
		CHECK ( jSlow("usecs")("total").UInt64()    >= 30000   );
		CHECK ( jSlow("usecs")("process").UInt64()  >= 30000   );
		CHECK ( jSlow("usecs")("p50").UInt64()      >= 25000   );
		CHECK ( jSlow("usecs")("p99").UInt64()      >= jSlow("usecs")("p50").UInt64() );
	}

	SECTION("route statistics of a server without metrics")
	{
		// with a timer header the server measures the phases, and keeps the statistics
		// of its routes as it did before there were metrics
		auto Options = TestOptions();
		auto Routes  = TestRoutes();

		Options.TimerHeader = "x-milliseconds";

		CHECK ( Options.Metrics == nullptr );

		{
			KREST Server;
			REQUIRE ( Server.Execute(Options, Routes) );

			KWebClient Client;

			Client.Get(kFormat("{}/hello", sBase));
			Client.Get(kFormat("{}/hello", sBase));
			Client.Get(kFormat("{}/slow", sBase));

			REQUIRE ( WaitForFinished(Routes.GetStatistics().GetMetrics(), 3) );
		}

		auto jStats = Routes.GetRouterStats();

		INFO ( jStats.dump(1, '\t') );

		REQUIRE ( jStats.size() == 2 );
		CHECK ( jStats(0)("route").String()            == "/hello" );
		CHECK ( jStats(0)("requests").UInt64()         == 2        );
		CHECK ( jStats(0)("usecs")("process").is_number()          );
		CHECK ( jStats(1)("route").String()            == "/slow"  );
		CHECK ( jStats(1)("usecs")("process").UInt64() >= 30000    );

		// the metrics behind these statistics have the phases, and can be read like any other
		CHECK ( Routes.GetStatistics().HasPhases() );
		CHECK ( Routes.GetStatistics().GetMetrics().Serialize().contains("http_requests_total{route=\"/hello\",method=\"GET\",status=\"2xx\"} 2\n") );
	}

	SECTION("no route statistics without metrics and without timers")
	{
		auto Options = TestOptions();
		auto Routes  = TestRoutes();

		{
			KREST Server;
			REQUIRE ( Server.Execute(Options, Routes) );

			KWebClient Client;

			CHECK ( Client.Get(kFormat("{}/hello", sBase)).contains("world") );
		}

		CHECK ( Routes.GetRouterStats().empty() );
	}

	SECTION("without metrics")
	{
		auto Options = TestOptions();
		auto Routes  = TestRoutes();

		CHECK ( Options.Metrics == nullptr );

		KREST Server;
		REQUIRE ( Server.Execute(Options, Routes) );

		KWebClient Client;

		CHECK ( Client.Get(kFormat("{}/hello", sBase)).contains("world") );

		// the endpoint has nothing to serve
		Client.Get(kFormat("{}/metrics", sBase));

		CHECK ( Client.GetStatusCode() == 404 );
	}

	SECTION("a registry of its own, and two servers with one registry")
	{
		// without a registry the options create one
		KREST::Options Own;
		Own.SetMetrics();

		REQUIRE ( Own.Metrics != nullptr );
		CHECK   ( Own.Metrics->GetMetrics().Serialize().contains("# TYPE http_requests_in_flight gauge\n") );

		// the same metrics again in the same registry: the second set of options counts
		// into the same values, and the gauge stays the one of the first
		auto Registry = std::make_shared<KMetrics>();

		KREST::Options First;
		KREST::Options Second;

		First .SetMetrics(Registry);
		Second.SetMetrics(Registry);

		First .Metrics->Started();
		Second.Metrics->Started();
		Second.Metrics->Finished();

		auto sText = Registry->Serialize();

		CHECK ( sText.contains("http_requests_started_total 2\n")  );
		CHECK ( sText.contains("http_requests_finished_total 1\n") );
		CHECK ( CountLines(sText, "http_requests_in_flight ") == 1 );
		CHECK ( sText.contains("http_requests_in_flight 1\n")      );

		// the values of a route for a method are created once
		KRESTMetrics::Route Route;

		auto& Get  = Route.Get(*First.Metrics, "/route", KHTTPMethod::GET);
		auto& Post = Route.Get(*First.Metrics, "/route", KHTTPMethod::POST);

		CHECK ( &Route.Get(*First.Metrics, "/route", KHTTPMethod::GET) == &Get );
		CHECK ( &Get != &Post );

		Get.Status[2].get().Inc();
		Get.Status[0].get().Inc();

		sText = Registry->Serialize();

		CHECK ( sText.contains("http_requests_total{route=\"/route\",method=\"GET\",status=\"2xx\"} 1\n")   );
		CHECK ( sText.contains("http_requests_total{route=\"/route\",method=\"GET\",status=\"other\"} 1\n") );
		CHECK ( sText.contains("http_requests_total{route=\"/route\",method=\"POST\",status=\"2xx\"} 0\n")  );

		// the copy of a route has values of its own, which count into the same metrics
		KRESTMetrics::Route Copy(Route);

		auto& CopyGet = Copy.Get(*First.Metrics, "/route", KHTTPMethod::GET);

		CHECK ( &CopyGet != &Get );

		CopyGet.Status[2].get().Inc();

		CHECK ( Registry->Serialize().contains("http_requests_total{route=\"/route\",method=\"GET\",status=\"2xx\"} 2\n") );

		// a method that is none
		Route.Get(*First.Metrics, "/route", KHTTPMethod::INVALID).Status[4].get().Inc();

		CHECK ( Registry->Serialize().contains("http_requests_total{route=\"/route\",method=\"invalid\",status=\"4xx\"} 1\n") );
	}
}
