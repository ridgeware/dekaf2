/*
// DEKAF(tm): Lighter, Faster, Smarter (tm)
//
// Copyright (c) 2026, Ridgeware, Inc.
//
// SPDX-License-Identifier: MIT
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

#include "karenaallocator.h"
#include <dekaf2/system/os/ksystem.h>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <new>

DEKAF2_NAMESPACE_BEGIN

namespace {

//-----------------------------------------------------------------------------
/// Round a size up to the next multiple of the given power-of-two alignment.
constexpr std::size_t align_up(std::size_t iSize, std::size_t iAlign) noexcept
//-----------------------------------------------------------------------------
{
	return (iSize + (iAlign - 1)) & ~(iAlign - 1);
}

} // anonymous namespace

//-----------------------------------------------------------------------------
KArenaAllocator::KArenaAllocator(std::size_t iBlockSize) noexcept
//-----------------------------------------------------------------------------
: m_iBlockSize(iBlockSize > 0 ? iBlockSize : DefaultBlockSize)
{
}

//-----------------------------------------------------------------------------
KArenaAllocator::KArenaAllocator(std::size_t iBlockSize, void* pInlineBuf, std::size_t iInlineCap) noexcept
//-----------------------------------------------------------------------------
: m_iBlockSize(iBlockSize > 0 ? iBlockSize : DefaultBlockSize)
, m_pInline(static_cast<char*>(pInlineBuf))
, m_iInlineCap(iInlineCap)
{
	if (m_pInline != nullptr && m_iInlineCap > 0)
	{
		// arm the inline buffer as the active "block" — cursor / end now
		// point into it; subsequent Allocate() calls bump the cursor here
		// until exhausted, then fall through to GrowBy() (heap blocks).
		m_pCursor = m_pInline;
		m_pEnd    = m_pInline + m_iInlineCap;
	}
}

//-----------------------------------------------------------------------------
KArenaAllocator::~KArenaAllocator()
//-----------------------------------------------------------------------------
{
	clear();
}

//-----------------------------------------------------------------------------
KArenaAllocator::KArenaAllocator(KArenaAllocator&& other) noexcept
//-----------------------------------------------------------------------------
: m_pHead     (other.m_pHead)
, m_pFreeList (other.m_pFreeList)
, m_pCursor   (other.m_pCursor)
, m_pEnd      (other.m_pEnd)
, m_iBlockSize(other.m_iBlockSize)
, m_iUsedBytes(other.m_iUsedBytes)
, m_pInline   (other.m_pInline)
, m_iInlineCap(other.m_iInlineCap)
{
	other.m_pHead      = nullptr;
	other.m_pFreeList  = nullptr;
	other.m_pCursor    = nullptr;
	other.m_pEnd       = nullptr;
	other.m_iUsedBytes = 0;
	other.m_pInline    = nullptr;
	other.m_iInlineCap = 0;
	// keep other.m_iBlockSize so the moved-from instance is still usable.
	//
	// IMPORTANT: m_pInline now points at the moved-from object's inline
	// buffer (which is at a different address than ours). The owning
	// container's move ctor must call AdoptInlineBuffer() to re-point us
	// at its own buffer. See khtml::Document::Document(Document&&).
}

//-----------------------------------------------------------------------------
KArenaAllocator& KArenaAllocator::operator=(KArenaAllocator&& other) noexcept
//-----------------------------------------------------------------------------
{
	if (this != &other)
	{
		clear();

		m_pHead      = other.m_pHead;
		m_pFreeList  = other.m_pFreeList;
		m_pCursor    = other.m_pCursor;
		m_pEnd       = other.m_pEnd;
		m_iBlockSize = other.m_iBlockSize;
		m_iUsedBytes = other.m_iUsedBytes;
		m_pInline    = other.m_pInline;
		m_iInlineCap = other.m_iInlineCap;

		other.m_pHead      = nullptr;
		other.m_pFreeList  = nullptr;
		other.m_pCursor    = nullptr;
		other.m_pEnd       = nullptr;
		other.m_iUsedBytes = 0;
		other.m_pInline    = nullptr;
		other.m_iInlineCap = 0;
	}
	return *this;
}

//-----------------------------------------------------------------------------
void KArenaAllocator::AdoptInlineBuffer(void* pInlineBuf, std::size_t iInlineCap) noexcept
//-----------------------------------------------------------------------------
{
	char* pNewInline = static_cast<char*>(pInlineBuf);

	// If the active cursor was inside the OLD inline buffer (which now
	// belongs to the moved-from container), relocate the cursor to the
	// equivalent offset inside the new buffer. The bytes in the new
	// buffer are expected to have been memcpy'd from the old buffer by
	// the owning container's move ctor — pointers stored in arena
	// objects that reference the OLD address are NOT rewritten (UB).
	if (m_pInline != nullptr
		&& m_pCursor >= m_pInline
		&& m_pCursor <= m_pInline + m_iInlineCap)
	{
		std::ptrdiff_t iOffset = m_pCursor - m_pInline;
		m_pCursor = pNewInline + iOffset;
		m_pEnd    = pNewInline + iInlineCap;
	}

	m_pInline    = pNewInline;
	m_iInlineCap = iInlineCap;
}

//-----------------------------------------------------------------------------
void* KArenaAllocator::Allocate(std::size_t iSize, std::size_t iAlign)
//-----------------------------------------------------------------------------
{
	if (iSize == 0)
	{
		return nullptr;
	}

	// caller is responsible for passing a power-of-two alignment;
	// fall back to the default if zero or odd is given
	if (iAlign == 0 || (iAlign & (iAlign - 1)) != 0)
	{
		iAlign = DefaultAlignment;
	}

	// align the cursor inside the current block
	std::uintptr_t iCur     = reinterpret_cast<std::uintptr_t>(m_pCursor);
	std::uintptr_t iAligned = (iCur + (iAlign - 1)) & ~(static_cast<std::uintptr_t>(iAlign) - 1);
	std::size_t    iPadding = static_cast<std::size_t>(iAligned - iCur);

	if (m_pCursor == nullptr || m_pCursor + iPadding + iSize > m_pEnd)
	{
		// not enough room in current block (or no block yet); grow
		GrowBy(iSize + iAlign);

		iCur     = reinterpret_cast<std::uintptr_t>(m_pCursor);
		iAligned = (iCur + (iAlign - 1)) & ~(static_cast<std::uintptr_t>(iAlign) - 1);
	}

	void* pResult = reinterpret_cast<void*>(iAligned);
	m_pCursor    = reinterpret_cast<char*>(iAligned) + iSize;
	m_iUsedBytes += iSize;
	return pResult;
}

//-----------------------------------------------------------------------------
KStringView KArenaAllocator::AllocateString(KStringView sSource)
//-----------------------------------------------------------------------------
{
	// The test for kIsInsideDataSegment() is very cheap on Linux (just two
	// address compares). It is relatively complex though on MacOS, where a
	// number of address ranges need to be checked. If in doubt if this
	// destroys the performance gain of the arena allocator do your own
	// benchmark.
	if (sSource.empty() || kIsInsideDataSegment(sSource.data()))
	{
		// no allocation needed - this string is empty or a literal in the data segment
		return sSource;
	}

	// strings have alignment 1 — no padding needed
	void* pDest = Allocate(sSource.size(), 1);
	std::memcpy(pDest, sSource.data(), sSource.size());
	return KStringView(static_cast<const char*>(pDest), sSource.size());
}

//-----------------------------------------------------------------------------
void KArenaAllocator::clear() noexcept
//-----------------------------------------------------------------------------
{
	// release the active list. ::operator delete() (rather than std::free)
	// so the bench-time global new/delete tracker accounts for arena memory
	// too — see benchmarks/allocation_counter.cpp.
	Block* pBlock = m_pHead;
	while (pBlock != nullptr)
	{
		Block* pPrev = pBlock->m_pPrevious;
		::operator delete(pBlock);
		pBlock = pPrev;
	}

	// release the free list (recycled blocks from prior reset() calls)
	pBlock = m_pFreeList;
	while (pBlock != nullptr)
	{
		Block* pPrev = pBlock->m_pPrevious;
		::operator delete(pBlock);
		pBlock = pPrev;
	}

	m_pHead      = nullptr;
	m_pFreeList  = nullptr;
	m_iUsedBytes = 0;

	// Re-arm the inline buffer (if any) so subsequent allocations land
	// there before going to heap blocks again. The inline buffer is
	// caller-owned and is NOT freed by us.
	if (m_pInline != nullptr && m_iInlineCap > 0)
	{
		m_pCursor = m_pInline;
		m_pEnd    = m_pInline + m_iInlineCap;
	}
	else
	{
		m_pCursor = nullptr;
		m_pEnd    = nullptr;
	}
}

//-----------------------------------------------------------------------------
void KArenaAllocator::reset() noexcept
//-----------------------------------------------------------------------------
{
	// move every active block to the head of the free list (LIFO). The
	// blocks stay allocated and a future Allocate() / GrowBy() will
	// recycle them before going to ::operator new().
	Block* pBlock = m_pHead;
	while (pBlock != nullptr)
	{
		Block* pPrev = pBlock->m_pPrevious;
		pBlock->m_pPrevious = m_pFreeList;
		m_pFreeList         = pBlock;
		pBlock              = pPrev;
	}

	m_pHead      = nullptr;
	m_iUsedBytes = 0;

	// Re-arm the inline buffer (if any) — same behaviour as clear()
	// but the heap blocks stay around on the free list for reuse.
	if (m_pInline != nullptr && m_iInlineCap > 0)
	{
		m_pCursor = m_pInline;
		m_pEnd    = m_pInline + m_iInlineCap;
	}
	else
	{
		m_pCursor = nullptr;
		m_pEnd    = nullptr;
	}
}

//-----------------------------------------------------------------------------
std::size_t KArenaAllocator::BlockCount() const noexcept
//-----------------------------------------------------------------------------
{
	std::size_t iCount = 0;

	for (const Block* p = m_pHead; p != nullptr; p = p->m_pPrevious)
	{
		++iCount;
	}

	return iCount;
}

//-----------------------------------------------------------------------------
std::size_t KArenaAllocator::TotalCapacity() const noexcept
//-----------------------------------------------------------------------------
{
	std::size_t iTotal = 0;

	for (const Block* p = m_pHead; p != nullptr; p = p->m_pPrevious)
	{
		iTotal += p->m_iSize;
	}

	return iTotal;
}

//-----------------------------------------------------------------------------
std::size_t KArenaAllocator::FreeBlockCount() const noexcept
//-----------------------------------------------------------------------------
{
	std::size_t iCount = 0;

	for (const Block* p = m_pFreeList; p != nullptr; p = p->m_pPrevious)
	{
		++iCount;
	}

	return iCount;
}

//-----------------------------------------------------------------------------
void KArenaAllocator::GrowBy(std::size_t iMinPayload)
//-----------------------------------------------------------------------------
{
	// pick a payload size: at least the requested bytes (with some slack to
	// make alignment padding fit in any case), but never below the configured
	// block size
	std::size_t iPayload = std::max(m_iBlockSize, iMinPayload);

	// we want every block to start at an alignment of alignof(std::max_align_t)
	// for the payload. The Block header is followed by payload, and ::operator new()
	// returns max_align_t-aligned memory, so as long as sizeof(Block) is
	// itself max_align_t-aligned, the payload start is aligned too. Round up
	// the header size to be safe.
	constexpr std::size_t kHeaderAlign = alignof(std::max_align_t);
	constexpr std::size_t kHeaderSize  = align_up(sizeof(Block), kHeaderAlign);

	// before mallocing, try to recycle a block from the free list.
	// First-fit walk: pick the first block that is large enough. Blocks
	// in the free list keep their original m_iSize, so the search is
	// O(N) in the free-list length (typically 1-2 entries in practice).
	{
		Block** ppPrev = &m_pFreeList;
		for (Block* pCur = m_pFreeList; pCur != nullptr; pCur = pCur->m_pPrevious)
		{
			if (pCur->m_iSize >= iPayload)
			{
				// unlink from the free list
				*ppPrev = pCur->m_pPrevious;

				// link to the head of the active list
				pCur->m_pPrevious = m_pHead;
				m_pHead           = pCur;

				m_pCursor = reinterpret_cast<char*>(pCur) + kHeaderSize;
				m_pEnd    = m_pCursor + pCur->m_iSize;
				return;
			}
			ppPrev = &pCur->m_pPrevious;
		}
	}

	// ::operator new() so the bench-time global new/delete tracker can
	// account for arena block allocations (see comments in clear()).
	void* pRaw = ::operator new(kHeaderSize + iPayload);
	// ::operator new() either returns a valid pointer or throws.

	auto* pBlock = static_cast<Block*>(pRaw);
	pBlock->m_pPrevious = m_pHead;
	pBlock->m_iSize     = iPayload;

	m_pHead   = pBlock;
	m_pCursor = static_cast<char*>(pRaw) + kHeaderSize;
	m_pEnd    = m_pCursor + iPayload;
}

DEKAF2_NAMESPACE_END
