/*
//-----------------------------------------------------------------------------//
//
// DEKAF(tm): Lighter, Faster, Smarter (tm)
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
*/

#include "kstringutils.h"
#include "kurl.h"
#include "dekaf2.h"
#include "kregex.h"

namespace dekaf2
{

//-----------------------------------------------------------------------------
bool kStrIn (const char* sNeedle, const char* sHaystack, char iDelim/*=','*/)
//-----------------------------------------------------------------------------
{
	if (!sHaystack || !sNeedle)
	{
		return false;
	}

	size_t iNeedle = 0, iHaystack = 0; // Beginning indices

	while (sHaystack[iHaystack])
	{
		iNeedle = 0;

		// Search for matching tokens
		while (  sNeedle[iNeedle] &&
			   (sNeedle[iNeedle] == sHaystack[iHaystack]))
		{
			++iNeedle;
			++iHaystack;
		}

		// If end of needle or haystack at delimiter or end of haystack
		if ( !sNeedle[iNeedle] &&
			((sHaystack[iHaystack] == iDelim) || !sHaystack[iHaystack]))
		{
			return true;
		}

		// Advance to next delimiter
		while (sHaystack[iHaystack] && sHaystack[iHaystack] != iDelim)
		{
			++iHaystack;
		}

		// Pass by the delimiter if it exists
		iHaystack += (!!sHaystack[iHaystack]);
	}
	return false;

} // kStrIn

//----------------------------------------------------------------------
bool kStrIn (KStringView sNeedle, const char* Haystack[])
//----------------------------------------------------------------------
{
	for (std::size_t ii=0; Haystack[ii] && *(Haystack[ii]); ++ii)
	{
		if (!strncmp(sNeedle.data(), Haystack[ii], sNeedle.size()))
		{
			return (true);
		}
	}

	return (false); // not found

} // kStrIn

//-----------------------------------------------------------------------------
char* KASCII::ktrimleft (const char* str)
//-----------------------------------------------------------------------------
{
	if (str)
	{
		while (std::isspace(*str))
		{
			++str;
		}
	}

	return (const_cast<char*>(str));

} // ktrimleft

//-----------------------------------------------------------------------------
char* KASCII::ktrimright (char* str)
//-----------------------------------------------------------------------------
{
	if (str && *str)
	{
		auto ii = static_cast<ssize_t>(strlen (str)) - 1;
		for (; (ii >= 0); --ii)
		{
			if (!std::isspace(str[ii]))
			{
				str[ii+1] = 0;
				return (str);
			}
		}
		*str = 0;
	}

	return (str);

} // ktrimright

//-----------------------------------------------------------------------------
char* KASCII::ktoupper (char* szString)
//-----------------------------------------------------------------------------
{
	char* szOrig = szString;
	while (szString && *szString)
	{
		if ((*szString >= 'a') && (*szString <= 'z'))
		{
			*szString -= ('a'-'A');
		}
		++szString;
	}

	return (szOrig ? szOrig : (char*) "");

} // ktoupper

//-----------------------------------------------------------------------------
char* KASCII::ktolower (char* szString)
//-----------------------------------------------------------------------------
{
	char* szOrig = szString;
	while (szString && *szString)
	{
		if ((*szString >= 'A') && (*szString <= 'Z'))
		{
			*szString += ('a'-'A');
		}
		++szString;
	}

	return (szOrig ? szOrig : (char*) "");

} // ktolower

//-----------------------------------------------------------------------------
char* kstrncpy (char* szTarget, const char* szSource, size_t iMaxAllocTarget)
//-----------------------------------------------------------------------------
{
	char* rtn = strncpy (szTarget, szSource, iMaxAllocTarget);

	if (iMaxAllocTarget > 1)
	{
		szTarget[iMaxAllocTarget-1] = 0;
	}

	return (rtn);

} // kstrncpy

//-----------------------------------------------------------------------------
uint16_t kFromHexChar(char ch) noexcept
//-----------------------------------------------------------------------------
{
	switch (ch)
	{
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			return ch - '0';
		case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
			return ch - ('A' - 10);
		case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
			return ch - ('a' - 10);
		default:
			return static_cast<uint16_t>(-1);
	}

} // kFromHexChar

//-----------------------------------------------------------------------------
KString kFormString(KStringView sInp, typename KString::value_type separator, typename KString::size_type every)
//-----------------------------------------------------------------------------
{
	KString result{sInp};

	if (every > 0)
	{
		// now insert the separator every N digits
		auto last = every;
		auto pos = result.length();
		while (pos > last)
		{
			result.insert(pos-every, 1, separator);
			pos -= every;
		}
	}
	return result;

} // kFormString

//-----------------------------------------------------------------------------
KString kFormTimestamp (time_t tTime, const char* szFormat)
//-----------------------------------------------------------------------------
{
	enum { iMaxBuf = 100 };
	char szBuffer[iMaxBuf+1];

	struct tm ptmStruct;

	if (!tTime)
	{
		tTime = Dekaf().GetCurrentTime();
	}

	gmtime_r (&tTime, &ptmStruct);

	strftime (szBuffer, iMaxBuf, szFormat, &ptmStruct);

	return { szBuffer };

} // FormTimestamp

//-----------------------------------------------------------------------------
KString kTranslateSeconds(int64_t iNumSeconds, bool bLongForm)
//-----------------------------------------------------------------------------
{
	enum { MAX = 100 };
	char szBuf[MAX+1];

	int64_t iOrigSeconds{iNumSeconds};

	*szBuf = 0;

	if (!iNumSeconds)
	{
		strncpy (szBuf, "less than a second", MAX - strlen(szBuf) - 1);
		return (szBuf);
	}

	enum
	{
		SECS_PER_YEAR  = (60*60*24*365),
		SECS_PER_WEEK  = (60*60*24*7),
		SECS_PER_DAY   = (60*60*24),
		SECS_PER_HOUR  = (60*60),
		SECS_PER_MIN   = (60)
	};

	int iYears = (iNumSeconds / SECS_PER_YEAR);
	iNumSeconds -= (iYears * SECS_PER_YEAR);

	int iWeeks = (iNumSeconds / SECS_PER_WEEK);
	iNumSeconds -= (iWeeks * SECS_PER_WEEK);

	int iDays = (iNumSeconds / SECS_PER_DAY);
	iNumSeconds -= (iDays * SECS_PER_DAY);

	int iHours = (iNumSeconds / SECS_PER_HOUR);
	iNumSeconds -= (iHours * SECS_PER_HOUR);

	int iMins  = (iNumSeconds / SECS_PER_MIN);
	iNumSeconds -= (iMins * SECS_PER_MIN);

	if (bLongForm) // e.g. "1 yr, 2 wks, 3 days, 6 hrs, 23 min, 10 sec"
	{
		if (iYears)
		{
			if (*szBuf)
			{
				strncat (szBuf, ", ", MAX);
			}
			snprintf (szBuf+strlen(szBuf), MAX, "%d yr%s", iYears, (iYears >  1) ? "s" : "");
		}
		if (iWeeks)
		{
			if (*szBuf)
			{
				strncat (szBuf, ", ", MAX);
			}
			snprintf (szBuf+strlen(szBuf), MAX, "%d wk%s", iWeeks, (iWeeks >  1) ? "s" : "");
		}
		if (iDays)
		{
			if (*szBuf)
			{
				strncat (szBuf, ", ", MAX);
			}
			snprintf (szBuf+strlen(szBuf), MAX, "%d day%s", iDays, (iDays >  1) ? "s" : "");
		}
		if (iHours)
		{
			if (*szBuf)
			{
				strncat (szBuf, ", ", MAX);
			}
			snprintf (szBuf+strlen(szBuf), MAX, "%d hr%s", iHours, (iHours >  1) ? "s" : "");
		}
		if (iMins)
		{
			if (*szBuf)
			{
				strncat (szBuf, ", ", MAX);
			}
			snprintf (szBuf+strlen(szBuf), MAX, "%d min%s", iMins, (iMins >  1) ? "s" : "");
		}
		if (iNumSeconds)
		{
			if (*szBuf)
			{
				strncat (szBuf, ", ", MAX);
			}
			snprintf (szBuf+strlen(szBuf), MAX, "%d sec%s", (int)iNumSeconds, (iNumSeconds >  1) ? "s" : "");
		}

	}
	else // smarter, generally more useful logic: display something that makes sense
	{
		if (iOrigSeconds == SECS_PER_YEAR)
		{
			strncat (szBuf, "1 yr", MAX);
		}
		else if (iYears)
		{
			double nFraction = (double)iOrigSeconds / (double)SECS_PER_YEAR;
			snprintf (szBuf, MAX, "%.1f yrs", nFraction);
		}
		else if (iOrigSeconds == SECS_PER_WEEK)
		{
			strncat (szBuf, "1 wk", MAX);
		}
		else if (iWeeks)
		{
			double nFraction = (double)iOrigSeconds / (double)SECS_PER_WEEK;
			snprintf (szBuf, MAX, "%.1f wks", nFraction);
		}
		else if (iOrigSeconds == SECS_PER_DAY)
		{
			strncat (szBuf, "1 day", MAX);
		}
		else if (iDays)
		{
			double nFraction = (double)iOrigSeconds / (double)SECS_PER_DAY;
			snprintf (szBuf, MAX, "%.1f days", nFraction);
		}
		else if (iHours)
		{
			// show hours and minutes, but not seconds:
			snprintf (szBuf, MAX, "%d hr%s", iHours, (iHours >  1) ? "s" : "");
			if (iMins)
			{
				strncat (szBuf, ", ", MAX);
				snprintf (szBuf+strlen(szBuf), MAX, "%d min%s", iMins, (iMins >  1) ? "s" : "");
			}
		}
		else if (iMins)
		{
			// show minutes and seconds:
			snprintf (szBuf, MAX, "%d min%s", iMins, (iMins >  1) ? "s" : "");
			if (iNumSeconds)
			{
				strncat (szBuf, ", ", MAX);
				snprintf (szBuf+strlen(szBuf), MAX, "%d sec%s", (int)iNumSeconds, (iNumSeconds >  1) ? "s" : "");
			}
		}
		else if (iNumSeconds)
		{
			snprintf (szBuf, MAX, "%d sec%s", (int)iNumSeconds, (iNumSeconds >  1) ? "s" : "");
		}

	}

	return { szBuf };

} // kTranslateSeconds

//-----------------------------------------------------------------------------
bool kIsInteger(KStringView str) noexcept
//-----------------------------------------------------------------------------
{
	if (str.empty())
	{
		return false;
	}

	const char* start = str.data();
	const char* buf   = start;
	size_t size       = str.size();

	while (size--)
	{
		if (!kIsDigit(*buf++))
		{
			if (buf != start + 1 || (*start != '-' && *start != '+'))
			{
				return false;
			}
		}
	}
	return true;

} // kIsDecimal

//-----------------------------------------------------------------------------
bool kIsEmail(KStringView str) noexcept
//-----------------------------------------------------------------------------
{
	if (str.empty())
	{
		return false;
	}

 	// see https://stackoverflow.com/a/201378
	static constexpr KStringView sRegex =
	R"foo((?:[a-z0-9!#$%&'*+/=?^_`{|}~-]+(?:\.[a-z0-9!#$%&'*+/=?^_`{|}~-]+)*|"(?:[\x01-\x08\x0b\x0c\x0e-\x1f\x21\x23-\x5b\x5d-\x7f]|\\[\x01-\x09\x0b\x0c\x0e-\x7f])*")@(?:(?:[a-z0-9](?:[a-z0-9-]*[a-z0-9])?\.)+[a-z0-9](?:[a-z0-9-]*[a-z0-9])?|\[(?:(?:(2(5[0-5]|[0-4][0-9])|1[0-9][0-9]|[1-9]?[0-9]))\.){3}(?:(2(5[0-5]|[0-4][0-9])|1[0-9][0-9]|[1-9]?[0-9])|[a-z0-9-]*[a-z0-9]:(?:[\x01-\x08\x0b\x0c\x0e-\x1f\x21-\x5a\x53-\x7f]|\\[\x01-\x09\x0b\x0c\x0e-\x7f])+)\]))foo";

	return KRegex::Matches(str, sRegex);

} // kIsEmail

//-----------------------------------------------------------------------------
bool kIsURL(KStringView str) noexcept
//-----------------------------------------------------------------------------
{
	return KURL(str).IsURL();
}

} // end of namespace dekaf2

