/*
//
// DEKAF(tm): Lighter, Faster, Smarter(tm)
//
// Copyright (c) 2017, Ridgeware, Inc.
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
//
*/

#pragma once

/// @file khash.h
/// provides a Fowler-Noll-Vo hash

#include <cinttypes>
#include <climits>
#include "bits/kcppcompat.h"

namespace dekaf2
{

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// a Fowler-Noll-Vo hash function (FNV)
// from: https://stackoverflow.com/questions/34597260/stdhash-value-on-char-value-and-not-on-memory-address
template <typename ResultT, ResultT OffsetBasis, ResultT Prime>
class basic_fnv1a final
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

	static_assert(std::is_unsigned<ResultT>::value, "need unsigned integer");

//----------
public:
//----------
	using result_type = ResultT;

//----------
private:
//----------
	result_type m_State {};

//----------
public:
//----------

	//---------------------------------------------------------------------------
#ifdef DEKAF2_HAS_CPP_14
	constexpr
#endif
	basic_fnv1a() noexcept
	//---------------------------------------------------------------------------
	    : m_State {OffsetBasis}
	{
	}

	//---------------------------------------------------------------------------
#ifdef DEKAF2_HAS_CPP_14
	constexpr
#endif
	void update(const void* const data, const std::size_t size) noexcept
	//---------------------------------------------------------------------------
	{
		const auto cdata = static_cast<const unsigned char*>(data);
		auto acc = this->m_State;
		for (auto i = std::size_t {}; i < size; ++i)
		{
			const auto next = std::size_t {cdata[i]};
			acc = (acc ^ next) * Prime;
		}
		this->m_State = acc;
	}

	//---------------------------------------------------------------------------
#ifdef DEKAF2_HAS_CPP_14
	constexpr
#endif
	result_type digest() const noexcept
	//---------------------------------------------------------------------------
	{
		return this->m_State;
	}

};

/// the 32 bit instance
using fnv1a_32 = basic_fnv1a<std::uint32_t,
                             UINT32_C(2166136261),
                             UINT32_C(16777619)>;

/// the 64 bit instance
using fnv1a_64 = basic_fnv1a<std::uint64_t,
                             UINT64_C(14695981039346656037),
                             UINT64_C(1099511628211)>;

/// generic FNV hash template
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
template <std::size_t Bits>
struct fnv1a;
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

/// 32 bit FNV hash template specialization
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
template <>
struct fnv1a<32>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	using type = fnv1a_32;
};

/// 64 bit FNV hash template specialization
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
template <>
struct fnv1a<64>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	using type = fnv1a_64;
};

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
template <std::size_t Bits>
using fnv1a_t = typename fnv1a<Bits>::type;
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

//---------------------------------------------------------------------------
/// Fowler-Noll-Vo hash function for arbitrary data
#ifdef DEKAF2_HAS_CPP_14
constexpr
std::size_t hash_bytes_FNV(const void* const data, const std::size_t size) noexcept
//---------------------------------------------------------------------------
{
	auto hashfn = fnv1a_t<CHAR_BIT * sizeof(std::size_t)> {};
	hashfn.update(data, size);
	return hashfn.digest();
}
#else
std::size_t hash_bytes_FNV(const void* const data, const std::size_t size) noexcept;
#endif
} // end of namespace dekaf2

