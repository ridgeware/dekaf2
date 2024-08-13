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

/// @file kcompatibility.h
/// compatibility layer to provide same interfaces for C++11 to 17

#include "kdefinitions.h"
#include <type_traits>

DEKAF2_NAMESPACE_BEGIN

/// suppress compiler warnings on seemingly unused variables
template <typename... T> constexpr
void kIgnoreUnused(const T&...) {}

/// returns true if we are in a constexpr context (with C++20..)
constexpr
#if DEKAF2_HAS_CPP_17
inline
#endif
bool kIsConstantEvaluated(bool default_value = false) noexcept
{
#ifdef __cpp_lib_is_constant_evaluated
	kIgnoreUnused(default_value);
	return std::is_constant_evaluated();
#else
	return default_value;
#endif
}

DEKAF2_NAMESPACE_END

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
		// but we do not care anymore, as we are in 2023..
		using shared_mutex = mutex;
		template<class T>
		using shared_lock = unique_lock<T>;
	#endif
#endif

} // end of namespace std

// Make sure we have standard helper templates from C++14 available in namespace std::
// It does not matter if they had been declared by other code already. The compiler
// simply picks the first one that matches.
#ifndef DEKAF2_HAS_CPP_14

#if DEKAF2_HAS_INCLUDE("bits/kmake_unique.h")
	#include "bits/kmake_unique.h"
#endif

namespace std
{

template<bool B, class T, class F>
using conditional_t = typename conditional<B,T,F>::type;

template <bool B, typename T = void>
using enable_if_t = typename enable_if<B,T>::type;

template< class T >
using decay_t = typename decay<T>::type;

} // end of namespace std
#endif

#ifndef DEKAF2_HAS_CPP_17
namespace std
{

template <typename...>
using void_t = void;

template<class...> struct conjunction : true_type { };
template<class T1> struct conjunction<T1> : T1 { };
template<class T1, class... Tn>
struct conjunction<T1, Tn...>
: conditional<bool(T1::value), conjunction<Tn...>, T1>::type {};

template<bool T>
using bool_constant = integral_constant<bool, T>;

template<class T>
struct negation : bool_constant<!bool(T::value)> { };

} // end of namespace std
#endif

#ifndef DEKAF2_HAS_CPP_23
namespace std 
{
	template<class Enum, typename enable_if<is_enum<Enum>::value, int>::type = 0>
	constexpr typename underlying_type<Enum>::type to_underlying(Enum e) noexcept
	{
		return static_cast<typename underlying_type<Enum>::type>(e);
	}
} // end of namespace std
#endif

// Make sure we have std::apply from C++17 available in namespace std::
// It does not matter if they had been declared by other code already. The compiler
// simply picks the first one that matches.
// Old gcc versions < 7 do not have std::apply even in C++17 mode
#if !defined(DEKAF2_HAS_FULL_CPP_17)
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
		void apply(F&& f, Tuple&& t)
		{
			static_assert(false, "dekaf2 misses a C++11-only implementation of std::apply, if possible update to C++14 or newer");
		}
	#endif
} // end of namespace std
#endif

// make windows look a bit more like linux
#ifdef DEKAF2_IS_WINDOWS

	#include <BaseTsd.h>
	using ssize_t = SSIZE_T;
	using pid_t = int;

	#include <process.h>
	#ifndef getpid
		#define getpid _getpid
	#endif

	#include <cstdio>
	#ifndef popen
		#define popen _popen
	#endif
	#ifndef pclose
		#define pclose _pclose
	#endif
	#ifndef strncasecmp
		#define strncasecmp _strnicmp
	#endif
	#ifndef strcasecmp
		#define strcasecmp _stricmp
	#endif
#if 0
	// we do not add these here anymore because they interfere with
	// the fmt::fmtlib built on Windows
	#ifndef timegm
		#define timegm _mkgmtime
	#endif
	#ifndef localtime_r
		#define localtime_r localtime_s
	#endif
	#ifndef gmtime_r
		#define gmtime_r gmtime_s
	#endif
#endif
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

#endif // of DEKAF2_IS_WINDOWS
