/*
 //
 // DEKAF(tm): Lighter, Faster, Smarter (tm)
 //
 // Copyright (c) 2021, Ridgeware, Inc.
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

#include "ktime.h"
#include "kstringutils.h"
#include "dekaf2.h"
#include "bits/kcppcompat.h"

#ifdef DEKAF2_IS_WINDOWS
	#include <windows.h>
	#include <timezoneapi.h>
	#ifdef GetCurrentTime
		#undef GetCurrentTime
	#endif
#endif

namespace dekaf2 {

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

	kDebug(3, "ix:{} d:{} m:{} y:{} h:{} m:{} s:{}",
		   tTime,
		   time.tm_mday, time.tm_mon+1, time.tm_year+1900,
		   time.tm_hour, time.tm_min,   time.tm_sec);

#else

	if (bAsLocalTime)
	{
		localtime_r(&tTime, &time);
	}
	else
	{
		gmtime_r(&tTime, &time);
	}

	kDebug(3, "ix:{} d:{} m:{} y:{} h:{} m:{} s:{} offs:{} zone:{}",
		   tTime,
		   time.tm_mday,   time.tm_mon+1, time.tm_year+1900,
		   time.tm_hour,   time.tm_min,   time.tm_sec,
		   time.tm_gmtoff, time.tm_zone);

#endif

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

//-----------------------------------------------------------------------------
time_t kParseWebTimestamp (KStringView sTime, bool bOnlyGMT)
//-----------------------------------------------------------------------------
{
	time_t tTime { 0 };

	// "Tue, 03 Aug 2021 10:23:42 GMT"
	// "Tue, 03 Aug 2021 10:23:42 -0500"

	auto Part = sTime.Split(" ,:");

	if (Part.size() == 8)
	{
		struct tm time;

		time.tm_mday = Part[1].UInt16();

		auto it = std::find_if(AbbreviatedMonths.begin(), AbbreviatedMonths.end(), [&Part](const KStringViewZ mon)
		{
			return mon == Part[2];
		});

		if (it != AbbreviatedMonths.end())
		{
			time.tm_mon    = static_cast<int>(it - AbbreviatedMonths.begin());
			time.tm_year   = Part[3].UInt16() - 1900;
			time.tm_hour   = Part[4].UInt16();
			time.tm_min    = Part[5].UInt16();
			time.tm_sec    = Part[6].UInt16();
			auto sTimezone = Part[7];

			if (bOnlyGMT && sTimezone == "GMT")
			{
				tTime = timegm(&time);
			}
			else
			{
				// check for numeric timezone, like -0200

				if (kIsInteger(sTimezone))
				{
					bool bMinus = sTimezone.front() == '-';

					if (bMinus || sTimezone.front() == '+')
					{
						sTimezone.remove_prefix(1);
					}

					if (sTimezone.size() == 4)
					{
						auto   iHours   = sTimezone.Left(2).UInt16();
						auto   iMinutes = sTimezone.Right(2).UInt16();
						time_t iOffset  = iHours * 60 * 60 + iMinutes * 60;

						if (iOffset < 24*60*60)
						{
							if (bMinus)
							{
								iOffset *= -1;
							}

							tTime = timegm(&time) + iOffset;
						}
					}
				}
			}
		}
	}

	if (!tTime && !sTime.empty())
	{
		kDebug(2, "bad timestamp: {}", sTime);
	}

	return tTime;

} // kParseWebTimestamp

} // end of anonymous namespace

namespace detail {

//-----------------------------------------------------------------------------
KBrokenDownTimeBase::KBrokenDownTimeBase(time_t tGMTime, bool bAsLocalTime)
//-----------------------------------------------------------------------------
: m_time(kGetBrokenDownTime(tGMTime, bAsLocalTime))
{
} // ctor

//-----------------------------------------------------------------------------
uint16_t KBrokenDownTimeBase::GetWeekday() const
//-----------------------------------------------------------------------------
{
	// we do not use the tm_wday value as it is not updated
	// when the date is modified by the setters - instead we
	// compute the weekday on the fly from the date
	return kDayOfWeek(GetDay(), GetMonth(), GetYear());

} // GetWeekday

//-----------------------------------------------------------------------------
KStringViewZ KBrokenDownTimeBase::GetDayName() const
//-----------------------------------------------------------------------------
{
	return kGetAbbreviatedWeekday(GetWeekday());

} // GetDayName

//-----------------------------------------------------------------------------
KStringViewZ KBrokenDownTimeBase::GetMonthName() const
//-----------------------------------------------------------------------------
{
	return kGetAbbreviatedMonth(m_time.tm_mon);

} // GetMonthName

} // end of namespace detail

//-----------------------------------------------------------------------------
KLocalTime::KLocalTime(const KUTCTime& gmtime)
//-----------------------------------------------------------------------------
: KLocalTime(gmtime.ToTimeT())
{
}

//-----------------------------------------------------------------------------
time_t KLocalTime::ToTimeT() const
//-----------------------------------------------------------------------------
{
	return mktime(const_cast<::tm*>(&m_time));

} // ToTimeT

//-----------------------------------------------------------------------------
int32_t KLocalTime::GetUTCOffset() const
//-----------------------------------------------------------------------------
{
#ifdef DEKAF2_IS_UNIX

	return m_time.tm_gmtoff;

#else

	/* The problem with GetDynamicTimeZoneInformation() is that it does not
	 * return the correct timezone for the user, only for the system. Therefore
	 * we use the costly approach to compute the diff through calling both
	 * mktime() and timegm()..

	DYNAMIC_TIME_ZONE_INFORMATION TZID;
	auto iTZID = GetDynamicTimeZoneInformation(&TZID);
	if (iTZD != TIME_ZONE_ID_INVALID)
	{
		return TZID.Bias * 60;
	}

	kDebug(2, "cannot read time zone information");

	 *
	 */

	return static_cast<int32_t>(timegm(const_cast<::tm*>(&m_time)) - ToTimeT());

#endif

} // GetUTCOffset

//-----------------------------------------------------------------------------
KUTCTime::KUTCTime(const KLocalTime& localtime)
//-----------------------------------------------------------------------------
: KUTCTime(localtime.ToTimeT())
{
}

//-----------------------------------------------------------------------------
time_t KUTCTime::ToTimeT() const
//-----------------------------------------------------------------------------
{
	return timegm(const_cast<::tm*>(&m_time));

} // ToTimeT

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
	static constexpr std::array<uint16_t, 12> MonthOffsets { 0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4 };

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
KString kFormTimestamp (const ::tm& time, const char* szFormat)
//-----------------------------------------------------------------------------
{
	std::array<char, 100> sBuffer;

	auto iLength = strftime (sBuffer.data(), sBuffer.size(), szFormat, &time);

	if (iLength == sBuffer.size())
	{
		kWarning("format string too long: {}", szFormat);
	}

	return { sBuffer.data(), iLength };

} // kFormTimestamp

//-----------------------------------------------------------------------------
KString kFormTimestamp (time_t tTime, const char* szFormat, bool bAsLocalTime)
//-----------------------------------------------------------------------------
{
	return kFormTimestamp(kGetBrokenDownTime(tTime, bAsLocalTime), szFormat);

} // kFormTimestamp

//-----------------------------------------------------------------------------
KString kFormCommonLogTimestamp(time_t tTime, bool bAsLocalTime)
//-----------------------------------------------------------------------------
{
	auto time = kGetBrokenDownTime(tTime, bAsLocalTime);

	if (!bAsLocalTime)
	{
		// [18/Sep/2011:19:18:28 +0000]
		return kFormat("[{:02}/{}/{:04}:{:02}:{:02}:{:02} +0000]",
					   time.tm_mday,
					   kGetAbbreviatedMonth(time.tm_mon),
					   time.tm_year + 1900,
					   time.tm_hour,
					   time.tm_min,
					   time.tm_sec);
	}
	else
	{

#ifdef DEKAF2_IS_WINDOWS

		int32_t iBias { 0 };
		DYNAMIC_TIME_ZONE_INFORMATION TZID;
		auto iTZID          = GetDynamicTimeZoneInformation(&TZID);
		if (iTZID != TIME_ZONE_ID_INVALID)
		{
			iBias = pTZID.Bias * 60;
		}
		else
		{
			kDebug(2, "cannot read time zone information");
			iBias = static_cast<int32_t>(timegm(const_cast<::tm*>(&time)) - mktime(const_cast<::tm*>(&time)));
		}
		char chSign         = iBias < 0 ? '-' : '+';
		auto iMinutes       = abs(iBias / 60);
		auto iHoursOffset   = iMinutes / 60;
		auto iMinutesOffset = iMinutes % 60;

#else

		char chSign         = time.tm_gmtoff < 0 ? '-' : '+';
		auto iMinutesOffset = abs(time.tm_gmtoff / 60);
		auto iHoursOffset   = iMinutesOffset / 60;
		iMinutesOffset      = iMinutesOffset % 60;

#endif

		// [18/Sep/2011:19:18:28 -0400]
		return kFormat("[{:02}/{}/{:04}:{:02}:{:02}:{:02} {}{:02}{:02}]",
					   time.tm_mday,
					   kGetAbbreviatedMonth(time.tm_mon),
					   time.tm_year + 1900,
					   time.tm_hour,
					   time.tm_min,
					   time.tm_sec,
					   chSign,
					   iHoursOffset,
					   iMinutesOffset);
	}

} // kFormCommonLogTimestamp

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
time_t kParseHTTPTimestamp (KStringView sTime)
//-----------------------------------------------------------------------------
{
	return kParseWebTimestamp(sTime, true);

} // kParseHTTPTimestamp

//-----------------------------------------------------------------------------
time_t kParseSMTPTimestamp (KStringView sTime)
//-----------------------------------------------------------------------------
{
	return kParseWebTimestamp(sTime, false);

} // kParseSMTPTimestamp

//-----------------------------------------------------------------------------
KString kTranslateSeconds(uint64_t iNumSeconds, bool bLongForm)
//-----------------------------------------------------------------------------
{
	KString sOut;

	//-----------------------------------------------------------------------------
	auto PrintInt = [&sOut](const char* sLabel, uint64_t iValue)
	//-----------------------------------------------------------------------------
	{
		if (iValue)
		{
			if (!sOut.empty())
			{
				sOut += ", ";
			}

			sOut += kIntToString(iValue);
			sOut += ' ';
			sOut += sLabel;

			if (iValue > 1)
			{
				sOut += 's';
			}
		}

	}; // PrintInt

	//-----------------------------------------------------------------------------
	auto PrintFloat = [&sOut](const char* sLabel, uint64_t iValue, uint64_t iDivider)
	//-----------------------------------------------------------------------------
	{
		if (iValue == iDivider)
		{
			sOut = "1";
			sOut += ' ';
			sOut += sLabel;
		}
		else
		{
			sOut = kFormat("{:.1f} {}s", (double)iValue / (double)iDivider, sLabel);
		}

	}; // PrintFloat

	enum
	{
		SECS_PER_YEAR  = (60*60*24*365),
		SECS_PER_WEEK  = (60*60*24*7),
		SECS_PER_DAY   = (60*60*24),
		SECS_PER_HOUR  = (60*60),
		SECS_PER_MIN   = (60)
	};

	if (!iNumSeconds)
	{
		sOut = "less than a second";
	}
	else if (bLongForm) // e.g. "1 yr, 2 wks, 3 days, 6 hrs, 23 min, 10 sec"
	{
		auto iYears  = iNumSeconds / SECS_PER_YEAR;
		iNumSeconds -= (iYears * SECS_PER_YEAR);

		auto iWeeks  = iNumSeconds / SECS_PER_WEEK;
		iNumSeconds -= (iWeeks * SECS_PER_WEEK);

		auto iDays   = iNumSeconds / SECS_PER_DAY;
		iNumSeconds -= (iDays * SECS_PER_DAY);

		auto iHours  = iNumSeconds / SECS_PER_HOUR;
		iNumSeconds -= (iHours * SECS_PER_HOUR);

		auto iMins   = iNumSeconds / SECS_PER_MIN;
		iNumSeconds -= (iMins * SECS_PER_MIN);

		PrintInt ("yr"  , iYears     );
		PrintInt ("wk"  , iWeeks     );
		PrintInt ("day" , iDays      );
		PrintInt ("hr"  , iHours     );
		PrintInt ("min" , iMins      );
		PrintInt ("sec" , iNumSeconds);
	}
	else // smarter, generally more useful logic: display something that makes sense
	{
		if (iNumSeconds >= SECS_PER_YEAR)
		{
			PrintFloat ("yr" , iNumSeconds, SECS_PER_YEAR);
		}
		else if (iNumSeconds >= SECS_PER_WEEK)
		{
			PrintFloat ("wk" , iNumSeconds, SECS_PER_WEEK);
		}
		else if (iNumSeconds >= SECS_PER_DAY)
		{
			PrintFloat ("day", iNumSeconds, SECS_PER_DAY);
		}
		else if (iNumSeconds >= SECS_PER_HOUR)
		{
			// show hours and minutes, but not seconds:
			auto iHours  = iNumSeconds / SECS_PER_HOUR;
			iNumSeconds -= (iHours * SECS_PER_HOUR);
			auto iMins   = iNumSeconds / SECS_PER_MIN;

			PrintInt ("hr" , iHours);
			PrintInt ("min", iMins);
		}
		else
		{
			// show minutes and seconds, or only seconds:
			auto iMins   = iNumSeconds / SECS_PER_MIN;
			iNumSeconds -= (iMins * SECS_PER_MIN);

			PrintInt ("min", iMins);
			PrintInt ("sec", iNumSeconds);
		}
	}

	return sOut;

} // kTranslateSeconds

} // end of namespace dekaf2
