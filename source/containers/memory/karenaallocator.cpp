/*
// DEKAF(tm): Lighter, Faster, Smarter (tm)
//
// Copyright (c) 2026, Ridgeware, Inc.
//
// SPDX-License-Identifier: MIT
*/

#include "karenaallocator.h"
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
KArenaAllocator::~KArenaAllocator()
//-----------------------------------------------------------------------------
{
	clear();
}

//-----------------------------------------------------------------------------
KArenaAllocator::KArenaAllocator(KArenaAllocator&& other) noexcept
//-----------------------------------------------------------------------------
: m_pHead     (other.m_pHead)
, m_pCursor   (other.m_pCursor)
, m_pEnd      (other.m_pEnd)
, m_iBlockSize(other.m_iBlockSize)
, m_iUsedBytes(other.m_iUsedBytes)
{
	other.m_pHead      = nullptr;
	other.m_pCursor    = nullptr;
	other.m_pEnd       = nullptr;
	other.m_iUsedBytes = 0;
	// keep other.m_iBlockSize so the moved-from instance is still usable
}

//-----------------------------------------------------------------------------
KArenaAllocator& KArenaAllocator::operator=(KArenaAllocator&& other) noexcept
//-----------------------------------------------------------------------------
{
	if (this != &other)
	{
		clear();

		m_pHead      = other.m_pHead;
		m_pCursor    = other.m_pCursor;
		m_pEnd       = other.m_pEnd;
		m_iBlockSize = other.m_iBlockSize;
		m_iUsedBytes = other.m_iUsedBytes;

		other.m_pHead      = nullptr;
		other.m_pCursor    = nullptr;
		other.m_pEnd       = nullptr;
		other.m_iUsedBytes = 0;
	}
	return *this;
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
	if (sSource.empty())
	{
		return KStringView{};
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
	Block* pBlock = m_pHead;

	while (pBlock != nullptr)
	{
		Block* pPrev = pBlock->m_pPrevious;
		std::free(pBlock);
		pBlock = pPrev;
	}

	m_pHead      = nullptr;
	m_pCursor    = nullptr;
	m_pEnd       = nullptr;
	m_iUsedBytes = 0;
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
void KArenaAllocator::GrowBy(std::size_t iMinPayload)
//-----------------------------------------------------------------------------
{
	// pick a payload size: at least the requested bytes (with some slack to
	// make alignment padding fit in any case), but never below the configured
	// block size
	std::size_t iPayload = std::max(m_iBlockSize, iMinPayload);

	// we want every block to start at an alignment of alignof(std::max_align_t)
	// for the payload. The Block header is followed by payload, and malloc()
	// returns max_align_t-aligned memory, so as long as sizeof(Block) is
	// itself max_align_t-aligned, the payload start is aligned too. Round up
	// the header size to be safe.
	constexpr std::size_t kHeaderAlign = alignof(std::max_align_t);
	constexpr std::size_t kHeaderSize  = align_up(sizeof(Block), kHeaderAlign);

	void* pRaw = std::malloc(kHeaderSize + iPayload);
	if (pRaw == nullptr)
	{
		throw std::bad_alloc();
	}

	auto* pBlock = static_cast<Block*>(pRaw);
	pBlock->m_pPrevious = m_pHead;
	pBlock->m_iSize     = iPayload;

	m_pHead   = pBlock;
	m_pCursor = static_cast<char*>(pRaw) + kHeaderSize;
	m_pEnd    = m_pCursor + iPayload;
}

DEKAF2_NAMESPACE_END
