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
#include "kfrozen.h"
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
std::tm kGetBrokenDownTime (time_t tTime, bool bAsLocalTime)
//-----------------------------------------------------------------------------
{
	std::tm time;

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

		auto it = std::find(AbbreviatedMonths.begin(), AbbreviatedMonths.end(), Part[2]);

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

//-----------------------------------------------------------------------------
time_t kGetTimezoneOffset(KStringView sTimezone)
//-----------------------------------------------------------------------------
{
	// abbreviated timezone support is brittle - as there are many duplicate names,
	// and we only support english abbreviations

#ifdef DEKAF2_HAS_FROZEN
	static constexpr auto s_Timezones = frozen::make_unordered_map<KStringView, int16_t>(
#else
	static const std::unordered_map<KStringView, int16_t> s_Timezones
#endif
	{
		{ "IDLW", -12 * 60 },
		{ "BIT" , -12 * 60 },
		{ "NUT" , -11 * 60 },
		{ "HST" , -10 * 60 },
		{ "CKT" , -10 * 60 },
		{ "TAHT", -10 * 60 },
		{ "MART",  -1 * (9 * 60 + 30) },
		{ "HDT" ,  -9 * 60 },
		{ "AKST",  -9 * 60 },
		{ "AKDT",  -8 * 60 },
		{ "CIST",  -8 * 60 },
		{ "PST" ,  -8 * 60 },
		{ "MST" ,  -7 * 60 },
		{ "PDT" ,  -7 * 60 },
		{ "MDT" ,  -6 * 60 },
		{ "CST" ,  -6 * 60 }, // that is US and China and Cuba ..
		{ "GALT",  -6 * 60 },
		{ "CDT" ,  -5 * 60 },
		{ "EST" ,  -5 * 60 },
		{ "COT" ,  -5 * 60 },
		{ "PET" ,  -5 * 60 },
		{ "EDT" ,  -4 * 60 },
		{ "BOT" ,  -4 * 60 },
		{ "AMT" ,  -4 * 60 },
		{ "COST",  -4 * 60 },
		{ "CLT" ,  -4 * 60 },
		{ "FKT" ,  -4 * 60 },
		{ "GYT" ,  -4 * 60 },
		{ "PYT" ,  -4 * 60 },
		{ "VET" ,  -4 * 60 },
		{ "NST" ,  -1 * (3 * 60 + 30) },
		{ "AMST",  -3 * 60 },
		{ "ADT" ,  -3 * 60 },
		{ "ART" ,  -3 * 60 },
		{ "BRT" ,  -3 * 60 },
		{ "CLST",  -3 * 60 },
		{ "FKST",  -3 * 60 },
		{ "PYST",  -3 * 60 },
		{ "GFT" ,  -3 * 60 },
		{ "SRT" ,  -3 * 60 },
		{ "UYT" ,  -3 * 60 },
		{ "NDT" ,  -1 * (2 * 60 + 30) },
		{ "BRST",  -2 * 60 },
		{ "UYST",  -2 * 60 },
		{ "AZOT",  -1 * 60 },
		{ "CVT" ,  -1 * 60 },
		{ "EGT" ,  -1 * 60 },
		{ "UTC" ,        0 },
		{ "GMT" ,        0 },
		{ "WET" ,        0 },
		{ "EGST",        0 },
		{ "BST" ,   1 * 60 },
		{ "CET" ,   1 * 60 },
		{ "DFT" ,   1 * 60 }, // AIX uses this as synonym for CET
		{ "MET" ,   1 * 60 },
		{ "WAT" ,   1 * 60 },
		{ "WEST",   1 * 60 },
		{ "CEST",   2 * 60 },
		{ "MEST",   2 * 60 },
		{ "HAEC",   2 * 60 },
		{ "EET" ,   2 * 60 },
		{ "CAT" ,   2 * 60 },
		{ "KALT",   2 * 60 },
		{ "WAST",   2 * 60 },
		{ "SAST",   2 * 60 },
		{ "EAT" ,   3 * 60 },
		{ "FET" ,   3 * 60 },
		{ "MSK" ,   3 * 60 },
		{ "TRT" ,   3 * 60 },
		{ "EEST",   3 * 60 },
		{ "IDT" ,   3 * 60 },
		{ "IOT" ,   3 * 60 },
		{ "IRST",   3 * 60 + 30 },
		{ "GST" ,   4 * 60 },
		{ "MUT" ,   4 * 60 },
		{ "AZT" ,   4 * 60 },
		{ "GET" ,   4 * 60 },
		{ "RET" ,   4 * 60 },
		{ "SCT" ,   4 * 60 },
		{ "SAMT",   4 * 60 },
		{ "VOLT",   4 * 60 },
		{ "IRDT",   4 * 60 + 30 },
		{ "MVT" ,   5 * 60 },
		{ "PKT" ,   5 * 60 },
		{ "TJT" ,   5 * 60 },
		{ "TMT" ,   5 * 60 },
		{ "UZT" ,   5 * 60 },
		{ "ORAT",   5 * 60 },
		{ "YEKT",   5 * 60 },
		{ "SLST",   5 * 60 + 30 },
		{ "NPT" ,   5 * 60 + 45 },
		{ "BTT" ,   6 * 60 },
		{ "BIOT",   6 * 60 },
		{ "KGT" ,   6 * 60 },
		{ "OMST",   6 * 60 },
		{ "QYZT",   6 * 60 },
		{ "CCT" ,   6 * 60 + 30 },
		{ "MMT" ,   6 * 60 + 30 },
		{ "ICT" ,   7 * 60 },
		{ "THA" ,   7 * 60 },
		{ "CXT" ,   7 * 60 },
		{ "WIB" ,   7 * 60 },
		{ "HOVT",   7 * 60 },
		{ "KRAT",   7 * 60 },
		{ "NOVT",   7 * 60 },
		{ "BNT" ,   8 * 60 },
		{ "HKT" ,   8 * 60 },
		{ "SGT" ,   8 * 60 },
		{ "MYT" ,   8 * 60 },
		{ "WST" ,   8 * 60 },
		{ "PHT" ,   8 * 60 },
		{ "PHST",   8 * 60 },
		{ "AWST",   8 * 60 },
		{ "IRKT",   8 * 60 },
		{ "WITA",   8 * 60 },
		{ "ULAT",   8 * 60 },
		{ "CWST",   8 * 60 + 45 },
		{ "JST" ,   9 * 60 },
		{ "KST" ,   9 * 60 },
		{ "PWT" ,   9 * 60 },
		{ "TLT" ,   9 * 60 },
		{ "WIT" ,   9 * 60 },
		{ "YAKT",   9 * 60 },
		{ "ACST",   9 * 60 + 30 },
		{ "PGT" ,  10 * 60 },
		{ "AEST",  10 * 60 },
		{ "CHST",  10 * 60 },
		{ "CHUT",  10 * 60 },
		{ "VLAT",  10 * 60 },
		{ "ACDT",  10 * 60 + 30 },
		{ "NCT" ,  11 * 60 },
		{ "VUT" ,  11 * 60 },
		{ "AEDT",  11 * 60 },
		{ "KOST",  11 * 60 },
		{ "SAKT",  11 * 60 },
		{ "SRET",  11 * 60 },
		{ "MAGT",  12 * 60 },
		{ "MHT" ,  12 * 60 },
		{ "FJT" ,  12 * 60 },
		{ "TVT" ,  12 * 60 },
		{ "NZST",  12 * 60 },
		{ "PETT",  12 * 60 },
		{ "TKT" ,  13 * 60 },
		{ "TOT" ,  13 * 60 },
		{ "NZDT",  13 * 60 },
		{ "LINT",  14 * 60 }
	}
#ifdef DEKAF2_HAS_FROZEN
	)
#endif
	; // do not erase..

	auto tz = s_Timezones.find(sTimezone);

	if (tz == s_Timezones.end())
	{
		kDebug(2, "unrecognized time zone: {}", sTimezone);
		return -1;
	}

	return tz->second * 60;

} // kGetTimezoneOffset

//-----------------------------------------------------------------------------
time_t kParseTimestamp(KStringView sFormat, KStringView sTimestamp)
//-----------------------------------------------------------------------------
{
	//-----------------------------------------------------------------------------
	auto AddDigit = [](char ch, int iMax, int& iValue) -> bool
	//-----------------------------------------------------------------------------
	{
		if (!KASCII::kIsDigit(ch))
		{
			return false;
		}

		iValue *= 10;
		iValue += ch - '0';
		return iValue <= iMax;
	};

	auto iSize = sFormat.size();

	if (iSize != sTimestamp.size() || iSize == 0)
	{
		return 0;
	}

	KString  sMonthName;
	KString  sTimezoneName;
	std::tm  tm               {   };
	int      iTimezoneHours   { 0 };
	int      iTimezoneMinutes { 0 };
	uint16_t iTimezonePos     { 0 };
	uint16_t iYearPos         { 0 };
	uint16_t iDayPos          { 0 };
	int16_t  iTimezoneIsNeg   { 0 };

	auto iTs = sTimestamp.begin();

	for (auto chFormat : sFormat)
	{
		auto ch = *iTs++;

		// compare timestamp with format
		switch (chFormat)
		{
			case 'a': // am/pm
				ch = KASCII::kToLower(ch);

				switch (ch)
				{
					case 'a':
						if (tm.tm_hour > 11) return 0;
						break;

					case 'm':
						break;

					case 'p':
						if (tm.tm_hour > 11) return 0;
						tm.tm_hour += 12;
						break;

					default:
						return 0;
				}
				break;

			case 'h':
				// hour digit
				if (!AddDigit(ch, 23, tm.tm_hour)) return 0;
				break;

			case 'm':
				// min digit
				if (!AddDigit(ch, 59, tm.tm_min )) return 0;
				break;

			case 's':
				// sec digit
				if (!AddDigit(ch, 60, tm.tm_sec )) return 0;
				break;

			case 'S':
				// msec digit
				// we currently ignore any milliseconds value, but it has to be a digit if defined
				if (!KASCII::kIsDigit(ch)) return 0;
				break;

			case 'D':
				// day digit
				// leading spaces are allowed for dates
				if (++iDayPos == 1 && ch == ' ') break;
				if (!AddDigit(ch, 31, tm.tm_mday)) return 0;
				break;

			case 'M':
				// month digit
				if (!AddDigit(ch, 12, tm.tm_mon )) return 0;
				break;

			case 'Y':
				// year digit
				if (!AddDigit(ch, 9999, tm.tm_year)) return 0;
				++iYearPos;
				break;

			case 'N':
				// english month name (abbreviated)
				if (!KASCII::kIsAlpha(ch)) return 0;
				if (sMonthName.empty())
				{
					sMonthName += KASCII::kToUpper(ch);
				}
				else
				{
					sMonthName += KASCII::kToLower(ch);
				}
				break;

			case 'z': // PST, GMT, ..
				if (!KASCII::kIsAlpha(ch)) return 0;
				sTimezoneName += KASCII::kToUpper(ch);
				break;

			case 'Z': // -0800
				// timezone information
				switch (++iTimezonePos)
				{
					case 1:
						switch (ch)
						{
							case '+':
								iTimezoneIsNeg = +1;
								break;

							case '-':
								iTimezoneIsNeg = -1;
								break;

							default:
								return 0;
						}
						break;

					case 2:
					case 3:
						if (!AddDigit(ch, 23, iTimezoneHours)) return 0;
						break;

					case 4:
					case 5:
						if (!AddDigit(ch, 59, iTimezoneMinutes)) return 0;
						break;

					default:
						return 0;
				}
				break;

			case '?': // any char is valid
				break;

			default: // match char exactly
				if (chFormat != ch) return 0;
				break;
		}
	}

	if (tm.tm_mday == 0)
	{
		return 0;
	}

	if (iYearPos < 4 && tm.tm_year < 100)
	{
		// posix says:
		// years 069..099 are in the 20st century
		if (tm.tm_year > 68)
		{
			tm.tm_year += 1900;
		}
		// years 000..068 are in the 21st century
		else
		{
			tm.tm_year += 2000;
		}
	}

	// and correct for std::tm's base year
	tm.tm_year -= 1900;

	if (tm.tm_mon == 0)
	{
		if (sMonthName.size() != 3)
		{
			return 0;
		}

		auto it = std::find(AbbreviatedMonths.begin(), AbbreviatedMonths.end(), sMonthName);

		if (it == AbbreviatedMonths.end())
		{
			return 0;
		}

		tm.tm_mon = static_cast<int>(it - AbbreviatedMonths.begin());
	}
	else
	{
		tm.tm_mon -= 1;
	}

	time_t iTimezoneOffset = (iTimezoneHours * 60 * 60 + iTimezoneMinutes * 60) * iTimezoneIsNeg;

	if (!sTimezoneName.empty())
	{
		if (iTimezoneOffset) return 0;
		iTimezoneOffset = kGetTimezoneOffset(sTimezoneName);
		if (iTimezoneOffset == -1) return 0;
	}

	return timegm(const_cast<std::tm*>(&tm)) - iTimezoneOffset;

} // kParseTimestamp

//-----------------------------------------------------------------------------
time_t kParseTimestamp(KStringView sTimestamp)
//-----------------------------------------------------------------------------
{
	struct TimeFormat
	{
		KStringView sFormat;        // format specification
		uint16_t    iPos    { 0 };  // test character at position

	}; // TimeFormat

	// order formats by size
	static constexpr std::array<TimeFormat, 54> Formats
	{{
		{ "???, DD NNN YYYY hh:mm:ss ZZZZZ", 25 }, // WWW timestamp with timezone
		{ "???, DD NNN YYYY hh:mm:ss zzzz" , 25 }, // WWW timestamp with abbreviated timezone
		{ "???, DD NNN YYYY hh:mm:ss zzz"  , 25 }, // WWW timestamp with abbreviated timezone
		{ "YYYY-MM-DD hh:mm:ss.SSS ZZZZZ"  , 19 }, // 2018-04-13 22:08:13.211 -0700
		{ "YYYY-MM-DD hh:mm:ss,SSS ZZZZZ"  , 19 }, // 2018-04-13 22:08:13,211 -0700
		{ "YYYY NNN DD hh:mm:ss.SSS zzzz"  , 24 }, // 2017 Mar 03 05:12:41.211 CEST
		{ "YYYY NNN DD hh:mm:ss.SSS zzz"   , 24 }, // 2017 Mar 03 05:12:41.211 PDT
		{ "YYYY-MM-DD hh:mm:ss.SSSZZZZZ"   , 19 }, // 2018-04-13 22:08:13.211-0700
		{ "YYYY-MM-DD hh:mm:ss,SSSZZZZZ"   , 19 }, // 2018-04-13 22:08:13,211-0700
		{ "DD/NNN/YYYY:hh:mm:ss ZZZZZ"     , 11 }, // 19/Apr/2017:06:36:15 -0700
		{ "DD/NNN/YYYY hh:mm:ss ZZZZZ"     , 11 }, // 19/Apr/2017 06:36:15 -0700
		{ "NNN DD hh:mm:ss ZZZZZ YYYY"     , 15 }, // Jan 21 18:20:11 +0000 2017
		{ "YYYY-MM-DD hh:mm:ss ZZZZZ"      , 19 }, // 2017-10-14 22:11:20 +0000
		{ "NNN DD, YYYY hh:mm:ss aa"       , 21 }, // Dec 02, 2017 2:39:58 AM
		{ "YYYY-MM-DD hh:mm:ssZZZZZ"       , 10 }, // 2017-10-14 22:11:20+0000
		{ "YYYY-MM-DDThh:mm:ssZZZZZ"       , 10 }, // 2017-10-14T22:11:20+0000
		{ "YYYY-MM-DDThh:mm:ss.SSS?"       , 19 }, // 2002-12-06T19:23:15.372Z
		{ "YYYY NNN DD hh:mm:ss.SSS"       ,  4 }, // 2002 Dec 06 19:23:15.372
		{ "DD-NNN-YYYY hh:mm:ss.SSS"       ,  2 }, // 17-Apr-1998 14:32:12.372
		{ "YYYY NNN DD hh:mm:ss zzz"       , 20 }, // 2017 Mar 03 05:12:41 PDT
		{ "YYYY-MM-DD hh:mm:ss zzz"        , 19 }, // 2017-10-14 22:11:20 PDT
		{ "YYYY-MM-DD hh:mm:ss.SSS"        , 10 }, // 2002-12-06 19:23:15.372
		{ "YYYY-MM-DDThh:mm:ss.SSS"        , 10 }, // 2002-12-06T19:23:15.372
		{ "YYYY-MM-DD hh:mm:ss,SSS"        , 19 }, // 2002-12-06 19:23:15,372
		{ "YYYY-MM-DD*hh:mm:ss:SSS"        , 10 }, // 2002-12-06*19:23:15:372
		{ "YYYYMMDD hh:mm:ss.SSS"          , 17 }, // 20211230 12:23:54.372
		{ "NNN DD YYYY hh:mm:ss"           ,  3 }, // May 01 1967 14:16:24
		{ "hh:mm:ss NNN DD YYYY"           ,  2 }, // 14:16:24 May 01 1967
		{ "NNN DD hh:mm:ss YYYY"           ,  9 }, // Apr 17 00:00:35 2010
		{ "YYYY-MM-DDThh:mm:ss?"           , 10 }, // 2002-12-06T19:23:15Z
		{ "DD NNN YYYY hh:mm:ss"           ,  2 }, // 17 Apr 1998 14:32:12
		{ "DD-NNN-YYYY hh:mm:ss"           ,  2 }, // 17-Apr-1998 14:32:12
		{ "DD/NNN/YYYY hh:mm:ss"           , 11 }, // 17/Apr/1998 14:32:12
		{ "DD/NNN/YYYY:hh:mm:ss"           , 11 }, // 17/Apr/1998:14:32:12
		{ "YYYY-MM-DD hh:mm:ss"            , 10 }, // 2002-12-06 19:23:15
		{ "YYYY-MM-DDThh:mm:ss"            , 10 }, // 2002-12-06T19:23:15
		{ "YYYY-MM-DD*hh:mm:ss"            ,  4 }, // 2002-12-06*19:23:15
		{ "YYYY/MM/DD hh:mm:ss"            , 10 }, // 2002/12/31 23:59:59
		{ "YYYY/MM/DD*hh:mm:ss"            , 10 }, // 2002/12/31*23:59:59
		{ "hh:mm:ss DD-MM-YYYY"            , 11 }, // 17:12:34 30-12-2002
		{ "DD-MM-YYYY hh:mm:ss"            ,  2 }, // 30-12-2002 17:12:34
		{ "hh:mm:ss DD.MM.YYYY"            , 11 }, // 17:12:34 30.12.2002
		{ "DD.MM.YYYY hh:mm:ss"            ,  2 }, // 30.12.2002 17:12:34
		{ "hh:mm:ss DD/MM/YYYY"            ,  2 }, // 17:12:34 30/12/2002
		{ "DD/MM/YYYY hh:mm:ss"            ,  2 }, // 30/12/2002 17:12:34
		// we do not add the US form MM/DD/YYYY hh:mm:ss here as it causes too many ambiguities -
		// (for any first 12 days of each month) - if you want to decode it you have to do it
		// explicitly based on the input source or the set locale
		{ "hh:mm:ss DD-MM-YY"              , 11 }, // 17:12:34 30-12-02
		{ "DD-MM-YY hh:mm:ss"              ,  2 }, // 30-12-02 17:12:34
		{ "hh:mm:ss DD.MM.YY"              , 11 }, // 17:12:34 30.12.02
		{ "DD.MM.YY hh:mm:ss"              ,  2 }, // 30.12.02 17:12:34
		{ "hh:mm:ss DD/MM/YY"              ,  2 }, // 17:12:34 30/12/02
		{ "DD/MM/YY hh:mm:ss"              ,  2 }, // 30/12/02 17:12:34
		{ "YYYYMMDDThhmmss?"               ,  8 }, // 20021230T171234Z
		{ "YYMMDD hh:mm:ss"                ,  6 }, // 211230 12:23:54
		{ "YYYYMMDDhhmmss"                 ,  0 }, // 20021230171234
	}};

	auto iSize = sTimestamp.size();

	for (auto Format : Formats)
	{
		auto iFSize = Format.sFormat.size();

		if (iSize == iFSize)
		{
			auto iCheckPos = Format.iPos;

			if (iCheckPos == 0 || Format.sFormat[iCheckPos] == sTimestamp[iCheckPos])
			{
				auto tTime = kParseTimestamp(Format.sFormat, sTimestamp);

				if (tTime)
				{
					return tTime;
				}
			}
		}
		else if (iSize > iFSize)
		{
			break;
		}
	}

	return 0;

} // kParseTimestamp

namespace detail {

//-----------------------------------------------------------------------------
KBrokenDownTime::KBrokenDownTime(time_t tGMTime, bool bAsLocalTime)
//-----------------------------------------------------------------------------
: m_time(kGetBrokenDownTime(tGMTime, bAsLocalTime))
{
} // ctor

//-----------------------------------------------------------------------------
uint16_t KBrokenDownTime::GetWeekday() const
//-----------------------------------------------------------------------------
{
	// we do not use the tm_wday value as it is not updated
	// when the date is modified by the setters - instead we
	// compute the weekday on the fly from the date
	return kDayOfWeek(GetDay(), GetMonth(), GetYear());

} // GetWeekday

//-----------------------------------------------------------------------------
KStringViewZ KBrokenDownTime::GetDayName() const
//-----------------------------------------------------------------------------
{
	return kGetAbbreviatedWeekday(GetWeekday());

} // GetDayName

//-----------------------------------------------------------------------------
KStringViewZ KBrokenDownTime::GetMonthName() const
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
	return mktime(const_cast<std::tm*>(&m_time));

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

	return static_cast<int32_t>(timegm(const_cast<std::tm*>(&m_time)) - ToTimeT());

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
	return timegm(const_cast<std::tm*>(&m_time));

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
KString kFormTimestamp (const std::tm& time, const char* szFormat)
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
			iBias = TZID.Bias * 60;
		}
		else
		{
			kDebug(2, "cannot read time zone information");
			iBias = static_cast<int32_t>(timegm(const_cast<std::tm*>(&time)) - mktime(const_cast<std::tm*>(&time)));
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
