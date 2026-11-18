#include "catch.hpp"
#include <dekaf2/util/metrics/kmetrics.h>
#include <dekaf2/data/json/kjson.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/time/duration/kduration.h>
#include <atomic>
#include <limits>
#include <thread>
#include <vector>

using namespace dekaf2;

TEST_CASE("KMetrics")
{
	SECTION("counter")
	{
		KMetrics Metrics;

		auto& Requests = Metrics.AddCounter("requests_total", "count of requests");

		CHECK ( Requests.GetName() == "requests_total"        );
		CHECK ( Requests.GetHelp() == "count of requests"     );
		CHECK ( Requests.GetType() == KMetrics::Type::Counter );
		CHECK ( Requests.GetLabelNames().empty()              );

		auto& Counter = Requests.Get();

		CHECK ( Counter.Get() == 0 );

		Counter.Inc();
		Counter.Inc(41);

		CHECK ( Counter.Get() == 42 );

		// the same value again, and the same metric again
		CHECK ( &Requests.Get() == &Counter );
		CHECK ( &Metrics.AddCounter("requests_total", "another help") == &Requests );
		CHECK ( Requests.GetHelp() == "count of requests" );

		CHECK ( Metrics.Serialize() ==
			"# HELP requests_total count of requests\n"
			"# TYPE requests_total counter\n"
			"requests_total 42\n" );
	}

	SECTION("gauge")
	{
		KMetrics Metrics;

		auto& Gauge = Metrics.AddGauge("in_flight", "requests in flight").Get();

		Gauge.Inc();
		Gauge.Inc(4);
		Gauge.Dec();

		CHECK ( Gauge.Get() == 4 );

		Gauge.Dec(10);

		CHECK ( Gauge.Get() == -6 );

		Gauge.Set(17);

		CHECK ( Gauge.Get() == 17 );

		CHECK ( Metrics.Serialize() ==
			"# HELP in_flight requests in flight\n"
			"# TYPE in_flight gauge\n"
			"in_flight 17\n" );
	}

	SECTION("labels")
	{
		KMetrics Metrics;

		auto& Requests = Metrics.AddCounter("http_requests_total", "count of requests", { "route", "status" });

		auto& Users2xx = Requests.Get({ "/users", "2xx" });
		auto& Users5xx = Requests.Get({ "/users", "5xx" });
		auto& Files2xx = Requests.Get({ "/files", "2xx" });

		CHECK ( &Users2xx != &Users5xx );
		CHECK ( &Users2xx != &Files2xx );
		CHECK ( &Requests.Get({ "/users", "2xx" }) == &Users2xx );

		Users2xx.Inc(3);
		Users5xx.Inc(1);
		Files2xx.Inc(7);

		// missing label values are empty, further ones are dropped
		auto& Short = Requests.Get({ "/short" });

		CHECK ( &Requests.Get({ "/short", "" })               == &Short    );
		CHECK ( &Requests.Get({ "/users", "2xx", "ignored" }) == &Users2xx );

		Short.Inc();

		// in the order the values were created
		CHECK ( Metrics.Serialize() ==
			"# HELP http_requests_total count of requests\n"
			"# TYPE http_requests_total counter\n"
			"http_requests_total{route=\"/users\",status=\"2xx\"} 3\n"
			"http_requests_total{route=\"/users\",status=\"5xx\"} 1\n"
			"http_requests_total{route=\"/files\",status=\"2xx\"} 7\n"
			"http_requests_total{route=\"/short\",status=\"\"} 1\n" );

		// references stay valid while values are added
		for (int i = 0; i < 1000; ++i)
		{
			Requests.Get({ KString::to_string(i), "2xx" }).Inc();
		}

		CHECK ( Users2xx.Get() == 3 );
		CHECK ( &Requests.Get({ "/users", "2xx" }) == &Users2xx );
	}

	SECTION("escapes")
	{
		KMetrics Metrics;

		Metrics.AddCounter("escaped_total", "a \"help\" with \\ and\na line feed", { "label" })
			.Get({ "a \"value\" with \\ and\na line feed" }).Inc();

		CHECK ( Metrics.Serialize() ==
			"# HELP escaped_total a \"help\" with \\\\ and\\na line feed\n"
			"# TYPE escaped_total counter\n"
			"escaped_total{label=\"a \\\"value\\\" with \\\\ and\\na line feed\"} 1\n" );

		// label values that are not ASCII pass as they are
		KMetrics UTF8;

		UTF8.AddGauge("names", "names", { "name" }).Get({ "Jörg 水" }).Set(1);

		CHECK ( UTF8.Serialize().contains("names{name=\"Jörg 水\"} 1\n") );
	}

	SECTION("histogram of durations")
	{
		KMetrics Metrics;

		auto& Durations = Metrics.AddHistogram("request_duration_seconds", "duration of requests", { "route" });

		CHECK ( Durations.GetType() == KMetrics::Type::Histogram );

		auto& Histogram = Durations.Get({ "/users" });

		// the default buckets: 5 ms to 10 s
		REQUIRE ( Histogram.GetBuckets().Bounds.size() == 11 );
		CHECK   ( Histogram.GetBuckets().Bounds.front() == 5000000     );
		CHECK   ( Histogram.GetBuckets().Bounds.back()  == 10000000000 );
		CHECK   ( Histogram.GetCount()    == 0   );
		CHECK   ( Histogram.Quantile(0.5) == 0.0 );

		Histogram.Observe(chrono::milliseconds(3));    // up to 5 ms
		Histogram.Observe(chrono::milliseconds(5));    // a bound belongs to its bucket
		Histogram.Observe(chrono::microseconds(5001)); // up to 10 ms
		Histogram.Observe(chrono::milliseconds(70));   // up to 100 ms
		Histogram.Observe(chrono::milliseconds(70));
		Histogram.Observe(chrono::seconds(3));         // up to 5 s
		Histogram.Observe(chrono::seconds(30));        // above all bounds
		Histogram.Observe(chrono::seconds(-1));        // counts as 0

		CHECK ( Histogram.GetCount() == 8 );
		CHECK ( Histogram.GetSum()   == 33153001000ull );

		auto Counts = Histogram.GetCounts();

		REQUIRE ( Counts.size() == 12 );
		CHECK ( Counts[0]  == 3 );
		CHECK ( Counts[1]  == 1 );
		CHECK ( Counts[2]  == 0 );
		CHECK ( Counts[4]  == 2 );
		CHECK ( Counts[9]  == 1 );
		CHECK ( Counts[10] == 0 );
		CHECK ( Counts[11] == 1 );

		CHECK ( Metrics.Serialize() ==
			"# HELP request_duration_seconds duration of requests\n"
			"# TYPE request_duration_seconds histogram\n"
			"request_duration_seconds_bucket{route=\"/users\",le=\"0.005\"} 3\n"
			"request_duration_seconds_bucket{route=\"/users\",le=\"0.01\"} 4\n"
			"request_duration_seconds_bucket{route=\"/users\",le=\"0.025\"} 4\n"
			"request_duration_seconds_bucket{route=\"/users\",le=\"0.05\"} 4\n"
			"request_duration_seconds_bucket{route=\"/users\",le=\"0.1\"} 6\n"
			"request_duration_seconds_bucket{route=\"/users\",le=\"0.25\"} 6\n"
			"request_duration_seconds_bucket{route=\"/users\",le=\"0.5\"} 6\n"
			"request_duration_seconds_bucket{route=\"/users\",le=\"1\"} 6\n"
			"request_duration_seconds_bucket{route=\"/users\",le=\"2.5\"} 6\n"
			"request_duration_seconds_bucket{route=\"/users\",le=\"5\"} 7\n"
			"request_duration_seconds_bucket{route=\"/users\",le=\"10\"} 7\n"
			"request_duration_seconds_bucket{route=\"/users\",le=\"+Inf\"} 8\n"
			"request_duration_seconds_sum{route=\"/users\"} 33.153001\n"
			"request_duration_seconds_count{route=\"/users\"} 8\n" );
	}

	SECTION("histogram without labels and with own buckets")
	{
		KMetrics Metrics;

		// bounds are sorted, and a bound that comes twice counts once
		KMetrics::Buckets Sizes;
		Sizes.Bounds = { 1000, 10, 100, 100 };

		auto& Histogram = Metrics.AddHistogram("size_bytes", "sizes", {}, Sizes).Get();

		REQUIRE ( Histogram.GetBuckets().Bounds.size() == 3 );
		CHECK   ( Histogram.GetBuckets().Bounds[0] == 10   );
		CHECK   ( Histogram.GetBuckets().Bounds[2] == 1000 );

		Histogram.Observe(0);
		Histogram.Observe(10);
		Histogram.Observe(11);
		Histogram.Observe(5000);

		CHECK ( Metrics.Serialize() ==
			"# HELP size_bytes sizes\n"
			"# TYPE size_bytes histogram\n"
			"size_bytes_bucket{le=\"10\"} 2\n"
			"size_bytes_bucket{le=\"100\"} 3\n"
			"size_bytes_bucket{le=\"1000\"} 3\n"
			"size_bytes_bucket{le=\"+Inf\"} 4\n"
			"size_bytes_sum 5021\n"
			"size_bytes_count 4\n" );

		// own bounds for durations
		auto& Fast = Metrics.AddHistogram("fast_seconds", "fast", {},
			KMetrics::Buckets::Durations({ chrono::microseconds(100), chrono::milliseconds(1) })).Get();

		Fast.Observe(chrono::microseconds(50));

		CHECK ( Metrics.Serialize().contains("fast_seconds_bucket{le=\"0.0001\"} 1\n") );
		CHECK ( Metrics.Serialize().contains("fast_seconds_bucket{le=\"0.001\"} 1\n")  );

		// a histogram without any bound still counts
		KMetrics::Buckets None;

		auto& Unbounded = Metrics.AddHistogram("unbounded", "no bounds", {}, None).Get();

		Unbounded.Observe(7);

		CHECK ( Unbounded.GetCount()    == 1   );
		CHECK ( Unbounded.GetSum()      == 7   );
		CHECK ( Unbounded.Quantile(0.5) == 0.0 );
	}

	SECTION("quantiles")
	{
		KMetrics Metrics;

		KMetrics::Buckets Tens;
		Tens.Bounds = { 10, 20, 30, 40 };

		auto& Histogram = Metrics.AddHistogram("values", "values", {}, Tens).Get();

		// ten values in each of the first two buckets
		for (int i = 0; i < 10; ++i)
		{
			Histogram.Observe(5);
			Histogram.Observe(15);
		}

		// the values of a bucket count as spread evenly over it
		CHECK ( Histogram.Quantile(0.5)  == Approx(10.0) );
		CHECK ( Histogram.Quantile(0.25) == Approx(5.0)  );
		CHECK ( Histogram.Quantile(0.75) == Approx(15.0) );
		CHECK ( Histogram.Quantile(1.0)  == Approx(20.0) );
		CHECK ( Histogram.Quantile(0.0)  == Approx(0.0)  );
		// out of range
		CHECK ( Histogram.Quantile(7.0)  == Approx(20.0) );
		CHECK ( Histogram.Quantile(-1.0) == Approx(0.0)  );

		// twenty values above all bounds: the upper half is only known as "more than 40"
		for (int i = 0; i < 20; ++i)
		{
			Histogram.Observe(1000);
		}

		CHECK ( Histogram.Quantile(0.25) == Approx(10.0) );
		CHECK ( Histogram.Quantile(0.5)  == Approx(20.0) );
		CHECK ( Histogram.Quantile(0.75) == Approx(40.0) );
		CHECK ( Histogram.Quantile(0.99) == Approx(40.0) );

		// in the unit of the output
		auto& Durations = Metrics.AddHistogram("durations_seconds", "durations").Get();

		for (int i = 0; i < 100; ++i)
		{
			Durations.Observe(chrono::milliseconds(20));
		}

		// all in the bucket from 10 to 25 ms
		CHECK ( Durations.Quantile(0.5) == Approx(0.0175) );
		CHECK ( Durations.Quantile(1.0) == Approx(0.025)  );
	}

	SECTION("callbacks")
	{
		KMetrics Metrics;

		double fValue { 1.5 };

		CHECK ( Metrics.AddCallback("pool_objects", "objects of a pool", KMetrics::Type::Gauge,
		                            [&fValue]() { return fValue; }, { "pool", "state" }, { "db", "used" }) );
		CHECK ( Metrics.AddCallback("pool_objects", "objects of a pool", KMetrics::Type::Gauge,
		                            []() { return 20.0; }, { "pool", "state" }, { "db", "available" }) );
		CHECK ( Metrics.AddCallback("uptime_seconds_total", "seconds since the start", KMetrics::Type::Counter,
		                            []() { return 12345678901.0; }) );

		// the same label values again, no callback, and a histogram
		CHECK ( Metrics.AddCallback("pool_objects", "objects of a pool", KMetrics::Type::Gauge,
		                            []() { return 0.0; }, { "pool", "state" }, { "db", "used" }) == false );
		CHECK ( Metrics.AddCallback("nothing", "nothing", KMetrics::Type::Gauge, KMetrics::Callback{}) == false );
		CHECK ( Metrics.AddCallback("histogram", "histogram", KMetrics::Type::Histogram, []() { return 0.0; }) == false );

		CHECK ( Metrics.Serialize() ==
			"# HELP pool_objects objects of a pool\n"
			"# TYPE pool_objects gauge\n"
			"pool_objects{pool=\"db\",state=\"used\"} 1.5\n"
			"pool_objects{pool=\"db\",state=\"available\"} 20\n"
			"# HELP uptime_seconds_total seconds since the start\n"
			"# TYPE uptime_seconds_total counter\n"
			"uptime_seconds_total 12345678901\n" );

		// the value of the moment of the output
		fValue = 3;

		CHECK ( Metrics.Serialize().contains("pool_objects{pool=\"db\",state=\"used\"} 3\n") );

		// values that are no numbers
		fValue = std::numeric_limits<double>::infinity();
		CHECK ( Metrics.Serialize().contains("state=\"used\"} +Inf\n") );
		fValue = -std::numeric_limits<double>::infinity();
		CHECK ( Metrics.Serialize().contains("state=\"used\"} -Inf\n") );
		fValue = std::numeric_limits<double>::quiet_NaN();
		CHECK ( Metrics.Serialize().contains("state=\"used\"} NaN\n")  );

		// a removed callback is not asked anymore
		CHECK ( Metrics.RemoveCallback("pool_objects", { "db", "used" })          );
		CHECK ( Metrics.RemoveCallback("pool_objects", { "db", "used" }) == false );
		CHECK ( Metrics.RemoveCallback("unknown")                        == false );

		CHECK ( Metrics.Serialize().contains("state=\"used\"")      == false );
		CHECK ( Metrics.Serialize().contains("state=\"available\"")          );

		CHECK ( Metrics.RemoveCallback("uptime_seconds_total") );
	}

	SECTION("a name with two kinds of metrics")
	{
		KMetrics Metrics;

		auto& Counter = Metrics.AddCounter("name", "a counter").Get();

		Counter.Inc();

		// the second metric works, but only the first is written
		auto& Gauge     = Metrics.AddGauge("name", "a gauge").Get();
		auto& Histogram = Metrics.AddHistogram("name", "a histogram").Get();

		Gauge.Set(5);
		Histogram.Observe(1);

		CHECK ( Gauge.Get()          == 5 );
		CHECK ( Histogram.GetCount() == 1 );
		CHECK ( Metrics.AddCallback("name", "a callback", KMetrics::Type::Counter, []() { return 1.0; }) == false );
		CHECK ( Metrics.RemoveCallback("name") == false );

		CHECK ( Metrics.Serialize() ==
			"# HELP name a counter\n"
			"# TYPE name counter\n"
			"name 1\n" );
	}

	SECTION("JSON")
	{
		KMetrics Metrics;

		Metrics.AddCounter("requests_total", "count of requests", { "route" }).Get({ "/users" }).Inc(3);
		Metrics.AddGauge("in_flight", "requests in flight").Get().Set(2);
		Metrics.AddCallback("memory_bytes", "memory", KMetrics::Type::Gauge, []() { return 1024.0; });

		KMetrics::Buckets Tens;
		Tens.Bounds = { 10, 20 };

		auto& Histogram = Metrics.AddHistogram("values", "values", { "kind" }, Tens).Get({ "a" });

		Histogram.Observe(5);
		Histogram.Observe(15);
		Histogram.Observe(15);
		Histogram.Observe(50);

		auto jMetrics = Metrics.ToJSON();

		REQUIRE ( jMetrics.size() == 4 );

		CHECK ( jMetrics(0)("name").String() == "requests_total"    );
		CHECK ( jMetrics(0)("type").String() == "counter"           );
		CHECK ( jMetrics(0)("help").String() == "count of requests" );
		REQUIRE ( jMetrics(0)("values").size() == 1 );
		CHECK ( jMetrics(0)("values")(0)("labels")("route").String() == "/users" );
		CHECK ( jMetrics(0)("values")(0)("value").UInt64()            == 3       );

		CHECK ( jMetrics(1)("type").String() == "gauge" );
		CHECK ( jMetrics(1)("values")(0)("value").Int64() == 2 );
		CHECK ( jMetrics(1)("values")(0)("labels").is_object() );
		CHECK ( jMetrics(1)("values")(0)("labels").empty()     );

		CHECK ( jMetrics(2)("name").String() == "memory_bytes" );
		CHECK ( jMetrics(2)("values")(0)("value").UInt64() == 1024 );

		auto& jHistogram = jMetrics(3)("values")(0);

		CHECK ( jMetrics(3)("type").String() == "histogram" );
		CHECK ( jHistogram("labels")("kind").String() == "a" );
		CHECK ( jHistogram("count").UInt64() == 4  );
		CHECK ( jHistogram("sum").UInt64()   == 85 );
		// cumulative counts, without the values above all bounds
		REQUIRE ( jHistogram("buckets").size() == 2 );
		CHECK ( jHistogram("buckets")(0)("le").UInt64()    == 10 );
		CHECK ( jHistogram("buckets")(0)("count").UInt64() == 1  );
		CHECK ( jHistogram("buckets")(1)("le").UInt64()    == 20 );
		CHECK ( jHistogram("buckets")(1)("count").UInt64() == 3  );
		CHECK ( jHistogram("p50").is_number() );
		CHECK ( jHistogram("p90").is_number() );
		CHECK ( jHistogram("p99").is_number() );

		// an empty registry
		KMetrics Empty;

		CHECK ( Empty.Serialize() == ""     );
		CHECK ( Empty.ToJSON().is_array()   );
		CHECK ( Empty.ToJSON().empty()      );

		// appended to a string with content
		KString sOut("# before\n");
		Empty.AddGauge("one", "one").Get().Set(1);
		Empty.Serialize(sOut);

		CHECK ( sOut == "# before\n# HELP one one\n# TYPE one gauge\none 1\n" );
	}

	SECTION("several threads")
	{
		KMetrics Metrics;

		auto& Requests  = Metrics.AddCounter("requests_total", "count of requests", { "thread" });
		auto& Total     = Metrics.AddCounter("total", "all requests").Get();
		auto& InFlight  = Metrics.AddGauge("in_flight", "requests in flight").Get();
		auto& Histogram = Metrics.AddHistogram("durations_seconds", "durations").Get();

		constexpr int iThreads = 4;
		constexpr int iRounds  = 20000;

		std::atomic<bool>        bStop { false };
		std::vector<std::thread> Threads;

		for (int iThread = 0; iThread < iThreads; ++iThread)
		{
			Threads.emplace_back([&, iThread]()
			{
				for (int iRound = 0; iRound < iRounds; ++iRound)
				{
					InFlight.Inc();
					// every thread asks for its own value, and all of them for a shared one
					Requests.Get({ KString::to_string(iThread) }).Inc();
					Requests.Get({ "shared" }).Inc();
					Total.Inc();
					Histogram.Observe(chrono::milliseconds(iRound % 20));
					InFlight.Dec();
				}
			});
		}

		// the output runs while the values change
		std::thread Reader([&]()
		{
			while (!bStop)
			{
				auto sText = Metrics.Serialize();
				auto jAll  = Metrics.ToJSON();
			}
		});

		for (auto& Thread : Threads)
		{
			Thread.join();
		}

		bStop = true;
		Reader.join();

		CHECK ( Total.Get()                      == iThreads * iRounds );
		CHECK ( Requests.Get({ "shared" }).Get() == iThreads * iRounds );
		CHECK ( Requests.Get({ "0" }).Get()      == iRounds            );
		CHECK ( InFlight.Get()                   == 0                  );
		CHECK ( Histogram.GetCount()             == iThreads * iRounds );
	}
}
