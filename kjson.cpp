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

#include "kjson.h"
#include "klog.h"
#include "krow.h"
#include "kutf8.h"
#include "kctype.h"
#include "kstringutils.h"
#include "kreader.h"
#ifndef DEKAF2_WRAPPED_KJSON
	#include "kscopeguard.h"
#endif

DEKAF2_NAMESPACE_BEGIN

namespace kjson {

//-----------------------------------------------------------------------------
/// remove leading "[json.exception.out_of_range.401] " etc from sMessage
const char* kStripJSONExceptionMessage(const char* sMessage) noexcept
//-----------------------------------------------------------------------------
{
	auto sOrigMessage = sMessage;

	if (sMessage && *sMessage == '[')
	{
		for (;;)
		{
			auto ch = *sMessage;

			if (ch == ']')
			{
				while (KASCII::kIsSpace(*++sMessage)) {}
				return sMessage;
			}
			else if (ch == '\0')
			{
				break;
			}
			else
			{
				++sMessage;
			}
		}
	}

	return sOrigMessage;

} // kStripJSONExceptionMessage

//-----------------------------------------------------------------------------
void SetStringFromUTF8orLatin1(LJSON& json, KStringView sInput)
//-----------------------------------------------------------------------------
{
	if (Unicode::ValidUTF(sInput))
	{
		json = sInput;
	}
	else
	{
		// make sure we have a json string type
		json = LJSON::string_t();
		auto& sUTF8 = json.get_ref<KString&>();
		sUTF8.reserve(sInput.size());

		for (auto ch : sInput)
		{
			Unicode::ToUTF(ch, sUTF8);
		}
	}

} // SetStringFromUTF8orLatin1

//-----------------------------------------------------------------------------
bool Parse (LJSON& json, KStringView sJSON, KStringRef& sError) noexcept
//-----------------------------------------------------------------------------
{
	bool bReturn = true;

	// reset the json object
	json = LJSON{};

	sJSON.TrimLeft();

	// avoid throwing an exception for empty input - JSON will simply
	// be empty too, so no error.

	if (!sJSON.empty())
	{
#ifndef DEKAF2_WRAPPED_KJSON
		bool bResetFlag = KLog::getInstance().ShowStackOnJsonError(false);
#endif
		DEKAF2_TRY
		{
			json = LJSON::parse(sJSON.cbegin(), sJSON.cend());
		}

		DEKAF2_CATCH (const LJSON::exception& exc)
		{
			sError = kFormat ("JSON[{:03d}]: {}", exc.id, exc.what()).str();
			bReturn = false;
		}
#ifndef DEKAF2_WRAPPED_KJSON
		KLog::getInstance().ShowStackOnJsonError(bResetFlag);
#endif
	}

	return bReturn;

} // Parse

//-----------------------------------------------------------------------------
void Parse (LJSON& json, KStringView sJSON)
//-----------------------------------------------------------------------------
{
	json = LJSON{};

	sJSON.TrimLeft();

	if (!sJSON.empty())
	{
		// avoid throwing an exception for empty input - JSON will simply
		// be empty too, so no error.
		json = LJSON::parse(sJSON.cbegin(), sJSON.cend());
	}

} // Parse

//-----------------------------------------------------------------------------
KJSON Parse (KStringView sJSON) noexcept
//-----------------------------------------------------------------------------
{
	LJSON   json;
	KString sError;

	if (!Parse (json, sJSON, sError))
	{
		kDebug (1, sError);
	}

	return json;

} // Parse

//-----------------------------------------------------------------------------
KJSON ParseOrThrow (KStringView sJSON)
//-----------------------------------------------------------------------------
{
	LJSON json;

	Parse (json, sJSON); // will throw if invalid

	return json;

} // Parse

//-----------------------------------------------------------------------------
KJSON Parse (KInStream& istream) noexcept
//-----------------------------------------------------------------------------
{
	LJSON   json;
	KString sError;

	if (!Parse (json, istream, sError))
	{
		kDebug (1, sError);
	}

	return json;

} // Parse

//-----------------------------------------------------------------------------
KJSON ParseOrThrow (KInStream& istream)
//-----------------------------------------------------------------------------
{
	LJSON json;

	Parse (json, istream); // will throw if invalid

	return json;

} // Parse

//-----------------------------------------------------------------------------
bool SkipLeadingSpace(KInStream& InStream)
//-----------------------------------------------------------------------------
{
	for (;;)
	{
		auto ch = InStream.Read();

		if (ch == std::istream::traits_type::eof())
		{
			// avoid throwing an exception for empty input - JSON will simply
			// be empty too, so no error.
			return false;
		}

		if (!KASCII::kIsSpace(ch))
		{
			if (!InStream.UnRead())
			{
				kWarning("cannot un-read first char");
			}
			return true;
		}
	}
}

//-----------------------------------------------------------------------------
bool Parse (LJSON& json, KInStream& InStream, KStringRef& sError) noexcept
//-----------------------------------------------------------------------------
{
	json = LJSON{};

	if (!SkipLeadingSpace(InStream))
	{
		// empty input == empty json
		return true;
	}

	DEKAF2_TRY
	{
		InStream >> json;
		return true;
	}

	DEKAF2_CATCH (const LJSON::exception& exc)
	{
		sError = kFormat ("JSON[{:03d}]: {}", exc.id, exc.what()).str();
		return false;
	}

} // kParse

//-----------------------------------------------------------------------------
void Parse (LJSON& json, KInStream& InStream)
//-----------------------------------------------------------------------------
{
	json = LJSON{};

	if (SkipLeadingSpace(InStream))
	{
		InStream >> json;
	}

} // Parse

namespace {
const KString s_sEmpty;
const LJSON   s_oEmpty;
}

//-----------------------------------------------------------------------------
const KString& GetStringRef(const LJSON& json, KStringView sKey) noexcept
//-----------------------------------------------------------------------------
{
	auto it = json.find(sKey);

	if (it != json.end() && it.value().is_string())
	{
		return it.value().get_ref<const KString&>();
	}

	return s_sEmpty;

} // GetStringRef

//-----------------------------------------------------------------------------
KString GetString(const LJSON& json, KStringView sKey) noexcept
//-----------------------------------------------------------------------------
{
	KString sReturn;

	auto it = json.find(sKey);

	if (it != json.end())
	{
		// convert to string whatever the input..
		sReturn = Print(*it);
	}

	return sReturn;

} // GetString

//-----------------------------------------------------------------------------
uint64_t GetUInt(const LJSON& json, KStringView sKey) noexcept
//-----------------------------------------------------------------------------
{
	uint64_t iReturn { 0 };

	auto it = json.find(sKey);

	if (it != json.end())
	{
		if (it->is_number())
		{
			iReturn = it.value().get<uint64_t>();
		}
		else if (it->is_string())
		{
			iReturn = it.value().get_ref<const KString&>().UInt64();
		}
	}

	return iReturn;

} // GetUInt

//-----------------------------------------------------------------------------
int64_t GetInt(const LJSON& json, KStringView sKey) noexcept
//-----------------------------------------------------------------------------
{
	int64_t iReturn { 0 };

	auto it = json.find(sKey);

	if (it != json.end())
	{
		if (it->is_number())
		{
			iReturn = it.value().get<int64_t>();
		}
		else if (it->is_string())
		{
			iReturn = it.value().get_ref<const KString&>().Int64();
		}
	}

	return iReturn;

} // GetInt

//-----------------------------------------------------------------------------
bool GetBool(const LJSON& json, KStringView sKey) noexcept
//-----------------------------------------------------------------------------
{
	bool bReturn { 0 };

	auto it = json.find(sKey);

	if (it != json.end())
	{
		if (it->is_boolean())
		{
			bReturn = it.value().get<bool>();
		}
		else if (it->is_number())
		{
			bReturn = it.value().get<int64_t>();
		}
		else if (it->is_string())
		{
			bReturn = it.value().get_ref<const KString&>().Bool();
		}
	}

	return bReturn;

} // GetBool

//-----------------------------------------------------------------------------
const LJSON& GetObjectRef (const LJSON& json, KStringView sKey) noexcept
//-----------------------------------------------------------------------------
{
	auto it = json.find(sKey);

	if (it != json.end())
	{
		return it.value();
	}

	return s_oEmpty;

} // GetObjectRef

//-----------------------------------------------------------------------------
const LJSON& GetArray (const LJSON& json, KStringView sKey) noexcept
//-----------------------------------------------------------------------------
{
	auto it = json.find(sKey);

	if (it != json.end() && it.value().is_array())
	{
		return it.value();
	}

	return s_oEmpty;

} // GetArray

//-----------------------------------------------------------------------------
const LJSON& GetObject (const LJSON& json, KStringView sKey) noexcept
//-----------------------------------------------------------------------------
{
	DEKAF2_TRY
	{
		auto it = json.find(sKey);

		if (it != json.end())
		{
			return it.value();
		}
	}

	DEKAF2_CATCH (const LJSON::exception& exc)
	{
		kDebug(1, "JSON[{:03d}]: {}", exc.id, exc.what());
	}

	return s_oEmpty;

} // GetObject

//-----------------------------------------------------------------------------
DEKAF2_ALWAYS_INLINE
bool IsJsonPointer(KStringView sSelector)
//-----------------------------------------------------------------------------
{
	return sSelector.front() == '/';

} // IsJsonPointer

//-----------------------------------------------------------------------------
DEKAF2_ALWAYS_INLINE
bool IsJsonPath(KStringView sSelector)
//-----------------------------------------------------------------------------
{
	return sSelector.front() == '.';

} // IsJsonPath

//-----------------------------------------------------------------------------
KString ToJsonPointer(KStringView sSelector)
//-----------------------------------------------------------------------------
{
	// TODO make this a char loop
	// assumes we have a json path in sSelector
	KString sPointer;
	sPointer.reserve(sSelector.size());

	static const KFindSetOfChars Trim(" \f\n\r\t\v\b]");
	static const KFindSetOfChars Dots(".[");

	for (const auto& sElement : sSelector.Split(Dots, Trim))
	{
		if (!sElement.empty())
		{
			sPointer += '/';
			sPointer += sElement;
		}
	}

	return sPointer;

} // ToJsonPointer

namespace {

constexpr uint16_t iMaxJSONPointerDepth = 1000;

//-----------------------------------------------------------------------------
const LJSON& RecurseJSONPointer(const LJSON& json, KStringView sSelector, uint16_t iRecursions = 0)
//-----------------------------------------------------------------------------
{
	// -> /segment/moreSegment/object/array/2/object/property
	// we are wrapped in a catch/try on json that will return an empty object on failure

	if (iRecursions > iMaxJSONPointerDepth)
	{
		kWarning("recursion depth > {}", iMaxJSONPointerDepth);
		return s_oEmpty;
	}

	if (DEKAF2_UNLIKELY(sSelector.remove_prefix('/') == false))
	{
		kDebug(2, "expected a leading '/', but none found");
	}

	// isolate the next segment
	auto iNext    = sSelector.find('/');
	auto sSegment = sSelector.substr(0, iNext);

	const LJSON* next;

	if (json.is_object())
	{
		// need an object key here
		auto it = json.find(sSegment);

		if (DEKAF2_UNLIKELY(it == json.end()))
		{
			// abort traversal here - we are const and cannot add missing segments
			return s_oEmpty;
		}

		next = &it.value();
	}
	else if (json.is_array())
	{
		// need an array index here
		if (!kIsInteger(sSegment))
		{
			return s_oEmpty;
		}
		// if this will throw it will be caught in the caller and return an empty object
		next = &json.at(sSegment.UInt64());
	}
	else
	{
		// further segment requested, but this is a primitive (or null)..
		return s_oEmpty;
	}

	if (iNext == KStringView::npos)
	{
		return *next;
	}

	sSelector.remove_prefix(iNext);

	return RecurseJSONPointer(*next, sSelector, ++iRecursions);

} // RecurseJSONPointer

} // end of anonymous namespace

//-----------------------------------------------------------------------------
const LJSON& Select (const LJSON& json, KStringView sSelector) noexcept
//-----------------------------------------------------------------------------
{
#ifndef DEKAF2_WRAPPED_KJSON
	bool bResetFlag = KLog::getInstance().ShowStackOnJsonError(false);
	KAtScopeEnd( KLog::getInstance().ShowStackOnJsonError(bResetFlag) );
#endif

	DEKAF2_TRY
	{
		switch (sSelector.front())
		{
			case '.': // a dotted notation
				return RecurseJSONPointer(json, ToJsonPointer(sSelector));

			case '/': // a json pointer
				return RecurseJSONPointer(json, sSelector);

			default:
				if (json.is_object())
				{
					auto it = json.find(sSelector);

					if (it != json.end())
					{
						return it.value();
					}
				}
				return s_oEmpty;
		}
	}

	DEKAF2_CATCH (const LJSON::exception& exc)
	{
		kDebug(1, kStripJSONExceptionMessage(exc.what()));
	}

	return s_oEmpty;

} // Select

namespace { // hide the path traversal in an anonymous namespace

//-----------------------------------------------------------------------------
LJSON& RecurseJSONPointer(LJSON& json, KStringView sSelector, uint16_t iRecursions = 0)
//-----------------------------------------------------------------------------
{
	// -> /segment/moreSegment/object/array/2/object/property

	if (DEKAF2_UNLIKELY(iRecursions > iMaxJSONPointerDepth))
	{
		kWarning("recursion depth > {}", iMaxJSONPointerDepth);
		return json;
	}

	if (DEKAF2_UNLIKELY(sSelector.remove_prefix('/')) == false)
	{
		kDebug(2, "expected a leading '/', but none found");
	}

	// isolate the next segment
	auto iNext      = sSelector.find('/');
	auto sSegment   = sSelector.substr(0, iNext);

	LJSON* next;

	if (DEKAF2_LIKELY(json.is_object()))
	{
		// need an object key here
		auto it = json.find(sSegment);

		if (DEKAF2_LIKELY(it != json.end()))
		{
			next = &it.value();
		}
		else
		{
			// insert a new json null object here
			next = &json[sSegment];
		}
	}
	else if (json.is_array())
	{
		if (!kIsInteger(sSegment))
		{
			// make this an object
			json = LJSON::object();
			// and create a new null object
			next = &json[sSegment];
		}
		else
		{
			// expand array if necessary
			next = &json[sSegment.UInt64()];
		}
	}
	else
	{
		// further segment requested, but this is a primitive or null..
		if (!kIsInteger(sSegment))
		{
			// make this an object
			json = LJSON::object();
			// and create a new null object
			next = &json[sSegment];
		}
		else
		{
			// make this an array
			json = LJSON::array();
			// and create a new null object
			next = &json[sSegment.UInt64()];
		}
	}

	if (iNext == KStringView::npos)
	{
		return *next;
	}

	sSelector.remove_prefix(iNext);

	return RecurseJSONPointer(*next, sSelector, ++iRecursions);

} // RecurseJSONPointer

} // end of anonymous namespace

//-----------------------------------------------------------------------------
LJSON& Select (LJSON& json, KStringView sSelector) noexcept
//-----------------------------------------------------------------------------
{
#ifndef DEKAF2_WRAPPED_KJSON
	bool bResetFlag = KLog::getInstance().ShowStackOnJsonError(false);
	KAtScopeEnd( KLog::getInstance().ShowStackOnJsonError(bResetFlag) );
#endif

	DEKAF2_TRY
	{
		if (sSelector.empty())
		{
			return json;
		}

		switch (sSelector.front())
		{
			case '.': // a dotted notation
				return RecurseJSONPointer(json, ToJsonPointer(sSelector));

			case '/': // a json pointer
				return RecurseJSONPointer(json, sSelector);

			default:
				// direct element access
				if (DEKAF2_LIKELY(json.is_object()))
				{
					// need an object key here
					auto it = json.find(sSelector);

					if (DEKAF2_LIKELY(it != json.end()))
					{
						return it.value();
					}
					else
					{
						// insert a new json null object here
						return json[sSelector];
					}
				}

				// this is an array, a primitive, or null..
				// make this an object
				json = LJSON::object();
				// and create a new null object
				return json[sSelector];
		}
	}

	DEKAF2_CATCH (const LJSON::exception& exc)
	{
		kDebug(1, kStripJSONExceptionMessage(exc.what()));
	}

	return json;

} // Select

//-----------------------------------------------------------------------------
const LJSON& Select (const LJSON& json, std::size_t iSelector) noexcept
//-----------------------------------------------------------------------------
{
#ifndef DEKAF2_WRAPPED_KJSON
	bool bResetFlag = KLog::getInstance().ShowStackOnJsonError(false);
	KAtScopeEnd( KLog::getInstance().ShowStackOnJsonError(bResetFlag) );
#endif

	DEKAF2_TRY
	{
		if (json.is_array())
		{
			return json.at(iSelector);
		}
		kDebug(1, "not an array");
	}

	DEKAF2_CATCH (const LJSON::exception& exc)
	{
		kDebug(1, kStripJSONExceptionMessage(exc.what()));
	}

	return s_oEmpty;

} // Select

//-----------------------------------------------------------------------------
LJSON& Select (LJSON& json, std::size_t iSelector) noexcept 
//-----------------------------------------------------------------------------
{
#ifndef DEKAF2_WRAPPED_KJSON
	bool bResetFlag = KLog::getInstance().ShowStackOnJsonError(false);
	KAtScopeEnd( KLog::getInstance().ShowStackOnJsonError(bResetFlag) );
#endif

	DEKAF2_TRY
	{
		if (!json.is_array())
		{
			// make this an array
			json = LJSON::array();
		}
		// expand array if necessary
		return json[iSelector];
	}

	DEKAF2_CATCH (const LJSON::exception& exc)
	{
		kDebug(1, kStripJSONExceptionMessage(exc.what()));
	}

	return json;

} // Select

//-----------------------------------------------------------------------------
const KString& SelectString (const LJSON& json, KStringView sSelector)
//-----------------------------------------------------------------------------
{
	auto& element = Select(json, sSelector);

	if (element.is_string())
	{
		return element.get_ref<const KString&>();
	}
	else
	{
		kDebug(2, "not a string: {}", sSelector);
	}

	return s_sEmpty;

} // SelectString

//-----------------------------------------------------------------------------
const LJSON& SelectObject (const LJSON& json, KStringView sSelector)
//-----------------------------------------------------------------------------
{
	auto& element = Select(json, sSelector);

	if (element.is_object())
	{
		return element;
	}
	else
	{
		kDebug(2, "not an object: {}", sSelector);
	}

	return s_oEmpty;

} // SelectObject

//-----------------------------------------------------------------------------
bool Exists (const LJSON& json, KStringView sKey) noexcept
//-----------------------------------------------------------------------------
{
	auto it = json.find(sKey);
	return (it != json.end());
}

//-----------------------------------------------------------------------------
bool IsObject(const LJSON& json, KStringView sKey) noexcept
//-----------------------------------------------------------------------------
{
	auto it = json.find(sKey);
	return (it != json.end() && it->is_object());
}

//-----------------------------------------------------------------------------
bool IsArray(const LJSON& json, KStringView sKey) noexcept
//-----------------------------------------------------------------------------
{
	auto it = json.find(sKey);
	return (it != json.end() && it->is_array());
}

//-----------------------------------------------------------------------------
bool IsString(const LJSON& json, KStringView sKey) noexcept
//-----------------------------------------------------------------------------
{
	auto it = json.find(sKey);
	return (it != json.end() && it->is_string());
}

//-----------------------------------------------------------------------------
bool IsInteger(const LJSON& json, KStringView sKey) noexcept
//-----------------------------------------------------------------------------
{
	auto it = json.find(sKey);
	return (it != json.end() && it->is_number_integer());
}

//-----------------------------------------------------------------------------
bool IsFloat(const LJSON& json, KStringView sKey) noexcept
//-----------------------------------------------------------------------------
{
	auto it = json.find(sKey);
	return (it != json.end() && it->is_number_float());
}

//-----------------------------------------------------------------------------
bool IsNull(const LJSON& json, KStringView sKey) noexcept
//-----------------------------------------------------------------------------
{
	auto it = json.find(sKey);
	return (it != json.end() && it->is_null());
}

//-----------------------------------------------------------------------------
bool IsBoolean(const LJSON& json, KStringView sKey) noexcept
//-----------------------------------------------------------------------------
{
	auto it = json.find(sKey);
	return (it != json.end() && it->is_boolean());
}

//-----------------------------------------------------------------------------
void Increment(LJSON& json, KStringView sKey, uint64_t iAddMe) noexcept
//-----------------------------------------------------------------------------
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

//-----------------------------------------------------------------------------
void Decrement(LJSON& json, KStringView sKey, uint64_t iSubstractMe) noexcept
//-----------------------------------------------------------------------------
{
	auto it = json.find(sKey);
	if (DEKAF2_UNLIKELY((it == json.end()) || !it->is_number()))
	{
		json[sKey] = 0UL;
		it = json.find(sKey);
	}
	auto iValue = it.value().get<uint64_t>();
	it.value() = iValue - iSubstractMe;
}

//-----------------------------------------------------------------------------
void Escape (KStringView sInput, KStringRef& sOutput)
//-----------------------------------------------------------------------------
{
	static constexpr KStringView::value_type BACKSPACE         = 0x08;
	static constexpr KStringView::value_type HORIZONTAL_TAB    = 0x09;
	static constexpr KStringView::value_type NEWLINE           = 0x0a;
	static constexpr KStringView::value_type FORMFEED          = 0x0c;
	static constexpr KStringView::value_type CARRIAGE_RETURN   = 0x0d;
	static constexpr KStringView::value_type DOUBLEQUOTE       = 0x22;
	static constexpr KStringView::value_type BACKSLASH         = 0x5c;
	static constexpr KStringView::value_type MAX_CONTROL_CHARS = 0x1f;

	// reserve at least the bare size of sInput in sOutput
	sOutput.reserve(sOutput.size() + sInput.size());

	for (auto ch : sInput)
	{
		switch (ch)
		{
			case BACKSPACE:
			{
				sOutput += '\\';
				sOutput += 'b';
				break;
			}

			case HORIZONTAL_TAB:
			{
				sOutput += '\\';
				sOutput += 't';
				break;
			}

			case NEWLINE:
			{
				sOutput += '\\';
				sOutput += 'n';
				break;
			}

			case FORMFEED:
			{
				sOutput += '\\';
				sOutput += 'f';
				break;
			}

			case CARRIAGE_RETURN:
			{
				sOutput += '\\';
				sOutput += 'r';
				break;
			}

			case DOUBLEQUOTE:
			{
				sOutput += '\\';
				sOutput += '\"';
				break;
			}

			case BACKSLASH:
			{
				sOutput += '\\';
				sOutput += '\\';
				break;
			}

			default:
			{
				// escape control characters (0x00..0x1F)
				if (Unicode::CodepointCast(ch) <= MAX_CONTROL_CHARS)
				{
					sOutput += "\\u00";
					sOutput += KString::to_hexstring(Unicode::CodepointCast(ch));
				}
				else
				{
					sOutput += ch;
				}
				break;
			}
		}
	}

} // Escape

//-----------------------------------------------------------------------------
KString Escape (KStringView sInput)
//-----------------------------------------------------------------------------
{
	KString sReturn;
	Escape(sInput, sReturn);
	return sReturn;

} // Escape

//-----------------------------------------------------------------------------
KString Print (const LJSON& json) noexcept
//-----------------------------------------------------------------------------
{
	DEKAF2_TRY
	{
		switch (json.type())
		{
			case LJSON::value_t::object:
			case LJSON::value_t::array:
				return json.dump(-1);

			case LJSON::value_t::string:
				return json.get<LJSON::string_t>();

			case LJSON::value_t::number_integer:
				return KString::to_string(json.get<LJSON::number_integer_t>());

			case LJSON::value_t::number_unsigned:
				return KString::to_string(json.get<LJSON::number_unsigned_t>());

			case LJSON::value_t::number_float:
				return KString::to_string(json.get<LJSON::number_float_t>());

			case LJSON::value_t::boolean:
				return json.get<LJSON::boolean_t>() ? "true" : "false";

			case LJSON::value_t::null:
				return "NULL";

			default:
			case LJSON::value_t::discarded:
				return "(UNKNOWN)";
		}
	}
	
	DEKAF2_CATCH (const LJSON::exception& exc)
	{
		kDebug(1, kStripJSONExceptionMessage(exc.what()));
		return "(ERROR)";
	}

	return "";

} // Print

//-----------------------------------------------------------------------------
LJSON::const_iterator Find (const LJSON& json, KStringView sString) noexcept
//-----------------------------------------------------------------------------
{
	DEKAF2_TRY
	{
		if (!json.empty())
		{
			if (json.is_array())
			{
				if (json.front().is_string())
				{
					return std::find(json.begin(), json.end(), sString);
				}
			}
			else if (json.is_object())
			{
				return json.find(sString);
			}
		}
	}

	DEKAF2_CATCH (const LJSON::exception& exc)
	{
		kDebug(1, kStripJSONExceptionMessage(exc.what()));
	}

	return json.end();

} // Find

//-----------------------------------------------------------------------------
bool Contains (const LJSON& json, KStringView sString) noexcept
//-----------------------------------------------------------------------------
{
	return Find(json, sString) != json.end();

} // Contains

//-----------------------------------------------------------------------------
bool RecursiveMatchValue (const LJSON& json, KStringView sSearch)
//-----------------------------------------------------------------------------
{
	if (json.empty())
	{
		return false;  // empty json matches nothing
	}
	else if (!sSearch)
	{
		return true;  // empty string matches anything
	}

	KString sLower (sSearch);
	sLower.MakeLower();

	if (json.is_primitive())
	{
		KString sLowerValue = json.dump();
		sLowerValue.MakeLower();
		if (sLowerValue.contains (sSearch))
		{
			return true; // this value contains the string
		}
	}
	else if (json.is_array())
	{
		for (const auto& it : json.items())
		{
			if (kjson::RecursiveMatchValue (it.value(), sLower))
			{
				return true; // found a value that contains string
			}
		}
	}
	else if (json.is_object())
	{
	    for (auto it = json.begin(); it != json.end(); ++it)
	    {
			if (RecursiveMatchValue (it.value(), sLower))
			{
				return true; // found a value that contains string
			}
	    }
	}

	return false; // no value contains the string

} // RecursiveMatchValue

//-----------------------------------------------------------------------------
void Merge (LJSON& left, LJSON right)
//-----------------------------------------------------------------------------
{
	// this merge will always succeed

	DEKAF2_TRY
	{
		// null is empty, too - but take care for arrays as (left) - the caller may just want
		// to add an empty object at the end of an array (perhaps to manipulate it later)
		if (right.is_null() && left.is_array() == false)
		{
			// nothing to merge..
			return;
		}

		if (left.is_null())
		{
			// just copy the right json value - also works for strings, arrays, ints, etc.
			left = std::move(right);
			return;
		}

		// both json values are non-null..

		if (left.is_primitive() || (left.is_object() && !right.is_object()))
		{
			// convert string/int/bool into an array first
			// convert left into an array if right is not an object..
			LJSON copy = LJSON::array();
			copy.push_back(std::move(left));
			left = std::move(copy);
		}

		// (left) is now either an array or an object

		if (left.is_array())
		{
			if (right.is_array())
			{
				for (auto& item : right)
				{
					left.push_back(std::move(item));
				}
			}
			else
			{
				left.push_back(std::move(right));
			}
			return;
		}

		// left and right are objects here.. see the tests and conversions above

		for (auto& it : right.items())
		{
			left[it.key()] = std::move(it.value());
		}
	}

	DEKAF2_CATCH (const LJSON::exception& exc)
	{
		kDebug(1, kStripJSONExceptionMessage(exc.what()));
	}

} // Merge

} // end of namespace kjson

DEKAF2_NAMESPACE_END
