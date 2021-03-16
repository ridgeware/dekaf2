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

#include <map>
#include "kstringview.h"
#include "kstring.h"
#include "kassociative.h"

namespace dekaf2 {

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Replace variables with values, with configurable lead-in and lead-out
/// sequences like "{{" "}}"
class KReplacer
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	using RepMap = std::map<KString, KString>;
	using const_iterator = RepMap::const_iterator;

	/// make sure we have all 5 special class methods
	KReplacer() = default;

	/// ctor: sLeadIn and sLeadOut form the frame for a replaceable variable like "{{"
	/// "}}", bRemoveAllVariables if true removes variables which had not been found
	/// in the replace list
	KReplacer(KStringView sLeadIn, KStringView sLeadOut, bool bRemoveAllVariables = false)
	: m_sLeadIn(sLeadIn)
	, m_sLeadOut(sLeadOut)
	, m_bRemoveAllVariables((sLeadIn.empty() || sLeadOut.empty()) ? false : bRemoveAllVariables)
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

	/// add a search and replace value to the replace list (without lead-in/lead-out),
	/// replaces existing value
	bool insert(KStringView sSearch, KStringView sReplace);

	/// remove a replacement from the replace list
	bool erase(KStringView sSearch);

	/// returns true if all unfound variables should be removed from the input text
	bool GetRemoveAllVariables() const { return m_bRemoveAllVariables; }

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

	/// inserts (copies) another replace list into this
	KReplacer& operator+=(const KReplacer& other)
	{
		insert(other);
		return *this;
	}

	/// Replaces all variables in sIn with their values, returns new string
	KString Replace(KStringView sIn) const;

	/// Replaces all variables in sIn with their values
	void ReplaceInPlace(KString& sIn) const;

//----------
private:
//----------

	KString m_sLeadIn;
	KString m_sLeadOut;
	RepMap m_RepMap;
	bool m_bRemoveAllVariables;

}; // KReplacer

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Replace variables with values, using "{{" and "}}" as the lead-in/out
class KVariables : public KReplacer
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	/// ctor, bRemoveAllVariables if true removes variables which had not been found
	/// in the replace list
	KVariables(bool bRemoveAllVariables = false)
	: KReplacer("{{", "}}", bRemoveAllVariables)
	{}

}; // KVariables

} // of namespace dekaf2

