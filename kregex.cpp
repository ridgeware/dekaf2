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
#include "kcache.h"
#include "klog.h"
#include <re2/re2.h>

DEKAF2_NAMESPACE_BEGIN

namespace detail {
namespace kregex {

struct Loader
{
	// teach RE2 how to load from a KString(View)
	KSharedRef<re2::RE2, true> operator()(KStringView s)
	{
		return re2::StringPiece(s.data(), s.size());
	}
};

using cache_t = KSharedCache<KString, re2::RE2, Loader>;
using regex_t = cache_t::value_type;

} // end of namespace kregex
} // end of namespace detail

detail::kregex::cache_t s_Cache;

//-----------------------------------------------------------------------------
inline const detail::kregex::regex_t& rget(const KUniqueVoidPtr& p)
//-----------------------------------------------------------------------------
{
	return *static_cast<detail::kregex::regex_t*>(p.get());
}

//-----------------------------------------------------------------------------
auto kRegexDeleter = [](void* data)
//-----------------------------------------------------------------------------
{
	delete static_cast<detail::kregex::regex_t*>(data);
};

//-----------------------------------------------------------------------------
void KRegex::LogExpError() const
//-----------------------------------------------------------------------------
{
	kDebug(2,
		   "{} regex '{}', here: '{}'",
		   Error(),
	       Pattern(),
	       ErrorArg());
}

//-----------------------------------------------------------------------------
const std::string& KRegex::Pattern() const
//-----------------------------------------------------------------------------
{
	return rget(m_Regex)->pattern();
}

//-----------------------------------------------------------------------------
size_t KRegex::Cost() const
//-----------------------------------------------------------------------------
{
	return static_cast<size_t>(rget(m_Regex)->ProgramSize());
}

//-----------------------------------------------------------------------------
/// returns false if the regular expression could not be compiled
bool KRegex::Good() const
//-----------------------------------------------------------------------------
{
	return rget(m_Regex)->ok();
}

//-----------------------------------------------------------------------------
/// returns error string if !Good()
const std::string& KRegex::Error() const
//-----------------------------------------------------------------------------
{
	return rget(m_Regex)->error();
}

//-----------------------------------------------------------------------------
/// returns erroneous argument if !Good()
const std::string& KRegex::ErrorArg() const
//-----------------------------------------------------------------------------
{
	return rget(m_Regex)->error_arg();
}

//-----------------------------------------------------------------------------
/// converting constructor, takes string literals and strings
KRegex::KRegex(KStringView expression)
//-----------------------------------------------------------------------------
: m_Regex(new detail::kregex::regex_t(s_Cache.Get(expression)), kRegexDeleter)
, m_bIsEmpty(expression.empty())
{
	if (!Good())
	{
		LogExpError();
	}
}

using reGroup  = re2::StringPiece;
using reGroups = std::vector<reGroup>;

//-----------------------------------------------------------------------------
inline void re2groups(reGroups& inGroups, KRegex::Groups& outGroups)
//-----------------------------------------------------------------------------
{
	outGroups.clear();
	outGroups.reserve(inGroups.size());

	for (auto it : inGroups)
	{
		outGroups.push_back(KStringView(it.data(), it.size()));
	}
}

//-----------------------------------------------------------------------------
KRegex::Groups KRegex::MatchGroups(KStringView sStr, size_type pos) const
//-----------------------------------------------------------------------------
{
	Groups vGroups;

	if DEKAF2_LIKELY((Good()))
	{
		if (DEKAF2_UNLIKELY(m_bIsEmpty))
		{
			vGroups.push_back(KStringView(sStr.data(), 0));
		}
		else
		{
			reGroups resGroups;
			resGroups.resize(static_cast<size_t>(rget(m_Regex)->NumberOfCapturingGroups()+1));
			if (rget(m_Regex)->Match(re2::StringPiece(sStr.data(), sStr.size()), pos, sStr.size(), re2::RE2::UNANCHORED, &resGroups[0], static_cast<int>(resGroups.size())))
			{
				re2groups(resGroups, vGroups);
			}
		}
	}
	else
	{
		LogExpError();
	}

	return vGroups;
}

//-----------------------------------------------------------------------------
KStringView KRegex::Match(KStringView sStr, size_type pos) const
//-----------------------------------------------------------------------------
{
	KStringView sGroup;

	if (DEKAF2_LIKELY(Good()))
	{
		if (DEKAF2_UNLIKELY(m_bIsEmpty))
		{
			sGroup = KStringView(sStr.data(), 0);
		}
		else
		{
			reGroup resGroup;
			if (rget(m_Regex)->Match(re2::StringPiece(sStr.data(), sStr.size()), pos, sStr.size(), re2::RE2::UNANCHORED, &resGroup, 1))
			{
				sGroup.assign(resGroup.data(), resGroup.size());
			}
		}
	}
	else
	{
		LogExpError();
	}

	return sGroup;
}

//-----------------------------------------------------------------------------
bool KRegex::Matches(KStringView sStr, size_type pos) const
//-----------------------------------------------------------------------------
{
	if (DEKAF2_LIKELY(Good()))
	{
		if (m_bIsEmpty || rget(m_Regex)->Match(re2::StringPiece(sStr.data(), sStr.size()), pos, sStr.size(), re2::RE2::UNANCHORED, nullptr, 0))
		{
			return true;
		}
	}
	else
	{
		LogExpError();
	}

	return false;
}

//-----------------------------------------------------------------------------
size_t KRegex::Replace(std::string& sStr, KStringView sReplaceWith, bool bReplaceAll) const
//-----------------------------------------------------------------------------
{
	size_t iCount{0};

	if (Good())
	{
		if (bReplaceAll)
		{
			iCount = static_cast<size_t>(re2::RE2::GlobalReplace(&sStr, rget(m_Regex).get(), re2::StringPiece(sReplaceWith.data(), sReplaceWith.size())));
		}
		else
		{
			if (re2::RE2::Replace(&sStr, rget(m_Regex).get(), re2::StringPiece(sReplaceWith.data(), sReplaceWith.size())))
			{
				iCount = 1;
			}
		}
	}
	else
	{
		LogExpError();
	}

	return iCount;
}

// ---------------------
// the static calls to the member functions:

//-----------------------------------------------------------------------------
KRegex::Groups KRegex::MatchGroups(KStringView sStr, KStringView sRegex, size_type pos)
//-----------------------------------------------------------------------------
{
	KRegex regex(sRegex);
	return regex.MatchGroups(sStr, pos);
}

//-----------------------------------------------------------------------------
KStringView KRegex::Match(KStringView sStr, KStringView sRegex, size_type pos)
//-----------------------------------------------------------------------------
{
	KRegex regex(sRegex);
	return regex.Match(sStr, pos);
}

//-----------------------------------------------------------------------------
bool KRegex::Matches(KStringView sStr, KStringView sRegex, size_type pos)
//-----------------------------------------------------------------------------
{
	KRegex regex(sRegex);
	return regex.Matches(sStr, pos);
}

//-----------------------------------------------------------------------------
size_t KRegex::Replace(std::string& sStr, KStringView sRegex, KStringView sReplaceWith, bool bReplaceAll)
//-----------------------------------------------------------------------------
{
	KRegex regex(sRegex);
	return regex.Replace(sStr, sReplaceWith, bReplaceAll);
}

//-----------------------------------------------------------------------------
KString kWildCard2Regex(KStringView sInput)
//-----------------------------------------------------------------------------
{
	KString sRegex;
	sRegex.reserve(sInput.size() + 5);
	sRegex += '^';

	for (auto ch : sInput)
	{
		switch (ch)
		{
			case '.':
			case '<':
			case '>':
			case '$':
			case '^':
			case '(':
			case ')':
			case '{':
			case '}':
			case '|':
			case '+':
			case '[':
			case ']':
			case '\\':
				sRegex += '\\';
				sRegex += ch;
				break;

			case '*':
				sRegex += ".*";
				break;

			case '?':
				sRegex += '.';
				break;

			default:
				sRegex += ch;
				break;
		}
	}

	sRegex += '$';

	return sRegex;

} // kWildCard2Regex

//-----------------------------------------------------------------------------
void KRegex::SetMaxCacheSize(size_t iCacheSize)
//-----------------------------------------------------------------------------
{
	s_Cache.SetMaxSize(iCacheSize);
}

//-----------------------------------------------------------------------------
void KRegex::ClearCache()
//-----------------------------------------------------------------------------
{
	s_Cache.clear();
}

//-----------------------------------------------------------------------------
size_t KRegex::GetMaxCacheSize()
//-----------------------------------------------------------------------------
{
	return s_Cache.GetMaxSize();
}

//-----------------------------------------------------------------------------
size_t KRegex::GetCacheSize()
//-----------------------------------------------------------------------------
{
	return s_Cache.size();
}

static_assert(std::is_nothrow_move_constructible<KRegex>::value,
			  "KRegex is intended to be nothrow move constructible, but is not!");

DEKAF2_NAMESPACE_END

