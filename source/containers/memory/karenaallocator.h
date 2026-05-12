/*
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

/// @file karenaallocator.h
/// Block-allocating arena (memory pool) for trivially-destructible objects.
/// Pattern is the same as rapidxml's memory_pool: large blocks, no per-allocation
/// free, all memory released only via clear() or destructor. Designed to back
/// data structures with thousands of small POD nodes.

#include <dekaf2/core/init/kdefinitions.h>
#include <dekaf2/core/logging/klog.h>
#include <dekaf2/core/strings/kstringview.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>
#include <type_traits>
#include <utility>

DEKAF2_NAMESPACE_BEGIN

/// @addtogroup containers_memory
/// @{

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Heap-backed bump allocator that hands out memory in large blocks. Memory is
/// only ever released en bloc via clear() or destruction. Allocate() is O(1)
/// in the common case (cursor advance), occasionally O(block-allocation) when
/// the current block is exhausted. There is no individual free.
///
/// Designed for graphs of trivially-destructible PODs that share a common
/// lifetime with the arena (typical use: DOM-like trees, scratch buffers).
/// Construct() statically asserts the trivial-destructibility requirement to
/// prevent silent destructor leaks.
///
/// Allocations larger than the block size get their own oversized block, so
/// the arena does not waste a full block when a single very large request
/// shows up.
///
/// Move-only — copying an arena is not meaningful (the contained pointers
/// would alias). For deep duplication use the higher-level data structure's
/// own clone primitive.
///
/// @see KStackArena for a stack-allocated, fixed-size alternative.
class DEKAF2_PUBLIC KArenaAllocator
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	// KArenaStringBuilder holds an "open lease" on the cursor while it is
	// growing a string. The arena rejects new Allocate() / AllocateString()
	// / Construct<T>() calls during that window (would corrupt the open
	// string) — see Allocate() for the kWarning + nullptr return.
	friend class KArenaStringBuilder;

//------
public:
//------

	/// Default block size - requested allocations larger than this size will get
	/// their own memory blocks fitting the requested size
	static constexpr std::size_t DefaultBlockSize = 8 * 1024;

	/// Default alignment used by Allocate() when the caller does not specify
	/// one. Sufficient for any pointer or integer type up to 64 bits.
	static constexpr std::size_t DefaultAlignment = sizeof(void*);

	//-----------------------------------------------------------------------------
	/// Construct an empty arena. No memory is allocated until the first call
	/// to Allocate() or Construct().
	/// @param iBlockSize payload bytes per block (excluding the small header).
	///                   Reasonable values are 4 KiB to 64 KiB.
	explicit KArenaAllocator(std::size_t iBlockSize = DefaultBlockSize) noexcept;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Construct an arena that uses a caller-owned inline buffer as its
	/// first backing region. Subsequent allocations beyond the inline
	/// capacity fall through to heap-allocated blocks of size `iBlockSize`.
	/// The inline buffer is NOT owned by the allocator — it is never
	/// freed; its lifetime must outlive the allocator.
	///
	/// Typical use: an arena-owning container embeds a fixed-size char
	/// array as a member and hands its pointer to the allocator at
	/// construction time. Documents that fit in the inline buffer pay
	/// zero heap allocations.
	/// @param iBlockSize   payload bytes per heap block (used after the
	///                     inline buffer is exhausted)
	/// @param pInlineBuf   caller-owned buffer, must be 8-byte-aligned
	/// @param iInlineCap   capacity of the inline buffer in bytes
	KArenaAllocator(std::size_t iBlockSize, void* pInlineBuf, std::size_t iInlineCap) noexcept;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	~KArenaAllocator();
	//-----------------------------------------------------------------------------

	KArenaAllocator(KArenaAllocator&& other) noexcept;
	KArenaAllocator& operator=(KArenaAllocator&& other) noexcept;

	KArenaAllocator(const KArenaAllocator&) = delete;
	KArenaAllocator& operator=(const KArenaAllocator&) = delete;

	//-----------------------------------------------------------------------------
	/// Allocate iSize bytes aligned to iAlign. The returned pointer is valid
	/// until clear() or destruction. Throws std::bad_alloc on allocation
	/// failure.
	/// @param iSize  number of bytes to allocate (may be 0; returns nullptr)
	/// @param iAlign alignment (must be a power of two, default sizeof(void*))
	void* Allocate(std::size_t iSize, std::size_t iAlign = DefaultAlignment);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Copy the bytes referenced by sSource into the arena and return a view
	/// of the copy. The returned view is valid for the lifetime of the arena
	/// or until clear(). Empty input returns an empty view without
	/// allocating. The stored bytes are NOT null-terminated.
	KStringView AllocateString(KStringView sSource);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Construct a T in arena-owned storage with forwarded ctor arguments.
	/// T must be trivially destructible — the arena will not call its
	/// destructor when freeing memory.
	template<class T, class... Args>
	T* Construct(Args&&... args)
	{
		static_assert(std::is_trivially_destructible<T>::value,
			"KArenaAllocator::Construct<T>(): T must be trivially destructible");
		void* p = Allocate(sizeof(T), alignof(T));
		// Allocate() returns nullptr if a KArenaStringBuilder holds an
		// open lease — pass that through so the caller can detect it.
		if (p == nullptr) return nullptr;
		return ::new (p) T(std::forward<Args>(args)...);
	}
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Release every block back to the system. After clear(), the arena is
	/// empty (BlockCount() == 0 and FreeBlockCount() == 0) and ready to
	/// receive new allocations.
	void clear() noexcept;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Reset the arena for reuse without releasing memory: every block
	/// currently in use is moved to an internal free list, the cursor and
	/// usage counter are reset, and BlockCount() drops to 0. Subsequent
	/// allocations recycle blocks from the free list (cheapest-fit walk in
	/// GrowBy()) before falling back to ::operator new(). This is the
	/// preferred reset for hot reparse paths (e.g. KHTML::PodResetTree())
	/// where the same arena is reused many times for similarly-sized
	/// inputs. To actually release the recycled memory call clear().
	void reset() noexcept;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// @returns the configured block size (payload bytes per regular block).
	std::size_t BlockSize() const noexcept { return m_iBlockSize; }
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// @returns the number of blocks currently held by the arena.
	std::size_t BlockCount() const noexcept;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// @returns the sum of payload bytes that have been handed out by
	///          Allocate() / AllocateString() / Construct() since
	///          construction or the last clear(). Does not count internal
	///          alignment padding.
	std::size_t UsedBytes() const noexcept { return m_iUsedBytes; }
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// @returns the total payload capacity of all blocks currently held by
	///          the arena (i.e. the sum of block sizes). The block headers
	///          themselves are not included.
	std::size_t TotalCapacity() const noexcept;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// @returns the number of blocks currently retained on the internal
	///          free list (i.e. recycled by reset() and waiting to be
	///          reused by a future Allocate() / GrowBy()).
	std::size_t FreeBlockCount() const noexcept;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Re-point the inline backing buffer. Used by the owning container's
	/// move constructor: after the base-class move has copied the
	/// allocator state, the moved-to allocator's `m_pInline` still points
	/// at the moved-from container's inline buffer (which is at a
	/// different address). Call this with the new owner's inline buffer
	/// to install it; the cursor is relocated by the same offset.
	///
	/// **Note**: this relocates the cursor only. Pointers stored in
	/// arena-allocated objects that reference bytes inside the old inline
	/// buffer are *not* rewritten. Moving a populated arena is therefore
	/// undefined behaviour (mirrors rapidxml's xml_document constraint).
	/// NRVO typically elides the move for return-by-value, which is the
	/// safe common case.
	void AdoptInlineBuffer(void* pInlineBuf, std::size_t iInlineCap) noexcept;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Semantic no-op: returns sSource unchanged, with zero overhead.
	/// Call this instead of AllocateString() when the caller already knows
	/// the bytes referenced by sSource live in a stable region for the
	/// lifetime of the arena (data segment literal, arena-owned storage,
	/// owned source buffer, etc.). Skips the data-segment / stable-region
	/// check entirely; the intent is made explicit at the call site.
	static KStringView Adopt(KStringView sSource) noexcept { return sSource; }
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Maximum number of caller-registered stable regions we track. Two
	/// is enough for the typical KHTML pattern (source buffer + inline
	/// buffer); the extra slots cover layered consumers and stay-small.
	static constexpr std::size_t kMaxStableRegions = 4;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Register a memory range as "stable for the lifetime of this arena".
	/// `AllocateString()` will then return views into this range
	/// unchanged (no copy) when the source view falls entirely within it.
	/// Used e.g. by `KHTML` to register its owned source buffer, letting
	/// parser-emitted views into it pass through `AllocateString` for
	/// free.
	///
	/// The arena does *not* own the registered memory — it just remembers
	/// the range. The caller must guarantee the range outlives the
	/// arena (or any string view derived from `AllocateString`).
	///
	/// Up to `kMaxStableRegions` regions can be registered; further calls
	/// emit a kWarning and are ignored. Zero-size or null ranges are
	/// silently ignored.
	void RegisterStableRegion(const void* pStart, std::size_t iSize) noexcept;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Forget all registered stable regions. Automatically invoked by
	/// `clear()`. Does not affect inline-buffer state.
	void ClearStableRegions() noexcept;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// @returns the number of currently-registered stable regions.
	std::size_t StableRegionCount() const noexcept { return m_iStableRegionCount; }
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	// Open-ended cursor API
	//
	// Allows a caller (typically `KArenaStringBuilder`) to extend a string
	// allocation char-by-char without knowing the final size up front.
	// Pattern:
	//   * capture `Cursor()` as the string start
	//   * write one byte (or N) at `Cursor()`, then call `AdvanceCursor(N)`
	//   * before writing, if `Cursor() == CursorEnd()`, call
	//     `RelocateOpenString(...)` to migrate the partial string to a
	//     fresh block (cursor is now at the relocated copy's end)
	//   * the final view is `KStringView(start, advanced-bytes)`
	//
	// Only one open-ended allocation may be in flight at a time per arena.
	// Mixing open-ended writes with `Allocate()` calls is allowed at
	// well-defined boundaries (commit the open-ended string first).
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// @returns the current write cursor of the active block, or nullptr
	///          if no block has been allocated yet (no inline backing,
	///          no Allocate() ever called).
	char* Cursor() const noexcept { return m_pCursor; }
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// @returns one-past-the-last writable byte of the active block, or
	///          nullptr if no block exists. Together with `Cursor()`, used
	///          to detect when an open-ended string has reached the block
	///          boundary and needs to relocate.
	char* CursorEnd() const noexcept { return m_pEnd; }
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Advance the cursor by `iBytes`. The caller must have written
	/// `iBytes` starting at `Cursor()` and must have verified that
	/// `Cursor() + iBytes <= CursorEnd()`. Updates the used-bytes counter.
	void AdvanceCursor(std::size_t iBytes) noexcept
	{
		m_pCursor    += iBytes;
		m_iUsedBytes += iBytes;
	}
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Mid-string block-boundary handler: caller has been writing chars
	/// into the active block starting at `pPartialStart`, accumulating
	/// `iPartialLen` bytes. The block is now exhausted; allocate a fresh
	/// block (large enough for the partial bytes plus `iMoreNeeded`
	/// headroom), memcpy the partial bytes to it, and return the new
	/// start pointer. After return, `Cursor()` points just past the
	/// relocated bytes; subsequent `AdvanceCursor()` calls continue
	/// normally.
	/// @param pPartialStart pointer to the partial string start (in the
	///                      previous block); may be nullptr if iPartialLen == 0
	/// @param iPartialLen   how many bytes have been written so far
	/// @param iMoreNeeded   optional hint for additional bytes to follow
	/// @returns the new start pointer of the (now relocated) partial string
	char* RelocateOpenString(const char* pPartialStart,
	                         std::size_t iPartialLen,
	                         std::size_t iMoreNeeded = 0);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// @returns true while a KArenaStringBuilder holds an open lease on
	/// the cursor. During this window, all `Allocate()` /
	/// `AllocateString()` / `Construct<T>()` calls fail with a
	/// kWarning + nullptr / empty-view return, because they would
	/// corrupt the open string by advancing the cursor underneath it.
	bool HasOpenBuilder() const noexcept { return m_bOpenBuilder; }
	//-----------------------------------------------------------------------------

//------
private:
//------

	//-----------------------------------------------------------------------------
	/// Begin / end an open builder lease. Called by KArenaStringBuilder's
	/// ctor and Finalize() (also its dtor as a safety net). While a lease
	/// is active, `Allocate()` short-circuits with a kWarning.
	void BeginOpenBuilder() noexcept { m_bOpenBuilder = true;  }
	void EndOpenBuilder()   noexcept { m_bOpenBuilder = false; }
	//-----------------------------------------------------------------------------

//------
private:
//------

	//-----------------------------------------------------------------------------
	/// Per-block header. Lives at the start of each allocated chunk; the
	/// payload follows immediately and runs up to (header + 1) + size.
	struct Block
	{
		Block*      m_pPrevious;   ///< previous (older) block, or nullptr
		std::size_t m_iSize;       ///< payload bytes in this block
	};
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Allocate a fresh block big enough to satisfy iMinPayload bytes (which
	/// already accounts for any alignment padding). Pushes the new block to
	/// the head of the chain and updates the cursor.
	void GrowBy(std::size_t iMinPayload);
	//-----------------------------------------------------------------------------

	Block*      m_pHead     { nullptr };  ///< most-recently-allocated block (active list)
	Block*      m_pFreeList { nullptr };  ///< recycled blocks (head of LIFO free list)
	char*       m_pCursor   { nullptr };  ///< next free byte in m_pHead
	char*       m_pEnd      { nullptr };  ///< one-past-end of m_pHead's payload
	std::size_t m_iBlockSize{ DefaultBlockSize };
	std::size_t m_iUsedBytes{ 0 };
	char*       m_pInline    { nullptr }; ///< caller-owned inline backing buffer (not freed)
	std::size_t m_iInlineCap { 0 };       ///< capacity of m_pInline in bytes
	bool        m_bOpenBuilder { false }; ///< set while a KArenaStringBuilder is active

	//-----------------------------------------------------------------------------
	/// Inline-tracked stable regions. Stays small (4 slots) — for the
	/// expected use case (1 source buffer + 1 inline buffer) a heap-
	/// allocated vector would be wasteful.
	struct StableRegion
	{
		const char* pStart { nullptr };
		std::size_t iSize  { 0 };
	};
	std::array<StableRegion, kMaxStableRegions> m_StableRegions {};
	std::size_t                                 m_iStableRegionCount { 0 };
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// @returns true iff sSource is entirely contained in a registered
	/// stable region. Linear scan over up to kMaxStableRegions entries.
	bool IsInStableRegion(KStringView sSource) const noexcept;
	//-----------------------------------------------------------------------------

}; // KArenaAllocator

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Helper for building a string of unknown final length inside a
/// `KArenaAllocator`. The bytes are written directly at the arena's current
/// cursor, growing without re-allocation as long as the active arena block
/// has room. When a block boundary is hit mid-string, the partial bytes
/// are migrated to a fresh block (one memcpy of the partial length); the
/// resulting view is stable for the lifetime of the arena.
///
/// Typical use (parser-style char-by-char accumulation):
///
///   KArenaStringBuilder sb(arena);
///   while (ParserHasMoreChars())
///   {
///       sb.Append(ParserNextChar());
///   }
///   KStringView sView = sb.Finalize();    // stable in arena
///
/// Only one builder per arena may be in flight at a time — they share the
/// arena's cursor as their write position. Mixing builders with direct
/// `Allocate()` calls is allowed at well-defined boundaries (finalize one
/// before starting another or before calling `Allocate`).
///
/// After `Finalize()`, the builder is "spent"; further `Append` calls are
/// undefined.
class DEKAF2_PUBLIC KArenaStringBuilder
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	//-----------------------------------------------------------------------------
	/// Default-construct in an inactive state. Use `Start(arena)` to
	/// arm. Useful when the builder is held as a value member of a
	/// long-lived container and should not take a lease until first
	/// use.
	KArenaStringBuilder() noexcept = default;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Bind to an arena. Captures the arena's current cursor as the
	/// (eventual) start of the string. If the arena has no block yet,
	/// the first `Append()` triggers block allocation. Takes out an
	/// "open builder lease" on the arena — until `Finalize()` or the
	/// destructor fires, the arena will reject regular `Allocate()` /
	/// `AllocateString()` / `Construct<T>()` calls with a kWarning.
	explicit KArenaStringBuilder(KArenaAllocator& arena) noexcept
	: m_pArena(&arena)
	, m_pStart(arena.Cursor())
	{
		m_pArena->BeginOpenBuilder();
	}
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Safety net: if a builder is destroyed without an explicit
	/// `Finalize()`, the arena lease is released (and the partial bytes
	/// stay where they are — they become harmless arena waste).
	~KArenaStringBuilder() noexcept
	{
		if (m_pArena != nullptr && !m_bFinalized)
		{
			m_pArena->EndOpenBuilder();
		}
	}
	//-----------------------------------------------------------------------------

	KArenaStringBuilder(const KArenaStringBuilder&)            = delete;
	KArenaStringBuilder& operator=(const KArenaStringBuilder&) = delete;

	//-----------------------------------------------------------------------------
	/// Move-construct. Transfers the (possibly-active) lease from
	/// `other` to this; `other` becomes inactive. The arena's
	/// open-builder flag is unchanged — there is still exactly one
	/// active builder if the source was active.
	KArenaStringBuilder(KArenaStringBuilder&& other) noexcept
	: m_pArena    (other.m_pArena)
	, m_pStart    (other.m_pStart)
	, m_iLen      (other.m_iLen)
	, m_bFinalized(other.m_bFinalized)
	{
		other.m_pArena      = nullptr;
		other.m_pStart      = nullptr;
		other.m_iLen        = 0;
		other.m_bFinalized  = false;
	}
	//-----------------------------------------------------------------------------

	KArenaStringBuilder& operator=(KArenaStringBuilder&&) = delete;

	//-----------------------------------------------------------------------------
	/// (Re-)arm an inactive builder against `arena`. Captures the arena's
	/// current cursor as the new start position; takes out an open-builder
	/// lease. Must only be called when the builder is inactive (just
	/// default-constructed or after a `Finalize()`); calling Start() on
	/// an active builder logs a kWarning and silently retains the prior
	/// state.
	void Start(KArenaAllocator& arena) noexcept
	{
		if (m_pArena != nullptr && !m_bFinalized)
		{
			kWarning("Start() called on an active KArenaStringBuilder — ignored");
			return;
		}
		m_pArena      = &arena;
		m_pStart      = arena.Cursor();
		m_iLen        = 0;
		m_bFinalized  = false;
		m_pArena->BeginOpenBuilder();
	}
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// @returns true while the builder holds an open lease on an arena
	/// (i.e. between a `Start()` / arena-ctor and the matching
	/// `Finalize()` or destructor).
	bool IsActive() const noexcept
	{
		return m_pArena != nullptr && !m_bFinalized;
	}
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Append a single byte to the open-ended string. If the active arena
	/// block is exhausted, the partial bytes are relocated to a fresh
	/// block (one memcpy). Amortised O(1) per char.
	void Append(char ch)
	{
		if (m_pArena->Cursor() == m_pArena->CursorEnd())
		{
			// block exhausted (or no block yet) — relocate the partial
			// string (if any) to a fresh block; m_pStart now points
			// into the new block.
			m_pStart = m_pArena->RelocateOpenString(m_pStart, m_iLen);
		}
		*m_pArena->Cursor() = ch;
		m_pArena->AdvanceCursor(1);
		++m_iLen;
	}
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Append `iN` bytes from `pData`. Equivalent to N calls to `Append`
	/// for the boundary-crossing logic, but faster on the common path
	/// (no per-char branch when the bytes fit in the current block).
	void Append(const char* pData, std::size_t iN)
	{
		while (iN > 0)
		{
			if (m_pArena->Cursor() == m_pArena->CursorEnd())
			{
				m_pStart = m_pArena->RelocateOpenString(m_pStart, m_iLen, iN);
			}
			const std::size_t iAvail = static_cast<std::size_t>(m_pArena->CursorEnd() - m_pArena->Cursor());
			const std::size_t iThis  = (iN <= iAvail) ? iN : iAvail;
			std::memcpy(m_pArena->Cursor(), pData, iThis);
			m_pArena->AdvanceCursor(iThis);
			m_iLen += iThis;
			pData  += iThis;
			iN     -= iThis;
		}
	}

	void Append(KStringView sData) { Append(sData.data(), sData.size()); }
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// @returns the number of bytes written so far.
	std::size_t Size()  const noexcept { return m_iLen; }
	bool        Empty() const noexcept { return m_iLen == 0; }
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Close the string. Releases the arena's open-builder lease, so the
	/// next regular `Allocate()` succeeds. After this call, the arena's
	/// cursor sits right after the string, so subsequent allocations are
	/// well-placed. The returned view is stable for the lifetime of the
	/// arena. The builder becomes inactive and may be re-armed via
	/// `Start(arena)`.
	KStringView Finalize() noexcept
	{
		if (m_pArena != nullptr && !m_bFinalized)
		{
			m_pArena->EndOpenBuilder();
			m_bFinalized = true;
		}
		return (m_iLen == 0) ? KStringView{}
		                     : KStringView{m_pStart, m_iLen};
	}
	//-----------------------------------------------------------------------------

//------
private:
//------

	KArenaAllocator* m_pArena      { nullptr };
	char*            m_pStart      { nullptr };
	std::size_t      m_iLen        { 0       };
	bool             m_bFinalized  { false   };

}; // KArenaStringBuilder

/// @}

DEKAF2_NAMESPACE_END
