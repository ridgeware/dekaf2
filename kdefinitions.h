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
//
*/

#pragma once

/// @file kdefinitions.h
/// dekaf2 preprocessor defines

#include <climits>
#include <cstdint>
#include <cstddef>

// While __has_include only became a standard in C++20, it is supported
// by clang since ever, by gcc since 5.0.0, and by MSVC since some time
// as well. Therefore we take it for granted that it is available and
// working, and use it to test for features of either the C++ lib or
// of dekaf2 itself.
#ifndef __has_include
	#define DEKAF2_HAS_INCLUDE(x) 0
#else
	#define DEKAF2_HAS_INCLUDE(x) __has_include(x)
#endif

#if DEKAF2_HAS_INCLUDE("kconfiguration.h")
	#include "kconfiguration.h"
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

#if (__cplusplus >= 202002L) && DEKAF2_HAS_INCLUDE(<version>)
	#include <version>
#else
	// see https://stackoverflow.com/a/31658120
	#include <ciso646>
#endif

#define DEKAF2_xstringify(x) #x
#define DEKAF2_stringify(x) DEKAF2_xstringify(x)

#define DEKAF2_concatx(x, y) x##y
#define DEKAF2_concat(x, y) DEKAF2_concatx(x, y)

#if defined __clang__
	#define DEKAF2_CLANG_VERSION_MAJOR __clang_major__
	#define DEKAF2_CLANG_VERSION (__clang_major__ * 10000 + __clang_minor__ * 100 + __clang_patchlevel__)
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

#if defined _GLIBCXX_RELEASE
	// e.g.: 11
	#define DEKAF2_GLIBCXX_VERSION_MAJOR _GLIBCXX_RELEASE
#elif defined DEKAF2_IS_GCC
	// check if this is actually (an old) GCC, then the libstdc++ release is the same as __GNUC__
	#define DEKAF2_GLIBCXX_VERSION_MAJOR __GNUC__
#endif

#if defined _LIBCPP_VERSION
	// e.g.: 13000
	#define DEKAF2_LIBCXX_VERSION _LIBCPP_VERSION
	// e.g.: 13
	#define DEKAF2_LIBCXX_VERSION_MAJOR (_LIBCPP_VERSION / 1000)
#endif

#if defined __GLIBC__
	#define DEKAF2_GLIBC_VERSION __GLIBC__.__GLIBC_MINOR__
	#define DEKAF2_GLIBC_VERSION_MAJOR __GLIBC__
	#define DEKAF2_GLIBC_VERSION_MINOR __GLIBC_MINOR__
#endif

#if DEKAF2_HAS_INCLUDE(<source_location>)
	#include <source_location>
#endif
#if __cpp_lib_source_location >= 201907L
	#define DEKAF2_FUNCTION_NAME std::source_location::current().function_name()
#elif defined __GNUC__
	#define DEKAF2_FUNCTION_NAME __PRETTY_FUNCTION__
#elif defined _MSC_VER
	#define DEKAF2_FUNCTION_NAME __FUNCSIG__
#else
	#define DEKAF2_FUNCTION_NAME __FUNCTION__
#endif

#if !defined(DEKAF2_IS_OSX) && defined(__APPLE__) && defined(__MACH__)
	// keep the old define, but change new code to DEKAF2_IS_MACOS
	#define DEKAF2_IS_OSX 1
#endif
#if !defined(DEKAF2_IS_MACOS) && defined(__APPLE__) && defined(__MACH__)
	#define DEKAF2_IS_MACOS 1
#endif

#if !defined(DEKAF2_IS_UNIX) && (defined(unix) || defined(__unix__) || defined(DEKAF2_IS_MACOS))
	#define DEKAF2_IS_UNIX 1
#endif

#if !defined(DEKAF2_IS_UNIX) && (defined(_MSC_VER))
	#define DEKAF2_IS_WINDOWS 1
 	// works on < VS 2013
	#define _CRT_SECURE_NO_WARNINGS
	// works on >= VS 2013
	#pragma warning(disable:4996)
	// "warning C4307: '*': integral constant overflow" (on an unsigned type..) - this is a comp bug
	// that was only fixed in recent (> 2017) MSVC versions
	#pragma warning(disable:4307)
#endif

#if (__cplusplus < 201103L)
	#error "this version of dekaf needs at least a C++11 compiler"
#endif

#define DEKAF2_HAS_CPP_11 1

#if (__cplusplus >= 201402L)
	#define DEKAF2_HAS_CPP_14 1
#endif

// this test is a bit bogus (by just testing if the cpp date
// is younger than that of C++14), but it should probably even
// be kept after C++17 defines an official date, as older
// compilers would not know it (but support C++17)
#if (__cplusplus > 201402L && !defined(DEKAF2_HAS_CPP_17))
	#define DEKAF2_HAS_CPP_17 1
#endif

#if defined(DEKAF2_HAS_CPP_17)
	#if (__cplusplus < 201703L)
		#define DEKAF2_HAS_INCOMPLETE_CPP_17 1
	#else
		#define DEKAF2_HAS_FULL_CPP_17 1
	#endif
#endif

// this test is a bit bogus (by just testing if the cpp date
// is younger than that of C++17), but it should probably even
// be kept after C++20 defines an official date, as older
// compilers would not know it (but support C++20)
#if (__cplusplus > 201703L)
	#define DEKAF2_HAS_CPP_20 1
#endif

#if (__cplusplus > 202002L)
	#define DEKAF2_HAS_CPP_23 1
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

#ifdef DEKAF2_HAS_FULL_CPP_17
	#define DEKAF2_FULL_CONSTEXPR_17 constexpr
#else
	#define DEKAF2_FULL_CONSTEXPR_17 inline
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
#if !defined(DEKAF2_HAS_FULL_CPP_17)
	#define DEKAF2_REPEAT_CONSTEXPR_VARIABLE 1
#endif

#if (__cpp_if_constexpr)
	#define DEKAF2_CONSTEXPR_IF constexpr
	#define DEKAF2_HAS_CONSTEXPR_IF 1
#else
	#define DEKAF2_CONSTEXPR_IF
#endif

#ifndef DEKAF2_HAS_CHRONO_ROUND
	#if defined(_MSC_FULL_VER) && (_MSC_FULL_VER >= 190023918 || (_MSC_FULL_VER >= 190000000 && defined (__clang__)))
		#define DEKAF2_HAS_CHRONO_ROUND 1
	#elif DEKAF2_HAS_CPP_17 && __cpp_lib_chrono >= 201510L
		#define DEKAF2_HAS_CHRONO_ROUND 1
	#elif DEKAF2_HAS_CPP_17 && _LIBCPP_VERSION >= 3800
		#define DEKAF2_HAS_CHRONO_ROUND 1
	#endif
#endif

#if DEKAF2_HAS_CPP_17 && DEKAF2_HAS_CPP_ATTRIBUTE(fallthrough)
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

#if DEKAF2_HAS_CPP_17 && DEKAF2_HAS_CPP_ATTRIBUTE(nodiscard)
	#define DEKAF2_NODISCARD [[nodiscard]]
#elif DEKAF2_HAS_CPP_ATTRIBUTE(clang::nodiscard)
	#define DEKAF2_NODISCARD [[clang::nodiscard]]
#elif DEKAF2_HAS_CPP_ATTRIBUTE(gnu::nodiscard)
	#define DEKAF2_NODISCARD [[gnu::nodiscard]]
#else
	// we do not check for the older explicit attribute (warn_unused_result),
	// as those gcc versions then typically run into other issues with nodiscard
	// analysis, e.g. when overloading & and && versions of member functions
	#define DEKAF2_NODISCARD
#endif

#define DEKAF2_NODISCARD_PEDANTIC DEKAF2_NODISCARD

#if defined(__clang__) || defined(__GNUC__)
	#define DEKAF2_DEPRECATED(msg) __attribute__((__deprecated__(msg)))
#elif defined(_MSC_VER)
	#define DEKAF2_DEPRECATED(msg) __declspec(deprecated(msg))
#else
	#define DEKAF2_DEPRECATED(msg)
#endif

#if DEKAF2_HAS_CPP_ATTRIBUTE(gsl::Owner)
	#define DEKAF2_GSL_OWNER(x) [[gsl::Owner(x)]]
#else
	#define DEKAF2_GSL_OWNER(x)
#endif

#if DEKAF2_HAS_CPP_ATTRIBUTE(gsl::Pointer)
	#define DEKAF2_GSL_POINTER(x) [[gsl::Pointer(x)]]
#else
	#define DEKAF2_GSL_POINTER(x)
#endif

#if DEKAF2_HAS_CPP_20
	#define DEKAF2_IS_CONSTANT_EVALUATED() std::is_constant_evaluated()
#elif defined __GNUC__ // valid for both GCC and CLANG
//	#if __has_builtin(__builtin_is_constant_evaluated)
//		#define DEKAF2_IS_CONSTANT_EVALUATED() __builtin_is_constant_evaluated()
//	#else
		#define DEKAF2_IS_CONSTANT_EVALUATED() (false)
//	#endif
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
	// disables Address Sanitizer for functions which are not
	// well understood by ASAN
	#define DEKAF2_NO_ASAN __attribute__((no_sanitize_address))
	// disables Undefined Behavior Sanitizer's named check for functions which are not
	// well understood by UBSAN - e.g. use "undefined" for nearly all checks, or "enum"
	// or "return" or any other check's name
	#define DEKAF2_NO_UBSAN_CHECK(which) __attribute__((no_sanitize(which)))
	// disables Undefined Behavior Sanitizer for functions which are not
	// well understood by UBSAN
	#define DEKAF2_NO_UBSAN DEKAF2_NO_UBSAN_CHECK("undefined")
#else
	#define DEKAF2_NO_ASAN
	#define DEKAF2_NO_UBSAN_CHECK(which)
	#define DEKAF2_NO_UBSAN
#endif

#if defined(__SANITIZE_ADDRESS__)
	#define DEKAF2_HAS_ASAN
#elif defined(__clang__)
	#if __has_feature(address_sanitizer)
		#define DEKAF2_HAS_ASAN
	#endif
#endif

#if DEKAF2_HAS_INCLUDE(<features.h>)
	#ifndef _GNU_SOURCE
		#define _GNU_SOURCE
		#define DEKAF2_GNU_SOURCE
	#endif
	#include <features.h>
	#ifndef __USE_GNU
		#define DEKAF2_HAS_MUSL
	#endif
	#ifdef DEKAF2_GNU_SOURCE
		#undef _GNU_SOURCE
		#undef DEKAF2_GNU_SOURCE
	#endif
#endif

// define macros to teach the compiler which branch is more likely
// to be taken - the effects are actually minimal to nonexistent,
// so do not bother for code that is not really core
#if defined __clang__ || (defined(__GNUC__) && __GNUC__ >= 4)
	#define DEKAF2_LIKELY(expression)   (__builtin_expect((expression), 1))
	#define DEKAF2_UNLIKELY(expression) (__builtin_expect((expression), 0))
#else
	#define DEKAF2_LIKELY(expression)   (expression)
	#define DEKAF2_UNLIKELY(expression) (expression)
#endif

// force the compiler to respect the inline - better do not use it, the
// compiler is smarter than us in almost all cases
#if DEKAF2_HAS_ATTRIBUTE(always_inline)
	#define DEKAF2_ALWAYS_INLINE inline __attribute__((__always_inline__))
#elif defined(_MSC_VER)
	#define DEKAF2_ALWAYS_INLINE __forceinline
#else
	#define DEKAF2_ALWAYS_INLINE inline
#endif

#if DEKAF2_HAS_ATTRIBUTE(noinline)
	#define DEKAF2_NEVER_INLINE __attribute__((__noinline__))
#elif defined(_MSC_VER)
	#define DEKAF2_NEVER_INLINE __declspec(noinline)
#else
	#define DEKAF2_NEVER_INLINE
#endif

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
	// this is a 32 bit arm, never 64 bit
	#define DEKAF2_ARM 1
	#define DEKAF2_ARM32 1
#endif

#ifdef __aarch64__
	// this is a 64 bit arm, never 32 bit
	#define DEKAF2_AARCH64 1
	#define DEKAF2_ARM64 1
#endif

#ifdef __powerpc64__
	#define DEKAF2_PPC64 1
#endif

#if defined(DEKAF2_MAY_HAVE_INT128) && defined(DEKAF2_IS_64_BITS)
	#define DEKAF2_HAS_INT128 1

	#ifndef int128_t
		using int128_t = __int128;
	#endif

	#ifndef uint128_t
		using uint128_t = unsigned __int128;
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

// provide symbol export flags for dynamic linking
#ifdef DEKAF2_IS_WINDOWS
	#ifdef DEKAF2_DYNAMIC_LIBRARY
		#ifdef DEKAF2_LIBRARY_BUILD
			#define DEKAF2_PUBLIC __declspec(dllexport)
		#else
			#define DEKAF2_PUBLIC __declspec(dllimport)
		#endif
	#else
		#define DEKAF2_PUBLIC
	#endif
	#define DEKAF2_PRIVATE
#else
	#if __GNUC__ >= 4 // which includes clang
		#define DEKAF2_PUBLIC  __attribute__((visibility("default")))
		#define DEKAF2_PRIVATE __attribute__((visibility("hidden")))
	#else
		#define DEKAF2_PUBLIC
		#define DEKAF2_PRIVATE
	#endif
#endif

#ifndef DEKAF2_HAS_SPACESHIP_OPERATOR
	#if defined(__cpp_impl_three_way_comparison) && __cpp_impl_three_way_comparison >= 201907L && \
		defined(__cpp_lib_three_way_comparison) && __cpp_lib_three_way_comparison >= 201907L
		#define DEKAF2_HAS_SPACESHIP_OPERATOR 1
	#endif
#endif

#if __cpp_concepts >= 202002L
	#define DEKAF2_HAS_CONCEPTS 1
#endif

// Helper macros to make an enum type a "flag" type, that is, bit operations with enum values are permitted
// in a type-safe fashion.
//
// There is no way except macros to selectively enable bit operators in C++ - the alternative would be to
// enable them for all enums, which is not desirable.
//
// Use preferably with bitset enums like:
//
// enum Flags { RED = 1 << 0, BLUE = 1 << 1, GREEN = 1 << 2 };
// DEKAF2_ENUM_IS_FLAG(Flags);
// Flags MyFlags = Flags::RED | Flags::BLUE;

#define DEKAF2_DETAIL_ENUM_INNER_BIN_OP(Type, Operator) \
constexpr Type operator Operator (Type left, Type right) \
{ \
	return static_cast<Type>(static_cast<typename std::underlying_type<Type>::type>(left) Operator static_cast<typename std::underlying_type<Type>::type>(right)); \
}

#define DEKAF2_DETAIL_ENUM_INNER_UNA_OP(Type, Operator) \
constexpr Type operator Operator (Type other) \
{ \
	return static_cast<Type>(Operator static_cast<typename std::underlying_type<Type>::type>(other)); \
}

#define DEKAF2_DETAIL_ENUM_INNER_REF_OP(Type, Operator) \
inline Type& operator Operator (Type& left, Type right) \
{ \
	return reinterpret_cast<Type&>(reinterpret_cast<typename std::underlying_type<Type>::type&>(left) Operator static_cast<typename std::underlying_type<Type>::type>(right)); \
}

#define DEKAF2_ENUM_IS_FLAG(Type) \
 DEKAF2_DETAIL_ENUM_INNER_BIN_OP(Type,  |) \
 DEKAF2_DETAIL_ENUM_INNER_BIN_OP(Type,  &) \
 DEKAF2_DETAIL_ENUM_INNER_BIN_OP(Type,  ^) \
 DEKAF2_DETAIL_ENUM_INNER_UNA_OP(Type,  ~) \
 DEKAF2_DETAIL_ENUM_INNER_REF_OP(Type, |=) \
 DEKAF2_DETAIL_ENUM_INNER_REF_OP(Type, &=) \
 DEKAF2_DETAIL_ENUM_INNER_REF_OP(Type, ^=)

// helpers for enums end here

// helper macro to generate the remaining comparison operators for a type from existing
// equality and less operators
#define DEKAF2_COMPARISON_OPERATORS_WITH_ATTR(Attr, Type) \
 Attr bool operator!=(const Type& left, const Type& right) { return !(left == right); } \
 Attr bool operator> (const Type& left, const Type& right) { return   right < left;   } \
 Attr bool operator<=(const Type& left, const Type& right) { return !(right < left);  } \
 Attr bool operator>=(const Type& left, const Type& right) { return !(left  < right); }

// assume "inline" as the function attribute
#define DEKAF2_COMPARISON_OPERATORS(Type) DEKAF2_COMPARISON_OPERATORS_WITH_ATTR(inline, Type)

// helper macro to generate all comparison operators for one type from some
// transformation, like function call or type conversion. Refer to the objects
// in the wrapper with "left" or "right"
#define DEKAF2_WRAPPED_COMPARISON_OPERATORS_ONE_TYPE_WITH_ATTR(Attr, Type, WrappedTypeLeft, WrappedTypeRight) \
 Attr bool operator==(const Type&  left , const Type& right) { return WrappedTypeLeft  == WrappedTypeRight; } \
 Attr bool operator< (const Type&  left , const Type& right) { return WrappedTypeLeft  <  WrappedTypeRight; } \
 DEKAF2_COMPARISON_OPERATORS_WITH_ATTR(Attr, Type)

// assume "inline" as the function attribute
#define DEKAF2_WRAPPED_COMPARISON_OPERATORS_ONE_TYPE(Type, WrappedTypeLeft, WrappedTypeRight) \
 DEKAF2_WRAPPED_COMPARISON_OPERATORS_ONE_TYPE_WITH_ATTR(inline, Type, WrappedTypeLeft, WrappedTypeRight)

#define DEKAF2_DETAIL_COMPARISON_OPERATORS_TWO_TYPES_WITH_ATTR(Attr, TFirst, TSecond) \
 Attr bool operator!=(const TFirst& left, const TSecond& right) { return !(left == right); } \
 Attr bool operator> (const TFirst& left, const TSecond& right) { return   right < left;   } \
 Attr bool operator<=(const TFirst& left, const TSecond& right) { return !(right < left);  } \
 Attr bool operator>=(const TFirst& left, const TSecond& right) { return !(left  < right); } \

// helper macro to generate the remaining comparison operators for two types from existing
// equality and less operators (must exist in both directions)
#define DEKAF2_COMPARISON_OPERATORS_TWO_TYPES_WITH_ATTR(Attr, TFirst, TSecond) \
 DEKAF2_DETAIL_COMPARISON_OPERATORS_TWO_TYPES_WITH_ATTR(Attr, TFirst, TSecond) \
 DEKAF2_DETAIL_COMPARISON_OPERATORS_TWO_TYPES_WITH_ATTR(Attr, TSecond, TFirst) \

// helper macro to generate all comparison operators for two types from some
// transformation, like function call or type conversion. If one of the two types
// does not need a "wrapped" conversion, name it first or second..
#define DEKAF2_WRAPPED_COMPARISON_OPERATORS_TWO_TYPES_WITH_ATTR(Attr, TFirst, TSecond, WrappedFirst, WrappedSecond) \
 Attr bool operator==(const TFirst&  first , const TSecond& second) { return WrappedFirst  == WrappedSecond; } \
 Attr bool operator< (const TFirst&  first , const TSecond& second) { return WrappedFirst  <  WrappedSecond; } \
 Attr bool operator==(const TSecond& second, const TFirst&  first ) { return WrappedSecond == WrappedFirst;  } \
 Attr bool operator< (const TSecond& second, const TFirst&  first ) { return WrappedSecond <  WrappedFirst;  } \
 DEKAF2_DETAIL_COMPARISON_OPERATORS_TWO_TYPES_WITH_ATTR(Attr, TFirst, TSecond) \
 DEKAF2_DETAIL_COMPARISON_OPERATORS_TWO_TYPES_WITH_ATTR(Attr, TSecond, TFirst)

// assume "inline" as the function attribute
#define DEKAF2_WRAPPED_COMPARISON_OPERATORS_TWO_TYPES(TFirst, TSecond, WrappedFirst, WrappedSecond) \
 DEKAF2_WRAPPED_COMPARISON_OPERATORS_TWO_TYPES_WITH_ATTR(inline, TFirst, TSecond, WrappedFirst, WrappedSecond)

#ifndef DEKAF2_NAMESPACE_NAME
	#define DEKAF2_NAMESPACE_NAME dekaf2
#endif

#ifndef DEKAF2_PREFIX
	#define DEKAF2_PREFIX DEKAF2_NAMESPACE_NAME::
#endif

#ifndef DEKAF2_NAMESPACE_BEGIN
	#define DEKAF2_NAMESPACE_BEGIN namespace DEKAF2_NAMESPACE_NAME {
#endif

#ifndef DEKAF2_NAMESPACE_END
	#define DEKAF2_NAMESPACE_END }
#endif

#ifndef DEKAF2_USING_NAMESPACE_DEKAF2
	#define DEKAF2_USING_NAMESPACE_DEKAF2 using namespace DEKAF2_NAMESPACE_NAME;
#endif

DEKAF2_NAMESPACE_BEGIN

#if (UINTPTR_MAX == 0xffff)
	#define DEKAF2_IS_16_BITS 1
	#define DEKAF2_BITS 16
	static constexpr uint16_t KiBits = 16;
#elif (UINTPTR_MAX == 0xffffffff)
	#define DEKAF2_IS_32_BITS 1
	#define DEKAF2_BITS 32
	static constexpr uint16_t KiBits = 32;
#elif (UINTPTR_MAX == 0xffffffffffffffff)
	#define DEKAF2_IS_64_BITS 1
	#define DEKAF2_BITS 64
	static constexpr uint16_t KiBits = 64;
#else
	#error "unsupported maximum pointer type"
#endif
	static constexpr bool kIs16Bits() noexcept { return KiBits == 16; }
	static constexpr bool kIs32Bits() noexcept { return KiBits == 32; }
	static constexpr bool kIs64Bits() noexcept { return KiBits == 64; }

	// KDefaultCopyBufSize is used in dekaf2 as default size for all temp buffers
	static constexpr std::size_t KDefaultCopyBufSize = 4096;

	// npos is used in dekaf2 as error return for unsigned return types
	static constexpr std::size_t npos = static_cast<std::size_t>(-1);

DEKAF2_NAMESPACE_END
