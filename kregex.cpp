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

#ifdef DEKAF2_ADD_REGEX_FOR_KSTRING
#include <re2/../util/utf.h>

DEKAF2_NAMESPACE_BEGIN

namespace detail
{

/// The code in this detail namespace is a copy from
/// re2::RE2, with KString instead of std::string as
/// the string type.
///
/// It is needed when we use fbstring as the underlying
/// type for KString, as unfortunately RE2 is not working
/// with templates for the string type and we otherwise
/// would have to copy the string in and out of KString,
/// as there is no way to map fbstring to a std::string
/// reference.

// The code in namespace detail::kregex is:
//
// Copyright 2003-2009 The RE2 Authors.  All Rights Reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

namespace kregex
{

static const int kVecSize = 17;

//-----------------------------------------------------------------------------
/// copied from re2::RE2 for KString string type
bool Rewrite(KString& out,
             const re2::StringPiece &rewrite,
             const re2::StringPiece *vec,
             int veclen)
//-----------------------------------------------------------------------------
{
	for (const char *s = rewrite.data(), *end = s + rewrite.size(); s < end; s++)
	{
		if (*s != '\\')
		{
			out.push_back(*s);
			continue;
		}
		s++;
		int c = (s < end) ? *s : -1;
		if (isdigit(c))
		{
			int n = (c - '0');
			if (n >= veclen)
			{
				return false;
			}
			re2::StringPiece snip = vec[n];
			if (!snip.empty())
			{
				out.append(snip.data(), snip.size());
			}
		}
		else if (c == '\\')
		{
			out.push_back('\\');
		}
		else
		{
			return false;
		}
	}
	return true;
}

//-----------------------------------------------------------------------------
/// copied from re2::RE2 for KString string type
bool Replace(KString& str,
             const RE2& re,
             const re2::StringPiece& rewrite)
//-----------------------------------------------------------------------------
{
	re2::StringPiece vec[kVecSize];
	int nvec = 1 + re2::RE2::MaxSubmatch(rewrite);
	if (nvec > 1 + re.NumberOfCapturingGroups())
	{
		return false;
	}
	if (nvec > kVecSize)
	{
		return false;
	}
	if (!re.Match(re2::StringPiece(str.data(), str.size()),
	              0, str.size(),
	              re2::RE2::UNANCHORED, vec, nvec))
	{
		return false;
	}

	KString s;
	if (!Rewrite(s, rewrite, vec, nvec))
	{
		return false;
	}

	assert(vec[0].data() >= str.data());
	assert(vec[0].data() + vec[0].size() <= str.data() + str.size());

	str.replace(vec[0].data() - str.data(), vec[0].size(), s);

	return true;
}

//-----------------------------------------------------------------------------
/// copied from re2::RE2 for KString string type
int GlobalReplace(KString& str,
                  const RE2& re,
                  const re2::StringPiece& rewrite)
//-----------------------------------------------------------------------------
{
	re2::StringPiece vec[kVecSize];
	int nvec = 1 + re2::RE2::MaxSubmatch(rewrite);
	if (nvec > 1 + re.NumberOfCapturingGroups())
	{
		return false;
	}
	if (nvec > kVecSize)
	{
		return false;
	}

	const char* p = str.data();
	const char* ep = p + str.size();
	const char* lastend = nullptr;
	KString out;
	int count = 0;
	re2::StringPiece sv(str.data(), str.size());

	while (p <= ep)
	{
		if (!re.Match(sv,
		              static_cast<size_t>(p - sv.data()),
		              sv.size(), re2::RE2::UNANCHORED, vec, nvec))
		{
			break;
		}
		if (p < vec[0].data())
		{
			out.append(p, vec[0].data() - p);
		}
		if (vec[0].data() == lastend && vec[0].empty())
		{
			// Disallow empty match at end of last match: skip ahead.
			//
			// fullrune() takes int, not ptrdiff_t. However, it just looks
			// at the leading byte and treats any length >= 4 the same.
			if (re.options().encoding() == RE2::Options::EncodingUTF8 &&
				re2::fullrune(p, static_cast<int>(std::min(ptrdiff_t{4}, ep - p))))
			{
				// re is in UTF-8 mode and there is enough left of str
				// to allow us to advance by up to UTFmax bytes.
				re2::Rune r;
				int n = re2::chartorune(&r, p);
				// Some copies of chartorune have a bug that accepts
				// encodings of values in (10FFFF, 1FFFFF] as valid.
				if (r > re2::Runemax)
				{
					n = 1;
					r = re2::Runeerror;
				}
				if (!(n == 1 && r == re2::Runeerror))
				{  // no decoding error
					out.append(p, n);
					p += n;
					continue;
				}
			}
			// Most likely, re is in Latin-1 mode. If it is in UTF-8 mode,
			// we fell through from above and the GIGO principle applies.
			if (p < ep)
			{
				out.append(p, 1);
			}
			p++;
			continue;
		}
		Rewrite(out, rewrite, vec, nvec);
		p = vec[0].data() + vec[0].size();
		lastend = p;
		count++;
	}

	if (count == 0)
	{
		return 0;
	}

	if (p < ep)
	{
		out.append(p, ep - p);
	}
	using std::swap;
	swap(out, str);
	return count;
}

} // end of namespace kregex

} // end of namespace detail

DEKAF2_NAMESPACE_END

#endif

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

#ifdef DEKAF2_ADD_REGEX_FOR_KSTRING
//-----------------------------------------------------------------------------
size_t KRegex::Replace(KString& sStr, KStringView sReplaceWith, bool bReplaceAll) const
//-----------------------------------------------------------------------------
{
	size_t iCount{0};

	if (Good())
	{
		if (bReplaceAll)
		{
			iCount = static_cast<size_t>(detail::kregex::GlobalReplace(sStr, rget(m_Regex).get(), re2::StringPiece(sReplaceWith.data(), sReplaceWith.size())));
		}
		else
		{
			if (detail::kregex::Replace(sStr, rget(m_Regex).get(), re2::StringPiece(sReplaceWith.data(), sReplaceWith.size())))
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
#endif

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

#ifdef DEKAF2_ADD_REGEX_FOR_KSTRING
//-----------------------------------------------------------------------------
size_t KRegex::Replace(KString& sStr, KStringView sRegex, KStringView sReplaceWith, bool bReplaceAll)
//-----------------------------------------------------------------------------
{
	KRegex regex(sRegex);
	return regex.Replace(sStr, sReplaceWith, bReplaceAll);
}
#endif

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

