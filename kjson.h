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

#include "kconfiguration.h"
#include "klog.h"
#define JSON_THROW_USER(exception) { kJSONTrace(); throw exception; }
#include "kcrashexit.h"
#define JSON_ASSERT(bMustBeTrue) { dekaf2::kAssert(bMustBeTrue,"crash in KJSON"); }
#if defined(DEKAF2_IS_DEBUG_BUILD) && !defined(DEKAF2_USE_PRECOMPILED_HEADERS)
	// add exact location to json exceptions in debug mode - we switch them off
	// in release mode because they come with a modest cost
	// This works starting with nlohmann::json v3.10.0
	// Unfortunately NDEBUG is set when precompiling headers - do not use
	// this with PCH, the symbols will not match when linking from external code
	#define JSON_DIAGNOSTICS 1
#endif
#include <nlohmann/json.hpp>
#include <fmt/format.h>
#include "kstring.h"
#include "kstringview.h"
#include "kreader.h"

namespace dekaf2 {

using KJSON = nlohmann::basic_json<std::map, std::vector, dekaf2::KString>;

class KROW;

// ADL resolvers

void to_json  (KJSON& j, const KROW& row);
void from_json(const KJSON& j, KROW& row);

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
namespace kjson
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	/// Parse a string, throws with KJSON::exception in case of error
	/// @param json the json output
	/// @param sJSON the input string to parse
	DEKAF2_PUBLIC
	void Parse (KJSON& json, KStringView sJSON);

	/// Parse a string, returns error in sError if any, never throws
	/// @param json the json output
	/// @param sJSON the input string to parse
	/// @param sError the error string
	DEKAF2_PUBLIC
	bool Parse (KJSON& json, KStringView sJSON, KStringRef& sError) noexcept;

	/// Parse a stream, throws with KJSON::exception in case of error
	/// @param json the json output
	/// @param InStream the input stream to parse
	DEKAF2_PUBLIC
	void Parse (KJSON& json, KInStream& InStream);

	/// Parse a stream, returns error in sError if any, never throws
	/// @param json the json output
	/// @param InStream the input stream to parse
	/// @param sError the error string
	DEKAF2_PUBLIC
	bool Parse (KJSON& json, KInStream& InStream, KStringRef& sError) noexcept;

	/// Returned parsed string as a JSON object
	/// if invalid, just return a null object on failure
	/// does NOT throw
	DEKAF2_PUBLIC
	KJSON Parse (KStringView sJSON) noexcept;

	/// Returned parsed string as a JSON object
	/// if invalid, throw KError
	DEKAF2_PUBLIC
	KJSON ParseOrThrow (KStringView sJSON);

	/// returns a string representation for the KJSON object, never throws
	/// @param json the json input
	DEKAF2_PUBLIC
	KString Print (const KJSON& json) noexcept;

	/// Sets a JSON string from a KStringView, checks if the string was
	/// valid UTF8 or converts from (assumed) Latin1 to UTF8. That is
	/// needed to avoid throws from the KJSON serializer on invalid UTF8.
	/// Use this whenever you add string data coming from unreliable
	/// input.
	/// @param json the json output
	/// @param sInput the string input
	DEKAF2_PUBLIC
	void SetStringFromUTF8orLatin1(KJSON& json, KStringView sInput);

	/// Returns a string ref for a key, never throws. Returns empty ref
	/// for non-string values.
	/// @param json the json input
	/// @param sKey the key to search for
	DEKAF2_PUBLIC
	const KString& GetStringRef(const KJSON& json, KStringView sKey) noexcept;

	/// Returns a string value for a key, never throws. Prints non-string
	/// values into string representation.
	/// @param json the json input
	/// @param sKey the key to search for
	DEKAF2_PUBLIC
	KString GetString(const KJSON& json, KStringView sKey) noexcept;

	/// Returns a uint value for a key, never throws. Returns 0 if value was
	/// neither an integer nor a string representation of an integer.
	/// @param json the json input
	/// @param sKey the key to search for
	DEKAF2_PUBLIC
	uint64_t GetUInt(const KJSON& json, KStringView sKey) noexcept;

	/// Returns an int value for a key, never throws. Returns 0 if value was
	/// neither an integer nor a string representation of an integer.
	/// @param json the json input
	/// @param sKey the key to search for
	DEKAF2_PUBLIC
	int64_t GetInt(const KJSON& json, KStringView sKey) noexcept;

	/// Returns a bool value for a key, never throws. Returns 0 if value was
	/// neither an integer nor a string representation of an integer.
	/// @param json the json input
	/// @param sKey the key to search for
	DEKAF2_PUBLIC
	bool GetBool(const KJSON& json, KStringView sKey) noexcept;

	/// Returns a const object ref for a key, never throws. Returns empty ref
	/// for non-object values.
	/// @param json the json input
	/// @param sKey the key to search for
	DEKAF2_PUBLIC
	const KJSON& GetObjectRef (const KJSON& json, KStringView sKey) noexcept;

	/// returns a const object ref for a key, never throws. Returns empty
	/// object for non-object values.
	/// @param json the json input
	/// @param sKey the key to search for
	DEKAF2_PUBLIC
	const KJSON& GetObject (const KJSON& json, KStringView sKey) noexcept;

	/// returns an array value for a key, never throws. Returns empty
	/// array for non-array values.
	/// @param json the json input
	/// @param sKey the key to search for
	DEKAF2_PUBLIC
	const KJSON& GetArray (const KJSON& json, KStringView sKey) noexcept;

	/// returns true if the key exists, never throws
	/// @param json the json input
	/// @param sKey the key to search for
	DEKAF2_PUBLIC
	bool Exists (const KJSON& json, KStringView sKey) noexcept;

	/// returns true if the key exists and contains an object, never throws
	/// @param json the json input
	/// @param sKey the key to search for
	DEKAF2_PUBLIC
	bool IsObject (const KJSON& json, KStringView sKey) noexcept;

	/// returns true if the key exists and contains an array, never throws
	/// @param json the json input
	/// @param sKey the key to search for
	DEKAF2_PUBLIC
	bool IsArray (const KJSON& json, KStringView sKey) noexcept;

	/// returns true if the key exists and contains a string, never throws
	/// @param json the json input
	/// @param sKey the key to search for
	DEKAF2_PUBLIC
	bool IsString (const KJSON& json, KStringView sKey) noexcept;

	/// returns true if the key exists and contains an integer, never throws
	/// @param json the json input
	/// @param sKey the key to search for
	DEKAF2_PUBLIC
	bool IsInteger (const KJSON& json, KStringView sKey) noexcept;

	/// returns true if the key exists and contains a float, never throws
	/// @param json the json input
	/// @param sKey the key to search for
	DEKAF2_PUBLIC
	bool IsFloat (const KJSON& json, KStringView sKey) noexcept;

	/// returns true if the key exists and contains null, never throws
	/// @param json the json input
	/// @param sKey the key to search for
	DEKAF2_PUBLIC
	bool IsNull (const KJSON& json, KStringView sKey) noexcept;

	/// returns true if the key exists and contains a bool, never throws
	/// @param json the json input
	/// @param sKey the key to search for
	DEKAF2_PUBLIC
	bool IsBoolean (const KJSON& json, KStringView sKey) noexcept;

	/// adds the given integer to the given key, creates key if it does not yet exist
	DEKAF2_PUBLIC
	void Increment (KJSON& json, KStringView sKey, uint64_t iAddMe=1) noexcept;

	/// subtracts the given integer from the given key, creates key if it does not yet exist
	DEKAF2_PUBLIC
	void Decrement (KJSON& json, KStringView sKey, uint64_t iSubstractMe=1) noexcept;

	/// proper json string escaping
	/// @param sInput the unescaped input string
	/// @param sOutput the escaped output string
	DEKAF2_PUBLIC
	void Escape (KStringView sInput, KStringRef& sOutput);

	/// proper json string escaping
	/// @param sInput the unescaped input string
	/// @returns the escaped output string
	DEKAF2_PUBLIC
	KString Escape (KStringView sInput);

	/// returns an iterator to the found element if object is a string array or an object and contains the given string, never throws
	DEKAF2_PUBLIC
	KJSON::const_iterator Find (const KJSON& json, KStringView sString) noexcept;

	/// returns true if object is a string array or an object and contains the given string, never throws
	DEKAF2_PUBLIC
	bool Contains (const KJSON& json, KStringView sString) noexcept;

	/// RecursiveMatchValue (json, m_sSearchX);
	DEKAF2_PUBLIC
	bool RecursiveMatchValue (const KJSON& json, KStringView sSearch);

	/// merges keys from object2 to object1
	DEKAF2_PUBLIC
	void Merge (KJSON& object1, const KJSON& object2);

	/// returns true if a selector seems to be a json pointer
	DEKAF2_PUBLIC
	bool IsJsonPointer(KStringView sSelector);

	/// returns true if a selector seems to be a json path
	DEKAF2_PUBLIC
	bool IsJsonPath(KStringView sSelector);

	/// converts a json path into a json pointer
	DEKAF2_PUBLIC
	KString ToJsonPointer(KStringView sSelector);

	/// use a path-style selector to isolate any type of value inside a JSON structure, never throws
	DEKAF2_PUBLIC
	const KJSON& Select (const KJSON& json, KStringView sSelector);

	/// use a path-style selector to isolate any type of value inside a JSON structure, throws on error
	DEKAF2_PUBLIC
	KJSON& Select (KJSON& json, KStringView sSelector);

	/// use an integer selector to get an array value, never throws
	DEKAF2_PUBLIC
	const KJSON& Select (const KJSON& json, std::size_t iSelector);

	/// use an integer selector to get an array value, throws on error
	DEKAF2_PUBLIC
	KJSON& Select (KJSON& json, std::size_t iSelector);

	/// use a path-style selector to isolate a string inside a JSON structure, never throws
	/// e.g. data.object.payment.sources[0].creditCard.lastFourDigits
	/// or /data/object/payment/sources/0/creditCard/lastFourDigits
	DEKAF2_PUBLIC
	const KString& SelectString (const KJSON& json, KStringView sSelector);

	/// use a path-style selector to isolate an object reference inside a JSON structure, never throws
	/// e.g. data.object.payment.sources[0].creditCard
	/// or /data/object/payment/sources/0/creditCard
	DEKAF2_PUBLIC
	const KJSON& SelectObject (const KJSON& json, KStringView sSelector);

} // end of namespace kjson

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
