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

#include <nlohmann/json.hpp>
#include "kstring.h"
#include "kstringview.h"

namespace dekaf2 {

using LJSON = nlohmann::json;

inline void to_json(LJSON& j, const KString& s)
{
	j = nlohmann::json{s.ToStdString()};
}

inline void from_json(const LJSON& j, KString& s)
{
	s = j.get<std::string>();
}

inline void to_json(LJSON& j, const KStringView& s)
{
	std::string s1(s.data(), s.size());
	j = nlohmann::json{s1};
}

inline void from_json(const LJSON& j, KStringView& s)
{
	s = j.get<std::string>();
}

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KJSON
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
//----------
public:
//----------
	bool        Parse     (KStringView sJSON);
	KString     GetString (const KString& sKey);
	KJSON       GetObject (const KString& sKey);
	KStringView GetLastError () { return m_sLastError; }

	void        clear     () { m_obj.clear(); }
	bool        empty     () const { return m_obj.empty(); }
	auto        begin     () { return m_obj.begin(); }
	auto        end       () { return m_obj.end(); }
	auto        cbegin    () const { return m_obj.cbegin(); }
	auto        cend      () const { return m_obj.cend(); }
	bool        FormError (const LJSON::exception& exc);
	const LJSON& Object   () const { return m_obj; }
	LJSON&      Object    () { return m_obj; }

	/// wrap the given string with double-quotes and escape it for legal json
	static KString EscWrap (KString sString);
	static KString EscWrap (KString sName, KString sValue, KStringView sPrefix="\n\t", KStringView sSuffix=",");

	/// do not wrap the given string with double-quotes if it is explicitly known to be Numeric
	static KString EscWrap (KString sName, int iNumber, KStringView sPrefix="\n\t", KStringView sSuffix=",");
	static KString EscWrapNumeric (KString sString, int iNumber, KStringView sPrefix="\n\t", KStringView sSuffix=",");
	static KString EscWrapNumeric (KString sName, KString sValue, KStringView sPrefix="\n\t", KStringView sSuffix=",");

//----------
private:
//----------
	void        ClearError() { m_sLastError.clear(); }

	LJSON       m_obj;
	KString     m_sLastError;

}; // KJSON


} // end of namespace dekaf2
