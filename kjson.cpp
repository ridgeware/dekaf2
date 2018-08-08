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

//-----------------------------------------------------------------------------
bool KJSON::Parse (KStringView sJSON)
//-----------------------------------------------------------------------------
{
	ClearError();

	if (sJSON.empty())
	{
		// avoid throwing an exception for empty input - JSON will simply
		// be empty too, so no error.
		return true;
	}

#ifdef DEKAF2_EXCEPTIONS
	clear();
	*this = LJSON::parse(sJSON.cbegin(), sJSON.cend());
	return true;
#else
	DEKAF2_TRY
	{
		clear();
		*this = LJSON::parse(sJSON.cbegin(), sJSON.cend());
		return true;
	}
	DEKAF2_CATCH (const LJSON::exception& exc)
	{
		kDebug (1, "exception was thrown.");
		return (FormError (exc));
	}
#endif

} // parse

//-----------------------------------------------------------------------------
KString KJSON::GetString(const KString& sKey)
//-----------------------------------------------------------------------------
{
	ClearError();

	KString oReturnMe;

#ifdef DEKAF2_EXCEPTIONS
	auto it = find(sKey);
	if (it != end() && it->is_string())
	{
		oReturnMe = it.value();
	}
	return (oReturnMe);
#else
	DEKAF2_TRY
	{
		auto it = find(sKey);
		if (it != end() && it->is_string())
		{
			oReturnMe = it.value();
		}
		return (oReturnMe);
	}
	DEKAF2_CATCH (const LJSON::exception& exc)
	{
		kDebug (1, "exception was thrown.");
		FormError(exc);
		return (oReturnMe);
	}
#endif

} // KJSON::GetString

//-----------------------------------------------------------------------------
KJSON KJSON::GetObject (const KString& sKey)
//-----------------------------------------------------------------------------
{
	ClearError();

	KJSON oReturnMe;

#ifdef DEKAF2_EXCEPTIONS
	auto it = find(sKey);
	if (it != end())
	{
		oReturnMe = it.value();
	}
#else
	DEKAF2_TRY
	{
		auto it = find(sKey);
		if (it != end())
		{
			oReturnMe = it.value();
		}
	}
	DEKAF2_CATCH (const LJSON::exception& exc)
	{
		kDebug (1, "exception was thrown.");
		FormError(exc);
	}
#endif

	return (oReturnMe);

} // KJSON::GetObject

//-----------------------------------------------------------------------------
bool KJSON::FormError (const LJSON::exception& exc) const
//-----------------------------------------------------------------------------
{
	m_sLastError.Printf ("JSON[%03d]: %s", exc.id, exc.what());

	return (false);

} // FormError

//-----------------------------------------------------------------------------
void KJSON::Escape (KStringView sInput, KString& sOutput)
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
KString KJSON::Escape (KStringView sInput)
//-----------------------------------------------------------------------------
{
	KString sReturn;
	Escape(sInput, sReturn);
	return sReturn;

} // Escape

//-----------------------------------------------------------------------------
KString KJSON::EscWrap (KStringView sString)
//-----------------------------------------------------------------------------
{
	KString sReturnMe;
	sReturnMe.reserve(sString.size() + 2 + 10);

	sReturnMe += '"';
	Escape(sString, sReturnMe);
	sReturnMe += '"';

	return (sReturnMe);

} // KSJON::EscWrap

//-----------------------------------------------------------------------------
KString KJSON::EscWrap (KStringView sName, KStringView sValue, KStringView sPrefix/*="\n\t"*/, KStringView sSuffix/*=","*/)
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

} // KSJON::EscWrap

//-----------------------------------------------------------------------------
KString KJSON::EscWrapNumeric (KStringView sName, KStringView sValue, KStringView sPrefix/*="\n\t"*/, KStringView sSuffix/*=","*/)
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

} // KSJON::EscWrapNumeric


KJSON::value_type KJSON::s_empty;

} // end of namespace dekaf2
