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
#include <fmt/format.h>
#include "kstring.h"
#include "kstringview.h"
#include "kreader.h"

namespace dekaf2 {

using KJSON = nlohmann::basic_json<std::map, std::vector, dekaf2::KString >;

class KROW;

// ADL resolvers for KStringView and KStringViewZ (KString is the
// built-in json string type, and therefore does not need conversion)

inline void to_json(KJSON& j, const dekaf2::KStringView& s)
{
	j = KJSON::string_t(s);
}

inline void to_json(KJSON& j, const dekaf2::KStringViewZ& s)
{
	j = KJSON::string_t(s);
}

void to_json(KJSON& j, const KROW& row);
void from_json(const KJSON& j, KROW& row);

inline void from_json(const KJSON& j, dekaf2::KStringViewZ& s)
{
	s = j.get_ref<const KJSON::string_t&>();
}

inline void from_json(const KJSON& j, dekaf2::KStringView& s)
{
	s = j.get_ref<const KJSON::string_t&>();
}

inline bool operator==(KStringView left, const KJSON& right)
{
	return left == right.get_ref<const KJSON::string_t&>();
}

inline bool operator==(const KJSON& left, KStringView right)
{
	return operator==(right, left);
}

inline bool operator!=(KStringView left, const KJSON& right)
{
	return !operator==(left, right);
}

inline bool operator!=(const KJSON& left, KStringView right)
{
	return !operator==(right, left);
}

inline bool operator==(const KString& left, const KJSON& right)
{
	return left == right.get_ref<const KJSON::string_t&>();
}

inline bool operator==(const KJSON& left, const KString& right)
{
	return operator==(right, left);
}

inline bool operator!=(const KString& left, const KJSON& right)
{
	return !operator==(left, right);
}

inline bool operator!=(const KJSON& left, const KString& right)
{
	return !operator==(right, left);
}

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
namespace kjson
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	/// Parse a string, throws with KJSON::exception in case of error
	void Parse (KJSON& json, KStringView sJSON);

	/// Parse a string, returns error in sError if any (does not throw)
	bool Parse (KJSON& json, KStringView sJSON, KString& sError) noexcept;

	/// Parse a stream, throws with KJSON::exception in case of error
	void Parse (KJSON& json, KInStream& InStream);

	/// Parse a stream, returns error in sError if any (does not throw)
	bool Parse (KJSON& json, KInStream& InStream, KString& sError) noexcept;

	/// returns a string representation for the KJSON object, never throws
	KString Print (const KJSON& json) noexcept;

	/// Sets a JSON string from a KStringView, checks if the string was
	/// valid UTF8 or converts from (assumed) Latin1 to UTF8. That is
	/// needed to avoid throws from the KJSON serializer on invalid UTF8.
	/// Use this whenever you add string data coming from unreliable
	/// input.
	void SetStringFromUTF8orLatin1(KJSON& json, KStringView sInput);

	/// Returns a ref for a string key, does never throw. Returns empty ref
	/// for non-string values.
	const KString& GetStringRef(const KJSON& json, KStringView sKey) noexcept;

	/// Returns a value for a string key, does never throw. Prints non-string
	/// values into string representation.
	KString GetString(const KJSON& json, KStringView sKey) noexcept;

	/// returns a value for an object key, does never throw
	KJSON GetObject (const KJSON& json, KStringView sKey) noexcept;

	/// returns true if the key exists
	inline bool Exists(const KJSON& json, KStringView sKey) noexcept
	{
		auto it = json.find(sKey);
		return (it != json.end());
	}

	/// returns true if the key exists and contains an object
	inline bool IsObject(const KJSON& json, KStringView sKey) noexcept
	{
		auto it = json.find(sKey);
		return (it != json.end() && it->is_object());
	}

	/// returns true if the key exists and contains an array
	inline bool IsArray(const KJSON& json, KStringView sKey) noexcept
	{
		auto it = json.find(sKey);
		return (it != json.end() && it->is_array());
	}

	/// returns true if the key exists and contains a string
	inline bool IsString(const KJSON& json, KStringView sKey) noexcept
	{
		auto it = json.find(sKey);
		return (it != json.end() && it->is_string());
	}

	/// returns true if the key exists and contains an integer
	inline bool IsInteger(const KJSON& json, KStringView sKey) noexcept
	{
		auto it = json.find(sKey);
		return (it != json.end() && it->is_number_integer());
	}

	/// returns true if the key exists and contains a float
	inline bool IsFloat(const KJSON& json, KStringView sKey) noexcept
	{
		auto it = json.find(sKey);
		return (it != json.end() && it->is_number_float());
	}

	/// returns true if the key exists and contains null
	inline bool IsNull(const KJSON& json, KStringView sKey) noexcept
	{
		auto it = json.find(sKey);
		return (it != json.end() && it->is_null());
	}

	/// returns true if the key exists and contains a bool
	inline bool IsBoolean(const KJSON& json, KStringView sKey) noexcept
	{
		auto it = json.find(sKey);
		return (it != json.end() && it->is_boolean());
	}

	/// proper json string escaping
	void Escape (KStringView sInput, KString& sOutput);

	/// proper json string escaping
	KString Escape (KStringView sInput);

	/// returns true if object is a string array or an object and contains the given string
	bool Contains (const KJSON& json, KStringView sString) noexcept;

}; // end of namespace kjson

// lift a few of the static methods into dekaf2's namespace

using kjson::Parse;
using kjson::Print;
using kjson::GetString;
using kjson::GetObject;
using kjson::Contains;

} // end of namespace dekaf2
