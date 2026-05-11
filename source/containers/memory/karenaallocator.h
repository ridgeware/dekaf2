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
#include <dekaf2/core/strings/kstringview.h>
#include <cstddef>
#include <cstdint>
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

}; // KArenaAllocator

/// @}

DEKAF2_NAMESPACE_END
