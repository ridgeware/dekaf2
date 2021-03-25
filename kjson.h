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

#include "klog.h"
#define JSON_THROW_USER(exception) { kJSONTrace(); throw exception; }
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
	/// @param json the json output
	/// @param sJSON the input string to parse
	void Parse (KJSON& json, KStringView sJSON);

	/// Parse a string, returns error in sError if any, never throws
	/// @param json the json output
	/// @param sJSON the input string to parse
	/// @param sError the error string
	bool Parse (KJSON& json, KStringView sJSON, KString& sError) noexcept;

	/// Parse a stream, throws with KJSON::exception in case of error
	/// @param json the json output
	/// @param InStream the input stream to parse
	void Parse (KJSON& json, KInStream& InStream);

	/// Parse a stream, returns error in sError if any, never throws
	/// @param json the json output
	/// @param InStream the input stream to parse
	/// @param sError the error string
	bool Parse (KJSON& json, KInStream& InStream, KString& sError) noexcept;

	/// Returned parsed string as a JSON object
	/// if invalid, just return a null object on failure
	/// does NOT throw
	KJSON Parse (KStringView sJSON) noexcept;

	/// Returned parsed string as a JSON object
	/// if invalid, throw KError
	KJSON ParseOrThrow (KStringView sJSON);

	/// returns a string representation for the KJSON object, never throws
	/// @param json the json input
	KString Print (const KJSON& json) noexcept;

	/// Sets a JSON string from a KStringView, checks if the string was
	/// valid UTF8 or converts from (assumed) Latin1 to UTF8. That is
	/// needed to avoid throws from the KJSON serializer on invalid UTF8.
	/// Use this whenever you add string data coming from unreliable
	/// input.
	/// @param json the json output
	/// @param sInput the string input
	void SetStringFromUTF8orLatin1(KJSON& json, KStringView sInput);

	/// Returns a string ref for a key, never throws. Returns empty ref
	/// for non-string values.
	/// @param json the json input
	/// @param sKey the key to search for
	const KString& GetStringRef(const KJSON& json, KStringView sKey) noexcept;

	/// Returns a string value for a key, never throws. Prints non-string
	/// values into string representation.
	/// @param json the json input
	/// @param sKey the key to search for
	KString GetString(const KJSON& json, KStringView sKey) noexcept;

	/// Returns a uint value for a key, never throws. Returns 0 if value was
	/// neither an integer nor a string representation of an integer.
	/// @param json the json input
	/// @param sKey the key to search for
	uint64_t GetUInt(const KJSON& json, KStringView sKey) noexcept;

	/// Returns an int value for a key, never throws. Returns 0 if value was
	/// neither an integer nor a string representation of an integer.
	/// @param json the json input
	/// @param sKey the key to search for
	int64_t GetInt(const KJSON& json, KStringView sKey) noexcept;

	/// Returns a bool value for a key, never throws. Returns 0 if value was
	/// neither an integer nor a string representation of an integer.
	/// @param json the json input
	/// @param sKey the key to search for
	bool GetBool(const KJSON& json, KStringView sKey) noexcept;

	/// Returns an object ref for a key, never throws. Returns empty ref
	/// for non-object values.
	/// @param json the json input
	/// @param sKey the key to search for
	const KJSON& GetObjectRef (const KJSON& json, KStringView sKey) noexcept;

	/// returns an object value for a key, never throws. Returns empty
	/// object for non-object values.
	/// @param json the json input
	/// @param sKey the key to search for
	const KJSON& GetObject (const KJSON& json, KStringView sKey) noexcept;

	/// returns an array value for a key, never throws. Returns empty
	/// array for non-array values.
	/// @param json the json input
	/// @param sKey the key to search for
	const KJSON& GetArray (const KJSON& json, KStringView sKey) noexcept;

	/// returns true if the key exists, never throws
	/// @param json the json input
	/// @param sKey the key to search for
	inline bool Exists(const KJSON& json, KStringView sKey) noexcept
	{
		auto it = json.find(sKey);
		return (it != json.end());
	}

	/// returns true if the key exists and contains an object, never throws
	/// @param json the json input
	/// @param sKey the key to search for
	inline bool IsObject(const KJSON& json, KStringView sKey) noexcept
	{
		auto it = json.find(sKey);
		return (it != json.end() && it->is_object());
	}

	/// returns true if the key exists and contains an array, never throws
	/// @param json the json input
	/// @param sKey the key to search for
	inline bool IsArray(const KJSON& json, KStringView sKey) noexcept
	{
		auto it = json.find(sKey);
		return (it != json.end() && it->is_array());
	}

	/// returns true if the key exists and contains a string, never throws
	/// @param json the json input
	/// @param sKey the key to search for
	inline bool IsString(const KJSON& json, KStringView sKey) noexcept
	{
		auto it = json.find(sKey);
		return (it != json.end() && it->is_string());
	}

	/// returns true if the key exists and contains an integer, never throws
	/// @param json the json input
	/// @param sKey the key to search for
	inline bool IsInteger(const KJSON& json, KStringView sKey) noexcept
	{
		auto it = json.find(sKey);
		return (it != json.end() && it->is_number_integer());
	}

	/// returns true if the key exists and contains a float, never throws
	/// @param json the json input
	/// @param sKey the key to search for
	inline bool IsFloat(const KJSON& json, KStringView sKey) noexcept
	{
		auto it = json.find(sKey);
		return (it != json.end() && it->is_number_float());
	}

	/// returns true if the key exists and contains null, never throws
	/// @param json the json input
	/// @param sKey the key to search for
	inline bool IsNull(const KJSON& json, KStringView sKey) noexcept
	{
		auto it = json.find(sKey);
		return (it != json.end() && it->is_null());
	}

	/// returns true if the key exists and contains a bool, never throws
	/// @param json the json input
	/// @param sKey the key to search for
	inline bool IsBoolean(const KJSON& json, KStringView sKey) noexcept
	{
		auto it = json.find(sKey);
		return (it != json.end() && it->is_boolean());
	}

	/// adds the given integer to the given key, creates key if it does not yet exist
	inline void Increment(KJSON& json, KStringView sKey, uint64_t iAddMe=1) noexcept
	{
		auto it = json.find(sKey);
		if (DEKAF2_UNLIKELY((it == json.end()) || !it->is_number()))
		{
			json[sKey] = 0UL;
			it = json.find(sKey);
		}
		auto iValue = it.value().get<uint64_t>();
		it.value()  = iValue + iAddMe;
	}

	/// subtracts the given integer from the given key, creates key if it does not yet exist
	inline void Decrement(KJSON& json, KStringView sKey, uint64_t iSubtractMe=1) noexcept
	{
		auto it = json.find(sKey);
		if (DEKAF2_UNLIKELY((it == json.end()) || !it->is_number()))
		{
			json[sKey] = 0UL;
			it = json.find(sKey);
		}
		auto iValue = it.value().get<uint64_t>();
		it.value() = iValue - iSubtractMe;
	}

	/// proper json string escaping
	/// @param sInput the unescaped input string
	/// @param sOutput the escaped output string
	void Escape (KStringView sInput, KString& sOutput);

	/// proper json string escaping
	/// @param sInput the unescaped input string
	/// @returns the escaped output string
	KString Escape (KStringView sInput);

	/// returns true if object is a string array or an object and contains the given string, never throws
	bool Contains (const KJSON& json, KStringView sString) noexcept;

	/// RecursiveMatchValue (json, m_sSearchX);
	bool RecursiveMatchValue (const KJSON& json, KStringView sSearch);

}; // end of namespace kjson

// lift a few of the static methods into dekaf2's namespace

using kjson::Parse;
using kjson::Print;
using kjson::GetStringRef;
using kjson::GetString;
using kjson::GetUInt;
using kjson::GetInt;
using kjson::GetBool;
using kjson::GetArray;
using kjson::GetObjectRef;
using kjson::GetObject;
using kjson::Contains;
using kjson::Increment;
using kjson::Decrement;

} // end of namespace dekaf2
