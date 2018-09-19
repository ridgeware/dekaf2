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
#include "krow.h"
#include "kstring.h"
#include "kstringview.h"
#include "kreader.h"

using KJSON = nlohmann::basic_json<std::map, std::vector, dekaf2::KString >;

namespace dekaf2 {

inline void to_json(KJSON& j, const dekaf2::KStringView& s)
{
	j = KJSON::string_t(s);
}

inline void to_json(KJSON& j, const dekaf2::KStringViewZ& s)
{
	j = KJSON::string_t(s);
}

inline void from_json(const KJSON& j, dekaf2::KStringViewZ& s)
{
	s = j.get<KJSON::string_t>();
}

inline void from_json(const KJSON& j, dekaf2::KStringView& s)
{
	s = j.get<KJSON::string_t>();
}

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
namespace kjson
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	void Parse (KJSON& json, KStringView sJSON);
	bool Parse (KJSON& json, KStringView sJSON, KString& sError);
	void Parse (KJSON& json, KInStream& InStream);
	bool Parse (KJSON& json, KInStream& InStream, KString& sError);
	KString GetString(const KJSON& json, KStringView sKey);
	KJSON GetObject (const KJSON& json, KStringView sKey);
	bool Add (KJSON& json, const KROW& row);

	inline
	bool Exists(const KJSON& json, KStringView Key)
	{
		auto it = json.find(Key);
		return (it != json.end());
	}

	inline
	bool IsObject(const KJSON& json, KStringView Key)
	{
		auto it = json.find(Key);
		return (it != json.end() && it->is_object());
	}

	inline
	bool IsArray(const KJSON& json, KStringView Key)
	{
		auto it = json.find(Key);
		return (it != json.end() && it->is_array());
	}

	inline
	bool IsString(const KJSON& json, KStringView Key)
	{
		auto it = json.find(Key);
		return (it != json.end() && it->is_string());
	}

	inline
	bool IsInteger(const KJSON& json, KStringView Key)
	{
		auto it = json.find(Key);
		return (it != json.end() && it->is_number_integer());
	}

	inline
	bool IsFloat(const KJSON& json, KStringView Key)
	{
		auto it = json.find(Key);
		return (it != json.end() && it->is_number_float());
	}

	inline
	bool IsNull(const KJSON& json, KStringView Key)
	{
		auto it = json.find(Key);
		return (it != json.end() && it->is_null());
	}

	inline
	bool IsBoolean(const KJSON& json, KStringView Key)
	{
		auto it = json.find(Key);
		return (it != json.end() && it->is_boolean());
	}

	/// proper json string escaping
	void Escape (KStringView sInput, KString& sOutput);
	KString Escape (KStringView sInput);

	/// wrap the given string with double-quotes and escape it for legal json
	KString EscWrap (KStringView sString);
	KString EscWrap (KStringView sName, KStringView sValue, KStringView sPrefix="\n\t", KStringView sSuffix=",");

	/// do not wrap the given string with double-quotes if it is explicitly known to be Numeric
	KString EscWrapNumeric (KStringView sName, KStringView sValue, KStringView sPrefix="\n\t", KStringView sSuffix=",");
	template<typename I, typename std::enable_if<!std::is_constructible<KStringView, I>::value, int>::type = 0>
	KString EscWrap (KStringView sName, I iValue, KStringView sPrefix="\n\t", KStringView sSuffix=",")
	{
		return EscWrapNumeric(sName, KString::to_string(iValue), sPrefix, sSuffix);
	}
	template<typename I, typename std::enable_if<!std::is_constructible<KStringView, I>::value, int>::type = 0>
	KString EscWrapNumeric (KStringView sName, I iValue, KStringView sPrefix="\n\t", KStringView sSuffix=",")
	{
		return EscWrap(sName, iValue, sPrefix, sSuffix);
	}

}; // end of namespace kjson

} // end of namespace dekaf2
