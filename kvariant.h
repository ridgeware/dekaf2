/*
//
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
//
*/

#pragma once

/// @file kvariant.h
/// test for include file locations for std::variant, and include if
/// available, otherwise provide an alternative boost implementation

#include "bits/kcppcompat.h"

// clang has multiple issues with variants:
//
// If used with Xcode 10, it needs the deployment target be set to 10.14 to allow
// the use of it.
//
// If used with current libstdc++ (gcc 8.1) it does not discover a friend declaration
// for variant::get, so it fails.
//
// Therefore for the time being, if clang then simply use the boost implementation

#if DEKAF2_HAS_INCLUDE(<variant>) && !DEKAF2_IS_CLANG
	#include <variant>
	#define DEKAF2_HAS_VARIANT 1
	#define DEKAF2_HAS_STD_VARIANT 1
	#define DEKAF2_VARIANT_NAMESPACE std
#elif DEKAF2_HAS_INCLUDE(<experimental/variant>) && !DEKAF2_IS_CLANG
	#include <experimental/variant>
	#define DEKAF2_HAS_VARIANT 1
	#define DEKAF2_HAS_STD_VARIANT 1
	#define DEKAF2_VARIANT_NAMESPACE std::experimental
#else
	#include <boost/variant.hpp>
	#include <boost/variant/multivisitors.hpp>
	#define DEKAF2_HAS_VARIANT 1
	#define DEKAF2_HAS_BOOST_VARIANT 1
	#define DEKAF2_VARIANT_NAMESPACE boost
	namespace boost
	{
		// not ideal, but we need to add some template name conversions, as the
		// std version calls the function visit, and not apply_visitor

		template <typename Visitor, typename...Visitable>
		typename Visitor::result_type visit(Visitor& visitor, Visitable&&...visitable)
		{
			return apply_visitor(visitor, std::forward<Visitable>(visitable)...);
		}

		template <typename Visitor, typename...Visitable>
		typename Visitor::result_type visit(const Visitor& visitor, Visitable&&...visitable)
		{
			return apply_visitor(visitor, std::forward<Visitable>(visitable)...);
		}

		// simulate std::holds_alternative with boost's pointer version of get<>()
		// which returns a nullptr when the type is wrong
		template <typename T, typename... Types>
		bool holds_alternative(const variant<Types...>& v) noexcept
		{
			return get<T>(&v) != nullptr;
		}

	} // end of namespace boost
#endif

#ifdef DEKAF2_VARIANT_NAMESPACE
	namespace var = DEKAF2_VARIANT_NAMESPACE;
#endif

namespace dekaf2 {

#ifdef DEKAF2_HAS_BOOST_VARIANT
	template<typename RETURNTYPE = void>
	struct KVisitorBase : public var::static_visitor<RETURNTYPE> {};
#else
	template<typename RETURNTYPE = void>
	struct KVisitorBase {};
#endif

#ifdef DEKAF2_HAS_CPP_17
	/// generic overload class - we need C++17 for the variadic using directive
	// we drop the RETURNTYPE and only allow for void returns as otherwise the template
	// would not resolve on lambdas..
	template <class ...Fs>
	struct KVisitor : Fs..., KVisitorBase<void>
	{
		KVisitor(Fs const&... fs) : Fs{fs}..., KVisitorBase<void>()
		{}

		using Fs::operator()...;
	};

	// the better solution would be the following overload,
	// as it also permits for move constructed lambdas and
	// generic function objects, but current versions of
	// gcc and clang do not compile it..
/*
	template <class ...Fs>
	struct overload : Fs...
 	{
		template <class ...Ts>
		overload(Ts&& ...ts) : Fs{std::forward<Ts>(ts)}...
		{}

		using Fs::operator()...;
	};

	template <class ...Ts>
	overload(Ts&&...) -> overload<std::remove_reference_t<Ts>...>;
 */

#endif

} // of namespace dekaf2
