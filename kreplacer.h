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
/// adds a class that replaces variables

#include <map>
#include "kstringview.h"
#include "kstring.h"

namespace dekaf2 {

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Replace variables with values
class KReplacer
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	KReplacer() = default;

	KReplacer(KStringView sLeadIn, KStringView sLeadOut, bool bRemoveAllVariables = false)
	: m_sLeadIn(sLeadIn)
	, m_sLeadOut(sLeadOut)
	, m_bRemoveAllVariables((sLeadIn.empty() || sLeadOut.empty()) ? false : bRemoveAllVariables)
	{}

	bool empty() const;
	size_t size() const;
	void clear();
	bool insert(KStringView sSearch, KStringView sReplace);

	template<class MapType>
	void insert(const MapType& map)
	{
		for (const auto& it : map)
		{
			insert(it.first, it.second);
		}
	}

	/// Replaces all variables in sIn with their values, returns new string
	KString Replace(KStringView sIn) const;

	/// Replaces all variables in sIn with their values
	void ReplaceInPlace(KString& sIn) const;

//----------
private:
//----------

	using RepMap = std::map<KString, KString>;

	KString m_sLeadIn;
	KString m_sLeadOut;
	RepMap m_RepMap;
	bool m_bRemoveAllVariables;

}; // KReplacer

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Replace variables with values
class KVariables : public KReplacer
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	KVariables(bool bRemoveAllVariables = false)
	: KReplacer("{{", "}}", bRemoveAllVariables)
	{}

}; // KVariables

} // of namespace dekaf2

