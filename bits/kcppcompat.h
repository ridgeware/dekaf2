/*
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
#include <climits>
#include <cinttypes>
#include <type_traits>

#define DEKAF2_xstringify(x) #x
#define DEKAF2_stringify(x) DEKAF2_xstringify(x)

#if defined __clang__
	#define DEKAF2_CLANG_VERSION __clang_major__ * 10000 + __clang_minor__ * 100 + __clang_patchlevel__
	#define DEKAF2_IS_CLANG 1
	#define DEKAF2_NO_GCC 1
#else
	#define DEKAF2_CLANG_VERSION 0
#endif

#if defined __GNUC__ && !defined DEKAF2_IS_CLANG
	#define DEKAF2_GCC_VERSION_MAJOR __GNUC__
	#define DEKAF2_GCC_VERSION __GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__
	#define DEKAF2_IS_GCC 1
#else
	#define DEKAF2_GCC_VERSION_MAJOR 0
	#define DEKAF2_GCC_VERSION 0
	#ifndef DEKAF2_NO_GCC
		#define DEKAF2_NO_GCC 1
	#endif
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

#if !defined(DEKAF2_IS_UNIX) && (defined(unix) || defined(__unix__) || defined(DEKAF2_IS_OSX))
	#define DEKAF2_IS_UNIX 1
#endif

#if !defined(DEKAF2_IS_UNIX) && (defined(_MSC_VER))
	#define DEKAF2_IS_WINDOWS 1
 	// works on < VS 2013
	#define _CRT_SECURE_NO_WARNINGS
	// works on >= VS 2013
	#pragma warning(disable:4996)
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

// this test is a bit bogus (by just testing if the cpp date
// is younger than that of C++17), but it should probably even
// be kept after C++20 defines an official date, as older
// compilers would not know it (but support C++20)
#if (__cplusplus > 201703L && !defined(DEKAF2_HAS_CPP_20))
	#define DEKAF2_HAS_CPP_20 1
#endif

#ifdef DEKAF2_HAS_CPP_14
	#define DEKAF2_CONSTEXPR_14 constexpr
#else
	#define DEKAF2_CONSTEXPR_14 inline
#endif

#ifdef DEKAF2_HAS_CPP_17
	#define DEKAF2_CONSTEXPR_17 constexpr
#else
	#define DEKAF2_CONSTEXPR_17 inline
#endif

#ifdef DEKAF2_HAS_CPP_20_NOT_YET
	// need to make sure first that string classes are constexpr on all platforms,
	// and all internal representations
	#define DEKAF2_CONSTEXPR_20 constexpr
#else
	#define DEKAF2_CONSTEXPR_20 inline
#endif

// unfortunately GCC < 7 require the repetition of a constexpr variable
// in the .cpp even if in c++17 mode
#if !defined(DEKAF2_HAS_CPP_17) || (defined(DEKAF2_IS_GCC) && DEKAF2_GCC_VERSION < 70000)
	#define DEKAF2_REPEAT_CONSTEXPR_VARIABLE 1
#endif

#ifndef __has_include
	#define DEKAF2_HAS_INCLUDE(x) 0
#else
	#define DEKAF2_HAS_INCLUDE(x) __has_include(x)
#endif

#ifndef __has_attribute
	#define DEKAF2_HAS_ATTRIBUTE(x) 0
#else
	#define DEKAF2_HAS_ATTRIBUTE(x) __has_attribute(x)
#endif

#ifndef __has_cpp_attribute
	#define DEKAF2_HAS_CPP_ATTRIBUTE(x) 0
#else
	#define DEKAF2_HAS_CPP_ATTRIBUTE(x) __has_cpp_attribute(x)
#endif

#ifndef __has_extension
	#define DEKAF2_HAS_EXTENSION(x) 0
#else
	#define DEKAF2_HAS_EXTENSION(x) __has_extension(x)
#endif

#if (__cpp_if_constexpr)
	#define DEKAF2_CONSTEXPR_IF constexpr
	#define DEKAF2_HAS_CONSTEXPR_IF 1
#else
	#define DEKAF2_CONSTEXPR_IF
#endif

#if (__cpp_lib_chrono >= 201510L || (DEKAF2_IS_OSX && DEKAF2_HAS_CPP_17))
	#define DEKAF2_HAS_CHRONO_ROUND 1
#endif

#if DEKAF2_HAS_CPP_ATTRIBUTE(fallthrough)
	#define DEKAF2_FALLTHROUGH [[fallthrough]]
#elif DEKAF2_HAS_CPP_ATTRIBUTE(clang::fallthrough)
	#define DEKAF2_FALLTHROUGH [[clang::fallthrough]]
#elif DEKAF2_HAS_CPP_ATTRIBUTE(gnu::fallthrough)
	#define DEKAF2_FALLTHROUGH [[gnu::fallthrough]]
#elif DEKAF2_HAS_ATTRIBUTE(fallthrough)
	#define DEKAF2_FALLTHROUGH __attribute__ ((fallthrough))
#else
	#define DEKAF2_FALLTHROUGH do {} while (0)
#endif

#if defined(__clang__) || defined(__GNUC__)
	#define DEKAF2_DEPRECATED(msg) __attribute__((__deprecated__(msg)))
#elif defined(_MSC_VER)
	#define DEKAF2_DEPRECATED(msg) __declspec(deprecated(msg))
#else
	#define DEKAF2_DEPRECATED(msg)
#endif

#if DEKAF2_HAS_CPP_20
	#define DEKAF2_IS_CONSTANT_EVALUATED() std::is_constant_evaluated()
#elif defined __GNUC__ // valid for both GCC and CLANG
	#if __has_builtin(__builtin_is_constant_evaluated)
		#define DEKAF2_IS_CONSTANT_EVALUATED() __builtin_is_constant_evaluated()
	#else
		#define DEKAF2_IS_CONSTANT_EVALUATED() (false)
	#endif
#endif

// configure exception behavior
#if (defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND))
	// The system has exception handling features. Now check
	// if we want to use exceptions as the error handling mechanism
	// in dekaf2 or not
	#define DEKAF2_THROW(exception) throw exception
	#define DEKAF2_TRY try
	#define DEKAF2_CATCH(exception) catch(exception)
	#if defined(DEKAF2_USE_EXCEPTIONS)
		// macro to test if exceptions are available for error handling
		#define DEKAF2_EXCEPTIONS
		// do not catch exceptions and log them, but pass
		// them on..
		#define DEKAF2_TRY_EXCEPTION {
		#define DEKAF2_LOG_EXCEPTION }
	#else
		#define DEKAF2_TRY_EXCEPTION try {
		#define DEKAF2_LOG_EXCEPTION } catch (const std::exception& e) { kException(e); }
	#endif
#else
	// no exception handling capabilities available
	#define DEKAF2_THROW(exception) std::abort()
	#define DEKAF2_TRY if(true)
	#define DEKAF2_CATCH(exception) if(false)
	#define DEKAF2_TRY_EXCEPTION {
	#define DEKAF2_LOG_EXCEPTION }
#endif

#if defined(__clang__) || defined(__GNUC__)
	// disables Address Sanitizer on request for functions which are not
	// well understood by ASAN
	#define DEKAF2_NO_ASAN __attribute__((no_sanitize_address))
#else
	#define DEKAF2_NO_ASAN
#endif

namespace dekaf2 {
#if (UINTPTR_MAX == 0xffff)
	#define DEKAF2_IS_16_BITS = 1
	#define DEKAF2_BITS = 16
	static constexpr uint16_t KiBits = 16;
#elif (UINTPTR_MAX == 0xffffffff)
	#define DEKAF2_IS_32_BITS = 1
	#define DEKAF2_BITS = 32
	static constexpr uint16_t KiBits = 32;
#elif (UINTPTR_MAX == 0xffffffffffffffff)
	#define DEKAF2_IS_64_BITS = 1
	#define DEKAF2_BITS = 64
	static constexpr uint16_t KiBits = 64;
#else
	#error "unsupported maximum pointer type"
#endif
	static constexpr bool kIs16Bits() { return KiBits == 16; }
	static constexpr bool kIs32Bits() { return KiBits == 32; }
	static constexpr bool kIs64Bits() { return KiBits == 64; }
} // end of namespace dekaf2

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

#ifndef DEKAF2_HAS_CPP_17
namespace std
{

template <typename...>
using void_t = void;

template<class T>
constexpr std::size_t tuple_size_v = tuple_size<T>::value;

template< class T, class... Args >
constexpr bool is_constructible_v = is_constructible<T, Args...>::value;

}
#endif

// Make sure we have std::apply from C++17 available in namespace std::
// It does not matter if they had been declared by other code already. The compiler
// simply picks the first one that matches.
#ifndef DEKAF2_HAS_CPP_17
namespace std
{
	#ifdef DEKAF2_HAS_CPP_14
		namespace detail
		{
			template<typename F, typename Tuple, std::size_t... I>
			decltype(auto) dekaf2_apply_impl(F&& f, Tuple&& t, std::index_sequence<I...>)
			{
				return std::forward<F>(f)(std::get<I>(std::forward<Tuple>(t))...);
			}
		} // of namespace detail

		template<typename F, typename Tuple>
		decltype(auto) apply(F&& f, Tuple&& t)
		{
			return detail::dekaf2_apply_impl(
					std::forward<F>(f), std::forward<Tuple>(t),
					std::make_index_sequence<std::tuple_size<std::remove_reference_t<Tuple>>::value>{});
		}
	#else
		// we lack a C++11 implementation..
		template<typename F, typename Tuple>
		decltype(auto) apply(F&& f, Tuple&& t)
		{
			std::static_assert(false, "dekaf2 misses a C++11-only implementation of std::apply");
		}
	#endif
} // of namespace std
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
#if DEKAF2_HAS_ATTRIBUTE(__always_inline__)
	#define DEKAF2_ALWAYS_INLINE inline __attribute__((__always_inline__))
#elif defined(_MSC_VER)
	#define DEKAF2_ALWAYS_INLINE __forceinline
#else
	#define DEKAF2_ALWAYS_INLINE inline
#endif

#if defined(__BYTE_ORDER__)
	// we can use the preprocessor defines, which is constant
	#define DEKAF2_LE_BE_CONSTEXPR constexpr
#elif DEKAF_HAS_CPP_20
	// we can use the std::endian enum, which is constant
	#define DEKAF2_LE_BE_CONSTEXPR constexpr
	#include <bit>
#elif !DEKAF_NO_GCC && DEKAF2_GCC_VERSION >= 50000 && DEKAF2_HAS_CPP_11
	// this causes an error message in clang, gcc >= 5 takes it
	#define DEKAF2_LE_BE_CONSTEXPR constexpr
#else
	// older gcc versions and newer clang do not compile the constexpr
	#define DEKAF2_LE_BE_CONSTEXPR inline
#endif

namespace dekaf2
{

DEKAF2_LE_BE_CONSTEXPR bool kIsBigEndian()
{
#if defined(__BYTE_ORDER__)
	return __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__;
#elif DEKAF_HAS_CPP_20
	return std::endian::native == std::endian::big;
#else
	union endian_t
	{
		const uint32_t i;
		const unsigned char ch[4];
	};
	const endian_t endian{0x01020304};
	return endian.ch[0] == 1;
#endif
}

DEKAF2_LE_BE_CONSTEXPR bool kIsLittleEndian()
{
	// this is theoretically wrong, as there may be other
	// byte orders than big or little, but we ignore that
	// until DEC resurrects.. and as dekaf2's home is at
	// the former DEC headquarter building we should notice!
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
		for (std::size_t i = 0, e = len-1, lc = len/2; i < lc; ++i, --e)
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

#ifndef npos
	// npos is used in dekaf2 as error return for unsigned return types
	static constexpr size_t npos = static_cast<size_t>(-1);
#endif

} // end of namespace dekaf2

#ifdef DEKAF2_IS_WINDOWS
	#ifdef _M_X64
		#ifndef __x86_64__
			#define __x86_64__
		#endif
	#endif
	#ifdef _M_ARM
		#ifndef __arm__
			#define __arm__
		#endif
	#endif

	#include <BaseTsd.h>
	using ssize_t = SSIZE_T;
	using pid_t = int;
	#include <cstdio>
	#define popen _popen
	#define pclose _pclose
	#include <process.h>
	#define getpid _getpid
	#define strncasecmp _strnicmp
	#define strcasecmp _stricmp

	#ifndef WIFSIGNALED
		#define WIFSIGNALED(x) ((x) == 3)
	#endif
	#ifndef WIFEXITED
		#define WIFEXITED(x) ((x) != 3)
	#endif
	#ifndef WIFSTOPPED
		#define WIFSTOPPED(x) 0
	#endif
	#ifndef WTERMSIG
		#define WTERMSIG(x) SIGTERM
	#endif
	#ifndef WEXITSTATUS
		#define WEXITSTATUS(x) (x)
	#endif
	#ifndef WCOREDUMP
		#define WCOREDUMP(x) 0
	#endif

	#define DEKAF2_POPEN_COMMAND_NOT_FOUND 1
	#define DEKAF2_CLOSE_ON_EXEC_FLAG 0

#else

	#define DEKAF2_POPEN_COMMAND_NOT_FOUND 127
	#define DEKAF2_CLOSE_ON_EXEC_FLAG O_CLOEXEC

#endif

#ifdef __i386__
	#define DEKAF2_X86 1
#endif

#ifdef __x86_64__
	#ifndef DEKAF2_X86
		#define DEKAF2_X86 1
	#endif
	#define DEKAF2_X86_64 1
#endif

#ifdef __arm__
	#define DEKAF2_ARM 1
#endif

#ifdef DEKAF2_X86_64

	#if (!defined(__clang__) || DEKAF2_IS_OSX) && (!defined DEKAF2_IS_WINDOWS)
		// clang has severe issues with int128 and adress sanitizer symbols on Linux
		#define DEKAF2_HAS_INT128 1

		#ifndef int128_t
			using int128_t = __int128;
		#endif

		#ifndef uint128_t
			using uint128_t = unsigned __int128;
		#endif
	#endif

/*
 // we cannot yet use the boost emulation of int128_t, as it does not
 // convert into uint64_t which we need as a workaround in the string
 // utests currently
#else

	#define DEKAF2_HAS_INT128 1

	#include <boost/multiprecision/cpp_int.hpp>
	using int128_t = boost::multiprecision::int128_t;
	using uint128_t = boost::multiprecision::uint128_t;
*/
#endif

#undef DEKAF2_LE_BE_CONSTEXPR

