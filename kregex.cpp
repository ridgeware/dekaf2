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

#include "kregex.h"
#include "klog.h"

namespace dekaf2
{

KRegex::cache_t KRegex::s_Cache;

//-----------------------------------------------------------------------------
void KRegex::LogExpError()
//-----------------------------------------------------------------------------
{
	KLog().warning("KRegex: {} regex '{}', here: '{}'",
	               m_Regex->error(),
	               m_Regex->pattern(),
	               m_Regex->error_arg());
}

//-----------------------------------------------------------------------------
/// converting constructor, takes string literals and strings
KRegex::KRegex(const KStringView& expression)
//-----------------------------------------------------------------------------
    : m_Regex(s_Cache.Get(re2::StringPiece(expression.data(), expression.size())))
{
	if (!OK())
	{
		LogExpError();
	}
}

//-----------------------------------------------------------------------------
bool KRegex::Matches(const KStringView& sStr, Groups& sGroups, size_t iMaxGroups)
//-----------------------------------------------------------------------------
{
	sGroups.clear();

	if (OK())
	{
		sGroups.resize(std::min(static_cast<size_t>(m_Regex->NumberOfCapturingGroups()+1), iMaxGroups));
		return m_Regex->Match(re2::StringPiece(sStr.data(), sStr.size()), 0, sStr.size(), re2::RE2::UNANCHORED, &sGroups[0], static_cast<int>(sGroups.size()));
	}

	return false;
}

//-----------------------------------------------------------------------------
bool KRegex::Matches(const KStringView& sStr, size_t& iStart, size_t& iSize)
//-----------------------------------------------------------------------------
{
	Groups sGroups;

	if (Matches(sStr, sGroups, 1) && !sGroups.empty() && sGroups[0].data() != nullptr)
	{
		iStart = static_cast<size_t>(sGroups[0].data() - sStr.data());
		iSize  = static_cast<size_t>(sGroups[0].size());
		return true;
	}
	else
	{
		iStart = 0;
		iSize  = 0;
		return false;
	}
}

//-----------------------------------------------------------------------------
bool KRegex::Matches(const KStringView& sStr)
//-----------------------------------------------------------------------------
{
	Groups sGroups;
	return Matches(sStr, sGroups, 0);
}

//-----------------------------------------------------------------------------
size_t KRegex::Replace(std::string& sStr, const KStringView& sReplaceWith, bool bReplaceAll)
//-----------------------------------------------------------------------------
{
	size_t iCount{0};

	if (OK())
	{
		if (bReplaceAll)
		{
			iCount = static_cast<size_t>(re2::RE2::GlobalReplace(&sStr, m_Regex.get(), re2::StringPiece(sReplaceWith.data(), sReplaceWith.size())));
		}
		else
		{
			if (re2::RE2::Replace(&sStr, m_Regex.get(), re2::StringPiece(sReplaceWith.data(), sReplaceWith.size())))
			{
				iCount = 1;
			}
		}
	}
	return iCount;
}



// ---------------------
// the static calls to the member functions:

//-----------------------------------------------------------------------------
bool KRegex::Matches(const KStringView& sStr, const KStringView& sRegex, Groups& sGroups, size_t iMaxGroups)
//-----------------------------------------------------------------------------
{
	KRegex regex(sRegex);
	return regex.Matches(sStr, sGroups, iMaxGroups);
}

//-----------------------------------------------------------------------------
bool KRegex::Matches(const KStringView& sStr, const KStringView& sRegex, size_t& iStart, size_t& iSize)
//-----------------------------------------------------------------------------
{
	KRegex regex(sRegex);
	return regex.Matches(sStr, iStart, iSize);
}

//-----------------------------------------------------------------------------
bool KRegex::Matches(const KStringView& sStr, const KStringView& sRegex)
//-----------------------------------------------------------------------------
{
	KRegex regex(sRegex);
	return regex.Matches(sStr);
}

//-----------------------------------------------------------------------------
size_t KRegex::Replace(std::string& sStr, const KStringView& sRegex, const KStringView& sReplaceWith, bool bReplaceAll)
//-----------------------------------------------------------------------------
{
	KRegex regex(sRegex);
	return regex.Replace(sStr, sReplaceWith, bReplaceAll);
}

} // of namespace dekaf2

