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

namespace dekaf2 {

namespace kjson {

//-----------------------------------------------------------------------------
bool Parse (KJSON& json, KStringView sJSON, KString& sError)
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

} // kParse

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
bool Parse (KJSON& json, KInStream& InStream, KString& sError)
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

} // kParse

//-----------------------------------------------------------------------------
KString GetString(const KJSON& json, KStringView sKey)
//-----------------------------------------------------------------------------
{
	KString sReturn;

	DEKAF2_TRY
	{
		auto it = json.find(sKey);
		if (it != json.end() && it->is_string())
		{
			sReturn = it.value();
		}
	}
	DEKAF2_CATCH (const KJSON::exception& exc)
	{
		kDebugLog(1, "JSON[%03d]: %s", exc.id, exc.what());
	}

	return sReturn;

} // kGetString

//-----------------------------------------------------------------------------
KJSON GetObject (const KJSON& json, KStringView sKey)
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
		kDebugLog(1, "JSON[%03d]: %s", exc.id, exc.what());
	}

	return (oReturnMe);

} // kGetObject

//-----------------------------------------------------------------------------
bool Add (KJSON& json, const KROW& row)
//-----------------------------------------------------------------------------
{
	DEKAF2_TRY
	{
		if (json.is_array())
		{
			// current object is an array: place KROW as another array element:
			KJSON child;
			Add (child, row);
			json += child;
		}
		else
		{
			// merge KROW elements into current level:
			for (size_t ii=0; ii < row.size(); ++ii)
			{
				kDebugLog (2, "kjson::Add: {}", row.ColumnInfoForLogOutput(ii));

				if (row.IsFlag (ii, KROW::NONCOLUMN))
				{
					continue;
				}

				const auto& sName  = row.GetName(ii);
				const auto& sValue = row.GetValue(ii);

				if (sName.empty())
				{
					continue;
				}

				if (row.IsFlag (ii, KROW::BOOLEAN))
				{
					json[sName] = sValue.Bool();
				}
				else if (row.IsFlag (ii, KROW::NUMERIC))
				{
					if (sValue.Contains("."))
					{
						json[sName] = sValue.Float();
					}
					else
					{
						json[sName] = sValue.UInt64();
					}
				}
				else // catch-all logic for all string values
				{
					json[sName] = sValue;
				}
			}
		}
	}

	DEKAF2_CATCH (const KJSON::exception& exc)
	{
		kDebugLog(1, "JSON[%03d]: %s", exc.id, exc.what());
		return false;
	}

	return true;

} // kAdd

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
KString EscWrap (KStringView sString)
//-----------------------------------------------------------------------------
{
	KString sReturnMe;
	sReturnMe.reserve(sString.size() + 2 + 10);

	sReturnMe += '"';
	Escape(sString, sReturnMe);
	sReturnMe += '"';

	return (sReturnMe);

} // EscWrap

//-----------------------------------------------------------------------------
KString EscWrap (KStringView sName, KStringView sValue, KStringView sPrefix/*="\n\t"*/, KStringView sSuffix/*=","*/)
//-----------------------------------------------------------------------------
{
	KString sReturnMe;
	sReturnMe.reserve(sPrefix.size()
	                + sName.size()
	                + sValue.size()
	                + sSuffix.size()
	                + 6 + 10);

	sReturnMe += sPrefix;
	sReturnMe += '"';
	Escape(sName, sReturnMe);
	sReturnMe += "\": \"";
	Escape(sValue, sReturnMe);
	sReturnMe += '"';
	sReturnMe += sSuffix;

	return (sReturnMe);

} // EscWrap

//-----------------------------------------------------------------------------
KString EscWrapNumeric (KStringView sName, KStringView sValue, KStringView sPrefix/*="\n\t"*/, KStringView sSuffix/*=","*/)
//-----------------------------------------------------------------------------
{
	KString sReturnMe;
	sReturnMe.reserve(sPrefix.size()
	                + sName.size()
	                + sValue.size()
	                + sSuffix.size()
	                + 4 + 10);

	sReturnMe += sPrefix;
	sReturnMe += '"';
	Escape(sName, sReturnMe);
	sReturnMe += "\": ";
	Escape(sValue, sReturnMe);
	sReturnMe += sSuffix;

	return (sReturnMe);

} // EscWrapNumeric

} // end of namespace kjson

} // end of namespace dekaf2
