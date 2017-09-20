/*
//-----------------------------------------------------------------------------//
//
// DEKAF(tm): Lighter, Faster, Smarter (tm)
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
// For documentation, try: http://www.ridgeware.com/home/dekaf/
//
*/

#pragma once

/// @file kcppcompat.h
/// compatibility layer to provide same interfaces for C++11 to 17

#include "kconfiguration.h"

#if defined __GNUC__
 #define DEKAF2_GCC_VERSION __GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__
#else
 #define DEKAF2_GCC_VERSION 0
#endif

#if defined __GNUC__
	#define DEKAF2_FUNCTION_NAME __PRETTY_FUNCTION__
#elif defined _MSC_VER
	#define DEKAF2_FUNCTION_NAME __FUNCSIG__
#else
	#define DEKAF2_FUNCTION_NAME __FUNCTION__
#endif

#if !defined(DEKAF2_IS_OSX) && defined(__APPLE__) && defined(__MACH__)
	#define DEKAF2_IS_OSX 1
#endif

#if !defined(UNIX) && (defined(unix) || defined(__unix__) || defined(DEKAF2_IS_OSX))
	#define UNIX 1
#endif

#if !defined(DEKAF2_IS_UNIX) && defined(UNIX)
	#define DEKAF2_IS_UNIX 1
#endif

#if (__cplusplus < 201103L && !DEKAF2_HAS_CPP_11)
	#error "this version of dekaf needs at least a C++11 compiler"
#endif

#ifndef DEKAF2_HAS_CPP_11
	#define DEKAF2_HAS_CPP_11 1
#endif

#if (__cplusplus >= 201402L && !defined(DEKAF2_HAS_CPP_14))
	#define DEKAF2_HAS_CPP_14 1
#endif

// this test is a bit bogus (by just testing if the cpp date
// is younger than that of C++14), but it should probably even
// be kept after C++17 defines an official date, as older
// compilers would not know it (but support C++17)
#if (__cplusplus > 201402L && !defined(DEKAF2_HAS_CPP_17))
	#define DEKAF2_HAS_CPP_17 1
#endif

// prepare for the shared_mutex enabler below - this has to go into
// the base namespace
#ifdef DEKAF2_HAS_CPP_14
 #include <shared_mutex>
 #include <mutex> // to be balanced with the C++11 case below
#else
 #include <mutex>
#endif

namespace std
{

// make sure we have a shared_mutex and a shared_lock by injecting
// matching types into the std:: namespace
#ifdef DEKAF2_HAS_CPP_14
 // C++17 has both already
 #ifndef DEKAF2_HAS_CPP_17
	// for C++14 that's easy - just alias a shared_timed_mutex - it's a superset
	using shared_mutex = shared_timed_mutex;
 #endif
#else
 #ifdef DEKAF2_HAS_CPP_11
	// for C++11 we alias a non-shared mutex - it can be costly on lock contention
	// TODO implement a platform native shared mutex..
	using shared_mutex = mutex;
	template<class T>
	using shared_lock = unique_lock<T>;
 #endif
#endif

}

// Make sure we have standard helper templates from C++14 available in namespace std::
// It does not matter if they had been declared by other code already. The compiler
// simply picks the first one that matches.
#ifndef DEKAF2_HAS_CPP_14

#include "kmake_unique.h"

namespace std
{

template<bool B, class T, class F>
using conditional_t = typename conditional<B,T,F>::type;

template <bool B, typename T = void>
using enable_if_t = typename enable_if<B,T>::type;

template< class T >
using decay_t = typename decay<T>::type;

}
#endif

// define macros to teach the compiler which branch is more likely
// to be taken - the effects are actually minimal to nonexisting,
// so do not bother for code that is not really core
#if defined(__GNUC__) && __GNUC__ >= 4
 #define DEKAF2_LIKELY(expression)   (__builtin_expect((expression), 1))
 #define DEKAF2_UNLIKELY(expression) (__builtin_expect((expression), 0))
#else
 #define DEKAF2_LIKELY(expression)   (expression)
 #define DEKAF2_UNLIKELY(expression) (expression)
#endif

// force the compiler to respect the inline - better do not use it, the
// compiler is smarter than us in almost all cases
#if defined(__GNUC__)
 #define DEKAF2_ALWAYS_INLINE inline __attribute__((__always_inline__))
#elif defined(_MSC_VER)
 #define DEKAF2_ALWAYS_INLINE __forceinline
#else
 #define DEKAF2_ALWAYS_INLINE inline
#endif

#if (!defined __GNUC__ || DEKAF2_GCC_VERSION >= 50000) && DEKAF2_HAS_CPP_11
 // this causes an error message in clang syntax analysis, but it compiles
 // in both gcc >= 5 and clang!
 #define DEKAF2_LE_BE_CONSTEXPR constexpr
#else
 // older gcc versions do not compile the constexpr
 #define DEKAF2_LE_BE_CONSTEXPR inline
#endif

namespace dekaf2
{

DEKAF2_LE_BE_CONSTEXPR bool kIsBigEndian()
{
	union endian_t
	{
		const uint32_t i;
		const unsigned char ch[4];
	};
	const endian_t endian{0x01020304};
	return endian.ch[0] == 1;
}

DEKAF2_LE_BE_CONSTEXPR bool kIsLittleEndian()
{
	return !kIsBigEndian();
}

template <class VALUE>
DEKAF2_LE_BE_CONSTEXPR void kSwapBytes(VALUE& value)
{
	static_assert(std::is_scalar<VALUE>::value, "operation only supported for scalar type");
	std::size_t len = sizeof(VALUE);
	if (DEKAF2_LIKELY(len > 1))
	{
		uint8_t* cp = (uint8_t*)&value;
		for (std::size_t i = 0, e = len-1; i < len/2; ++i, --e)
		{
			std::swap(cp[i], cp[e]);
		}
	}
}

template <class VALUE>
DEKAF2_LE_BE_CONSTEXPR void kToBigEndian(VALUE& value)
{
	static_assert(std::is_scalar<VALUE>::value, "operation only supported for scalar type");
	if (!kIsBigEndian())
	{
		swap_bytes(value);
	}
}

template <class VALUE>
DEKAF2_LE_BE_CONSTEXPR void kToLittleEndian(VALUE& value)
{
	static_assert(std::is_scalar<VALUE>::value, "operation only supported for scalar type");
	if (kIsBigEndian())
	{
		swap_bytes(value);
	}
}

template <class VALUE>
DEKAF2_LE_BE_CONSTEXPR void kFromBigEndian(VALUE& value)
{
	static_assert(std::is_scalar<VALUE>::value, "operation only supported for scalar type");
	if (!kIsBigEndian())
	{
		swap_bytes(value);
	}
}

template <class VALUE>
DEKAF2_LE_BE_CONSTEXPR void kFromLittleEndian(VALUE& value)
{
	static_assert(std::is_scalar<VALUE>::value, "operation only supported for scalar type");
	if (kIsBigEndian())
	{
		swap_bytes(value);
	}
}

} // end of namespace dekaf2

#undef DEKAF2_LE_BE_CONSTEXPR

