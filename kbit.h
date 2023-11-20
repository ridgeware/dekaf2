/*
// DEKAF(tm): Lighter, Faster, Smarter (tm)
//
// Copyright (c) 2023, Ridgeware, Inc.
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

/*
 * The discrete implementations of the bit functions were inspired by the
 * LLVM library implementation, which itself is under the Apache License v2.0
 * with LLVM Exceptions.
 * See https://llvm.org/LICENSE.txt for license information.
 */

#pragma once

/// @file kbit.h
/// generalized implementation of the C++20/23 bit functions

#include "bits/kcppcompat.h"
#include "kcrashexit.h"
#include <type_traits>
#include <limits>
#include <cstring> // memcpy

#if DEKAF2_HAS_CPP_20
	#if DEKAF2_HAS_INCLUDE(<bit>)
		#include <bit>
		#define DEKAF2_HAS_STD_BIT 1
		#if DEKAF2_IS_MACOS
			// unfortunately current macos implementations of libc++ do not 
			// define all __cpp_lib_bitops version markers, so when on macos
			// and C++>=20 and <bit> is existing, we assume we have all
			// implementations for C++20
			#define DEKAF2_BITS_HAS_BITOPS 1
			#define DEKAF2_BITS_HAS_POW2   1
		#else
			#define DEKAF2_BITS_HAS_BITOPS (__cpp_lib_bitops   >= 201907L)
			#define DEKAF2_BITS_HAS_POW2   (__cpp_lib_int_pow2 >= 202002L)
		#endif
		// these ones are properly signalled by macos with C++23
		#define DEKAF2_BITS_HAS_BYTESWAP (__cpp_lib_byteswap >= 202110L)
		#define DEKAF2_BITS_HAS_BITCAST  (__cpp_lib_bit_cast >= 201806L)
	#endif
#endif

namespace dekaf2 {

#if defined(__BYTE_ORDER__)
	// we can use the preprocessor defines, which is constant
	#define DEKAF2_LE_BE_CONSTEXPR constexpr
#elif DEKAF2_HAS_STD_BIT
	// we can use the std::endian enum, which is constant
	#define DEKAF2_LE_BE_CONSTEXPR constexpr
#elif !DEKAF_NO_GCC && DEKAF2_GCC_VERSION >= 50000 && DEKAF2_HAS_CPP_11
	// this causes an error message in clang, gcc >= 5 takes it
	#define DEKAF2_LE_BE_CONSTEXPR constexpr
#else
	// older gcc versions and newer clang do not compile the constexpr
	#define DEKAF2_LE_BE_CONSTEXPR inline
#endif

//-----------------------------------------------------------------------------
DEKAF2_LE_BE_CONSTEXPR bool kIsBigEndian() noexcept
//-----------------------------------------------------------------------------
{
#if defined(__BYTE_ORDER__)
	return __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__;
#elif DEKAF2_HAS_STD_BIT
	return std::endian::native == std::endian::big;
#else
	union endian_t
	{
		const uint32_t i;
		const unsigned char ch[4];
	};
	const endian_t endian { 0x01020304 };

	return endian.ch[0] == 1;
#endif
}

//-----------------------------------------------------------------------------
DEKAF2_LE_BE_CONSTEXPR bool kIsLittleEndian() noexcept
//-----------------------------------------------------------------------------
{
	// this is theoretically wrong, as there may be other
	// byte orders than big or little, but we ignore that
	// until DEC resurrects.. and as dekaf2's home is at
	// the former DEC headquarter building we should notice!
	return !kIsBigEndian();
}

//-----------------------------------------------------------------------------
template<class T>
// C++23
/// swap all bytes of a given integral type
constexpr typename std::enable_if<std::is_integral<T>::value, T>::type
kByteSwap(T iIntegral) noexcept
//-----------------------------------------------------------------------------
{
#if DEKAF2_BITS_HAS_BYTESWAP
	return std::byteswap(iIntegral);
#else
	auto len = sizeof(T);

	if (DEKAF2_LIKELY(len > 1))
	{
		uint8_t* cp = (uint8_t*)&iIntegral;

		for (std::size_t i = 0, e = len-1, lc = len/2; i < lc; ++i, --e)
		{
			std::swap(cp[i], cp[e]);
		}
	}
	return iIntegral;
#endif
}

//-----------------------------------------------------------------------------
template <class T>
DEKAF2_LE_BE_CONSTEXPR
void kToBigEndian(T& value) noexcept
//-----------------------------------------------------------------------------
{
	if (!kIsBigEndian())
	{
		value = kByteSwap(value);
	}
}

//-----------------------------------------------------------------------------
template <class T>
DEKAF2_LE_BE_CONSTEXPR
void kToLittleEndian(T& value) noexcept
//-----------------------------------------------------------------------------
{
	if (kIsBigEndian())
	{
		value = kByteSwap(value);
	}
}

//-----------------------------------------------------------------------------
template <class T>
DEKAF2_LE_BE_CONSTEXPR
void kFromBigEndian(T& value) noexcept
//-----------------------------------------------------------------------------
{
	if (!kIsBigEndian())
	{
		value = kByteSwap(value);
	}
}

//-----------------------------------------------------------------------------
template <class T>
DEKAF2_LE_BE_CONSTEXPR
void kFromLittleEndian(T& value) noexcept
//-----------------------------------------------------------------------------
{
	if (kIsBigEndian())
	{
		value = kByteSwap(value);
	}
}

namespace detail {

#if !DEKAF2_BITS_HAS_BITOPS
#ifndef _MSC_VER

inline DEKAF2_PUBLIC constexpr
int ctz(unsigned iValue) noexcept
{
	return __builtin_ctz(iValue);
}

inline DEKAF2_PUBLIC constexpr
int ctz(unsigned long iValue) noexcept
{
	return __builtin_ctzl(iValue);
}

inline DEKAF2_PUBLIC constexpr
int ctz(unsigned long long iValue) noexcept
{
	return __builtin_ctzll(iValue);
}

inline DEKAF2_PUBLIC constexpr
int clz(unsigned iValue) noexcept
{
	return __builtin_clz(iValue);
}

inline DEKAF2_PUBLIC constexpr
int clz(unsigned long iValue) noexcept
{
	return __builtin_clzl(iValue);
}

inline DEKAF2_PUBLIC constexpr
int clz(unsigned long long iValue) noexcept
{
	return __builtin_clzll(iValue);
}

inline DEKAF2_PUBLIC constexpr
int popcount(unsigned iValue) noexcept
{
	return __builtin_popcount(iValue);
}

inline DEKAF2_PUBLIC constexpr
int popcount(unsigned long iValue) noexcept
{
	return __builtin_popcountl(iValue);
}

inline DEKAF2_PUBLIC constexpr
int popcount(unsigned long long iValue) noexcept
{
	return __builtin_popcountll(iValue);
}

#else  // _MSC_VER

inline DEKAF2_PUBLIC
int ctz(unsigned iValue) noexcept
{
	static_assert(sizeof(unsigned) == sizeof(unsigned long), "unsigned int has a different size than unsigned long");
	static_assert(sizeof(unsigned long) == 4, "unsigned long has a size other than 4");

	unsigned long iPos;

	if (_BitScanForward(&iPos, iValue))
	{
		return static_cast<int>(iPos);
	}

	return 32;
}

inline DEKAF2_PUBLIC
int ctz(unsigned long iValue) noexcept
{
	return ctz(static_cast<unsigned>(iValue));
}

inline DEKAF2_PUBLIC
int ctz(unsigned long long iValue) noexcept
{
	unsigned long iPos;

#if defined(DEKAF2_IS_64_BITS)
	if (_BitScanForward64(&iPos, iValue))
	{
		return static_cast<int>(iPos);
	}
#else
	if (_BitScanForward(&iPos, static_cast<unsigned long>(iValue)))
	{
		return static_cast<int>(iPos);
	}
	if (_BitScanForward(&iPos, static_cast<unsigned long>(iValue >> 32)))
	{
		return static_cast<int>(iPos + 32);
	}
#endif

	return 64;
}

inline DEKAF2_PUBLIC
int clz(unsigned iValue) noexcept
{
	static_assert(sizeof(unsigned) == sizeof(unsigned long), "unsigned int has a different size than unsigned long");
	static_assert(sizeof(unsigned long) == 4, "unsigned long has a size other than 4");

	unsigned long iPos;

	if (_BitScanReverse(&iPos, iValue))
	{
		return static_cast<int>(31 - iPos);
	}

	return 32;
}

inline DEKAF2_PUBLIC
int clz(unsigned long iValue) noexcept
{
	return clz(static_cast<unsigned>(iValue));
}

inline DEKAF2_PUBLIC
int clz(unsigned long long iValue) noexcept
{
	unsigned long iPos;

#if defined(DEKAF2_IS_64_BITS)
	if (_BitScanReverse64(&iPos, iValue))
	{
		return static_cast<int>(63 - iPos);
	}
#else
	if (_BitScanReverse(&iPos, static_cast<unsigned long>(iValue >> 32)))
	{
		return static_cast<int>(63 - (iPos + 32));
	}
	if (_BitScanReverse(&iPos, static_cast<unsigned long>(iValue)))
	{
		return static_cast<int>(63 - iPos);
	}
#endif

	return 64;
}

inline DEKAF2_PUBLIC
int popcount(unsigned iValue) noexcept
{
	static_assert(sizeof(unsigned) == 4, "");
	return __popcnt(iValue);
}

inline DEKAF2_PUBLIC
int popcount(unsigned long iValue) noexcept
{
	static_assert(sizeof(unsigned long) == 4, "");
	return __popcnt(iValue);
}

inline DEKAF2_PUBLIC
int popcount(unsigned long long iValue) noexcept
{
	static_assert(sizeof(unsigned long long) == 8, "");
#if defined(DEKAF2_IS_64_BITS)
	return __popcnt64(iValue);
#else
	return __popcount(static_cast<unsigned long>(iValue >> 32)
	     + __popcount(static_cast<unsigned long>(iValue));
#endif
}

#endif // _MSC_VER
#endif // DEKAF2_BITS_HAS_BITOPS

} // end of namespace detail

//-----------------------------------------------------------------------------
template<class T>
// C++20
/// Computes the result of bitwise left-rotating iValue by iCount positions
DEKAF2_PUBLIC constexpr typename std::enable_if<std::is_unsigned<T>::value, T>::type
kRotateLeft(T iValue, unsigned int iCount) noexcept
//-----------------------------------------------------------------------------
{
#if DEKAF2_BITS_HAS_BITOPS
	return std::rotl(iValue, iCount);
#else
	const unsigned int iDigits = std::numeric_limits<T>::digits;

	if ((iCount % iDigits) == 0)
	{
		return iValue;
	}

	return (iValue << (iCount % iDigits)) | (iValue >> (iDigits - (iCount % iDigits)));
#endif
}

//-----------------------------------------------------------------------------
template<class T>
// C++20
/// Computes the result of bitwise right-rotating iValue by iCount positions
DEKAF2_PUBLIC constexpr typename std::enable_if<std::is_unsigned<T>::value, T>::type
kRotateRight(T iValue, unsigned int iCount) noexcept
//-----------------------------------------------------------------------------
{
#if DEKAF2_BITS_HAS_BITOPS
	return std::rotr(iValue, iCount);
#else
	const unsigned int iDigits = std::numeric_limits<T>::digits;

	if ((iCount % iDigits) == 0)
	{
		return iValue;
	}

	return (iValue >> (iCount % iDigits)) | (iValue << (iDigits - (iCount % iDigits)));
#endif
}

//-----------------------------------------------------------------------------
template <class T>
// C++20
///Checks if iValue is an integral power of two
DEKAF2_PUBLIC constexpr typename std::enable_if<std::is_unsigned<T>::value, bool>::type
kHasSingleBit(T iValue) noexcept
//-----------------------------------------------------------------------------
{
#if DEKAF2_BITS_HAS_POW2
	return std::has_single_bit(iValue);
#else
	return iValue != 0 && ((iValue & (iValue - 1)) == 0);
#endif
}

//-----------------------------------------------------------------------------
template<class T>
// C++20
/// Returns the number of consecutive 0 bits in the value of x, starting from the most significant bit ("left")
DEKAF2_PUBLIC constexpr typename std::enable_if<std::is_unsigned<T>::value, int>::type
kBitCountLeftZero(T iValue) noexcept
//-----------------------------------------------------------------------------
{
#if DEKAF2_BITS_HAS_BITOPS
	return std::countl_zero(iValue);
#else
	if (!iValue)
	{
		return std::numeric_limits<T>::digits;
	}
	else if (sizeof(T) <= sizeof(unsigned int))
	{
		return detail::clz(static_cast<unsigned int>(iValue)) - (std::numeric_limits<unsigned int>::digits - std::numeric_limits<T>::digits);
	}
	else if (sizeof(T) <= sizeof(unsigned long))
	{
		return detail::clz(static_cast<unsigned long>(iValue)) - (std::numeric_limits<unsigned long>::digits - std::numeric_limits<T>::digits);
	}
	else if (sizeof(T) <= sizeof(unsigned long long))
	{
		return detail::clz(static_cast<unsigned long long>(iValue)) - (std::numeric_limits<unsigned long long>::digits - std::numeric_limits<T>::digits);
	}
	else
	{
		int iResult = 0;
		int iIter   = 0;
		const unsigned int iULLDigits = std::numeric_limits<unsigned long long>::digits;

		for(;;)
		{
			iValue = kRotateRight(iValue, iULLDigits);

			if ((iIter = kBitCountLeftZero(static_cast<unsigned long long>(iValue))) != iULLDigits)
			{
				break;
			}

			iResult += iIter;
		}

		return iResult + iIter;
	}
#endif
}

#if !DEKAF2_BITS_HAS_POW2
namespace detail {
// integral log base 2
template<class T>
DEKAF2_PUBLIC DEKAF2_CONSTEXPR_14
unsigned bit_log2(T iValue) noexcept
{
	static_assert(std::is_unsigned<T>::value, "bit_log2 requires an unsigned integer type");
	return std::numeric_limits<T>::digits - 1 - kBitCountLeftZero(iValue);
}
} // end of namespace detail
#endif

#if DEKAF2_IS_GCC && DEKAF2_GCC_VERSION_MAJOR < 7
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshift-count-overflow"
#endif
//-----------------------------------------------------------------------------
// C++20
template<class T>
// C++20
/// Returns the number of consecutive 0 bits in the value of x, starting from the least significant bit ("right")
DEKAF2_PUBLIC constexpr typename std::enable_if<std::is_unsigned<T>::value, int>::type
kBitCountRightZero(T iValue) noexcept
//-----------------------------------------------------------------------------
{
#if DEKAF2_BITS_HAS_BITOPS
	return std::countr_zero(iValue);
#else
	if (!iValue)
	{
		return std::numeric_limits<T>::digits;
	}
	else if (sizeof(T) <= sizeof(unsigned int))
	{
		return detail::ctz(static_cast<unsigned int>(iValue));
	}
	else if (sizeof(T) <= sizeof(unsigned long))
	{
		return detail::ctz(static_cast<unsigned long>(iValue));
	}
	else if (sizeof(T) <= sizeof(unsigned long long))
	{
		return detail::ctz(static_cast<unsigned long long>(iValue));
	}
	else
	{
		int iResult { 0 };
		constexpr unsigned int iULLDigits = std::numeric_limits<unsigned long long>::digits;

		while (static_cast<unsigned long long>(iValue) == 0)
		{
			iResult += iULLDigits;
			iValue >>= iULLDigits;
			iValue   = kRotateRight(iValue, iULLDigits);
		}

		return iResult + detail::ctz(static_cast<unsigned long long>(iValue));
	}
#endif
}
#if DEKAF2_IS_GCC && DEKAF2_GCC_VERSION_MAJOR < 7
#pragma GCC diagnostic pop
#endif

//-----------------------------------------------------------------------------
template<class T>
// C++20
/// Returns the number of consecutive 1 bits in the value of x, starting from the most significant bit ("left")
DEKAF2_PUBLIC constexpr typename std::enable_if<std::is_unsigned<T>::value, int>::type
kBitCountLeftOne(T iValue) noexcept
//-----------------------------------------------------------------------------
{
#if DEKAF2_BITS_HAS_BITOPS
	return std::countl_one(iValue);
#else
	return (iValue != std::numeric_limits<T>::max())
	    ? kBitCountLeftZero(static_cast<T>(~iValue))
	    : std::numeric_limits<T>::digits;
#endif
}

//-----------------------------------------------------------------------------
template<class T>
// C++20
/// Returns the number of consecutive 1 bits in the value of x, starting from the least significant bit ("right")
DEKAF2_PUBLIC constexpr typename std::enable_if<std::is_unsigned<T>::value, int>::type
kBitCountRightOne(T iValue) noexcept
//-----------------------------------------------------------------------------
{
#if DEKAF2_BITS_HAS_BITOPS
	return std::countr_one(iValue);
#else
	return (iValue != std::numeric_limits<T>::max())
	    ? kBitCountRightZero(static_cast<T>(~iValue))
	    : std::numeric_limits<T>::digits;
#endif
}

//-----------------------------------------------------------------------------
template<class T>
// C++20
/// Calculates the smallest integral power of two that is not smaller than x
DEKAF2_PUBLIC constexpr typename std::enable_if<std::is_unsigned<T>::value, T>::type
kBitCeil(T iValue) noexcept
//-----------------------------------------------------------------------------
{
#if DEKAF2_BITS_HAS_POW2
	return std::bit_ceil(iValue);
#else
	if (iValue < 2)
	{
		return 1;
	}

	const uint16_t iBinaryDigits = std::numeric_limits<T>::digits - kBitCountLeftZero(static_cast<T>(iValue - 1u));
	kAssert(iBinaryDigits != std::numeric_limits<T>::digits, "number too large for type");

	if (sizeof(T) >= sizeof(unsigned))
	{
		return (1u << iBinaryDigits);
	}
	else
	{
		const uint16_t iExcess = std::numeric_limits<unsigned>::digits - std::numeric_limits<T>::digits;
		const uint16_t iRet    = 1u << (iBinaryDigits + iExcess);
		return (iRet >> iExcess);
	}
#endif
}

//-----------------------------------------------------------------------------
template<class T>
// C++20
/// If iValue is not zero, calculates the largest integral power of two that is not greater than iValue. If iValue is zero, returns zero.
DEKAF2_PUBLIC constexpr typename std::enable_if<std::is_unsigned<T>::value, T>::type
kBitFloor(T iValue) noexcept
//-----------------------------------------------------------------------------
{
#if DEKAF2_BITS_HAS_POW2
	return std::bit_floor(iValue);
#else
	return iValue == 0 ? 0 : T{ 1 } << detail::bit_log2(iValue);
#endif
}

//-----------------------------------------------------------------------------
template<class T>
// C++20
/// If iValue is not zero, calculates the number of bits needed to store the value iValue. If iValue is zero, returns zero.
DEKAF2_PUBLIC constexpr typename std::enable_if<std::is_unsigned<T>::value, int>::type
kBitWidth(T iValue) noexcept
//-----------------------------------------------------------------------------
{
#if DEKAF2_BITS_HAS_POW2
	return std::bit_width(iValue);
#else
	return iValue == 0 ? 0 : detail::bit_log2(iValue) + 1;
#endif
}

#if DEKAF2_IS_GCC && DEKAF2_GCC_VERSION_MAJOR < 7
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshift-count-overflow"
#endif
//-----------------------------------------------------------------------------
template<class T>
DEKAF2_PUBLIC constexpr
// C++20
/// Returns the number of 1 bits in the value of iValue
typename std::enable_if<std::is_unsigned<T>::value, int>::type
kBitCountOne(T iValue) noexcept
//-----------------------------------------------------------------------------
{
#if DEKAF2_BITS_HAS_BITOPS
	return std::popcount(iValue);
#else
	if (sizeof(T) <= sizeof(unsigned int))
	{
		return detail::popcount(static_cast<unsigned int>(iValue));
	}
	else if (sizeof(T) <= sizeof(unsigned long))
	{
		return detail::popcount(static_cast<unsigned long>(iValue));
	}
	else if (sizeof(T) <= sizeof(unsigned long long))
	{
		return detail::popcount(static_cast<unsigned long long>(iValue));
	}
	else
	{
		int iResult = 0;

		while (iValue != 0)
		{
			iResult += detail::popcount(static_cast<unsigned long long>(iValue));
			iValue >>= std::numeric_limits<unsigned long long>::digits;
		}

		return iResult;
	}
#endif
}
#if DEKAF2_IS_GCC && DEKAF2_GCC_VERSION_MAJOR < 7
#pragma GCC diagnostic pop
#endif

#if DEKAF2_BITS_HAS_BITCAST
template<class To, class From>
DEKAF2_PUBLIC constexpr
// C++20
/// convert one trivially copyable object into another trivially copyable object of the same size
To kBitCast(const From& src) noexcept
//-----------------------------------------------------------------------------
{
	return std::bit_cast<To, From>(src);
}
#else
// this emulation of bit_cast is not constexpr
template<class To, class From>
DEKAF2_PUBLIC typename std::enable_if<
       sizeof(To) == sizeof(From) 
	&& std::is_trivially_copyable<From>::value 
	&& std::is_trivially_copyable<To>::value, 
	To
>::type
/// convert one trivially copyable object into another trivially copyable object of the same size
kBitCast(const From& src) noexcept
{
    static_assert(std::is_trivially_constructible<To>::value,
	              "This implementation of std::bit_cast requires the destination type to be trivially constructible");
    To dst;
    std::memcpy(&dst, &src, sizeof(To));
    return dst;
}
#endif

} // end of namespace dekaf2
