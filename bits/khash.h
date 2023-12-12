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

#include "../kdefinitions.h"
#include "../kctype.h" // for ASCII lowercase conversion
#include <cinttypes>
#include <climits>

DEKAF2_NAMESPACE_BEGIN

namespace hash {

static constexpr int size = kIs64Bits() ? 64 : 32;

template<int tSize>
struct fnv_traits
{
	static_assert(tSize == 32 || tSize == 64, "This FNV hash implementation is only valid for 32 and 64 bit hash sizes");
};

template<>
struct fnv_traits<64>
{
	using hash_t = uint64_t;
	static constexpr hash_t basis    = UINT64_C(14695981039346656037);
	static constexpr hash_t prime    = UINT64_C(1099511628211);
};

template<>
struct fnv_traits<32>
{
	using hash_t = uint32_t;
	static constexpr hash_t basis    = UINT32_C(2166136261);
	static constexpr hash_t prime    = UINT32_C(16777619);
};

template<int iSize>
struct fnv1a
{
	using Hash   = typename fnv_traits<iSize>::hash_t;

	static constexpr Hash Basis  = fnv_traits<iSize>::basis;
	static constexpr Hash Prime  = fnv_traits<iSize>::prime;

static constexpr
Hash hash(const char data, Hash hash) noexcept
{
	return (hash ^ static_cast<unsigned char>(data)) * Prime;
}

static constexpr
Hash casehash(const char data, Hash hash) noexcept
{
	return (hash ^ static_cast<unsigned char>(KASCII::kToLower(data))) * Prime;
}

static DEKAF2_CONSTEXPR_14
Hash hash(const char* data, std::size_t size, Hash hash) noexcept
{
	while (size-- > 0)
	{
		// we previously implemented the FNV hash with unsigned bytes,
		// and because we want to keep data compatibility we continue
		// to do so
		hash ^= static_cast<unsigned char>(*data++);
		hash *= Prime;
	}
	return hash;
}

static DEKAF2_CONSTEXPR_14
Hash casehash(const char* data, std::size_t size, Hash hash) noexcept
{
	while (size-- > 0)
	{
		// we previously implemented the FNV hash with unsigned bytes,
		// and because we want to keep data compatibility we continue
		// to do so
		hash ^= static_cast<unsigned char>(KASCII::kToLower(*data++));
		hash *= Prime;
	}
	return hash;
}

// zero terminated strings
static DEKAF2_CONSTEXPR_14
Hash hash(const char* data, Hash hash) noexcept
{
	while (auto ch = *data++)
	{
		// we previously implemented the FNV hash with unsigned bytes,
		// and because we want to keep data compatibility we continue
		// to do so
		hash ^= static_cast<unsigned char>(ch);
		hash *= Prime;
	}
	return hash;
}

static DEKAF2_CONSTEXPR_14
Hash casehash(const char* data, Hash hash) noexcept
{
	while (auto ch = *data++)
	{
		// we previously implemented the FNV hash with unsigned bytes,
		// and because we want to keep data compatibility we continue
		// to do so
		hash ^= static_cast<unsigned char>(KASCII::kToLower(ch));
		hash *= Prime;
	}
	return hash;
}

#ifndef DEKAF2_HAS_CPP_14
// C++11 version of constexpr fnv computation - not well suited for runtime computation
// because of the recursive approach
static constexpr
Hash hash_constexpr(const char* data, std::size_t size, Hash hash) noexcept
{
	return size == 0 ? hash : hash_constexpr(data + 1, size - 1, (hash ^ static_cast<unsigned char>(*data)) * Prime);
}

static constexpr
Hash hash_constexpr(char c, const char* data, Hash hash) noexcept
{
	return c == 0 ? hash : hash_constexpr(data[0], data + 1, (hash ^ static_cast<unsigned char>(c)) * Prime);
}

// C++11 version of lowercase constexpr fnv computation - not well suited for runtime computation
// because of the recursive approach
static constexpr
Hash casehash_constexpr(const char* data, std::size_t size, Hash hash) noexcept
{
	return size == 0 ? hash : casehash_constexpr(data + 1, size - 1, (hash ^ static_cast<unsigned char>(KASCII::kToLower(*data))) * Prime);
}

static constexpr
Hash casehash_constexpr(char c, const char* data, Hash hash) noexcept
{
	return c == 0 ? hash : hash_constexpr(data[0], data + 1, (hash ^ static_cast<unsigned char>(KASCII::kToLower(c))) * Prime);
}
#endif

}; // fnv1a

template<int iSize = hash::size>
using Hash = typename hash::fnv1a<iSize>::Hash;

template<int iSize = hash::size>
constexpr Hash<iSize> kHashBasis = hash::fnv1a<iSize>::Basis;

} // end of namespace hash

inline namespace literals {

//---------------------------------------------------------------------------
/// literal type for constexpr hash computations, e.g. for switch statements
constexpr
DEKAF2_PREFIX hash::fnv1a<DEKAF2_PREFIX hash::size>::Hash operator"" _hash(const char* data, std::size_t size) noexcept
//---------------------------------------------------------------------------
{
#ifdef DEKAF2_HAS_CPP_14
	return size != 0 ? hash::fnv1a<DEKAF2_PREFIX hash::size>::hash(data, size, DEKAF2_PREFIX hash::fnv1a<DEKAF2_PREFIX hash::size>::Basis) : 0;
#else
	return size != 0 ? hash::fnv1a<DEKAF2_PREFIX hash::size>::hash_constexpr(data, size, DEKAF2_PREFIX hash::fnv1a<DEKAF2_PREFIX hash::size>::Basis) : 0;
#endif
}

//---------------------------------------------------------------------------
/// literal type for lowercase constexpr hash computations, e.g. for switch statements
constexpr
DEKAF2_PREFIX hash::fnv1a<DEKAF2_PREFIX hash::size>::Hash operator"" _casehash(const char* data, std::size_t size) noexcept
//---------------------------------------------------------------------------
{
#ifdef DEKAF2_HAS_CPP_14
	return size != 0 ? hash::fnv1a<DEKAF2_PREFIX hash::size>::casehash(data, size, DEKAF2_PREFIX hash::fnv1a<DEKAF2_PREFIX hash::size>::Basis) : 0;
#else
	return size != 0 ? hash::fnv1a<DEKAF2_PREFIX hash::size>::casehash_constexpr(data, size, DEKAF2_PREFIX hash::fnv1a<DEKAF2_PREFIX hash::size>::Basis) : 0;
#endif
}

} // end of namespace literals

//---------------------------------------------------------------------------
/// hash function for arbitrary data, feed back hash value for consecutive calls
template<typename T, int iSize = hash::size>
typename hash::fnv1a<iSize>::Hash kHash(const T* data, std::size_t size,
										typename hash::fnv1a<iSize>::Hash hash = hash::fnv1a<iSize>::Basis) noexcept
//---------------------------------------------------------------------------
{
	return size != 0 ? hash::fnv1a<iSize>::hash(reinterpret_cast<const char*>(data), size, hash) : 0;
}

//---------------------------------------------------------------------------
// constexpr specialisation
template<int iSize = hash::size>
DEKAF2_CONSTEXPR_14
typename hash::fnv1a<iSize>::Hash kHash(const char* data, std::size_t size,
										typename hash::fnv1a<iSize>::Hash hash = hash::fnv1a<iSize>::Basis) noexcept
//---------------------------------------------------------------------------
{
	return size != 0 ? hash::fnv1a<iSize>::hash(data, size, hash) : 0;
}

//---------------------------------------------------------------------------
/// hash function for zero terminated strings
template<int iSize = hash::size>
constexpr
typename hash::fnv1a<iSize>::Hash kHash(const char* data) noexcept
//---------------------------------------------------------------------------
{
#ifdef DEKAF2_HAS_CPP_14
	return *data != 0 ? hash::fnv1a<iSize>::hash(data, hash::fnv1a<iSize>::Basis) : 0;
#else
	return *data != 0 ? hash::fnv1a<iSize>::hash_constexpr(data[0], data + 1, hash::fnv1a<iSize>::Basis) : 0;
#endif
}

//---------------------------------------------------------------------------
/// hash function for one char, feed back hash value for consecutive calls
template<int iSize = hash::size>
constexpr
typename hash::fnv1a<iSize>::Hash kHash(char data, std::size_t hash = hash::fnv1a<iSize>::Basis) noexcept
//---------------------------------------------------------------------------
{
	return hash::fnv1a<iSize>::hash(data, hash);
}

// and now the same again for ASCII lowercase hashes

//---------------------------------------------------------------------------
// hash function for const char*, converted to ASCII lowercase
template<int iSize = hash::size>
DEKAF2_CONSTEXPR_14
typename hash::fnv1a<iSize>::Hash kCaseHash(const char* data, std::size_t size, std::size_t hash = hash::fnv1a<iSize>::Basis) noexcept
//---------------------------------------------------------------------------
{
	return size != 0 ? hash::fnv1a<iSize>::casehash(data, size, hash) : 0;
}

//---------------------------------------------------------------------------
/// hash function for zero terminated strings, converted to ASCII lowercase
template<int iSize = hash::size>
constexpr
typename hash::fnv1a<iSize>::Hash kCaseHash(const char* data) noexcept
//---------------------------------------------------------------------------
{
#ifdef DEKAF2_HAS_CPP_14
	return *data != 0 ? hash::fnv1a<iSize>::casehash(data, hash::fnv1a<iSize>::Basis) : 0;
#else
	return *data != 0 ? hash::fnv1a<iSize>::casehash_constexpr(data[0], data + 1, hash::fnv1a<iSize>::Basis) : 0;
#endif
}

//---------------------------------------------------------------------------
/// hash function for one char, converted to ASCII lowercase, feed back hash value for consecutive calls
template<int iSize = hash::size>
constexpr
typename hash::fnv1a<iSize>::Hash kCaseHash(char data, std::size_t hash = hash::fnv1a<iSize>::Basis) noexcept
//---------------------------------------------------------------------------
{
	return hash::fnv1a<iSize>::casehash(data, hash);
}

namespace kfrozen {

//---------------------------------------------------------------------------
/// compile time evaluation - for C++11 and later. Do not use it for runtime
/// computations with C++11.
template<int iSize = hash::size>
constexpr
typename hash::fnv1a<iSize>::Hash kHash(const char* data, std::size_t size, std::size_t hash = hash::fnv1a<iSize>::Basis) noexcept
//---------------------------------------------------------------------------
{
#ifdef DEKAF2_HAS_CPP_14
	return size != 0 ? hash::fnv1a<iSize>::hash(data, size, hash) : 0;
#else
	return size != 0 ? hash::fnv1a<iSize>::hash_constexpr(data, size, hash) : 0;
#endif
}

} // end of namespace frozen

DEKAF2_NAMESPACE_END
