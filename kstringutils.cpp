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
#include <time.h>

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

namespace detail {

// table with base36 integer values for lower and upper case ASCII
#ifdef _MSC_VER
const uint8_t LookupBase36[256] =
#else
constexpr uint8_t LookupBase36[256] =
#endif
{
	/* 0x00 */ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	/* 0x10 */ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	/* 0x20 */ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	/* 0x30 */ 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	/* 0x40 */ 0xFF, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
	/* 0x50 */ 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	/* 0x60 */ 0xFF, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
	/* 0x70 */ 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	/* 0x80 */ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	/* 0x90 */ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	/* 0xA0 */ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	/* 0xB0 */ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	/* 0xC0 */ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	/* 0xD0 */ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	/* 0xE0 */ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	/* 0xF0 */ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
};

} // end of namespace detail

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
bool kIsBinary(KStringView sBuffer)
//-----------------------------------------------------------------------------
{
	return !Unicode::ValidUTF8(sBuffer);

} // kIsBinary

namespace {

constexpr std::array<KStringViewZ, 7> AbbreviatedWeekdays
{
	{
		"Sun",
		"Mon",
		"Tue",
		"Wed",
		"Thu",
		"Fri",
		"Sat"
	}
};

constexpr std::array<KStringViewZ, 12> AbbreviatedMonths
{
	{
		"Jan",
		"Feb",
		"Mar",
		"Apr",
		"May",
		"Jun",
		"Jul",
		"Aug",
		"Sep",
		"Oct",
		"Nov",
		"Dec"
	}
};

//-----------------------------------------------------------------------------
struct tm kGetBrokenDownTime (time_t tTime, bool bAsLocalTime)
//-----------------------------------------------------------------------------
{
	struct tm time;

	if (!tTime)
	{
		tTime = Dekaf::getInstance().GetCurrentTime();
	}

#ifdef DEKAF2_IS_WINDOWS
	if (bAsLocalTime)
	{
		localtime_s(&time, &tTime);
	}
	else
	{
		gmtime_s(&time, &tTime);
	}
#else
	if (bAsLocalTime)
	{
		localtime_r(&tTime, &time);
	}
	else
	{
		gmtime_r(&tTime, &time);
	}
#endif

	if (kWouldLog(3))
	{

#ifndef DEKAF2_IS_WINDOWS

		kDebug(3, "ix:{} d:{} m:{} y:{} h:{} m:{} s:{} offs:{} zone:{}",
			   tTime,
			   time.tm_mday,   time.tm_mon+1, time.tm_year+1900,
			   time.tm_hour,   time.tm_min,   time.tm_sec,
			   time.tm_gmtoff, time.tm_zone);

#else

		kDebug(3, "ix:{} d:{} m:{} y:{} h:{} m:{} s:{}",
			   tTime,
			   time.tm_mday, time.tm_mon+1, time.tm_year+1900,
			   time.tm_hour, time.tm_min,   time.tm_sec);

#endif

	}

	return time;

}; // kGetBrokenDownTime

//-----------------------------------------------------------------------------
KString kFormWebTimestamp (time_t tTime, KStringView sTimezoneDesignator)
//-----------------------------------------------------------------------------
{
	auto time = kGetBrokenDownTime(tTime, false);

	return kFormat("{}, {:02} {} {:04} {:02}:{:02}:{:02} {}",
				   kGetAbbreviatedWeekday(time.tm_wday),
				   time.tm_mday,
				   kGetAbbreviatedMonth(time.tm_mon),
				   time.tm_year + 1900,
				   time.tm_hour,
				   time.tm_min,
				   time.tm_sec,
				   sTimezoneDesignator);

} // kFormWebTimestamp

} // end of anonymous namespace

//-----------------------------------------------------------------------------
KStringViewZ kGetAbbreviatedWeekday(uint16_t iDay)
//-----------------------------------------------------------------------------
{
	if (iDay < AbbreviatedWeekdays.size())
	{
		return AbbreviatedWeekdays[iDay];
	}

	return {};

} // kGetAbbreviatedWeekday

//-----------------------------------------------------------------------------
KStringViewZ kGetAbbreviatedMonth(uint16_t iMonth)
//-----------------------------------------------------------------------------
{
	if (iMonth < AbbreviatedMonths.size())
	{
		return AbbreviatedMonths[iMonth];
	}

	return {};

} // kGetAbbreviatedMonth

//-----------------------------------------------------------------------------
/// Returns day of week for every gregorian date. Sunday = 0.
uint16_t kDayOfWeek(uint16_t iDay, uint16_t iMonth, uint16_t iYear)
//-----------------------------------------------------------------------------
{
	static constexpr std::array<uint16_t, 12> MonthOffsets {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};

	if (iMonth < 1 || iMonth > 12)
	{
		// prevent us from crashing
		kDebug(1, "invalid month: {}", iMonth);
		return 0;
	}

	if (iMonth < 3)
	{
		iYear -= 1;
	}

	return (iYear + iYear/4 - iYear/100 + iYear/400 + MonthOffsets[iMonth-1] + iDay) % 7;

} // kDayOfWeek

//-----------------------------------------------------------------------------
KString kFormTimestamp (time_t tTime, const char* szFormat, bool bAsLocalTime)
//-----------------------------------------------------------------------------
{
	auto time = kGetBrokenDownTime(tTime, bAsLocalTime);

	std::array<char, 100> sBuffer;

	auto iLength = strftime (sBuffer.data(), sBuffer.size(), szFormat, &time);

	return { sBuffer.data(), iLength };

} // kFormTimestamp

//-----------------------------------------------------------------------------
KString kFormHTTPTimestamp (time_t tTime)
//-----------------------------------------------------------------------------
{
	return kFormWebTimestamp(tTime, "GMT");

} // kFormHTTPTimestamp

//-----------------------------------------------------------------------------
KString kFormSMTPTimestamp (time_t tTime)
//-----------------------------------------------------------------------------
{
	return kFormWebTimestamp(tTime, "-0000");

} // kFormSMTPTimestamp

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

	int iYears = static_cast<int>(iNumSeconds / SECS_PER_YEAR);
	iNumSeconds -= (iYears * SECS_PER_YEAR);

	int iWeeks = static_cast<int>(iNumSeconds / SECS_PER_WEEK);
	iNumSeconds -= (iWeeks * SECS_PER_WEEK);

	int iDays = static_cast<int>(iNumSeconds / SECS_PER_DAY);
	iNumSeconds -= (iDays * SECS_PER_DAY);

	int iHours = static_cast<int>(iNumSeconds / SECS_PER_HOUR);
	iNumSeconds -= (iHours * SECS_PER_HOUR);

	int iMins  = static_cast<int>(iNumSeconds / SECS_PER_MIN);
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
bool kIsInteger(KStringView str, bool bSigned) noexcept
//-----------------------------------------------------------------------------
{
	if (str.empty())
	{
		return false;
	}

	const auto* start = str.data();
	const auto* buf   = start;
	size_t size       = str.size();

	while (size--)
	{
		if (!KASCII::kIsDigit(*buf++))
		{
			if (!size || buf != start + 1 || ((!bSigned || *start != '-') && *start != '+'))
			{
				return false;
			}
		}
	}

	return true;

} // kIsInteger

//-----------------------------------------------------------------------------
bool kIsFloat(KStringView str, KStringView::value_type chDecimalSeparator) noexcept
//-----------------------------------------------------------------------------
{
	if (str.empty())
	{
		return false;
	}

	const auto* start = str.data();
	const auto* buf   = start;
	size_t size       = str.size();
	bool bDeciSeen    = false;
	bool bHadDigit    = false;

	while (size--)
	{
		if (!KASCII::kIsDigit(*buf))
		{
			if (bDeciSeen)
			{
				return false;
			}
			else if (chDecimalSeparator == *buf)
			{
				bDeciSeen = true;
			}
			else if (buf != start || (*start != '-' && *start != '+'))
			{
				return false;
			}
		}
		else
		{
			bHadDigit = true;
		}
		++buf;
	}

	// to qualify as a float we need a decimal separator (we do not accept exponential representations)
	return bDeciSeen && bHadDigit;

} // kIsFloat



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

