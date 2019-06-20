/*
//
// DEKAF(tm): Lighter, Faster, Smarter(tm)
//
// Copyright (c) 2019, Ridgeware, Inc.
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

#include "bits/ktemplate.h"
#include "kstringview.h"
#include "kstring.h"
#include <type_traits>

/// @file kjoin.h
/// Highly configurable join template.

namespace dekaf2
{

//-----------------------------------------------------------------------------
/// join for sequential containers
template<typename Container, typename Result,
	typename std::enable_if_t<detail::has_key_type<Container>::value == false, int> = 0 >
void kJoin (Result& sBuffer,
			const Container& ctContainer,
			KStringView svDelim = ",",
			// we add the svPairDelim here to give the same interface as for
			// the associative containers - the value is not used
			KStringView svPairDelim = "="
)
//-----------------------------------------------------------------------------
{
	auto it = ctContainer.begin();
	auto ie = ctContainer.end();

	for (;;)
	{
		sBuffer += *it;

		if (++it == ie)
		{
			break;
		}

		sBuffer += svDelim;
	}
}

//-----------------------------------------------------------------------------
/// join for associative containers
template<typename Container, typename Result,
	typename std::enable_if_t<detail::has_key_type<Container>::value == true, int> = 0 >
void kJoin (Result& sBuffer,
			const Container& ctContainer,
			KStringView svDelim = ",",
			KStringView svPairDelim = "="
)
//-----------------------------------------------------------------------------
{
	auto it = ctContainer.begin();
	auto ie = ctContainer.end();

	for (;;)
	{
		sBuffer += it->first;
		sBuffer += svPairDelim;
		sBuffer += it->second;

		if (++it == ie)
		{
			break;
		}

		sBuffer += svDelim;
	}
}

//-----------------------------------------------------------------------------
/// join for sequential containers, returns result
template<typename Container, typename Result = KString,
	typename std::enable_if_t<detail::has_key_type<Container>::value == false, int> = 0 >
Result kJoined (
			const Container& ctContainer,
			KStringView svDelim = ",",
			// we add the svPairDelim here to give the same interface as for
			// the associative containers - the value is not used
			KStringView svPairDelim = "="
)
//-----------------------------------------------------------------------------
{
	Result result;
	kJoin(result, ctContainer, svDelim, svPairDelim);
	return result;
}

//-----------------------------------------------------------------------------
/// join for associative containers, returns result
template<typename Container, typename Result = KString,
	typename std::enable_if_t<detail::has_key_type<Container>::value == true, int> = 0 >
Result kJoined (
			const Container& ctContainer,
			KStringView svDelim = ",",
			KStringView svPairDelim = "="
)
//-----------------------------------------------------------------------------
{
	Result result;
	kJoin(result, ctContainer, svDelim, svPairDelim);
	return result;
}

} // namespace dekaf2
