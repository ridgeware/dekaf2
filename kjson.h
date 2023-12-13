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
#include "klog.h" // needed for JSON_THROW_USER

#ifndef DEKAF2_WRAPPED_KJSON
	// When we define DEKAF2_WRAPPED_KJSON we effectively switch off stack trace
	// generation for JSON exceptions, as we do not need it anymore for error search
	#define JSON_THROW_USER(exception) { kJSONTrace(); throw exception; }

	// we definitely want to use implicit conversions, else all hell breaks loose
	#define JSON_USE_IMPLICIT_CONVERSIONS 1
#else
	// we do _not_ want to use implicit conversions, because otherwise, with gcc,
	// the converting operator of LJSON creates ambiguities with KJSON2 even
	// when the overload would not resolve (because it is private). This does
	// not happen with clang though, so we could make this a conditional based
	// on the compiler, but that would make the code behave differently depending
	// on the compiler.
	#define JSON_USE_IMPLICIT_CONVERSIONS 0
	// also, disable three way comparison, it does not work currently
	// with KString and JSON
	#define JSON_HAS_THREE_WAY_COMPARISON 0
#endif

#include "kcrashexit.h"

#define JSON_ASSERT(bMustBeTrue) { DEKAF2_PREFIX kAssert(bMustBeTrue, "crash in KJSON"); }

#if  defined(DEKAF2_IS_DEBUG_BUILD) \
 && !defined(DEKAF2_USE_PRECOMPILED_HEADERS) \
 && !defined(DEKAF2_WRAPPED_KJSON)
	// add exact location to json exceptions in debug mode - we switch them off
	// in release mode because they come with a modest cost
	// This works starting with nlohmann::json v3.10.0
	// Unfortunately NDEBUG is set when precompiling headers - do not use
	// this with PCH, the symbols will not match when linking from external code
	#define JSON_DIAGNOSTICS 1
	// Note: switching diagnostics to 1 changes the debug output, so tests may
	// need to be readjusted
#endif

// disable _json literals in global namespace
#define JSON_USE_GLOBAL_UDLS 0

#include <nlohmann/json.hpp>

#include "kstring.h"
#include "kstringview.h"
#include "kreader.h"

DEKAF2_NAMESPACE_BEGIN

// the native nlohmann::json type, using KString instead of std::string though
using LJSON = nlohmann::basic_json<std::map, std::vector, DEKAF2_PREFIX KString>;

#ifdef DEKAF2_WRAPPED_KJSON
	class KJSON2;
	using KJSON = KJSON2;
#else
	using KJSON = LJSON;
#endif

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
namespace kjson
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	/// advance a stream over leading spaces
	/// @return false if first non-space char could not be returned into the stream, true otherwise
	bool SkipLeadingSpace(KInStream& InStream);

	/// Parse a string, throws with KJSON::exception in case of error
	/// @param json the json output
	/// @param sJSON the input string to parse
	DEKAF2_PUBLIC
	void Parse (LJSON& json, KStringView sJSON);

	/// Parse a string, returns error in sError if any, never throws
	/// @param json the json output
	/// @param sJSON the input string to parse
	/// @param sError the error string
	DEKAF2_PUBLIC
	bool Parse (LJSON& json, KStringView sJSON, KStringRef& sError) noexcept;

	/// Parse a stream, throws with KJSON::exception in case of error
	/// @param json the json output
	/// @param InStream the input stream to parse
	DEKAF2_PUBLIC
	void Parse (LJSON& json, KInStream& InStream);

	/// Parse a stream, returns error in sError if any, never throws
	/// @param json the json output
	/// @param InStream the input stream to parse
	/// @param sError the error string
	DEKAF2_PUBLIC
	bool Parse (LJSON& json, KInStream& InStream, KStringRef& sError) noexcept;

	/// Returns parsed string as a JSON object
	/// if invalid, just return a null object on failure
	/// does NOT throw
	DEKAF2_PUBLIC
	KJSON Parse (KStringView sJSON) noexcept;

	/// Returns parsed string as a JSON object
	/// if invalid, throw KError
	DEKAF2_PUBLIC
	KJSON ParseOrThrow (KStringView sJSON);

	/// Returns parsed stream as a JSON object
	/// if invalid, just return a null object on failure
	/// does NOT throw
	DEKAF2_PUBLIC
	KJSON Parse (KInStream& istream) noexcept;

	/// Returns parsed stream as a JSON object
	/// if invalid, throw KError
	DEKAF2_PUBLIC
	KJSON ParseOrThrow (KInStream& istream);

	/// returns a string representation for the KJSON object, never throws
	/// @param json the json input
	DEKAF2_PUBLIC
	KString Print (const LJSON& json) noexcept;

	/// Sets a JSON string from a KStringView, checks if the string was
	/// valid UTF8 or converts from (assumed) Latin1 to UTF8. That is
	/// needed to avoid throws from the KJSON serializer on invalid UTF8.
	/// Use this whenever you add string data coming from unreliable
	/// input.
	/// @param json the json output
	/// @param sInput the string input
	DEKAF2_PUBLIC
	void SetStringFromUTF8orLatin1(LJSON& json, KStringView sInput);

	/// Returns a string ref for a key, never throws. Returns empty ref
	/// for non-string values.
	/// @param json the json input
	/// @param sKey the key to search for
	DEKAF2_PUBLIC
	const KString& GetStringRef(const LJSON& json, KStringView sKey) noexcept;

	/// Returns a string value for a key, never throws. Prints non-string
	/// values into string representation.
	/// @param json the json input
	/// @param sKey the key to search for
	DEKAF2_PUBLIC
	KString GetString(const LJSON& json, KStringView sKey) noexcept;

	/// Returns a uint value for a key, never throws. Returns 0 if value was
	/// neither an integer nor a string representation of an integer.
	/// @param json the json input
	/// @param sKey the key to search for
	DEKAF2_PUBLIC
	uint64_t GetUInt(const LJSON& json, KStringView sKey) noexcept;

	/// Returns an int value for a key, never throws. Returns 0 if value was
	/// neither an integer nor a string representation of an integer.
	/// @param json the json input
	/// @param sKey the key to search for
	DEKAF2_PUBLIC
	int64_t GetInt(const LJSON& json, KStringView sKey) noexcept;

	/// Returns a bool value for a key, never throws. Returns 0 if value was
	/// neither an integer nor a string representation of an integer.
	/// @param json the json input
	/// @param sKey the key to search for
	DEKAF2_PUBLIC
	bool GetBool(const LJSON& json, KStringView sKey) noexcept;

	/// Returns a const object ref for a key, never throws. Returns empty ref
	/// for non-object values.
	/// @param json the json input
	/// @param sKey the key to search for
	DEKAF2_PUBLIC
	const LJSON& GetObjectRef (const LJSON& json, KStringView sKey) noexcept;

	/// returns a const object ref for a key, never throws. Returns empty
	/// object for non-object values.
	/// @param json the json input
	/// @param sKey the key to search for
	DEKAF2_PUBLIC
	const LJSON& GetObject (const LJSON& json, KStringView sKey) noexcept;

	/// returns an array value for a key, never throws. Returns empty
	/// array for non-array values.
	/// @param json the json input
	/// @param sKey the key to search for
	DEKAF2_PUBLIC
	const LJSON& GetArray (const LJSON& json, KStringView sKey) noexcept;

	/// returns true if the key exists, never throws
	/// @param json the json input
	/// @param sKey the key to search for
	DEKAF2_PUBLIC
	bool Exists (const LJSON& json, KStringView sKey) noexcept;

	/// returns true if the key exists and contains an object, never throws
	/// @param json the json input
	/// @param sKey the key to search for
	DEKAF2_PUBLIC
	bool IsObject (const LJSON& json, KStringView sKey) noexcept;

	/// returns true if the key exists and contains an array, never throws
	/// @param json the json input
	/// @param sKey the key to search for
	DEKAF2_PUBLIC
	bool IsArray (const LJSON& json, KStringView sKey) noexcept;

	/// returns true if the key exists and contains a string, never throws
	/// @param json the json input
	/// @param sKey the key to search for
	DEKAF2_PUBLIC
	bool IsString (const LJSON& json, KStringView sKey) noexcept;

	/// returns true if the key exists and contains an integer, never throws
	/// @param json the json input
	/// @param sKey the key to search for
	DEKAF2_PUBLIC
	bool IsInteger (const LJSON& json, KStringView sKey) noexcept;

	/// returns true if the key exists and contains a float, never throws
	/// @param json the json input
	/// @param sKey the key to search for
	DEKAF2_PUBLIC
	bool IsFloat (const LJSON& json, KStringView sKey) noexcept;

	/// returns true if the key exists and contains null, never throws
	/// @param json the json input
	/// @param sKey the key to search for
	DEKAF2_PUBLIC
	bool IsNull (const LJSON& json, KStringView sKey) noexcept;

	/// returns true if the key exists and contains a bool, never throws
	/// @param json the json input
	/// @param sKey the key to search for
	DEKAF2_PUBLIC
	bool IsBoolean (const LJSON& json, KStringView sKey) noexcept;

	/// adds the given integer to the given key, creates key if it does not yet exist
	DEKAF2_PUBLIC
	void Increment (LJSON& json, KStringView sKey, uint64_t iAddMe=1) noexcept;

	/// subtracts the given integer from the given key, creates key if it does not yet exist
	DEKAF2_PUBLIC
	void Decrement (LJSON& json, KStringView sKey, uint64_t iSubstractMe=1) noexcept;

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
	LJSON::const_iterator Find (const LJSON& json, KStringView sString) noexcept;

	/// returns true if object is a string array or an object and contains the given string, never throws
	DEKAF2_PUBLIC
	bool Contains (const LJSON& json, KStringView sString) noexcept;

	/// RecursiveMatchValue (json, m_sSearchX);
	DEKAF2_PUBLIC
	bool RecursiveMatchValue (const LJSON& json, KStringView sSearch);

	/// merges objects or arrays, or arrays and objects
	DEKAF2_PUBLIC
	void Merge (LJSON& json1, LJSON json2);

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
	const LJSON& Select (const LJSON& json, KStringView sSelector) noexcept;

	/// use a path-style selector to isolate any type of value inside a JSON structure, never throws
	DEKAF2_PUBLIC
	LJSON& Select (LJSON& json, KStringView sSelector) noexcept;

	/// use an integer selector to get an array value, never throws
	DEKAF2_PUBLIC
	const LJSON& Select (const LJSON& json, std::size_t iSelector) noexcept;

	/// use an integer selector to get an array value, throws on error
	DEKAF2_PUBLIC
	LJSON& Select (LJSON& json, std::size_t iSelector) noexcept;

	/// use a path-style selector to isolate a string inside a JSON structure, never throws
	/// e.g. data.object.payment.sources[0].creditCard.lastFourDigits
	/// or /data/object/payment/sources/0/creditCard/lastFourDigits
	DEKAF2_PUBLIC
	const KString& SelectString (const LJSON& json, KStringView sSelector);

	/// use a path-style selector to isolate an object reference inside a JSON structure, never throws
	/// e.g. data.object.payment.sources[0].creditCard
	/// or /data/object/payment/sources/0/creditCard
	DEKAF2_PUBLIC
	const LJSON& SelectObject (const LJSON& json, KStringView sSelector);

	/// helper to trim down the exception message
	DEKAF2_PUBLIC
	const char* kStripJSONExceptionMessage(const char* sMessage) noexcept;

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

DEKAF2_NAMESPACE_END

#include <fmt/format.h>
#include "kjson2.h"

namespace fmt {

template <>
struct formatter<DEKAF2_PREFIX LJSON> : formatter<string_view>
{
	template <typename FormatContext>
	auto format(const DEKAF2_PREFIX LJSON& json, FormatContext& ctx) const
	{
		return formatter<string_view>::format(DEKAF2_PREFIX kjson::Print(json), ctx);
	}
};

} // end of namespace fmt
