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

namespace dekaf2
{

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// KRegex constructs a (shared) regex object. Normally you do not use it
/// directly, but rather use its static functions that construct regex
/// objects on the fly. The KRegex class uses a static regex object cache
/// to make reuse of constructed regexes easy.
class KRegex
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

	using Group  = KStringView;
	using Groups = std::vector<Group>;

	// member function interface
	//-----------------------------------------------------------------------------
	/// match a regular expression in sStr
	bool Matches(KStringView sStr);
	//-----------------------------------------------------------------------------

	// member function interface
	//-----------------------------------------------------------------------------
	/// match a regular expression in sStr and return all match groups, including the overall match in group 0.
	bool Matches(KStringView sStr, Groups& sGroups);
	//-----------------------------------------------------------------------------

	// member function interface
	//-----------------------------------------------------------------------------
	/// match a regular expression in sStr and return the overall match (group 0) in iStart, iSize as in a substring definition
	bool Matches(KStringView sStr, size_t& iStart, size_t& iSize);
	//-----------------------------------------------------------------------------

	// member function interface
	//-----------------------------------------------------------------------------
	/// replace a regular expression with new text. Sub groups can be addressed with \1 \2 etc. in the replacement text
	size_t Replace(std::string& sStr, KStringView sReplaceWith, bool bReplaceAll = true);
	//-----------------------------------------------------------------------------

#ifdef DEKAF2_USE_FBSTRING_AS_KSTRING
	// member function interface
	//-----------------------------------------------------------------------------
	/// replace a regular expression with new text. Sub groups can be addressed with \1 \2 etc. in the replacement text
	size_t Replace(KString& sStr, KStringView sReplaceWith, bool bReplaceAll = true);
	//-----------------------------------------------------------------------------
#endif

	// static interface
	//-----------------------------------------------------------------------------
	/// match a regular expression in sStr
	static bool Matches(KStringView sStr, KStringView sRegex);
	//-----------------------------------------------------------------------------

	// static interface
	//-----------------------------------------------------------------------------
	/// match a regular expression in sStr and return all match groups, including the overall match in group 0.
	static bool Matches(KStringView sStr, KStringView sRegex, Groups& sGroups);
	//-----------------------------------------------------------------------------

	// static interface
	//-----------------------------------------------------------------------------
	/// match a regular expression in sStr and return the overall match (group 0) in iStart, iSize as in a substring definition
	static bool Matches(KStringView sStr, KStringView sRegex, size_t& iStart, size_t& iSize);
	//-----------------------------------------------------------------------------

	// static interface
	//-----------------------------------------------------------------------------
	/// replace a regular expression with new text. Sub groups can be addressed with \1 \2 etc. in the replacement text
	static size_t Replace(std::string& sStr, KStringView sRegex, KStringView sReplaceWith, bool bReplaceAll = true);
	//-----------------------------------------------------------------------------

#ifdef DEKAF2_USE_FBSTRING_AS_KSTRING
	// static interface
	//-----------------------------------------------------------------------------
	/// replace a regular expression with new text. Sub groups can be addressed with \1 \2 etc. in the replacement text
	static size_t Replace(KString& sStr, KStringView sRegex, KStringView sReplaceWith, bool bReplaceAll = true);
	//-----------------------------------------------------------------------------
#endif

	//-----------------------------------------------------------------------------
	/// returns regular expression string
	const std::string& Pattern() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// returns an integer that increases with larger complexity of the regex search
	size_t Cost() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// returns false if the regular expression could not be compiled
	bool Good() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// returns error string if !Good()
	const std::string& Error() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// returns erroneous argument if !Good()
	const std::string& ErrorArg() const;
	//-----------------------------------------------------------------------------

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

	void* m_Regex;

}; // KRegex

/// Converts a wildcard expression for file matching into a regular expression.
/// * and ? are allowed expressions.
KString kWildCard2Regex(KStringView sInput);

} // of namespace dekaf2

