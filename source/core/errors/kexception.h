/*
// DEKAF(tm): Lighter, Faster, Smarter (tm)
//
// Copyright (c) 2018, Ridgeware, Inc.
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

/// @file kexception.h
/// provides basic dekaf2 exception class

#include <dekaf2/core/init/kdefinitions.h>
#include <dekaf2/core/strings/kstringview.h>
#include <exception>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <new>

DEKAF2_NAMESPACE_BEGIN

/// @addtogroup core_errors
/// @{

namespace detail {

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// An error code and an error message in the size of one pointer. Without an
/// error the pointer is null, and nothing is allocated. With an error it points
/// to a block with a reference count, the error code and the message: copies
/// share the block, and copying does not throw.
/// The lowest bit of the pointer is a flag for the owner of the object.
class DEKAF2_PUBLIC KErrorState
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	KErrorState() noexcept = default;

	/// constructs with error message and error code - an empty message with the
	/// error code 0 is no error, and allocates nothing
	KErrorState(KStringView sWhat, uint16_t iErrorCode)
	{
		if (!sWhat.empty() || iErrorCode != 0)
		{
			m_iBits = reinterpret_cast<std::uintptr_t>(Block::Create(sWhat, iErrorCode));
		}
	}

	/// copies the error and the flag
	KErrorState(const KErrorState& other) noexcept
	: m_iBits(other.m_iBits)
	{
		AddRef();
	}

	/// moves the error and copies the flag
	KErrorState(KErrorState&& other) noexcept
	: m_iBits(other.m_iBits)
	{
		other.m_iBits &= s_iFlag;
	}

	KErrorState& operator=(const KErrorState& other) noexcept
	{
		if (this != &other)
		{
			other.AddRef();
			Release();
			m_iBits = other.m_iBits;
		}

		return *this;
	}

	KErrorState& operator=(KErrorState&& other) noexcept
	{
		if (this != &other)
		{
			Release();
			m_iBits        = other.m_iBits;
			other.m_iBits &= s_iFlag;
		}

		return *this;
	}

	~KErrorState()
	{
		Release();
	}

	/// takes the error of another object, and keeps the own flag
	void SetError(const KErrorState& other) noexcept
	{
		if (this != &other)
		{
			other.AddRef();
			Release();
			m_iBits = (other.m_iBits & ~s_iFlag) | (m_iBits & s_iFlag);
		}
	}

	/// removes the error, and keeps the flag
	void clear() noexcept
	{
		Release();
		m_iBits &= s_iFlag;
	}

	/// returns error code, 0 if there is no error
	DEKAF2_NODISCARD
	uint16_t value() const noexcept
	{
		auto* pBlock = GetBlock();
		return pBlock ? pBlock->iErrorCode : 0;
	}

	/// returns error message, empty if there is none
	DEKAF2_NODISCARD
	const char* what() const noexcept
	{
		auto* pBlock = GetBlock();
		return pBlock ? pBlock->Text() : "";
	}

	/// returns error message, empty if there is none
	DEKAF2_NODISCARD
	KStringViewZ message() const noexcept
	{
		return what();
	}

	/// returns the flag of the owner
	DEKAF2_NODISCARD
	bool GetFlag() const noexcept
	{
		return (m_iBits & s_iFlag) != 0;
	}

	/// sets the flag of the owner
	void SetFlag(bool bYes) noexcept
	{
		m_iBits = (m_iBits & ~s_iFlag) | (bYes ? s_iFlag : 0);
	}

//----------
private:
//----------

	// the message follows the block in the same allocation, with a terminating zero
	struct Block
	{
		std::atomic<uint32_t> iRefs      { 1 };
		uint16_t              iErrorCode { 0 };

		const char* Text() const noexcept
		{
			return reinterpret_cast<const char*>(this) + sizeof(Block);
		}

		static Block* Create(KStringView sWhat, uint16_t iErrorCode)
		{
			auto* pBlock = new (::operator new(sizeof(Block) + sWhat.size() + 1)) Block;
			auto* sText  = reinterpret_cast<char*>(pBlock) + sizeof(Block);

			pBlock->iErrorCode = iErrorCode;

			if (!sWhat.empty())
			{
				std::memcpy(sText, sWhat.data(), sWhat.size());
			}

			sText[sWhat.size()] = 0;

			return pBlock;
		}
	};

	static constexpr std::uintptr_t s_iFlag = 1;

	Block* GetBlock() const noexcept
	{
		return reinterpret_cast<Block*>(m_iBits & ~s_iFlag);
	}

	void AddRef() const noexcept
	{
		auto* pBlock = GetBlock();

		if (pBlock)
		{
			pBlock->iRefs.fetch_add(1, std::memory_order_relaxed);
		}
	}

	void Release() noexcept
	{
		auto* pBlock = GetBlock();

		if (pBlock && pBlock->iRefs.fetch_sub(1, std::memory_order_acq_rel) == 1)
		{
			pBlock->~Block();
			::operator delete(pBlock);
		}
	}

	std::uintptr_t m_iBits { 0 };

}; // KErrorState

} // end of namespace detail

class KErrorBase;

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Basic dekaf2 exception class inheriting from std::exception, and adding a
/// uint16_t error code. Can also be used for error returns, like in an output
/// parameter that the caller may throw. A class that keeps its last error
/// derives from KErrorBase instead.
class DEKAF2_PUBLIC KException : public std::exception
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

	friend class KErrorBase;

//----------
public:
//----------

	KException() noexcept = default;

	/// constructs with error message and error code (default UINT16_MAX)
	KException(KStringViewZ sWhat, uint16_t iErrorCode = -1)
	: m_State(sWhat, iErrorCode)
	{
	}

	/// copy ctor is available, and does not throw
	KException(const KException&) noexcept = default;
	KException(KException&&) noexcept = default;
	KException& operator=(const KException&) noexcept = default;
	KException& operator=(KException&&) noexcept = default;

	/// returns error message
	const char* what() const noexcept override
	{
		return m_State.what();
	}

	/// clears state and resets error code to 0
	void clear() noexcept
	{
		m_State.clear();
	}

	/// returns error code
	uint16_t value() const noexcept
	{
		return m_State.value();
	}

	/// returns error message, alias for what() that returns a KStringViewZ and
	/// not a const char*
	KStringViewZ message() const noexcept
	{
		return m_State.message();
	}

	/// returns true when state is an error
	operator bool() const noexcept
	{
		return value() != 0;
	}

//----------
protected:
//----------

	/// constructs from the error of a KErrorBase, without its flag
	explicit KException(const detail::KErrorState& State) noexcept
	: m_State(State)
	{
		m_State.SetFlag(false);
	}

	detail::KErrorState m_State;

}; // KException

using KError = KException;


/// @}

DEKAF2_NAMESPACE_END
