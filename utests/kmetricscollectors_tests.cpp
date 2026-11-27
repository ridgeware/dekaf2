#include "catch.hpp"
#include <dekaf2/util/metrics/kmetricscollectors.h>
#include <dekaf2/util/metrics/kmetrics.h>
#include <dekaf2/containers/memory/kpool.h>
#include <dekaf2/threading/execution/kthreadpool.h>
#include <dekaf2/system/filesystem/kfilesystem.h>
#include <dekaf2/system/os/kheapmon.h>
#include <dekaf2/io/readwrite/kwriter.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/time/clock/ktime.h>
#include <dekaf2/time/duration/kcircuitbreaker.h>
#include <atomic>
#include <thread>

using namespace dekaf2;

namespace {

/// @return how many lines of the text start with this metric name
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

struct Pooled
{
	KString sValue;
};

} // end of anonymous namespace

TEST_CASE("KMetricsCollectors")
{
	SECTION("the values of the process")
	{
		auto Before = KMetricsCollectors::GetProcess();

		CHECK ( Before.fCPUSeconds >= 0.0 );
		// the library was loaded before this test, and not long ago
		CHECK ( Before.tStart <= KUnixTime::now() );
		CHECK ( Before.tStart >  KUnixTime::now() - chrono::hours(24) );

#if DEKAF2_IS_LINUX || DEKAF2_IS_MACOS
		// the test program has megabytes of memory, the standard descriptors and this thread
		CHECK ( Before.iResidentBytes   >  1024 * 1024           );
		CHECK ( Before.iVirtualBytes    >= Before.iResidentBytes );
		CHECK ( Before.iOpenDescriptors >= 3                     );
		CHECK ( Before.iMaxDescriptors  >= Before.iOpenDescriptors );
		CHECK ( Before.iThreads         >= 1                     );

		// one more file and one more thread
		KTempDir TempDir;
		KOutFile File(kFormat("{}/open.txt", TempDir.Name()));

		REQUIRE ( File.is_open() );

		std::atomic<bool> bStop { false };

		std::thread Thread([&bStop]()
		{
			while (!bStop)
			{
				std::this_thread::yield();
			}
		});

		auto During = KMetricsCollectors::GetProcess();

		bStop = true;
		Thread.join();

		CHECK ( During.iOpenDescriptors >= Before.iOpenDescriptors + 1 );
		CHECK ( During.iThreads         >= Before.iThreads + 1         );
		CHECK ( During.fCPUSeconds      >= Before.fCPUSeconds          );
#endif
	}

	SECTION("process metrics")
	{
		KMetrics Metrics;

		KMetricsCollectors::AddProcess(Metrics);

		auto sText = Metrics.Serialize();

		CHECK ( sText.contains("# TYPE process_cpu_seconds_total counter\n") );
		CHECK ( sText.contains("# TYPE process_start_time_seconds gauge\n")  );
		CHECK ( CountLines(sText, "process_cpu_seconds_total ")  == 1 );
		CHECK ( CountLines(sText, "process_start_time_seconds ") == 1 );

#if DEKAF2_IS_LINUX || DEKAF2_IS_MACOS
		CHECK ( CountLines(sText, "process_resident_memory_bytes ") == 1 );
		CHECK ( CountLines(sText, "process_virtual_memory_bytes ")  == 1 );
		CHECK ( CountLines(sText, "process_open_fds ")              == 1 );
		CHECK ( CountLines(sText, "process_max_fds ")               == 1 );
		CHECK ( CountLines(sText, "process_threads ")               == 1 );
#endif

		// the values are numbers, and the start is a time of today
		for (auto& jMetric : Metrics.ToJSON())
		{
			INFO ( jMetric("name").String() );

			REQUIRE ( jMetric("values").size() == 1 );
			CHECK   ( jMetric("values")(0)("value").is_number() );

			if (jMetric("name").String() == "process_start_time_seconds")
			{
				CHECK ( jMetric("values")(0)("value").UInt64() >  static_cast<uint64_t>((KUnixTime::now() - chrono::hours(24)).to_time_t()) );
				CHECK ( jMetric("values")(0)("value").UInt64() <= static_cast<uint64_t>(KUnixTime::now().to_time_t()) );
			}
		}

		// added a second time, every metric still has one value
		KMetricsCollectors::AddProcess(Metrics);

		CHECK ( CountLines(Metrics.Serialize(), "process_cpu_seconds_total ") == 1 );
	}

	SECTION("heap metrics")
	{
		KMetrics Metrics;

		Heap::Usage Usage;

		bool bHasUsage = Heap::GetUsage(Usage);
		bool bAdded    = KMetricsCollectors::AddHeap(Metrics);

		CHECK ( bAdded == bHasUsage );

		auto sText = Metrics.Serialize();

		if (bAdded)
		{
			CHECK ( Usage.iAllocated >  0                );
			CHECK ( Usage.iActive    >= Usage.iAllocated );
			CHECK ( CountLines(sText, "heap_allocated_bytes ") == 1 );
			CHECK ( CountLines(sText, "heap_active_bytes ")    == 1 );
			CHECK ( CountLines(sText, "heap_mapped_bytes ")    == 1 );
			// an old jemalloc does not tell the resident memory
			CHECK ( CountLines(sText, "heap_resident_bytes ")  == (Usage.iResident ? 1 : 0) );
			CHECK ( Metrics.ToJSON()(0)("values")(0)("value").UInt64() > 0 );
		}
		else
		{
			// without the numbers there is no metric
			CHECK ( sText.empty() );
		}
	}

	SECTION("thread pool metrics from diagnostics")
	{
		KMetrics Metrics;

		KThreadPool::Diagnostics Diagnostics;

		Diagnostics.iUsedThreads  = 3;
		Diagnostics.iIdleThreads  = 5;
		Diagnostics.iMaxThreads   = 20;
		Diagnostics.iWaitingTasks = 7;
		Diagnostics.iTotalTasks   = 1234;

		CHECK ( KMetricsCollectors::AddThreadPool(Metrics, "workers", [&Diagnostics]() { return Diagnostics; }) );
		// the name is taken
		CHECK ( KMetricsCollectors::AddThreadPool(Metrics, "workers", [&Diagnostics]() { return Diagnostics; }) == false );
		// no function
		CHECK ( KMetricsCollectors::AddThreadPool(Metrics, "none", std::function<KThreadPool::Diagnostics()>{}) == false );

		CHECK ( Metrics.Serialize() ==
			"# HELP threadpool_threads threads of the pool by their state\n"
			"# TYPE threadpool_threads gauge\n"
			"threadpool_threads{pool=\"workers\",state=\"used\"} 3\n"
			"threadpool_threads{pool=\"workers\",state=\"idle\"} 5\n"
			"# HELP threadpool_threads_max the most threads the pool may have\n"
			"# TYPE threadpool_threads_max gauge\n"
			"threadpool_threads_max{pool=\"workers\"} 20\n"
			"# HELP threadpool_tasks_waiting tasks that wait for a thread of the pool\n"
			"# TYPE threadpool_tasks_waiting gauge\n"
			"threadpool_tasks_waiting{pool=\"workers\"} 7\n"
			"# HELP threadpool_tasks_total tasks that the pool has taken\n"
			"# TYPE threadpool_tasks_total counter\n"
			"threadpool_tasks_total{pool=\"workers\"} 1234\n" );

		// the values of the moment
		Diagnostics.iUsedThreads = 4;

		CHECK ( Metrics.Serialize().contains("threadpool_threads{pool=\"workers\",state=\"used\"} 4\n") );

		// a second pool, and the first one removed
		CHECK ( KMetricsCollectors::AddThreadPool(Metrics, "others", [&Diagnostics]() { return Diagnostics; }) );

		KMetricsCollectors::RemoveThreadPool(Metrics, "workers");

		auto sText = Metrics.Serialize();

		CHECK ( sText.contains("pool=\"workers\"") == false );
		CHECK ( CountLines(sText, "threadpool_threads{pool=\"others\"")       == 2 );
		CHECK ( CountLines(sText, "threadpool_tasks_total{pool=\"others\"}")  == 1 );

		// removed pools can be added again
		CHECK ( KMetricsCollectors::AddThreadPool(Metrics, "workers", [&Diagnostics]() { return Diagnostics; }) );
	}

	SECTION("thread pool metrics from a pool")
	{
		KMetrics    Metrics;
		KThreadPool Pool(3);

		REQUIRE ( KMetricsCollectors::AddThreadPool(Metrics, "pool", Pool) );

		std::atomic<int> iDone { 0 };

		for (int i = 0; i < 10; ++i)
		{
			Pool.push([&iDone]() { ++iDone; }).wait();
		}

		CHECK ( iDone == 10 );

		auto Diagnostics = Pool.get_diagnostics();
		auto sText       = Metrics.Serialize();

		CHECK ( Diagnostics.iTotalTasks >= 10 );
		CHECK ( sText.contains(kFormat("threadpool_tasks_total{{pool=\"pool\"}} {}\n", Diagnostics.iTotalTasks)) );
		CHECK ( sText.contains(kFormat("threadpool_threads_max{{pool=\"pool\"}} {}\n", Diagnostics.iMaxThreads)) );
		CHECK ( sText.contains("threadpool_tasks_waiting{pool=\"pool\"} 0\n") );

		// before the pool goes
		KMetricsCollectors::RemoveThreadPool(Metrics, "pool");

		CHECK ( Metrics.Serialize().contains("pool=\"pool\"") == false );
	}

	SECTION("object pool metrics")
	{
		KMetrics Metrics;

		KMetricsCollectors::Pool Values;

		Values.iUsed      = 2;
		Values.iAvailable = 6;
		Values.iMax       = 50;

		CHECK ( KMetricsCollectors::AddPool(Metrics, "database", [&Values]() { return Values; }) );
		CHECK ( KMetricsCollectors::AddPool(Metrics, "database", [&Values]() { return Values; }) == false );
		CHECK ( KMetricsCollectors::AddPool(Metrics, "none", std::function<KMetricsCollectors::Pool()>{}) == false );

		CHECK ( Metrics.Serialize() ==
			"# HELP pool_objects objects of the pool by their state\n"
			"# TYPE pool_objects gauge\n"
			"pool_objects{pool=\"database\",state=\"used\"} 2\n"
			"pool_objects{pool=\"database\",state=\"available\"} 6\n"
			"# HELP pool_objects_max the most objects the pool may hold\n"
			"# TYPE pool_objects_max gauge\n"
			"pool_objects_max{pool=\"database\"} 50\n" );

		// a KPool and a KSharedPool
		KPool<Pooled>       Unique;
		KSharedPool<Pooled> Shared;

		CHECK ( KMetricsCollectors::AddPool(Metrics, "unique", Unique) );
		CHECK ( KMetricsCollectors::AddPool(Metrics, "shared", Shared) );

		CHECK ( Metrics.Serialize().contains("pool_objects{pool=\"unique\",state=\"used\"} 0\n") );

		{
			auto First  = Unique.get();
			auto Second = Unique.get();
			auto Third  = Shared.get();

			auto sText = Metrics.Serialize();

			CHECK ( sText.contains("pool_objects{pool=\"unique\",state=\"used\"} 2\n") );
			CHECK ( sText.contains("pool_objects{pool=\"shared\",state=\"used\"} 1\n") );
		}

		// back in the pools
		auto sText = Metrics.Serialize();

		CHECK ( sText.contains("pool_objects{pool=\"unique\",state=\"used\"} 0\n")      );
		CHECK ( sText.contains("pool_objects{pool=\"unique\",state=\"available\"} 2\n") );
		CHECK ( sText.contains("pool_objects{pool=\"shared\",state=\"available\"} 1\n") );
		CHECK ( sText.contains(kFormat("pool_objects_max{{pool=\"unique\"}} {}\n", Unique.GetStats().iMaxPool)) );

		KMetricsCollectors::RemovePool(Metrics, "unique");
		KMetricsCollectors::RemovePool(Metrics, "shared");
		KMetricsCollectors::RemovePool(Metrics, "database");

		CHECK ( CountLines(Metrics.Serialize(), "pool_objects") == 0 );
	}

	SECTION("circuit breaker metrics")
	{
		KMetrics Metrics;

		// opens after two failures, for 10 seconds
		KBackoff::Options OpenTime(chrono::seconds(10), chrono::seconds(10));
		OpenTime.dJitter = 0.0;

		KCircuitBreaker Breaker(KCircuitBreaker::Options(2, OpenTime));
		KSteadyTime     tStart = KSteadyTime::now();

		CHECK ( KMetricsCollectors::AddCircuitBreaker(Metrics, "payments", Breaker) );
		// the name is taken
		CHECK ( KMetricsCollectors::AddCircuitBreaker(Metrics, "payments", Breaker) == false );

		CHECK ( Metrics.Serialize() ==
			"# HELP circuitbreaker_state state of the circuit breaker: 1 for the current state, 0 for the others\n"
			"# TYPE circuitbreaker_state gauge\n"
			"circuitbreaker_state{breaker=\"payments\",state=\"closed\"} 1\n"
			"circuitbreaker_state{breaker=\"payments\",state=\"open\"} 0\n"
			"circuitbreaker_state{breaker=\"payments\",state=\"half_open\"} 0\n"
			"# HELP circuitbreaker_openings_total how often the circuit breaker opened\n"
			"# TYPE circuitbreaker_openings_total counter\n"
			"circuitbreaker_openings_total{breaker=\"payments\"} 0\n"
			"# HELP circuitbreaker_refused_total calls that the circuit breaker refused\n"
			"# TYPE circuitbreaker_refused_total counter\n"
			"circuitbreaker_refused_total{breaker=\"payments\"} 0\n" );

		// two failures open it
		Breaker.Try(tStart).Failure(tStart);
		Breaker.Try(tStart).Failure(tStart);

		// three calls are refused during the open time
		CHECK_FALSE ( Breaker.Try(tStart) );
		CHECK_FALSE ( Breaker.Try(tStart) );
		CHECK_FALSE ( Breaker.Try(tStart + chrono::seconds(9)) );

		auto sText = Metrics.Serialize();

		CHECK ( sText.contains("circuitbreaker_state{breaker=\"payments\",state=\"closed\"} 0\n")    );
		CHECK ( sText.contains("circuitbreaker_state{breaker=\"payments\",state=\"open\"} 1\n")      );
		CHECK ( sText.contains("circuitbreaker_state{breaker=\"payments\",state=\"half_open\"} 0\n") );
		CHECK ( sText.contains("circuitbreaker_openings_total{breaker=\"payments\"} 1\n")            );
		CHECK ( sText.contains("circuitbreaker_refused_total{breaker=\"payments\"} 3\n")             );

		{
			// the probe is under way
			auto Probe = Breaker.Try(tStart + chrono::seconds(10));

			REQUIRE ( Probe );

			sText = Metrics.Serialize();

			CHECK ( sText.contains("circuitbreaker_state{breaker=\"payments\",state=\"open\"} 0\n")      );
			CHECK ( sText.contains("circuitbreaker_state{breaker=\"payments\",state=\"half_open\"} 1\n") );

			Probe.Success();
		}

		sText = Metrics.Serialize();

		CHECK ( sText.contains("circuitbreaker_state{breaker=\"payments\",state=\"closed\"} 1\n")    );
		CHECK ( sText.contains("circuitbreaker_state{breaker=\"payments\",state=\"half_open\"} 0\n") );
		// the counts stay
		CHECK ( sText.contains("circuitbreaker_openings_total{breaker=\"payments\"} 1\n")            );
		CHECK ( sText.contains("circuitbreaker_refused_total{breaker=\"payments\"} 3\n")             );

		// a second breaker, and the first one removed
		KCircuitBreaker Other;

		CHECK ( KMetricsCollectors::AddCircuitBreaker(Metrics, "search", Other) );

		KMetricsCollectors::RemoveCircuitBreaker(Metrics, "payments");

		sText = Metrics.Serialize();

		CHECK ( sText.contains("breaker=\"payments\"") == false );
		CHECK ( CountLines(sText, "circuitbreaker_state{breaker=\"search\"")          == 3 );
		CHECK ( CountLines(sText, "circuitbreaker_refused_total{breaker=\"search\"}") == 1 );

		// removed breakers can be added again
		CHECK ( KMetricsCollectors::AddCircuitBreaker(Metrics, "payments", Breaker) );

		// before the breakers go
		KMetricsCollectors::RemoveCircuitBreaker(Metrics, "payments");
		KMetricsCollectors::RemoveCircuitBreaker(Metrics, "search");

		CHECK ( CountLines(Metrics.Serialize(), "circuitbreaker_") == 0 );
	}
}
