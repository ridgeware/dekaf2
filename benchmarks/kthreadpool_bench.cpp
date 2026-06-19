
#include <cinttypes>
#include <atomic>
#include <thread>
#include <dekaf2/time/duration/kprof.h>
#include <dekaf2/threading/execution/kthreadpool.h>

using namespace dekaf2;

// Measures the per-task scheduling overhead of KThreadPool: each task is trivial (a single
// atomic increment), so the wall time is dominated by push + dequeue + the worker handshake,
// not by the task body. This isolates what the fair scheduler adds over a plain FIFO:
//   - "untagged" uses the single-tag fast path (what untagged push() takes)
//   - the "DRR N tags" cases take the weighted Deficit Round Robin dequeue path (O(#tags))
//   - the "capped" case additionally exercises the per-tag concurrency-cap check
// Per-op numbers far below typical I/O latencies mean the scheduler cost is in the noise for
// I/O-bound pools (the use case: long-I/O tasks, up to ~1000 threads, bursts of queued work).

namespace {

constexpr std::size_t s_iTasks   = 200000;
constexpr std::size_t s_iThreads = 4;

} // anonymous namespace

void kthreadpool_bench()
{
	dekaf2::KProf ps("-KThreadPool");

	// 1) untagged: only the default tag exists -> single-tag fast path (no DRR arithmetic)
	{
		KThreadPool Pool(s_iThreads, KThreadPool::PrestartAll, KThreadPool::ShrinkNever);
		std::atomic<std::size_t> iDone { 0 };

		dekaf2::KProf prof("untagged push+exec (fast path)");
		prof.SetMultiplier(s_iTasks);

		for (std::size_t i = 0; i < s_iTasks; ++i)
		{
			Pool.push([&iDone]() { ++iDone; });
		}
		while (iDone.load() < s_iTasks) { std::this_thread::yield(); }

		KProf::Force(&iDone);
	}

	// 2) four work-class tags -> the DRR dequeue scans across the tags
	{
		KThreadPool Pool(s_iThreads, KThreadPool::PrestartAll, KThreadPool::ShrinkNever);
		for (KThreadPool::Tag t = 1; t <= 4; ++t) { Pool.ConfigureTag(t, KThreadPool::TagConfig{}); }
		std::atomic<std::size_t> iDone { 0 };

		dekaf2::KProf prof("tagged push+exec (DRR, 4 tags)");
		prof.SetMultiplier(s_iTasks);

		for (std::size_t i = 0; i < s_iTasks; ++i)
		{
			Pool.PushTagged(1 + (i % 4), [&iDone]() { ++iDone; });
		}
		while (iDone.load() < s_iTasks) { std::this_thread::yield(); }

		KProf::Force(&iDone);
	}

	// 3) sixteen tags -> shows how the O(#tags) dequeue scan trends with more work classes
	{
		KThreadPool Pool(s_iThreads, KThreadPool::PrestartAll, KThreadPool::ShrinkNever);
		for (KThreadPool::Tag t = 1; t <= 16; ++t) { Pool.ConfigureTag(t, KThreadPool::TagConfig{}); }
		std::atomic<std::size_t> iDone { 0 };

		dekaf2::KProf prof("tagged push+exec (DRR, 16 tags)");
		prof.SetMultiplier(s_iTasks);

		for (std::size_t i = 0; i < s_iTasks; ++i)
		{
			Pool.PushTagged(1 + (i % 16), [&iDone]() { ++iDone; });
		}
		while (iDone.load() < s_iTasks) { std::this_thread::yield(); }

		KProf::Force(&iDone);
	}

	// 4) a single tag with a concurrency cap -> exercises the per-tag running-count bookkeeping
	{
		KThreadPool Pool(s_iThreads, KThreadPool::PrestartAll, KThreadPool::ShrinkNever);
		KThreadPool::TagConfig Cfg;
		Cfg.iMaxConcurrency = 2;
		Pool.ConfigureTag(1, Cfg);
		std::atomic<std::size_t> iDone { 0 };

		dekaf2::KProf prof("tagged push+exec (cap=2)");
		prof.SetMultiplier(s_iTasks);

		for (std::size_t i = 0; i < s_iTasks; ++i)
		{
			Pool.PushTagged(1, [&iDone]() { ++iDone; });
		}
		while (iDone.load() < s_iTasks) { std::this_thread::yield(); }

		KProf::Force(&iDone);
	}

	// 5+6) void vs non-void on the SAME tag and scheduling path - isolates the cost of the extra
	// packaged_task the non-void overload wraps to carry the result (void builds a single task)
	{
		KThreadPool Pool(s_iThreads, KThreadPool::PrestartAll, KThreadPool::ShrinkNever);
		std::atomic<std::size_t> iDone { 0 };

		dekaf2::KProf prof("PushTagged void (1 task alloc)");
		prof.SetMultiplier(s_iTasks);

		for (std::size_t i = 0; i < s_iTasks; ++i)
		{
			Pool.PushTagged(1, [&iDone]() { ++iDone; });            // void overload -> single packaged_task
		}
		while (iDone.load() < s_iTasks) { std::this_thread::yield(); }

		KProf::Force(&iDone);
	}
	{
		KThreadPool Pool(s_iThreads, KThreadPool::PrestartAll, KThreadPool::ShrinkNever);
		std::atomic<std::size_t> iDone { 0 };

		dekaf2::KProf prof("PushTagged non-void (2 task allocs)");
		prof.SetMultiplier(s_iTasks);

		for (std::size_t i = 0; i < s_iTasks; ++i)
		{
			Pool.PushTagged(1, [&iDone]() { ++iDone; return 0; });  // non-void -> inner packaged_task<int()> wrapped
		}
		while (iDone.load() < s_iTasks) { std::this_thread::yield(); }

		KProf::Force(&iDone);
	}

} // kthreadpool_bench
