/*
//=============================================================================
//
// DEKAF(tm): Lighter, Faster, Smarter(tm)
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
//=============================================================================
*/

#include <boost/archive/iterators/base64_from_binary.hpp>
#include <boost/archive/iterators/binary_from_base64.hpp>
#include <boost/archive/iterators/insert_linebreaks.hpp>
#include <boost/archive/iterators/transform_width.hpp>
#include <boost/archive/iterators/remove_whitespace.hpp>

#include "kbase64.h"

namespace dekaf2 {

//-----------------------------------------------------------------------------
KString KBase64::Encode(KStringView sInput)
//-----------------------------------------------------------------------------
{
	using namespace boost::archive::iterators;
	using iterator_type = KStringView::const_iterator;
	using base64_enc    = insert_linebreaks<base64_from_binary<transform_width<iterator_type, 6, 8> >, 76>;

	KString out;

	// calculate final size for encoded string
	KString::size_type iSize = sInput.size() * 8 / 6 + sInput.size() % 3;
	// add linefeeds
	iSize += iSize / 76;
	// and reserve buffer to avoid reallocations
	out.reserve(iSize);

	// transform to base64
	out.assign(base64_enc(sInput.begin()), base64_enc(sInput.end()));

	// append the padding
	out.append((3 - sInput.size() % 3) % 3, '=');

	return out;
}

//-----------------------------------------------------------------------------
KString KBase64::Decode(KStringView sInput)
//-----------------------------------------------------------------------------
{
	using namespace boost::archive::iterators;
	using iterator_type = KStringView::const_iterator;
	using base64_dec    = transform_width<binary_from_base64<remove_whitespace<iterator_type> >, 8, 6>;

	KString out;

	// calculate approximate size for decoded string (input may contain whitespace)
	KString::size_type iSize = sInput.size() * 6 / 8;
	// and reserve buffer to avoid reallocations
	out.reserve(iSize);

	// transform from base64
	out.assign(base64_dec(sInput.begin()), base64_dec(sInput.end()));

	// remove the padding
	KStringView::size_type len = sInput.size();
	if (len > 2 && out.size() > 1)
	{
		// a padded sInput has at least 3 chars
		if (sInput[len-1] == '=')
		{
			if (sInput[len-2] == '=')
			{
				out.erase(out.size()-2, 2);
			}
			else
			{
				out.erase(out.size()-1, 1);
			}
		}
	}

	return out;
}

} // end of namespace dekaf2

