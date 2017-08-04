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

#pragma once

#include "kcppcompat.h"
#include <re2/re2.h>
#include <vector>
#include "kcache.h"
#include "kstring.h"
#include "khash.h"

namespace std
{
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	// add a hash function for re2::StringPiece
	template<>
	struct hash<re2::StringPiece>
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		typedef re2::StringPiece argument_type;
		typedef std::size_t result_type;
		result_type operator()(const argument_type& s) const
		{
#ifdef DEKAF2_HAS_CPP_17
			return std::hash<dekaf2::KStringView>{}({s.data(), s.size()});
#else
			return dekaf2::hash_bytes_FNV(s.data(), s.size());
#endif
		}
	};

}

namespace dekaf2
{

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// KRegex constructs a (shared) regex object. Normally you do not use it
/// directly, but rather use it's static functions that construct regex
/// objects on the fly. The KRegex class uses a static regex object cache
/// to make reuse of constructed regexes easy.
class KRegex
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
//----------
private:
//----------
	using cache_t = KSharedCacheTemplate<re2::StringPiece, re2::RE2>;
	using regex_t = cache_t::value_type;

//----------
public:
//----------

	//-----------------------------------------------------------------------------
	/// copy constructor
	KRegex(const KRegex& other)
	//-----------------------------------------------------------------------------
	    : m_Regex(other.m_Regex)
	{
	}

	//-----------------------------------------------------------------------------
	/// move constructor
	KRegex(KRegex&& other)
	//-----------------------------------------------------------------------------
	    : m_Regex(std::move(other.m_Regex))
	{
	}

	//-----------------------------------------------------------------------------
	/// converting constructor, takes string literals and strings
	KRegex(const KStringView& expression);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// assignment operator
	KRegex& operator=(const KRegex& other)
	//-----------------------------------------------------------------------------
	{
		m_Regex = other.m_Regex;
		return *this;
	}

	//-----------------------------------------------------------------------------
	/// move operator
	KRegex& operator=(KRegex&& other)
	//-----------------------------------------------------------------------------
	{
		m_Regex = std::move(other.m_Regex);
		return *this;
	}

	using Group  = re2::StringPiece;
	using Groups = std::vector<Group>;

	// member function interface
	//-----------------------------------------------------------------------------
	/// match a regular expression in sStr
	bool Matches(const KStringView& sStr);
	//-----------------------------------------------------------------------------

	// member function interface
	//-----------------------------------------------------------------------------
	/// match a regular expression in sStr and return all match groups, including the overall match in group 0.
	bool Matches(const KStringView& sStr, Groups& sGroups, size_t iMaxGroups = static_cast<size_t>(~0L));
	//-----------------------------------------------------------------------------

	// member function interface
	//-----------------------------------------------------------------------------
	/// match a regular expression in sStr and return the overall match (group 0) in iStart, iSize as in a substring definition
	bool Matches(const KStringView& sStr, size_t& iStart, size_t& iSize);
	//-----------------------------------------------------------------------------

	// member function interface
	//-----------------------------------------------------------------------------
	/// replace a regular expression with new text. Sub groups can be addressed with \1 \2 etc. in the replacement text
	size_t Replace(std::string& sStr, const KStringView& sReplaceWith, bool bReplaceAll = true);
	//-----------------------------------------------------------------------------

	// member function interface
	//-----------------------------------------------------------------------------
	/// replace a regular expression with new text. Sub groups can be addressed with \1 \2 etc. in the replacement text
	inline size_t Replace(KString& sStr, const KStringView& sReplaceWith, bool bReplaceAll = true)
	//-----------------------------------------------------------------------------
	{
		return Replace(sStr.s(), sReplaceWith, bReplaceAll);
	}

	// static interface
	//-----------------------------------------------------------------------------
	/// match a regular expression in sStr
	static bool Matches(const KStringView& sStr, const KStringView& sRegex);
	//-----------------------------------------------------------------------------

	// static interface
	//-----------------------------------------------------------------------------
	/// match a regular expression in sStr and return all match groups, including the overall match in group 0.
	static bool Matches(const KStringView& sStr, const KStringView& sRegex, Groups& sGroups, size_t iMaxGroups = static_cast<size_t>(~0L));
	//-----------------------------------------------------------------------------

	// static interface
	//-----------------------------------------------------------------------------
	/// match a regular expression in sStr and return the overall match (group 0) in iStart, iSize as in a substring definition
	static bool Matches(const KStringView& sStr, const KStringView& sRegex, size_t& iStart, size_t& iSize);
	//-----------------------------------------------------------------------------

	// static interface
	//-----------------------------------------------------------------------------
	/// replace a regular expression with new text. Sub groups can be addressed with \1 \2 etc. in the replacement text
	static size_t Replace(std::string& sStr, const KStringView& sRegex, const KStringView& sReplaceWith, bool bReplaceAll = true);
	//-----------------------------------------------------------------------------

	// static interface
	//-----------------------------------------------------------------------------
	/// replace a regular expression with new text. Sub groups can be addressed with \1 \2 etc. in the replacement text
	inline static size_t Replace(KString& sStr, const KStringView& sRegex, const KStringView& sReplaceWith, bool bReplaceAll = true)
	//-----------------------------------------------------------------------------
	{
		return Replace(sStr.s(), sRegex, sReplaceWith, bReplaceAll);
	}

	//-----------------------------------------------------------------------------
	/// returns regular expression string
	inline const std::string& Pattern() const
	//-----------------------------------------------------------------------------
	{
		return m_Regex->pattern();
	}

	//-----------------------------------------------------------------------------
	/// returns an integer that increases with larger complexity of the regex search
	inline size_t Cost() const
	//-----------------------------------------------------------------------------
	{
		return static_cast<size_t>(m_Regex->ProgramSize());
	}

	//-----------------------------------------------------------------------------
	/// returns false if the regular expression could not be compiled
	inline bool OK() const
	//-----------------------------------------------------------------------------
	{
		return m_Regex->ok();
	}

	//-----------------------------------------------------------------------------
	/// returns error string if !OK()
	inline const std::string& Error() const
	//-----------------------------------------------------------------------------
	{
		return m_Regex->error();
	}

	//-----------------------------------------------------------------------------
	/// returns erroneous argument if !OK()
	inline const std::string& ErrorArg() const
	//-----------------------------------------------------------------------------
	{
		return m_Regex->error_arg();
	}

//----------
protected:
//----------
	//-----------------------------------------------------------------------------
	/// prints the details of a regex error into the debug log
	void LogExpError();
	//-----------------------------------------------------------------------------

//----------
private:
//----------
	static cache_t s_Cache;

	regex_t m_Regex;

}; // KRegex

} // of namespace dekaf2

