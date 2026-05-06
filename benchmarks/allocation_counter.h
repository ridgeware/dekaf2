#pragma once

/// @file allocation_counter.h
/// Optional global new/delete tracker for benchmarks. Activated via the
/// DEKAF2_TRACK_ALLOCATIONS macro (see allocation_counter.cpp). When inactive,
/// the AllocationSnapshot RAII helper still compiles but reports zero deltas.

#include <cstddef>
#include <atomic>
#include <iostream>
#include <iomanip>

namespace dekaf2bench {

#if DEKAF2_TRACK_ALLOCATIONS

extern std::atomic<std::size_t> g_NewCount;
extern std::atomic<std::size_t> g_NewBytes;
extern std::atomic<std::size_t> g_DeleteCount;
extern std::atomic<std::size_t> g_PeakBytes;

#endif

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// RAII helper that captures allocation counters at construction and reports
/// the delta on destruction (or via Report()). Use around a benchmark section
/// to measure the allocation footprint of that scope. When DEKAF2_TRACK_ALLOCATIONS
/// is not defined, all values are zero.
class AllocationSnapshot
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
//------
public:
//------

	explicit AllocationSnapshot(const char* sLabel = "")
	    : m_sLabel(sLabel)
	{
#if DEKAF2_TRACK_ALLOCATIONS
		m_StartNewCount    = g_NewCount.load   (std::memory_order_relaxed);
		m_StartNewBytes    = g_NewBytes.load   (std::memory_order_relaxed);
		m_StartDeleteCount = g_DeleteCount.load(std::memory_order_relaxed);
#endif
	}

	~AllocationSnapshot()
	{
		if (!m_bReported && m_sLabel && m_sLabel[0] != '\0')
		{
			Report();
		}
	}

	AllocationSnapshot(const AllocationSnapshot&) = delete;
	AllocationSnapshot& operator=(const AllocationSnapshot&) = delete;

	std::size_t NewCount() const
	{
#if DEKAF2_TRACK_ALLOCATIONS
		return g_NewCount.load(std::memory_order_relaxed) - m_StartNewCount;
#else
		return 0;
#endif
	}

	std::size_t NewBytes() const
	{
#if DEKAF2_TRACK_ALLOCATIONS
		return g_NewBytes.load(std::memory_order_relaxed) - m_StartNewBytes;
#else
		return 0;
#endif
	}

	std::size_t DeleteCount() const
	{
#if DEKAF2_TRACK_ALLOCATIONS
		return g_DeleteCount.load(std::memory_order_relaxed) - m_StartDeleteCount;
#else
		return 0;
#endif
	}

	/// emit a one-line report to stdout, format:
	///     "  [label] allocs=N (M bytes) deletes=K"
	void Report()
	{
		m_bReported = true;
		std::cout << "    [" << (m_sLabel ? m_sLabel : "alloc") << "]"
		          << " allocs="  << NewCount()
		          << " bytes="   << NewBytes()
		          << " deletes=" << DeleteCount()
		          << std::endl;
	}

//------
private:
//------

	const char* m_sLabel = nullptr;
	bool        m_bReported = false;
#if DEKAF2_TRACK_ALLOCATIONS
	std::size_t m_StartNewCount    = 0;
	std::size_t m_StartNewBytes    = 0;
	std::size_t m_StartDeleteCount = 0;
#endif
};

} // namespace dekaf2bench
