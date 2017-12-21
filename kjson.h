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

void to_json(nlohmann::json& j, const KString& s) {
    j = nlohmann::json{s};
}

void from_json(const nlohmann::json& j, KString& s) {
	s = j.get<std::string>();
}

void to_json(nlohmann::json& j, const KStringView& s) {
    j = nlohmann::json{s};
}

void from_json(const nlohmann::json& j, KStringView& s) {
	s = j.get<std::string>();
}

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KJSON : public nlohmann::json
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
//----------
public:
//----------
 	using self_type = KJSON;
	using base_type = nlohmann::json;

 	template<typename ...Args>
 	KJSON(Args&& ...args)
 	    : base_type(std::forward<Args>(args)...)
 	{
 	}
 
 	self_type& operator=(const self_type& other)
 	{
 		base_type::operator=(other);
 		return *this;
 	}
 
 	self_type& operator=(self_type&& other)
 	{
 		base_type::operator=(std::move(other));
 		return *this;
 	}

    bool        parse     (KStringView sJSON);
    bool        FormError (const base_type::exception exc);
    KStringView GetLastError ()   { return m_sLastError; }

	/// wrap the given string with double-quotes and escape it for legal json
	static KString EscWrap (KStringView sString);
	static KString EscWrap (KStringView sString1, KStringView sString2, KStringView sPrefix="\n\t", KStringView sSuffix=",");
	static KString EscWrap (KStringView sString, int iNumber, KStringView sPrefix="\n\t", KStringView sSuffix=",");

//----------
public:
//----------
    KString m_sLastError;

}; // KJSON

} // end of namespace dekaf2
