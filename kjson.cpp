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
	json.clear();

	if (sJSON.empty())
	{
		// avoid throwing an exception for empty input - JSON will simply
		// be empty too, so no error.
		return true;
	}

	DEKAF2_TRY
	{
		json = KJSON::parse(sJSON.cbegin(), sJSON.cend());
		return true;
	}
	DEKAF2_CATCH (const KJSON::exception& exc)
	{
		sError.Printf ("JSON[%03d]: %s", exc.id, exc.what());
		return false;
	}

} // Parse

//-----------------------------------------------------------------------------
void Parse (KJSON& json, KStringView sJSON)
//-----------------------------------------------------------------------------
{
	json.clear();

	if (!sJSON.empty())
	{
		// avoid throwing an exception for empty input - JSON will simply
		// be empty too, so no error.
		json = KJSON::parse(sJSON.cbegin(), sJSON.cend());
	}

} // kParse

//-----------------------------------------------------------------------------
bool Parse (KJSON& json, KInStream& InStream, KString& sError) noexcept
//-----------------------------------------------------------------------------
{
	json.clear();

	if (InStream.InStream().eof())
	{
		// avoid throwing an exception for empty input - JSON will simply
		// be empty too, so no error.
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
	json.clear();

	if (!InStream.InStream().eof())
	{
		// avoid throwing an exception for empty input - JSON will simply
		// be empty too, so no error.
		InStream >> json;
	}

} // Parse

//-----------------------------------------------------------------------------
const KString& GetStringRef(const KJSON& json, KStringView sKey) noexcept
//-----------------------------------------------------------------------------
{
	static KString s_sEmpty{};

	auto it = json.find(sKey);
	if (it != json.end())
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
	// reserve at least the bare size of sInput in sOutput
	sOutput.reserve(sOutput.size() + sInput.size());

	for (auto ch : sInput)
	{
		switch (ch)
		{
			case 0x08: // backspace
			{
				sOutput += '\\';
				sOutput += 'b';
				break;
			}

			case 0x09: // horizontal tab
			{
				sOutput += '\\';
				sOutput += 't';
				break;
			}

			case 0x0A: // newline
			{
				sOutput += '\\';
				sOutput += 'n';
				break;
			}

			case 0x0C: // formfeed
			{
				sOutput += '\\';
				sOutput += 'f';
				break;
			}

			case 0x0D: // carriage return
			{
				sOutput += '\\';
				sOutput += 'r';
				break;
			}

			case 0x22: // quotation mark
			{
				sOutput += '\\';
				sOutput += '\"';
				break;
			}

			case 0x5C: // reverse solidus
			{
				sOutput += '\\';
				sOutput += '\\';
				break;
			}

			default:
			{
				// escape control characters (0x00..0x1F) or, if
				// ensure_ascii parameter is used, non-ASCII characters
#if !DEKAF2_NO_GCC && DEKAF2_GCC_VERSION < 70000
				if (ch <= 0x1F)
#else
				if ((ch >= 0) && (ch <= 0x1F))
#endif
				{
					sOutput += "\\u00";
					sOutput += KString::to_hexstring(static_cast<uint32_t>(ch));
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
KString Print (const KJSON& Value) noexcept
//-----------------------------------------------------------------------------
{
	DEKAF2_TRY
	{
		switch (Value.type())
		{
			case KJSON::value_t::object:
			case KJSON::value_t::array:
				return Value.dump(-1);

			case KJSON::value_t::string:
				return Value.get<dekaf2::KJSON::string_t>();

			case KJSON::value_t::number_integer:
				return KString::to_string(Value.get<KJSON::number_integer_t>());

			case KJSON::value_t::number_unsigned:
				return KString::to_string(Value.get<KJSON::number_unsigned_t>());

			case KJSON::value_t::number_float:
				return KString::to_string(Value.get<KJSON::number_float_t>());

			case KJSON::value_t::boolean:
				return Value.get<dekaf2::KJSON::boolean_t>() ? "true" : "false";

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

} // end of namespace kjson

} // end of namespace dekaf2
