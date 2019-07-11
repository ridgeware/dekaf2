/*
//
// DEKAF(tm): Lighter, Faster, Smarter(tm)
//
// Copyright (c) 2017-2019, Ridgeware, Inc.
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

namespace dekaf2 {

namespace hash {
namespace fnv1a {

#ifdef DEKAF2_IS_64_BITS
// FNV-1a constants for 64 bit size
static constexpr size_t basis = UINT64_C(14695981039346656037);
static constexpr size_t prime = UINT64_C(1099511628211);
#else
// FNV-1a constants for 32 bit size
static constexpr size_t basis = UINT32_C(2166136261);
static constexpr size_t prime = UINT32_C(16777619);
#endif

constexpr
inline
std::size_t hash(const char data, std::size_t hash) noexcept
{
	return (hash ^ static_cast<unsigned char>(data)) * prime;
}

#ifdef DEKAF2_HAS_CPP_14
constexpr
#endif
inline
std::size_t hash(const char* data, std::size_t size, std::size_t hash) noexcept
{
	while (size-- > 0)
	{
		// we previously implemented the FNV hash with unsigned bytes,
		// and because we want to keep data compatibility we continue
		// to do so
		hash ^= static_cast<unsigned char>(*data++);
		hash *= prime;
	}
	return hash;
}

// zero terminated strings
#ifdef DEKAF2_HAS_CPP_14
constexpr
#endif
inline
std::size_t hash(const char* data, std::size_t hash) noexcept
{
	while (auto ch = *data++)
	{
		// we previously implemented the FNV hash with unsigned bytes,
		// and because we want to keep data compatibility we continue
		// to do so
		hash ^= static_cast<unsigned char>(ch);
		hash *= prime;
	}
	return hash;
}

#ifndef DEKAF2_HAS_CPP_14
// C++11 version of constexpr fnv computation - not well suited for runtime computation
// because of the recursive approach
constexpr
std::size_t hash_constexpr(const char* data, std::size_t size, std::size_t hash) noexcept
{
	return size == 0 ? hash : hash_constexpr(data + 1, size - 1, (hash ^ static_cast<unsigned char>(*data)) * prime);
}

constexpr
std::size_t hash_constexpr(char c, const char* data, std::size_t hash) noexcept
{
	return c == 0 ? hash : hash_constexpr(data[0], data + 1, (hash ^ static_cast<unsigned char>(c)) * prime);
}

#endif

} // end of namespace fnv1a
} // end of namespace hash

constexpr std::size_t kHashBasis = hash::fnv1a::basis;

//---------------------------------------------------------------------------
/// literal type for constexpr hash computations, e.g. for switch statements
constexpr
inline
std::size_t operator"" _hash(const char* data, std::size_t size) noexcept
//---------------------------------------------------------------------------
{
#ifdef DEKAF2_HAS_CPP_14
	return size != 0 ? hash::fnv1a::hash(data, size, kHashBasis) : 0;
#else
	return size != 0 ? hash::fnv1a::hash_constexpr(data, size, kHashBasis) : 0;
#endif
}

//---------------------------------------------------------------------------
/// hash function for arbitrary data, feed back hash value for consecutive calls
template<typename T>
inline
std::size_t kHash(const T* data, std::size_t size, std::size_t hash = kHashBasis) noexcept
//---------------------------------------------------------------------------
{
	return size != 0 ? hash::fnv1a::hash(reinterpret_cast<const char*>(data), size, hash) : 0;
}

//---------------------------------------------------------------------------
// constexpr specialisation
template<>
#ifdef DEKAF2_HAS_CPP_14
constexpr
#endif
inline
std::size_t kHash(const char* data, std::size_t size, std::size_t hash) noexcept
//---------------------------------------------------------------------------
{
	return size != 0 ? hash::fnv1a::hash(data, size, hash) : 0;
}

//---------------------------------------------------------------------------
/// hash function for zero terminated strings
constexpr
inline
std::size_t kHash(const char* data) noexcept
//---------------------------------------------------------------------------
{
#ifdef DEKAF2_HAS_CPP_14
	return *data != 0 ? hash::fnv1a::hash(data, kHashBasis) : 0;
#else
	return *data != 0 ? hash::fnv1a::hash_constexpr(data[0], data + 1, kHashBasis) : 0;
#endif
}

//---------------------------------------------------------------------------
/// hash function for one char, feed back hash value for consecutive calls
constexpr
inline
std::size_t kHash(char data, std::size_t hash = kHashBasis) noexcept
//---------------------------------------------------------------------------
{
	return hash::fnv1a::hash(data, hash);
}

namespace kfrozen {

//---------------------------------------------------------------------------
/// compile time evaluation - for C++11 and later. Do not use it for runtime
/// computations with C++11.
constexpr
inline
std::size_t kHash(const char* data, std::size_t size, std::size_t hash = kHashBasis) noexcept
//---------------------------------------------------------------------------
{
#ifdef DEKAF2_HAS_CPP_14
	return size != 0 ? hash::fnv1a::hash(data, size, hash) : 0;
#else
	return size != 0 ? hash::fnv1a::hash_constexpr(data, size, hash) : 0;
#endif
}

} // end of namespace frozen

} // end of namespace dekaf2

