/*
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
*/

#include "kfind.h"
#include "../kstring.h"
#include "../kstringview.h"

namespace dekaf2
{

//------------------------------------------------------------------------------
std::size_t kFind(KStringView str,
                  const char* search,
                  std::size_t pos,
                  std::size_t n)
//------------------------------------------------------------------------------
{
	// GLIBC has a performant Boyer-Moore implementation for ::memmem, it
	// outperforms fbstring's simplyfied Boyer-Moore by one magnitude
	// (which means facebook uses it internally as well, as they mention a
	// search performance improvement by a factor of 30, but their code
	// in reality only improves search performance by a factor of 2
	// compared to std::string::find() - it is ::memmem() which brings it
	// to 30)
	if (DEKAF2_UNLIKELY(n == 1))
	{
		// flip to single char search if only one char is in the search argument
		return kFind(str.data(), str.size(), *search, pos);
	}
	if (DEKAF2_UNLIKELY(pos > str.size()))
	{
		return npos;
	}
	auto found = static_cast<const char*>(::memmem(str.data() + pos, str.size() - pos, search, n));
	if (DEKAF2_UNLIKELY(!found))
	{
		return npos;
	}
	else
	{
		return static_cast<std::size_t>(found - str.data());
	}
}

//------------------------------------------------------------------------------
std::size_t kReplace(KString& string,
                     KStringView sSearch,
                     KStringView sReplaceWith,
                     bool bReplaceAll)
//------------------------------------------------------------------------------
{
	if (DEKAF2_UNLIKELY(sSearch.empty() || string.size() < sSearch.size()))
	{
		return 0;
	}

	typedef KString::size_type size_type;
	typedef KString::value_type value_type;

	size_type iNumReplacement = 0;
	// use a non-const ref to the first element, as .data() is const with C++ < 17
	value_type* haystack = &string[0];
	size_type haystackSize = string.size();

	value_type* pszFound = static_cast<value_type*>(memmem(haystack, haystackSize, sSearch.data(), sSearch.size()));

	if (DEKAF2_LIKELY(pszFound != nullptr))
	{

		if (sReplaceWith.size() <= sSearch.size())
		{
			// execute an in-place substitution (C++17 actually has a non-const string.data())
			value_type* pszTarget = const_cast<value_type*>(haystack);

			while (pszFound)
			{
				auto untouchedSize = static_cast<size_type>(pszFound - haystack);
				if (pszTarget < haystack)
				{
					std::memmove(pszTarget, haystack, untouchedSize);
				}
				pszTarget += untouchedSize;

				if (DEKAF2_LIKELY(sReplaceWith.empty() == false))
				{
					std::memmove(pszTarget, sReplaceWith.data(), sReplaceWith.size());
					pszTarget += sReplaceWith.size();
				}

				haystack = pszFound + sSearch.size();
				haystackSize -= (sSearch.size() + untouchedSize);

				pszFound = static_cast<value_type*>(memmem(haystack, haystackSize, sSearch.data(), sSearch.size()));

				++iNumReplacement;

				if (DEKAF2_UNLIKELY(bReplaceAll == false))
				{
					break;
				}
			}

			if (DEKAF2_LIKELY(haystackSize > 0))
			{
				std::memmove(pszTarget, haystack, haystackSize);
				pszTarget += haystackSize;
			}

			auto iResultSize = static_cast<size_type>(pszTarget - string.data());
			string.resize(iResultSize);

		}
		else
		{
			// execute a copy substitution
			KString sResult;
			sResult.reserve(string.size());

			while (pszFound)
			{
				auto untouchedSize = static_cast<size_type>(pszFound - haystack);
				sResult.append(haystack, untouchedSize);
				sResult.append(sReplaceWith.data(), sReplaceWith.size());

				haystack = pszFound + sSearch.size();
				haystackSize -= (sSearch.size() + untouchedSize);

				pszFound = static_cast<value_type*>(memmem(haystack, haystackSize, sSearch.data(), sSearch.size()));

				++iNumReplacement;

				if (DEKAF2_UNLIKELY(bReplaceAll == false))
				{
					break;
				}
			}

			sResult.append(haystack, haystackSize);
			string.swap(sResult);
		}
	}

	return iNumReplacement;
}

} // end of namespace dekaf2

