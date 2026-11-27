/*
//
// DEKAF(tm): Lighter, Faster, Smarter (tm)
//
// Copyright (c) 2026, Ridgeware, Inc.
//
// +-------------------------------------------------------------------------+
// | /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\|
// |/+---------------------------------------------------------------------+/|
// |/|                                                                     |/|
// |\|  ** THIS NOTICE MUST NOT BE REMOVED FROM THE SOURCE CODE MODULE **  |\|
// |/|                                                                     |/|
// |\|   OPEN SOURCE LICENSE                                               |\|
// |/|                                                                     |/|
// |\|   Permission is hereby granted, free of charge, to any person       |\|
// |/|   obtaining a copy of this software and associated                  |/|
// |\|   documentation files (the "Software"), to deal in the              |\|
// |/|   Software without restriction, including without limitation        |/|
// |\|   the rights to use, copy, modify, merge, publish,                  |\|
// |/|   distribute, sublicense, and/or sell copies of the Software,       |/|
// |\|   and to permit persons to whom the Software is furnished to        |\|
// |/|   do so, subject to the following conditions:                       |/|
// |\|                                                                     |\|
// |/|   The above copyright notice and this permission notice shall       |/|
// |\|   be included in all copies or substantial portions of the          |\|
// |/|   Software.                                                         |/|
// |\|                                                                     |\|
// |/|   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY         |/|
// |\|   KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE        |\|
// |/|   WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR           |/|
// |\|   PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS        |\|
// |/|   OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR          |/|
// |\|   OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR        |\|
// |/|   OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE         |/|
// |\|   SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.            |\|
// |/|                                                                     |/|
// |/+---------------------------------------------------------------------+/|
// |\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/ |
// +-------------------------------------------------------------------------+
*/


#pragma once

/// @file kmetricscollectors.h
/// metrics of the process, the heap, thread pools, object pools and circuit breakers for a KMetrics registry

#include <dekaf2/core/init/kdefinitions.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/strings/kstringview.h>
#include <dekaf2/threading/execution/kthreadpool.h>
#include <dekaf2/time/clock/ktime.h>
#include <dekaf2/time/duration/kcircuitbreaker.h>
#include <dekaf2/util/metrics/kmetrics.h>
#include <cstdint>
#include <functional>

DEKAF2_NAMESPACE_BEGIN

/// @addtogroup util_metrics
/// @{

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Adds metrics to a KMetrics registry whose values the registry asks for when
/// it is read: of the process, of the heap, of thread pools, of object pools and
/// of circuit breakers.
///
/// @code
/// KMetrics Metrics;
///
/// KMetricsCollectors::AddProcess       (Metrics);
/// KMetricsCollectors::AddHeap          (Metrics);
/// KMetricsCollectors::AddThreadPool    (Metrics, "workers", Workers);
/// KMetricsCollectors::AddPool          (Metrics, "database", Connections);
/// KMetricsCollectors::AddCircuitBreaker(Metrics, "payments", Breaker);
/// @endcode
///
/// A thread pool, an object pool and a circuit breaker have to live until they
/// are removed again, or as long as the registry.
class DEKAF2_PUBLIC KMetricsCollectors
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// the values of this process. A value that a platform does not tell is 0.
	struct DEKAF2_PUBLIC Process
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		double    fCPUSeconds      { 0 }; ///< CPU time in user and system mode
		uint64_t  iResidentBytes   { 0 }; ///< memory in physical memory
		uint64_t  iVirtualBytes    { 0 }; ///< size of the address space, on Windows the committed memory
		uint64_t  iOpenDescriptors { 0 }; ///< open file descriptors, on Windows open handles
		uint64_t  iMaxDescriptors  { 0 }; ///< the limit for open file descriptors
		uint64_t  iThreads         { 0 }; ///< threads
		KUnixTime tStart;                 ///< when the process started, taken when dekaf2 was loaded
	};

	/// the values of an object pool
	struct DEKAF2_PUBLIC Pool
	{
		uint64_t iUsed      { 0 }; ///< objects in use
		uint64_t iAvailable { 0 }; ///< objects that wait for a user
		uint64_t iMax       { 0 }; ///< the most objects the pool may hold
	};

	/// @return the current values of this process
	DEKAF2_NODISCARD
	static Process GetProcess ();

	/// Add the metrics of this process, with the names of the Prometheus client libraries:
	/// process_cpu_seconds_total, process_resident_memory_bytes, process_virtual_memory_bytes,
	/// process_open_fds, process_max_fds, process_threads and process_start_time_seconds.
	/// The values are read at most once per second.
	static void AddProcess (KMetrics& Metrics);

	/// Add the metrics of the heap: heap_allocated_bytes, heap_active_bytes, heap_mapped_bytes, and
	/// heap_resident_bytes where the allocator tells it.
	/// @return false if the heap does not tell them, which is the case without jemalloc. No
	/// metric is added then.
	static bool AddHeap (KMetrics& Metrics);

	/// Add the metrics of a thread pool, with its name as the label "pool": threadpool_threads
	/// with the label "state" (used, idle), threadpool_threads_max, threadpool_tasks_waiting and
	/// threadpool_tasks_total.
	/// @param sName the name of the pool
	/// @param Pool the pool
	/// @return false if a pool of this name was added already
	static bool AddThreadPool (KMetrics& Metrics, KStringView sName, const KThreadPool& Pool);

	/// add the metrics of a thread pool that is only reachable through its diagnostics
	/// @param sName the name of the pool
	/// @param Diagnostics returns the diagnostics of the pool
	/// @return false if a pool of this name was added already
	static bool AddThreadPool (KMetrics& Metrics, KStringView sName, std::function<KThreadPool::Diagnostics()> Diagnostics);

	/// remove the metrics of a thread pool
	static void RemoveThreadPool (KMetrics& Metrics, KStringView sName);

	/// Add the metrics of an object pool, with its name as the label "pool": pool_objects with
	/// the label "state" (used, available) and pool_objects_max.
	/// @param sName the name of the pool
	/// @param Values returns the values of the pool
	/// @return false if a pool of this name was added already
	static bool AddPool (KMetrics& Metrics, KStringView sName, std::function<Pool()> Values);

	/// add the metrics of a KPool or a KSharedPool, or of any other class with their GetStats()
	/// @param sName the name of the pool
	/// @param pool the pool
	/// @return false if a pool of this name was added already
	template<class PoolType, typename = decltype(std::declval<const PoolType&>().GetStats().iUsed)>
	static bool AddPool (KMetrics& Metrics, KStringView sName, const PoolType& pool)
	{
		return AddPool(Metrics, sName, std::function<Pool()>([&pool]()
		{
			auto Stats = pool.GetStats();

			Pool Values;
			Values.iUsed      = Stats.iUsed;
			Values.iAvailable = Stats.iAvailable;
			Values.iMax       = Stats.iMaxPool;

			return Values;
		}));
	}

	/// remove the metrics of an object pool
	static void RemovePool (KMetrics& Metrics, KStringView sName);

	/// Add the metrics of a circuit breaker, with its name as the label "breaker":
	/// circuitbreaker_state with the label "state" (closed, open, half_open), which is 1 for
	/// the current state and 0 for the others, circuitbreaker_openings_total and
	/// circuitbreaker_refused_total.
	/// @param sName the name of the breaker
	/// @param Breaker the breaker
	/// @return false if a breaker of this name was added already
	static bool AddCircuitBreaker (KMetrics& Metrics, KStringView sName, const KCircuitBreaker& Breaker);

	/// remove the metrics of a circuit breaker
	static void RemoveCircuitBreaker (KMetrics& Metrics, KStringView sName);

}; // KMetricsCollectors

/// @}

DEKAF2_NAMESPACE_END
