/*
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
*/

#pragma once

/// @file kreplacer.h
/// adds a class that replaces variables in a string

#include "kstringview.h"
#include "kstring.h"
#include "kjson.h"
#include "krow.h"
#include <map>

namespace dekaf2 {

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Replace variables (aka tokens) with values, with configurable token-prefix and token-suffix
/// sequences like "{{" "}}"
class DEKAF2_PUBLIC KReplacer
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	using RepMap = std::map<KString, KString>;
	using const_iterator = RepMap::const_iterator;

	/// make sure we have all 5 special class methods
	KReplacer() = default;

	/// ctor: sTokenPrefix and sTokenSuffix form the frame for a replaceable token like "{{"
	/// "}}", bRemoveUnusedTokens if true removes variables which had not been found
	/// in the replace list
	KReplacer(KStringView sTokenPrefix, KStringView sTokenSuffix, bool bRemoveUnusedTokens = false)
	: m_sTokenPrefix(sTokenPrefix)
	, m_sTokenSuffix(sTokenSuffix)
	, m_bRemoveUnusedTokens((sTokenPrefix.empty() || sTokenSuffix.empty()) ? false : bRemoveUnusedTokens)
	{}

	/// do we have a replace list?
	bool empty() const;

	/// how many variables in the replace list?
	size_t size() const;

	/// clear replace list
	void clear();

	/// get begin iterator on first variable in the replace list
	const_iterator begin() const { return m_RepMap.begin(); }
	/// get end iterator after last variable in the replace list
 	const_iterator end() const { return m_RepMap.end(); }

	/// find variable in the replace list
	const_iterator find(KStringView sKey) const { return m_RepMap.find(sKey); }

	/// add a search and replace value to the replace list (without token-prefix/token-suffix),
	/// replaces existing value
	bool insert(KStringView sSearch, KStringView sReplace);

	/// remove a replacement from the replace list
	bool erase(KStringView sSearch);

	/// returns true if all unfound variables should be removed from the input text
	bool GetRemoveUnusedTokens() const { return m_bRemoveUnusedTokens; }

	/// inserts all key/value pairs from a map type into the replace list
	template<class MapType>
	void insert(const MapType& map)
	{
		for (const auto& it : map)
		{
			insert(it.first, it.second);
		}
	}

	/// inserts all key/value pairs from a map type into the replace list
	template<class MapType>
	KReplacer& operator+=(const MapType& map)
	{
		insert(map);
		return *this;
	}

	/// inserts (copies) another replace list into this
	void insert(const KReplacer& other);

	/// makes {{tokens}} from all json keys and assigns their values to the replacements.
	/// returns the number of tokens created.
	/// ignores any json objects that are not simple key/value pairs.
	uint16_t AddTokens (const KJSON& object, bool bFormNumbers=true);

	/// makes {{tokens}} from all database columns and assigns their values to the replacements.
	/// returns the number of tokens created.
	uint16_t AddTokensFromRow (const KROW& row, bool bFormNumbers=true);

	/// inserts (copies) another replace list into this
	KReplacer& operator+=(const KReplacer& other)
	{
		insert(other);
		return *this;
	}

	/// Note: add a token like {{DEBUG}} to replacement map which will be
	/// built and replaced to have all the current tokens in list form.
	void AddDebugToken (KStringView sTokenName="DEBUG");

	/// Replaces all variables (tokens) in sIn with their values, returns new string
	KString Replace(KStringView sIn) const;

	/// Replaces all variables in sIn with their values, modifying the string in place
	void ReplaceInPlace(KStringRef& sIn) const;

	/// convert tokens to json keys
	KJSON to_json ();

	/// dump all tokens (for debugging)
	KString dump (const int iIndent=1, const char indent_char='\t');

	/// helper function for SVGs and HTMLs that have messed up DEKAF {{tokens}}
	/// where pieces of the token get extra formatting.  so it cleans up this:
	///    {{to<span>k</span><span>e</span>n}}
	/// to this:
	///    {{token}}
	static KString& CleanupTokens (KString& sIn);

//----------
private:
//----------

	KString m_sTokenPrefix;
	KString m_sTokenSuffix;
	RepMap  m_RepMap;
	bool    m_bRemoveUnusedTokens;

}; // KReplacer

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Replace variables with values, using "{{" and "}}" as the token-prefix/out
class DEKAF2_PUBLIC KVariables : public KReplacer
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	/// ctor, bRemoveUnusedTokens if true removes variables which had not been found
	/// in the replace list
	KVariables(bool bRemoveUnusedTokens = false)
	: KReplacer("{{", "}}", bRemoveUnusedTokens)
	{}

}; // KVariables

} // of namespace dekaf2

