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


#include <dekaf2/util/metrics/kmetricscollectors.h>
#include <dekaf2/system/os/kheapmon.h>
#include <dekaf2/system/os/ksystem.h>
#include <dekaf2/system/filesystem/kfilesystem.h>
#include <dekaf2/io/readwrite/kreader.h>
#include "kconfiguration.h"
#include <chrono>
#include <memory>
#include <mutex>
#include <vector>

#if DEKAF2_IS_WINDOWS
	#include <windows.h>
	#include <psapi.h>
#else
	#include <sys/resource.h>
	#include <unistd.h>
#endif

#if DEKAF2_IS_MACOS && DEKAF2_HAS_LIBPROC
	#include <libproc.h>
#endif

DEKAF2_NAMESPACE_BEGIN

namespace {

// taken when the library is loaded, which is before main() runs
const KUnixTime s_tStart = KUnixTime::now();

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Values that several metrics share and that take a system call or more to
/// read: they are read at most once per second.
template<class Values>
class Cached
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	Cached(std::function<Values()> Read)
	: m_Read(std::move(Read))
	{
	}

	//-----------------------------------------------------------------------------
	Values Get()
	//-----------------------------------------------------------------------------
	{
		std::lock_guard<std::mutex> Lock(m_Mutex);

		auto tNow = std::chrono::steady_clock::now();

		if (!m_bRead || tNow - m_tRead >= std::chrono::seconds(1))
		{
			m_Values = m_Read();
			m_tRead  = tNow;
			m_bRead  = true;
		}

		return m_Values;
	}

//----------
private:
//----------

	std::function<Values()>               m_Read;
	std::mutex                            m_Mutex;
	Values                                m_Values;
	std::chrono::steady_clock::time_point m_tRead;
	bool                                  m_bRead { false };

}; // Cached

} // end of anonymous namespace

//-----------------------------------------------------------------------------
KMetricsCollectors::Process KMetricsCollectors::GetProcess()
//-----------------------------------------------------------------------------
{
	Process Values;

	Values.fCPUSeconds = static_cast<double>(kGetMicroTicksPerProcess()) / 1000000.0;
	Values.tStart      = s_tStart;

#if DEKAF2_IS_WINDOWS

	PROCESS_MEMORY_COUNTERS Memory;

	if (::K32GetProcessMemoryInfo(::GetCurrentProcess(), &Memory, sizeof(Memory)))
	{
		Values.iResidentBytes = Memory.WorkingSetSize;
		Values.iVirtualBytes  = Memory.PagefileUsage;
	}

	DWORD iHandles { 0 };

	if (::GetProcessHandleCount(::GetCurrentProcess(), &iHandles))
	{
		Values.iOpenDescriptors = iHandles;
	}

#else // not Windows

	struct rlimit Limit;

	if (::getrlimit(RLIMIT_NOFILE, &Limit) == 0 && Limit.rlim_cur != RLIM_INFINITY)
	{
		Values.iMaxDescriptors = Limit.rlim_cur;
	}

#if DEKAF2_IS_LINUX

	// the size of the address space and the resident memory, in pages
	auto sMemory = kReadAll("/proc/self/statm");
	auto Memory  = sMemory.Split(" ");

	if (Memory.size() >= 2)
	{
		Values.iVirtualBytes  = Memory[0].UInt64() * kGetPageSize();
		Values.iResidentBytes = Memory[1].UInt64() * kGetPageSize();
	}

	KInFile Status("/proc/self/status");

	for (auto& sLine : Status)
	{
		KStringView sValue = sLine;

		if (sValue.remove_prefix("Threads:"))
		{
			sValue.Trim();
			Values.iThreads = sValue.UInt64();
			break;
		}
	}

	// one entry per open descriptor, of which one is the descriptor of this listing
	KDirectory Descriptors("/proc/self/fd");

	if (Descriptors.size() > 0)
	{
		Values.iOpenDescriptors = Descriptors.size() - 1;
	}

#elif DEKAF2_IS_MACOS && DEKAF2_HAS_LIBPROC

	auto iPid = ::getpid();

	struct proc_taskinfo Info;

	if (::proc_pidinfo(iPid, PROC_PIDTASKINFO, 0, &Info, sizeof(Info)) == static_cast<int>(sizeof(Info)))
	{
		Values.iResidentBytes = Info.pti_resident_size;
		Values.iVirtualBytes  = Info.pti_virtual_size;
		Values.iThreads       = static_cast<uint64_t>(Info.pti_threadnum);
	}

	// without a buffer the call returns the size that the list of descriptors needs
	auto iBytes = ::proc_pidinfo(iPid, PROC_PIDLISTFDS, 0, nullptr, 0);

	if (iBytes > 0)
	{
		std::vector<struct proc_fdinfo> List(static_cast<std::size_t>(iBytes) / sizeof(struct proc_fdinfo));

		iBytes = ::proc_pidinfo(iPid, PROC_PIDLISTFDS, 0, List.data(), static_cast<int>(List.size() * sizeof(struct proc_fdinfo)));

		if (iBytes > 0)
		{
			Values.iOpenDescriptors = static_cast<std::size_t>(iBytes) / sizeof(struct proc_fdinfo);
		}
	}

#endif // of the platforms that are not Windows

#endif // not Windows

	return Values;

} // GetProcess

//-----------------------------------------------------------------------------
void KMetricsCollectors::AddProcess(KMetrics& Metrics)
//-----------------------------------------------------------------------------
{
	auto Values = std::make_shared<Cached<Process>>(&KMetricsCollectors::GetProcess);
	auto Now    = Values->Get();

	Metrics.AddCallback("process_cpu_seconds_total", "CPU time of the process in user and system mode, in seconds",
	                    KMetrics::Type::Counter, [Values]() { return Values->Get().fCPUSeconds; });

	Metrics.AddCallback("process_start_time_seconds", "start of the process, in seconds since the Unix epoch",
	                    KMetrics::Type::Gauge, [Values]() { return static_cast<double>(Values->Get().tStart.to_time_t()); });

	// a value that this platform does not tell gets no metric

	if (Now.iResidentBytes)
	{
		Metrics.AddCallback("process_resident_memory_bytes", "memory of the process in physical memory, in bytes",
		                    KMetrics::Type::Gauge, [Values]() { return static_cast<double>(Values->Get().iResidentBytes); });
	}

	if (Now.iVirtualBytes)
	{
		Metrics.AddCallback("process_virtual_memory_bytes", "size of the address space of the process, in bytes",
		                    KMetrics::Type::Gauge, [Values]() { return static_cast<double>(Values->Get().iVirtualBytes); });
	}

	if (Now.iOpenDescriptors)
	{
		Metrics.AddCallback("process_open_fds", "open file descriptors of the process",
		                    KMetrics::Type::Gauge, [Values]() { return static_cast<double>(Values->Get().iOpenDescriptors); });
	}

	if (Now.iMaxDescriptors)
	{
		Metrics.AddCallback("process_max_fds", "limit for the open file descriptors of the process",
		                    KMetrics::Type::Gauge, [Values]() { return static_cast<double>(Values->Get().iMaxDescriptors); });
	}

	if (Now.iThreads)
	{
		Metrics.AddCallback("process_threads", "threads of the process",
		                    KMetrics::Type::Gauge, [Values]() { return static_cast<double>(Values->Get().iThreads); });
	}

} // AddProcess

//-----------------------------------------------------------------------------
bool KMetricsCollectors::AddHeap(KMetrics& Metrics)
//-----------------------------------------------------------------------------
{
	Heap::Usage Usage;

	if (!Heap::GetUsage(Usage))
	{
		return false;
	}

	auto Values = std::make_shared<Cached<Heap::Usage>>([]()
	{
		Heap::Usage Usage;

		if (!Heap::GetUsage(Usage))
		{
			// a failed read gives no values
			Usage = Heap::Usage{};
		}

		return Usage;
	});

	Metrics.AddCallback("heap_allocated_bytes", "memory that the program has allocated from the heap, in bytes",
	                    KMetrics::Type::Gauge, [Values]() { return static_cast<double>(Values->Get().iAllocated); });

	Metrics.AddCallback("heap_active_bytes", "memory in the pages of the heap that hold allocations, in bytes",
	                    KMetrics::Type::Gauge, [Values]() { return static_cast<double>(Values->Get().iActive); });

	// an old jemalloc does not tell this value
	if (Usage.iResident)
	{
		Metrics.AddCallback("heap_resident_bytes", "memory of the heap in physical memory, in bytes",
		                    KMetrics::Type::Gauge, [Values]() { return static_cast<double>(Values->Get().iResident); });
	}

	Metrics.AddCallback("heap_mapped_bytes", "memory that the heap has mapped from the system, in bytes",
	                    KMetrics::Type::Gauge, [Values]() { return static_cast<double>(Values->Get().iMapped); });

	return true;

} // AddHeap

//-----------------------------------------------------------------------------
bool KMetricsCollectors::AddThreadPool(KMetrics& Metrics, KStringView sName, std::function<KThreadPool::Diagnostics()> Diagnostics)
//-----------------------------------------------------------------------------
{
	if (!Diagnostics)
	{
		return false;
	}

	// the first metric tells whether the name is taken
	if (!Metrics.AddCallback("threadpool_threads", "threads of the pool by their state", KMetrics::Type::Gauge,
	                         [Diagnostics]() { return static_cast<double>(Diagnostics().iUsedThreads); },
	                         { "pool", "state" }, { sName, "used" }))
	{
		return false;
	}

	Metrics.AddCallback("threadpool_threads", "threads of the pool by their state", KMetrics::Type::Gauge,
	                    [Diagnostics]() { return static_cast<double>(Diagnostics().iIdleThreads); },
	                    { "pool", "state" }, { sName, "idle" });

	Metrics.AddCallback("threadpool_threads_max", "the most threads the pool may have", KMetrics::Type::Gauge,
	                    [Diagnostics]() { return static_cast<double>(Diagnostics().iMaxThreads); },
	                    { "pool" }, { sName });

	Metrics.AddCallback("threadpool_tasks_waiting", "tasks that wait for a thread of the pool", KMetrics::Type::Gauge,
	                    [Diagnostics]() { return static_cast<double>(Diagnostics().iWaitingTasks); },
	                    { "pool" }, { sName });

	Metrics.AddCallback("threadpool_tasks_total", "tasks that the pool has taken", KMetrics::Type::Counter,
	                    [Diagnostics]() { return static_cast<double>(Diagnostics().iTotalTasks); },
	                    { "pool" }, { sName });

	return true;

} // AddThreadPool

//-----------------------------------------------------------------------------
bool KMetricsCollectors::AddThreadPool(KMetrics& Metrics, KStringView sName, const KThreadPool& Pool)
//-----------------------------------------------------------------------------
{
	return AddThreadPool(Metrics, sName, [&Pool]() { return Pool.get_diagnostics(); });
}

//-----------------------------------------------------------------------------
void KMetricsCollectors::RemoveThreadPool(KMetrics& Metrics, KStringView sName)
//-----------------------------------------------------------------------------
{
	Metrics.RemoveCallback("threadpool_threads"      , { sName, "used" });
	Metrics.RemoveCallback("threadpool_threads"      , { sName, "idle" });
	Metrics.RemoveCallback("threadpool_threads_max"  , { sName });
	Metrics.RemoveCallback("threadpool_tasks_waiting", { sName });
	Metrics.RemoveCallback("threadpool_tasks_total"  , { sName });
}

//-----------------------------------------------------------------------------
bool KMetricsCollectors::AddPool(KMetrics& Metrics, KStringView sName, std::function<Pool()> Values)
//-----------------------------------------------------------------------------
{
	if (!Values)
	{
		return false;
	}

	// the first metric tells whether the name is taken
	if (!Metrics.AddCallback("pool_objects", "objects of the pool by their state", KMetrics::Type::Gauge,
	                         [Values]() { return static_cast<double>(Values().iUsed); },
	                         { "pool", "state" }, { sName, "used" }))
	{
		return false;
	}

	Metrics.AddCallback("pool_objects", "objects of the pool by their state", KMetrics::Type::Gauge,
	                    [Values]() { return static_cast<double>(Values().iAvailable); },
	                    { "pool", "state" }, { sName, "available" });

	Metrics.AddCallback("pool_objects_max", "the most objects the pool may hold", KMetrics::Type::Gauge,
	                    [Values]() { return static_cast<double>(Values().iMax); },
	                    { "pool" }, { sName });

	return true;

} // AddPool

//-----------------------------------------------------------------------------
void KMetricsCollectors::RemovePool(KMetrics& Metrics, KStringView sName)
//-----------------------------------------------------------------------------
{
	Metrics.RemoveCallback("pool_objects"    , { sName, "used"      });
	Metrics.RemoveCallback("pool_objects"    , { sName, "available" });
	Metrics.RemoveCallback("pool_objects_max", { sName });
}

DEKAF2_NAMESPACE_END
