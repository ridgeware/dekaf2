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

#include <cstring>
#include "kstringutils.h"

namespace dekaf2
{

//-----------------------------------------------------------------------------
const char* KFormTimestamp (char* szBuffer, size_t iMaxBuf, time_t tTime, const char* pszFormat)
//-----------------------------------------------------------------------------
{
	struct tm ptmStruct;

	if (!tTime)
	{
		tTime = time (nullptr);
	}

	gmtime_r (&tTime, &ptmStruct);

	strftime (szBuffer, iMaxBuf, pszFormat, &ptmStruct);

	return (szBuffer);

} // FormTimestamp

//-----------------------------------------------------------------------------
const char* KFormTimestamp (time_t tTime, const char* pszFormat)
//-----------------------------------------------------------------------------
{
	enum { MAX_FT_BUF = 100 };
	static thread_local char tls_buf[MAX_FT_BUF+1];

	return KFormTimestamp(tls_buf, MAX_FT_BUF, tTime, pszFormat);
}

//-----------------------------------------------------------------------------
const char* KTranslateSeconds(char* s_szBuf, size_t MAX, int64_t iNumSeconds, bool bLongForm)
//-----------------------------------------------------------------------------
{
	int64_t iOrigSeconds{iNumSeconds};

	if (MAX)
	{
		*s_szBuf = 0;
	}
	else
	{
		return ">> error: buffer of size 0 passed to KTranslateSeconds";
	}

	if (!iNumSeconds)
	{
		strncpy (s_szBuf, "less than a second", MAX - strlen(s_szBuf) - 1);
		return (s_szBuf);
	}

	enum {
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
			if (*s_szBuf)
			{
				strncat (s_szBuf, ", ", MAX);
			}
			snprintf (s_szBuf+strlen(s_szBuf), MAX, "%d yr%s", iYears, (iYears >  1) ? "s" : "");
		}
		if (iWeeks)
		{
			if (*s_szBuf)
			{
				strncat (s_szBuf, ", ", MAX);
			}
			snprintf (s_szBuf+strlen(s_szBuf), MAX, "%d wk%s", iWeeks, (iWeeks >  1) ? "s" : "");
		}
		if (iDays)
		{
			if (*s_szBuf)
			{
				strncat (s_szBuf, ", ", MAX);
			}
			snprintf (s_szBuf+strlen(s_szBuf), MAX, "%d day%s", iDays, (iDays >  1) ? "s" : "");
		}
		if (iHours)
		{
			if (*s_szBuf)
			{
				strncat (s_szBuf, ", ", MAX);
			}
			snprintf (s_szBuf+strlen(s_szBuf), MAX, "%d hr%s", iHours, (iHours >  1) ? "s" : "");
		}
		if (iMins)
		{
			if (*s_szBuf)
			{
				strncat (s_szBuf, ", ", MAX);
			}
			snprintf (s_szBuf+strlen(s_szBuf), MAX, "%d min%s", iMins, (iMins >  1) ? "s" : "");
		}
		if (iNumSeconds)
		{
			if (*s_szBuf)
			{
				strncat (s_szBuf, ", ", MAX);
			}
			snprintf (s_szBuf+strlen(s_szBuf), MAX, "%d sec%s", (int)iNumSeconds, (iNumSeconds >  1) ? "s" : "");
		}

	}
	else // smarter, generally more useful logic: display something that makes sense
	{
		if (iOrigSeconds == SECS_PER_YEAR)
		{
			strncat (s_szBuf, "1 yr", MAX);
		}
		else if (iYears)
		{
			double nFraction = (double)iOrigSeconds / (double)SECS_PER_YEAR;
			snprintf (s_szBuf, MAX, "%.1f yrs", nFraction);
		}
		else if (iOrigSeconds == SECS_PER_WEEK)
		{
			strncat (s_szBuf, "1 wk", MAX);
		}
		else if (iWeeks)
		{
			double nFraction = (double)iOrigSeconds / (double)SECS_PER_WEEK;
			snprintf (s_szBuf, MAX, "%.1f wks", nFraction);
		}
		else if (iOrigSeconds == SECS_PER_DAY)
		{
			strncat (s_szBuf, "1 day", MAX);
		}
		else if (iDays)
		{
			double nFraction = (double)iOrigSeconds / (double)SECS_PER_DAY;
			snprintf (s_szBuf, MAX, "%.1f days", nFraction);
		}
		else if (iHours)
		{
			// show hours and minutes, but not seconds:
			snprintf (s_szBuf, MAX, "%d hr%s", iHours, (iHours >  1) ? "s" : "");
			if (iMins)
			{
				strncat (s_szBuf, ", ", MAX);
				snprintf (s_szBuf+strlen(s_szBuf), MAX, "%d min%s", iMins, (iMins >  1) ? "s" : "");
			}
		}
		else if (iMins)
		{
			// show minutes and seconds:
			snprintf (s_szBuf, MAX, "%d min%s", iMins, (iMins >  1) ? "s" : "");
			if (iNumSeconds)
			{
				strncat (s_szBuf, ", ", MAX);
				snprintf (s_szBuf+strlen(s_szBuf), MAX, "%d sec%s", (int)iNumSeconds, (iNumSeconds >  1) ? "s" : "");
			}
		}
		else if (iNumSeconds)
		{
			snprintf (s_szBuf, MAX, "%d sec%s", (int)iNumSeconds, (iNumSeconds >  1) ? "s" : "");
		}

	}

	return (s_szBuf);

}

//-----------------------------------------------------------------------------
const char* KTranslateSeconds(int64_t iNumSeconds, bool bLongForm)
//-----------------------------------------------------------------------------
{
	enum { MAX_BUF = 100 };
	static thread_local char tls_buf[MAX_BUF+1];

	return KTranslateSeconds(tls_buf, MAX_BUF, iNumSeconds, bLongForm);
}

//-----------------------------------------------------------------------------
size_t KCountChar(const char* string, const char ch) noexcept
//-----------------------------------------------------------------------------
{
	size_t ret{0};
	char v;
	while ((v = *string++))
	{
		if (v == ch) ++ret;
	}
	return ret;
}

//-----------------------------------------------------------------------------
bool KIsDecimal(const char* buf, size_t size) noexcept
//-----------------------------------------------------------------------------
{
	if (!size)
	{
		return false;
	}

	const char* start = buf;

	for (;size > 0; --size)
	{
		if (!KIsDigit(*buf++))
		{
			if (buf != start + 1 || (*start != '-' && *start != '+'))
			{
				return false;
			}
		}
	}
	return true;
}

//-----------------------------------------------------------------------------
bool KIsDecimal(const char* string) noexcept
//-----------------------------------------------------------------------------
{
	if (!*string)
	{
		return false;
	}

	const char* start = string;

	for (;;)
	{
		char ch = *string++;
		if (!ch)
		{
			return true;
		}
		if (!KIsDigit(ch))
		{
			if (string != start + 1 || (ch != '-' && ch != '+'))
			{
				return false;
			}
		}
	}
}

} // end of namespace dekaf2

