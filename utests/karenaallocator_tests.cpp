#include "catch.hpp"

#include <dekaf2/containers/memory/karenaallocator.h>
#include <dekaf2/core/strings/kstringview.h>
#include <dekaf2/system/os/ksystem.h>
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

	SECTION("AllocateString from data segment")
	{
		KArenaAllocator arena;

		KStringView empty   = arena.AllocateString({});
		CHECK ( empty.empty() );
		CHECK ( arena.BlockCount() == 0 );

		// the arena allocator doesn't allocate strings coming from the data segment
		KStringView hello = arena.AllocateString("hello world");
		CHECK ( arena.BlockCount() == 0 );
		REQUIRE ( hello.size() == 11 );
		CHECK ( hello == "hello world" );
		// the view should point to the data segment
		CHECK ( kIsInsideDataSegment(hello.data()) );
		CHECK ( hello.data() == KStringView("hello world").data() );

		KStringView other = arena.AllocateString(hello);
		CHECK ( arena.BlockCount() == 0 );
		CHECK ( other == "hello world" );
		CHECK ( other.data() == hello.data() );
	}

	SECTION("AllocateString round-trip")
	{
		KArenaAllocator arena;

		KStringView empty   = arena.AllocateString({});
		CHECK ( empty.empty() );
		CHECK ( arena.BlockCount() == 0 );

		// create a dynamic string
		KString sOrigin("hello world");
		// NOW the allocation should happen inside the arena
		KStringView hello = arena.AllocateString(sOrigin);
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

	SECTION("reset retains blocks for reuse via the free list")
	{
		KArenaAllocator arena(256);

		// allocate enough to force a second block
		(void)arena.Allocate(200);
		(void)arena.Allocate(200);

		const std::size_t iActiveBefore = arena.BlockCount();
		REQUIRE ( iActiveBefore >= 2 );
		CHECK   ( arena.FreeBlockCount() == 0 );

		arena.reset();

		// after reset(): no active blocks, but every previously-active
		// block lives on the free list and is ready to be recycled
		CHECK ( arena.BlockCount()     == 0              );
		CHECK ( arena.FreeBlockCount() == iActiveBefore  );
		CHECK ( arena.UsedBytes()      == 0              );

		// next Allocate() must recycle a free-list block (no malloc)
		(void)arena.Allocate(100);
		CHECK ( arena.BlockCount()     >= 1 );
		CHECK ( arena.FreeBlockCount() == iActiveBefore - 1 );

		// clear() releases everything — both active and free list
		arena.clear();
		CHECK ( arena.BlockCount()     == 0 );
		CHECK ( arena.FreeBlockCount() == 0 );
	}

	SECTION("reset followed by oversized request still mallocs a fresh block")
	{
		KArenaAllocator arena(256);

		// allocate two regular-sized blocks (each 256 bytes)
		(void)arena.Allocate(200);
		(void)arena.Allocate(200);
		const std::size_t iActiveBefore = arena.BlockCount();
		REQUIRE ( iActiveBefore >= 2 );

		arena.reset();
		REQUIRE ( arena.FreeBlockCount() == iActiveBefore );

		// request more than any free-list block can satisfy: the
		// allocator must malloc a fresh oversize block while leaving
		// the free-list intact (no fitting block was found).
		(void)arena.Allocate(4096);

		CHECK ( arena.FreeBlockCount() == iActiveBefore );  // unchanged
		CHECK ( arena.BlockCount()     == 1 );
		CHECK ( arena.UsedBytes()      == 4096 );
	}

	SECTION("reparse loop after first round triggers no further mallocs")
	{
		KArenaAllocator arena(256);

		// warm-up round: this allocates the initial blocks
		for (int i = 0; i < 5; ++i) (void)arena.Allocate(200);
		const std::size_t iBlocksAfterWarmup = arena.BlockCount();
		REQUIRE ( iBlocksAfterWarmup >= 1 );

		arena.reset();

		// subsequent rounds must reuse the recycled blocks: BlockCount()
		// climbs up to the warm-up high-water mark but never beyond.
		for (int round = 0; round < 50; ++round)
		{
			for (int i = 0; i < 5; ++i) (void)arena.Allocate(200);
			CHECK ( arena.BlockCount() <= iBlocksAfterWarmup );
			arena.reset();
		}

		// every block we ever allocated is still resident on the free list
		CHECK ( arena.FreeBlockCount() == iBlocksAfterWarmup );
	}
}

// =============================================================================
// Adopt(view) — semantic no-op marker
// =============================================================================

TEST_CASE("KArenaAllocator::Adopt")
{
	SECTION("returns the view unchanged with the same data pointer")
	{
		constexpr KStringView sLiteral { "the quick brown fox" };

		auto adopted = KArenaAllocator::Adopt(sLiteral);

		CHECK ( adopted        == sLiteral        );
		CHECK ( adopted.data() == sLiteral.data() );
		CHECK ( adopted.size() == sLiteral.size() );
	}

	SECTION("empty view passes through")
	{
		KStringView sEmpty;

		auto adopted = KArenaAllocator::Adopt(sEmpty);

		CHECK ( adopted.empty()                   );
		CHECK ( adopted.data() == sEmpty.data()   );
	}

	SECTION("view into arena-owned bytes round-trips identically")
	{
		KArenaAllocator arena;
		auto sStored = arena.AllocateString(KStringView{ "stored in arena" });

		auto adopted = KArenaAllocator::Adopt(sStored);

		CHECK ( adopted.data() == sStored.data() );
		CHECK ( adopted.size() == sStored.size() );
	}
}

// =============================================================================
// Cursor API + KArenaStringBuilder
// =============================================================================

TEST_CASE("KArenaStringBuilder")
{
	SECTION("Cursor() on a fresh arena is nullptr")
	{
		KArenaAllocator arena;
		CHECK ( arena.Cursor()    == nullptr );
		CHECK ( arena.CursorEnd() == nullptr );
	}

	SECTION("appends produce a stable view in the arena")
	{
		KArenaAllocator arena;
		KArenaStringBuilder sb(arena);

		for (auto ch : KStringView{ "hello" })
		{
			sb.Append(ch);
		}

		auto sView = sb.Finalize();

		CHECK ( sView      == "hello" );
		CHECK ( sView.size() == 5     );
		// the view must be readable through its own data pointer
		CHECK ( KStringView(sView.data(), sView.size()) == "hello" );
	}

	SECTION("first Append on an empty arena lazily allocates the first block")
	{
		KArenaAllocator arena;

		CHECK ( arena.BlockCount() == 0 );

		KArenaStringBuilder sb(arena);
		sb.Append('x');

		CHECK ( arena.BlockCount()    >= 1 );
		CHECK ( sb.Finalize().size()  == 1 );
	}

	SECTION("appending more bytes than fit in one block triggers in-place growth")
	{
		// small block so we provoke a mid-string boundary crossing
		KArenaAllocator arena(64);

		KArenaStringBuilder sb(arena);
		// 500 bytes — needs 7-8 boundary crossings at 64 bytes/block
		KString sExpected;
		for (std::size_t ii = 0; ii < 500; ++ii)
		{
			char ch = static_cast<char>('a' + (ii % 26));
			sb.Append(ch);
			sExpected += ch;
		}

		auto sView = sb.Finalize();

		CHECK ( sView.size() == 500 );
		CHECK ( sView == sExpected.ToView() );
		// the view's data pointer must be valid memory; round-trip via
		// std::string to detect any corruption
		CHECK ( KString(sView).size() == 500 );
	}

	SECTION("bulk Append(view) takes the fast in-block path when possible")
	{
		KArenaAllocator arena(8192);
		KArenaStringBuilder sb(arena);

		sb.Append(KStringView{ "the " });
		sb.Append(KStringView{ "quick brown fox " });
		sb.Append(KStringView{ "jumps over the lazy dog" });

		auto sView = sb.Finalize();

		CHECK ( sView == "the quick brown fox jumps over the lazy dog" );
	}

	SECTION("bulk Append(view) handles bytes spanning a block boundary")
	{
		KArenaAllocator arena(16);     // tiny block
		KArenaStringBuilder sb(arena);

		sb.Append(KStringView{ "one-two-three-four-five-six-seven" });
		// 32 bytes across multiple 16-byte blocks

		auto sView = sb.Finalize();

		CHECK ( sView == "one-two-three-four-five-six-seven" );
	}

	SECTION("two builders in sequence both produce stable, distinct views")
	{
		KArenaAllocator arena;

		KStringView sFirst;
		{
			KArenaStringBuilder sb(arena);
			sb.Append(KStringView{ "first" });
			sFirst = sb.Finalize();
		}

		KStringView sSecond;
		{
			KArenaStringBuilder sb(arena);
			sb.Append(KStringView{ "second" });
			sSecond = sb.Finalize();
		}

		CHECK ( sFirst  == "first"  );
		CHECK ( sSecond == "second" );
		// both views must be valid simultaneously
		CHECK ( sFirst.data() != sSecond.data() );
	}

	SECTION("Finalize then Allocate places the next allocation after the string")
	{
		KArenaAllocator arena(8192);

		KArenaStringBuilder sb(arena);
		sb.Append(KStringView{ "header" });
		auto sView = sb.Finalize();

		void* p = arena.Allocate(8);

		// p must lie at or after sView's end (no overlap)
		const auto* pStringEnd = sView.data() + sView.size();
		CHECK ( static_cast<const char*>(p) >= pStringEnd );
		// and the view itself remains intact
		CHECK ( sView == "header" );
	}

	SECTION("empty Finalize returns an empty view")
	{
		KArenaAllocator arena;
		KArenaStringBuilder sb(arena);

		auto sView = sb.Finalize();

		CHECK ( sView.empty() );
		CHECK ( sb.Size() == 0 );
	}

	SECTION("interleaving builders with regular Allocate is supported at boundaries")
	{
		KArenaAllocator arena(8192);

		void* pHead = arena.Allocate(16);
		CHECK ( pHead != nullptr );

		KArenaStringBuilder sb(arena);
		sb.Append(KStringView{ "between-allocs" });
		auto sMiddle = sb.Finalize();

		void* pTail = arena.Allocate(16);

		// all three regions must be non-overlapping
		const auto* pHeadEnd = static_cast<const char*>(pHead) + 16;
		const auto* pMidEnd  = sMiddle.data() + sMiddle.size();
		CHECK ( sMiddle.data()                >= pHeadEnd );
		CHECK ( static_cast<const char*>(pTail) >= pMidEnd );
		CHECK ( sMiddle == "between-allocs" );
	}

	SECTION("relocation across a block boundary keeps the view contents intact")
	{
		// Force a boundary crossing by sizing the block so that the
		// partial string fills it exactly, then add one more byte.
		KArenaAllocator arena(8);

		KArenaStringBuilder sb(arena);
		sb.Append(KStringView{ "ABCDEFGH" });   // exactly 8 bytes — fills the block
		// At this point the cursor is at CursorEnd(); the next append
		// must trigger RelocateOpenString.
		sb.Append('!');

		auto sView = sb.Finalize();

		CHECK ( sView.size() == 9 );
		CHECK ( sView == "ABCDEFGH!" );
	}

	SECTION("open builder rejects concurrent Allocate() with kWarning + nullptr")
	{
		KArenaAllocator arena;

		KArenaStringBuilder sb(arena);
		sb.Append('a');

		CHECK ( arena.HasOpenBuilder() );

		// Allocate while builder is open: must refuse to avoid corrupting
		// the open string. Returns nullptr, emits a kWarning (suppressed
		// from test output by the kDebug muting).
		void* p = arena.Allocate(16);
		CHECK ( p == nullptr );

		// Same for AllocateString — passes through Allocate, returns empty
		KString sTempForcePushToHeap(64, 'x');           // non-data-segment
		auto sCopy = arena.AllocateString(sTempForcePushToHeap.ToView());
		CHECK ( sCopy.empty() );

		// Same for Construct<T>
		struct POD { int i; int j; };
		POD* pPod = arena.Construct<POD>();
		CHECK ( pPod == nullptr );

		// After Finalize the lease is released and Allocate succeeds again.
		auto sView = sb.Finalize();
		CHECK ( !arena.HasOpenBuilder() );
		CHECK ( sView == "a" );

		void* p2 = arena.Allocate(16);
		CHECK ( p2 != nullptr );
	}

	SECTION("builder destructor releases the lease even without Finalize")
	{
		KArenaAllocator arena;
		{
			KArenaStringBuilder sb(arena);
			sb.Append('z');
			CHECK ( arena.HasOpenBuilder() );
			// sb goes out of scope without Finalize — dtor must clear the lease
		}
		CHECK ( !arena.HasOpenBuilder() );

		// Allocate succeeds after the builder is destroyed
		void* p = arena.Allocate(8);
		CHECK ( p != nullptr );
	}
}

// =============================================================================
// RegisterStableRegion — AllocateString skips copying for in-range views
// =============================================================================

TEST_CASE("KArenaAllocator stable regions")
{
	SECTION("empty arena starts with zero regions")
	{
		KArenaAllocator arena;
		CHECK ( arena.StableRegionCount() == 0 );
	}

	SECTION("AllocateString of a view inside a registered region is a no-op")
	{
		// caller-owned buffer outside the arena
		KString sCallerBuf(256, '\0');
		for (std::size_t i = 0; i < sCallerBuf.size(); ++i)
		{
			sCallerBuf[i] = static_cast<char>('a' + (i % 26));
		}

		KArenaAllocator arena;
		arena.RegisterStableRegion(sCallerBuf.data(), sCallerBuf.size());
		CHECK ( arena.StableRegionCount() == 1 );

		// take a slice of the caller's buffer — view points into it
		KStringView sSliceInBuf(sCallerBuf.data() + 10, 16);

		auto sAfter = arena.AllocateString(sSliceInBuf);

		// AllocateString must return the original pointer — no copy
		CHECK ( sAfter.data() == sSliceInBuf.data() );
		CHECK ( sAfter.size() == sSliceInBuf.size() );
		CHECK ( sAfter        == sSliceInBuf        );
		// arena should still have no allocations
		CHECK ( arena.BlockCount() == 0 );
		CHECK ( arena.UsedBytes()  == 0 );
	}

	SECTION("AllocateString of an out-of-region view falls back to copy")
	{
		KString sRegisteredBuf(64, 'x');
		KString sUnrelatedBuf(64, 'y');

		KArenaAllocator arena;
		arena.RegisterStableRegion(sRegisteredBuf.data(), sRegisteredBuf.size());

		KStringView sOutsideView(sUnrelatedBuf.data(), 16);

		auto sAfter = arena.AllocateString(sOutsideView);

		// must have been copied
		CHECK ( sAfter.data() != sOutsideView.data() );
		CHECK ( sAfter        == sOutsideView        );
		// at least one allocation happened in the arena
		CHECK ( arena.UsedBytes() >= 16 );
	}

	SECTION("clear() forgets registered regions")
	{
		KString sBuf(128, 'z');

		KArenaAllocator arena;
		arena.RegisterStableRegion(sBuf.data(), sBuf.size());
		CHECK ( arena.StableRegionCount() == 1 );

		arena.clear();
		CHECK ( arena.StableRegionCount() == 0 );

		// after clear, the same view is now out-of-region — gets copied
		KStringView sSlice(sBuf.data() + 4, 8);
		auto sAfter = arena.AllocateString(sSlice);
		CHECK ( sAfter.data() != sSlice.data() );
		CHECK ( sAfter        == sSlice        );
	}

	SECTION("multiple regions are all checked")
	{
		KString sBufA(64, 'A');
		KString sBufB(64, 'B');
		KString sBufC(64, 'C');

		KArenaAllocator arena;
		arena.RegisterStableRegion(sBufA.data(), sBufA.size());
		arena.RegisterStableRegion(sBufB.data(), sBufB.size());
		arena.RegisterStableRegion(sBufC.data(), sBufC.size());
		CHECK ( arena.StableRegionCount() == 3 );

		// views into each registered buffer should pass through unchanged
		for (const auto& sBuf : { std::ref(sBufA), std::ref(sBufB), std::ref(sBufC) })
		{
			KStringView sSlice(sBuf.get().data() + 8, 16);
			auto sAfter = arena.AllocateString(sSlice);
			CHECK ( sAfter.data() == sSlice.data() );
		}
		CHECK ( arena.BlockCount() == 0 );
	}

	SECTION("exceeding the slot capacity warns and silently ignores")
	{
		KString sBuf(64, 'x');

		KArenaAllocator arena;
		for (std::size_t i = 0; i < KArenaAllocator::kMaxStableRegions; ++i)
		{
			arena.RegisterStableRegion(sBuf.data(), 8);
		}
		CHECK ( arena.StableRegionCount() == KArenaAllocator::kMaxStableRegions );

		// one too many — emits kWarning, count unchanged
		arena.RegisterStableRegion(sBuf.data(), 8);
		CHECK ( arena.StableRegionCount() == KArenaAllocator::kMaxStableRegions );
	}

	SECTION("nullptr and zero-size registration are silently ignored")
	{
		KArenaAllocator arena;
		arena.RegisterStableRegion(nullptr,  64);
		arena.RegisterStableRegion("",        0);
		CHECK ( arena.StableRegionCount() == 0 );
	}
}
