#include "catch.hpp"

#include <dekaf2/kallocator.h>
#include <vector>
#include <list>
#include <unordered_set>
#include <new>

// see https://howardhinnant.github.io/stack_alloc.html

using namespace dekaf2;

// Replace new and delete just for the purpose of demonstrating that
// they are not called.

std::size_t memory = 0;
std::size_t allocations = 0;

#ifdef DEKAF2_NO_GCC

void* operator new(std::size_t s) noexcept(false)
{
	memory += s;
	++allocations;
	return malloc(s);
}

void operator delete(void* p) noexcept
{
	free(p);
}

void operator delete(void* p, std::size_t s) noexcept
{
	free(p);
}

#endif

// the CATCH framework extensively calls operator new and delete itself,
// therefore we need to reset, and make sure we have no CATCH macros in
// the probes. For a strange reason this means we cannot step through the
// code in the debugger either.
void reset_memuse()
{
	memory = 0;
	allocations = 0;
}

std::size_t memuse()
{
	return allocations;
}

// Create a vector<T> template with a small buffer of 200 bytes.
// Note for vector it is possible to reduce the alignment requirements
// down to alignof(T) because vector doesn't allocate anything but T's.
// And if we're wrong about that guess, it is a compile-time error, not
// a run time error.
template <class T, std::size_t BufSize = 200>
using SmallVector = std::vector<T, KStackAlloc<T, BufSize, alignof(T)>>;

template <class T, std::size_t BufSize = 200>
using SmallList = std::list<T, KStackAlloc<T, BufSize, alignof(T) < 8 ? 8 : alignof(T)>>;

template <class T, std::size_t BufSize = 200>
using SmallSet = std::unordered_set<T, std::hash<T>, std::equal_to<T>,
KStackAlloc<T, BufSize, alignof(T) < 8 ? 8 : alignof(T)>>;

TEST_CASE("KStackAlloc")
{
	SECTION("stack allocated vector")
	{
		reset_memuse();
		// Create the stack-based arena from which to allocate
		SmallVector<int>::allocator_type::arena_type a;
		// Create the vector which uses that arena.
		SmallVector<int> v{a};
		// Exercise the vector and note that new/delete are not getting called.
		v.push_back(1);
		int allocations = 0;
		allocations += memuse();
		v.push_back(2);
		allocations += memuse();
		v.push_back(3);
		allocations += memuse();
		v.push_back(4);
		allocations += memuse();
		CHECK ( allocations == 0 );
	}

	SECTION("stack allocated list")
	{
		reset_memuse();
		SmallList<int>::allocator_type::arena_type a;
		SmallList<int> v{a};
		v.push_back(1);
		int allocations = 0;
		allocations += memuse();
		v.push_back(2);
		allocations += memuse();
		v.push_back(3);
		allocations += memuse();
		v.push_back(4);
		allocations += memuse();
		CHECK ( allocations == 0 );
	}

	SECTION("stack allocated unordered_set")
	{
		reset_memuse();
		SmallSet<int>::allocator_type::arena_type a;
		SmallSet<int> v{a};
		v.insert(1);
		int allocations = 0;
		allocations += memuse();
		v.insert(2);
		allocations += memuse();
		v.insert(3);
		allocations += memuse();
		v.insert(4);
		allocations += memuse();
		CHECK ( allocations == 0 );
	}

}
