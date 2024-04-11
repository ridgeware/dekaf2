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

/// @file kregex.h
/// The Regular Expression encapsulation

#include <vector>
#include "kstring.h"
#include "kstringview.h"
#include "bits/kunique_deleter.h"

DEKAF2_NAMESPACE_BEGIN

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// KRegex constructs a (shared) regex object. Normally you do not use it
/// directly, but rather use its static functions that construct regex
/// objects on the fly. The KRegex class uses a static regex object cache
/// to make reuse of constructed regexes easy.
class DEKAF2_PUBLIC KRegex
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	
//----------
private:
//----------

//----------
public:
//----------

	//-----------------------------------------------------------------------------
	/// converting constructor, takes string literals and strings
	KRegex(KStringView expression);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// returns true if expression is empty
	bool empty() const
	//-----------------------------------------------------------------------------
	{
		return m_bIsEmpty;
	}

	using Group     = KStringView;
	using Groups    = std::vector<Group>;
	using size_type = KStringView::size_type;

	// member function interface
	//-----------------------------------------------------------------------------
	/// match a regular expression in sStr and return the overall match (group 0)
	DEKAF2_NODISCARD
	KStringView Match(KStringView sStr, size_type pos = 0) const;
	//-----------------------------------------------------------------------------

	// member function interface
	//-----------------------------------------------------------------------------
	/// match a regular expression in sStr and return all match groups, including the overall match in group 0.
	DEKAF2_NODISCARD
	Groups MatchGroups(KStringView sStr, size_type pos = 0) const;
	//-----------------------------------------------------------------------------

	// member function interface
	//-----------------------------------------------------------------------------
	/// match a regular expression in sStr and return true if found
	DEKAF2_NODISCARD
	bool Matches(KStringView sStr, size_type pos = 0) const;
	//-----------------------------------------------------------------------------

	// member function interface
	//-----------------------------------------------------------------------------
	/// replace a regular expression with new text. Sub groups can be addressed with \1 \2 etc. in the replacement text,
	/// where \0 is the overall match group
	size_t Replace(std::string& sStr, KStringView sReplaceWith, bool bReplaceAll = true) const;
	//-----------------------------------------------------------------------------

	// static interface
	//-----------------------------------------------------------------------------
	/// match a regular expression in sStr and return the overall match (group 0)
	DEKAF2_NODISCARD
	static KStringView Match(KStringView sStr, KStringView sRegex, size_type pos = 0);
	//-----------------------------------------------------------------------------

	// static interface
	//-----------------------------------------------------------------------------
	/// match a regular expression in sStr and return all match groups, including the overall match in group 0.
	DEKAF2_NODISCARD
	static Groups MatchGroups(KStringView sStr, KStringView sRegex, size_type pos = 0);
	//-----------------------------------------------------------------------------

	// static interface
	//-----------------------------------------------------------------------------
	/// match a regular expression in sStr and return true if found
	DEKAF2_NODISCARD
	static bool Matches(KStringView sStr, KStringView sRegex, size_type pos = 0);
	//-----------------------------------------------------------------------------

	// static interface
	//-----------------------------------------------------------------------------
	/// replace a regular expression with new text. Sub groups can be addressed with \1 \2 etc. in the replacement text,
	/// where \0 is the overall match group
	static size_t Replace(std::string& sStr, KStringView sRegex, KStringView sReplaceWith, bool bReplaceAll = true);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// returns regular expression string
	DEKAF2_NODISCARD
	const std::string& Pattern() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// returns an integer that increases with larger complexity of the regex search
	DEKAF2_NODISCARD
	size_t Cost() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// returns false if the regular expression could not be compiled
	DEKAF2_NODISCARD
	bool Good() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// returns error string if !Good()
	DEKAF2_NODISCARD
	const std::string& Error() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// returns erroneous argument if !Good()
	DEKAF2_NODISCARD
	const std::string& ErrorArg() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// set the maximum cache size for the regex expression cache (could also shrink)
	static void SetMaxCacheSize(size_t iCacheSize);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// clears the regex expression cache (why should you do that)
	static void ClearCache();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// returns the maximum cache size for the regex expression cache
	DEKAF2_NODISCARD
	static size_t GetMaxCacheSize();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// returns the current cache size for the regex expression cache
	DEKAF2_NODISCARD
	static size_t GetCacheSize();
	//-----------------------------------------------------------------------------

//----------
protected:
//----------

	//-----------------------------------------------------------------------------
	/// prints the details of a regex error into the debug log
	void LogExpError() const;
	//-----------------------------------------------------------------------------

//----------
private:
//----------

	KUniqueVoidPtr m_Regex;
	bool m_bIsEmpty;

}; // KRegex

/// Converts a wildcard expression for file matching into a regular expression.
/// * and ? are allowed expressions.
KString kWildCard2Regex(KStringView sInput);

DEKAF2_NAMESPACE_END

