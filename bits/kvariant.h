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
/// test for include file locations for std::varuant, and include if
/// available, otherwise provide an alternative boost implementation

#include "kcppcompat.h"

#if defined(DEKAF2_HAS_CPP_17)
	#if __has_include(<variant>)
		#include <variant>
		#define DEKAF2_HAS_VARIANT 1
		#define DEKAF2_VARIANT_NAMESPACE std
	#elif __has_include(<experimental/variant>)
		#include <experimental/variant>
		#define DEKAF2_HAS_VARIANT 1
		#define DEKAF2_VARIANT_NAMESPACE std::experimental
	#endif
#endif

#ifndef DEKAF2_HAS_VARIANT
	#include <boost/variant.hpp>
	#define DEKAF2_HAS_VARIANT 1
	#define DEKAF2_VARIANT_NAMESPACE boost
	namespace boost
	{
		// not ideal, but we need to add some conversions
		template <typename Visitor, typename Visitable>
		inline
		BOOST_VARIANT_AUX_GENERIC_RESULT_TYPE(typename Visitor::result_type)
		visit(Visitor& visitor, Visitable&& visitable)
		{
			return ::boost::forward<Visitable>(visitable).apply_visitor(visitor);
		}

		template <typename Visitor, typename Visitable>
		inline
		BOOST_VARIANT_AUX_GENERIC_RESULT_TYPE(typename Visitor::result_type)
		visit(const Visitor& visitor, Visitable&& visitable)
		{
			return ::boost::forward<Visitable>(visitable).apply_visitor(visitor);
		}
	} // end of namespace boost
#endif

#ifdef DEKAF2_VARIANT_NAMESPACE
	namespace var = DEKAF2_VARIANT_NAMESPACE;
#endif
