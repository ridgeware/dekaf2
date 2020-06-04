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

namespace dekaf2 {

//-----------------------------------------------------------------------------
/// ADL converter for KROW to KJSON
void to_json(KJSON& j, const KROW& row)
//-----------------------------------------------------------------------------
{
	j = row.to_json();
}

//-----------------------------------------------------------------------------
/// ADL converter for KROW to KJSON
void from_json(const KJSON& j, KROW& row)
//-----------------------------------------------------------------------------
{
	row.from_json(j);
}

namespace kjson {

//-----------------------------------------------------------------------------
void SetStringFromUTF8orLatin1(KJSON& json, KStringView sInput)
//-----------------------------------------------------------------------------
{
	if (Unicode::ValidUTF8(sInput))
	{
		json = sInput;
	}
	else
	{
		auto& sUTF8 = json.get_ref<KString&>();
		sUTF8.reserve(sInput.size());

		for (auto ch : sInput)
		{
			Unicode::ToUTF8(ch, sUTF8);
		}
	}

} // SetStringFromUTF8orLatin1

//-----------------------------------------------------------------------------
bool Parse (KJSON& json, KStringView sJSON, KString& sError) noexcept
//-----------------------------------------------------------------------------
{
	json = KJSON{};

	sJSON.TrimLeft();

	if (sJSON.empty())
	{
		// avoid throwing an exception for empty input - JSON will simply
		// be empty too, so no error.
		return true;
	}

	bool bResetFlag = KLog::getInstance().ShowStackOnJsonError(false);

	DEKAF2_TRY
	{
		json = KJSON::parse(sJSON.cbegin(), sJSON.cend());
		KLog::getInstance().ShowStackOnJsonError(bResetFlag);
		return true;
	}
	DEKAF2_CATCH (const KJSON::exception& exc)
	{
		sError.Printf ("JSON[%03d]: %s", exc.id, exc.what());
		KLog::getInstance().ShowStackOnJsonError(bResetFlag);
		return false;
	}

} // Parse

//-----------------------------------------------------------------------------
void Parse (KJSON& json, KStringView sJSON)
//-----------------------------------------------------------------------------
{
	json = KJSON{};

	sJSON.TrimLeft();

	if (!sJSON.empty())
	{
		// avoid throwing an exception for empty input - JSON will simply
		// be empty too, so no error.
		json = KJSON::parse(sJSON.cbegin(), sJSON.cend());
	}

} // kParse

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
bool Parse (KJSON& json, KInStream& InStream, KString& sError) noexcept
//-----------------------------------------------------------------------------
{
	json = KJSON{};

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
	DEKAF2_CATCH (const KJSON::exception& exc)
	{
		sError.Printf ("JSON[%03d]: %s", exc.id, exc.what());
		return false;
	}

} // kParse

//-----------------------------------------------------------------------------
void Parse (KJSON& json, KInStream& InStream)
//-----------------------------------------------------------------------------
{
	json = KJSON{};

	if (SkipLeadingSpace(InStream))
	{
		InStream >> json;
	}

} // Parse

//-----------------------------------------------------------------------------
const KString& GetStringRef(const KJSON& json, KStringView sKey) noexcept
//-----------------------------------------------------------------------------
{
	static KString s_sEmpty{};

	auto it = json.find(sKey);
	if (it != json.end() && it.value().is_string())
	{
		return it.value().get_ref<const KString&>();
	}

	return s_sEmpty;

} // GetString

//-----------------------------------------------------------------------------
KString GetString(const KJSON& json, KStringView sKey) noexcept
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
uint64_t GetUInt(const KJSON& json, KStringView sKey) noexcept
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
int64_t GetInt(const KJSON& json, KStringView sKey) noexcept
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
const KJSON& GetObjectRef (const KJSON& json, KStringView sKey) noexcept
//-----------------------------------------------------------------------------
{
	static KJSON s_oEmpty;

	auto it = json.find(sKey);
	if (it != json.end() && it.value().is_object())
	{
		return it.value();
	}

	return s_oEmpty;

} // GetObject

//-----------------------------------------------------------------------------
KJSON GetObject (const KJSON& json, KStringView sKey) noexcept
//-----------------------------------------------------------------------------
{
	KJSON oReturnMe;

	DEKAF2_TRY
	{
		auto it = json.find(sKey);
		if (it != json.end())
		{
			oReturnMe = it.value();
		}
	}
	DEKAF2_CATCH (const KJSON::exception& exc)
	{
		kDebug(1, "JSON[%03d]: %s", exc.id, exc.what());
	}

	return (oReturnMe);

} // GetObject

//-----------------------------------------------------------------------------
void Escape (KStringView sInput, KString& sOutput)
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
					// copy byte to buffer (all previous bytes
					// been copied have in default case above)
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
KString Print (const KJSON& json) noexcept
//-----------------------------------------------------------------------------
{
	DEKAF2_TRY
	{
		switch (json.type())
		{
			case KJSON::value_t::object:
			case KJSON::value_t::array:
				return json.dump(-1);

			case KJSON::value_t::string:
				return json.get<dekaf2::KJSON::string_t>();

			case KJSON::value_t::number_integer:
				return KString::to_string(json.get<KJSON::number_integer_t>());

			case KJSON::value_t::number_unsigned:
				return KString::to_string(json.get<KJSON::number_unsigned_t>());

			case KJSON::value_t::number_float:
				return KString::to_string(json.get<KJSON::number_float_t>());

			case KJSON::value_t::boolean:
				return json.get<dekaf2::KJSON::boolean_t>() ? "true" : "false";

			case KJSON::value_t::null:
				return "NULL";

			case KJSON::value_t::discarded:
				return "(UNKNOWN)";
		}
	}
	DEKAF2_CATCH (const KJSON::exception& exc)
	{
		kDebug(1, "JSON[%03d]: %s", exc.id, exc.what());
		return "(ERROR)";
	}

	return "";

} // Print

//-----------------------------------------------------------------------------
bool Contains (const KJSON& json, KStringView sString) noexcept
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
					for (auto& it : json)
					{
						if (it == sString)
						{
							return true;
						}
					}
				}
			}
			else if (json.is_object())
			{
				return json.find(sString) != json.end();
			}
		}
	}

	DEKAF2_CATCH (const KJSON::exception& exc)
	{
		kDebug(1, "JSON[%03d]: %s", exc.id, exc.what());
	}

	return false;

} // Contains

//-----------------------------------------------------------------------------
bool RecursiveMatchValue (const KJSON& json, KStringView sSearch)
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
		if (sLowerValue.Contains (sSearch))
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

} // end of namespace kjson

} // end of namespace dekaf2
