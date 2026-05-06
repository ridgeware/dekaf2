/// @file allocation_counter.cpp
/// Defines the global counters and the operator new/delete overrides that
/// drive the AllocationSnapshot helper. Only takes effect when the build
/// system defines DEKAF2_TRACK_ALLOCATIONS=1. Otherwise this TU is empty.

#include "allocation_counter.h"

#if DEKAF2_TRACK_ALLOCATIONS

#include <cstdlib>
#include <new>

namespace dekaf2bench {

std::atomic<std::size_t> g_NewCount   {0};
std::atomic<std::size_t> g_NewBytes   {0};
std::atomic<std::size_t> g_DeleteCount{0};
std::atomic<std::size_t> g_PeakBytes  {0};

} // namespace dekaf2bench

// the global new/delete overrides apply to every allocation in the bench
// binary -- including CATCH framework code and KLog. Use AllocationSnapshot
// for delta-window measurements over a defined region.

void* operator new(std::size_t n)
{
	dekaf2bench::g_NewCount.fetch_add(1, std::memory_order_relaxed);
	dekaf2bench::g_NewBytes.fetch_add(n, std::memory_order_relaxed);
	void* p = std::malloc(n);
	if (!p) throw std::bad_alloc();
	return p;
}

void* operator new[](std::size_t n)
{
	dekaf2bench::g_NewCount.fetch_add(1, std::memory_order_relaxed);
	dekaf2bench::g_NewBytes.fetch_add(n, std::memory_order_relaxed);
	void* p = std::malloc(n);
	if (!p) throw std::bad_alloc();
	return p;
}

void operator delete(void* p) noexcept
{
	if (p)
	{
		dekaf2bench::g_DeleteCount.fetch_add(1, std::memory_order_relaxed);
		std::free(p);
	}
}

void operator delete(void* p, std::size_t) noexcept
{
	if (p)
	{
		dekaf2bench::g_DeleteCount.fetch_add(1, std::memory_order_relaxed);
		std::free(p);
	}
}

void operator delete[](void* p) noexcept
{
	if (p)
	{
		dekaf2bench::g_DeleteCount.fetch_add(1, std::memory_order_relaxed);
		std::free(p);
	}
}

void operator delete[](void* p, std::size_t) noexcept
{
	if (p)
	{
		dekaf2bench::g_DeleteCount.fetch_add(1, std::memory_order_relaxed);
		std::free(p);
	}
}

#endif // DEKAF2_TRACK_ALLOCATIONS
