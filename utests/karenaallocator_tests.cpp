#include "catch.hpp"

#include <dekaf2/containers/memory/karenaallocator.h>
#include <dekaf2/core/strings/kstringview.h>
#include <cstdint>
#include <cstring>

using namespace dekaf2;

namespace {

// payload size of one default block, mirrored here so the tests can compute
// expected block-counts without exposing the constant publicly
constexpr std::size_t DefaultBlockSize = KArenaAllocator::DefaultBlockSize;

struct alignas(8) PodNode
{
	std::uint64_t a;
	std::uint32_t b;
	std::uint16_t c;
};

static_assert(std::is_trivially_destructible<PodNode>::value, "PodNode must be trivial");

} // anonymous namespace

TEST_CASE("KArenaAllocator")
{
	SECTION("empty arena")
	{
		KArenaAllocator arena;

		CHECK ( arena.BlockCount()    == 0 );
		CHECK ( arena.UsedBytes()     == 0 );
		CHECK ( arena.TotalCapacity() == 0 );
		CHECK ( arena.BlockSize()     == DefaultBlockSize );

		// zero-sized allocation returns nullptr without growing
		CHECK ( arena.Allocate(0) == nullptr );
		CHECK ( arena.BlockCount() == 0 );
	}

	SECTION("single small allocation")
	{
		KArenaAllocator arena;

		void* p = arena.Allocate(64);
		CHECK ( p != nullptr );
		CHECK ( arena.BlockCount()    == 1 );
		CHECK ( arena.UsedBytes()     == 64 );
		CHECK ( arena.TotalCapacity() == DefaultBlockSize );
	}

	SECTION("multiple allocations stay in one block")
	{
		KArenaAllocator arena;

		void* a = arena.Allocate(100);
		void* b = arena.Allocate(100);
		void* c = arena.Allocate(100);

		CHECK ( a != nullptr );
		CHECK ( b != nullptr );
		CHECK ( c != nullptr );
		CHECK ( arena.BlockCount() == 1 );
		// payload bytes (allocation size) should add up exactly; padding is
		// not counted into UsedBytes
		CHECK ( arena.UsedBytes() == 300 );
		// allocations should be in increasing address order
		CHECK ( a < b );
		CHECK ( b < c );
	}

	SECTION("alignment")
	{
		KArenaAllocator arena;

		// allocate a single byte to skew the cursor
		(void)arena.Allocate(1, 1);

		void* aligned8  = arena.Allocate(8,  8);
		void* aligned16 = arena.Allocate(8,  16);
		void* aligned64 = arena.Allocate(8,  64);

		CHECK ( reinterpret_cast<std::uintptr_t>(aligned8)  % 8  == 0 );
		CHECK ( reinterpret_cast<std::uintptr_t>(aligned16) % 16 == 0 );
		CHECK ( reinterpret_cast<std::uintptr_t>(aligned64) % 64 == 0 );
	}

	SECTION("block growth on overflow")
	{
		// small block size to force growth quickly
		KArenaAllocator arena(256);

		// fill the first block
		(void)arena.Allocate(200);
		CHECK ( arena.BlockCount() == 1 );

		// 100 bytes won't fit in the remaining 56 (alignment will eat some);
		// expect a second block
		(void)arena.Allocate(100);
		CHECK ( arena.BlockCount() == 2 );

		(void)arena.Allocate(200);
		CHECK ( arena.BlockCount() >= 2 );
	}

	SECTION("oversized allocation gets its own block")
	{
		KArenaAllocator arena(256);

		// 4 KiB > 256 — must succeed and live in a dedicated oversize block
		void* big = arena.Allocate(4096);
		CHECK ( big != nullptr );
		CHECK ( arena.BlockCount()    == 1 );
		CHECK ( arena.TotalCapacity() >= 4096 );
		CHECK ( arena.UsedBytes()     == 4096 );

		// further small allocations fit in a new normal block
		(void)arena.Allocate(50);
		CHECK ( arena.BlockCount() == 2 );
	}

	SECTION("AllocateString round-trip")
	{
		KArenaAllocator arena;

		KStringView empty   = arena.AllocateString({});
		CHECK ( empty.empty() );
		CHECK ( arena.BlockCount() == 0 );

		KStringView hello = arena.AllocateString("hello world");
		REQUIRE ( hello.size() == 11 );
		CHECK ( hello == "hello world" );
		// the view must point to arena-owned bytes, not the literal
		CHECK ( hello.data() != KStringView("hello world").data() );

		KStringView other = arena.AllocateString(hello);
		CHECK ( other == "hello world" );
		CHECK ( other.data() != hello.data() );
	}

	SECTION("Construct<T> places PODs in arena")
	{
		KArenaAllocator arena;

		PodNode* a = arena.Construct<PodNode>(PodNode{ 1, 2, 3 });
		PodNode* b = arena.Construct<PodNode>();
		b->a = 100;
		b->b = 200;
		b->c = 300;

		REQUIRE ( a != nullptr );
		REQUIRE ( b != nullptr );
		CHECK ( a->a == 1 );
		CHECK ( a->b == 2 );
		CHECK ( a->c == 3 );
		CHECK ( b->a == 100 );
		CHECK ( b->b == 200 );
		CHECK ( b->c == 300 );
		CHECK ( reinterpret_cast<std::uintptr_t>(a) % alignof(PodNode) == 0 );
		CHECK ( reinterpret_cast<std::uintptr_t>(b) % alignof(PodNode) == 0 );
	}

	SECTION("clear releases all blocks and resets state")
	{
		KArenaAllocator arena(256);

		(void)arena.Allocate(200);
		(void)arena.Allocate(200);
		(void)arena.Allocate(200);

		CHECK ( arena.BlockCount() >= 2 );
		CHECK ( arena.UsedBytes()  == 600 );

		arena.clear();

		CHECK ( arena.BlockCount()    == 0 );
		CHECK ( arena.UsedBytes()     == 0 );
		CHECK ( arena.TotalCapacity() == 0 );

		// the arena is reusable after clear()
		void* p = arena.Allocate(50);
		CHECK ( p != nullptr );
		CHECK ( arena.BlockCount() == 1 );
		CHECK ( arena.UsedBytes()  == 50 );
	}

	SECTION("move construction transfers ownership")
	{
		KArenaAllocator src(256);

		PodNode* p1 = src.Construct<PodNode>(PodNode{ 7, 8, 9 });
		PodNode* p2 = src.Construct<PodNode>(PodNode{ 4, 5, 6 });
		(void)p2;

		const std::size_t srcBlocks = src.BlockCount();
		const std::size_t srcBytes  = src.UsedBytes();

		KArenaAllocator dst(std::move(src));

		// dst owns everything now
		CHECK ( dst.BlockCount() == srcBlocks );
		CHECK ( dst.UsedBytes()  == srcBytes  );
		// the previously-allocated nodes must still be readable through dst's arena
		CHECK ( p1->a == 7 );
		CHECK ( p1->b == 8 );
		CHECK ( p1->c == 9 );

		// src is empty (and stays valid for further use)
		CHECK ( src.BlockCount() == 0 );
		CHECK ( src.UsedBytes()  == 0 );
		void* p = src.Allocate(16);
		CHECK ( p != nullptr );
	}

	SECTION("move assignment releases old contents")
	{
		KArenaAllocator a;
		KArenaAllocator b;

		(void)a.Allocate(1024);
		(void)b.Allocate(2048);

		const std::size_t aBytes = a.UsedBytes();
		REQUIRE ( aBytes  == 1024 );
		REQUIRE ( b.UsedBytes() == 2048 );

		a = std::move(b);

		CHECK ( a.UsedBytes() == 2048 );
		CHECK ( b.BlockCount() == 0 );
	}

	SECTION("repeated clear-and-reuse does not leak blocks")
	{
		KArenaAllocator arena(256);

		for (int round = 0; round < 100; ++round)
		{
			(void)arena.Allocate(200);
			(void)arena.Allocate(200);
			arena.clear();
		}

		CHECK ( arena.BlockCount() == 0 );
		CHECK ( arena.UsedBytes()  == 0 );
	}

	SECTION("string views remain valid for arena lifetime")
	{
		KArenaAllocator arena(256);

		const char* literal = "the quick brown fox jumps over the lazy dog";
		KStringView v = arena.AllocateString(literal);

		// force the cursor past the view's storage by allocating more
		for (int i = 0; i < 100; ++i)
		{
			(void)arena.AllocateString("padding");
		}

		// content must still match — the original bytes have not been
		// overwritten or relocated
		CHECK ( v == literal );
	}
}
