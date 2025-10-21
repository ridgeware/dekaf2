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

#include "kduration.h"
#include "klog.h"
#if DEKAF2_HAS_INCLUDE("kctype.h")
	#define DEKAF2_KDURATION_HAS_KCTYPE 1
	#include "kctype.h"
#else
	#include <ctype.h>
#endif
#if DEKAF2_HAS_INCLUDE("kformat.h")
	#include "kformat.h"
#else
	#include <cstdio> // for snprintf
	#include <array>
	#include <algorithm>
#endif

DEKAF2_NAMESPACE_BEGIN

#ifdef DEKAF2_IS_MSC
KStopTime::ConstructHalted KStopTime::Halted;
KStopWatch::ConstructHalted KStopWatch::Halted;
#endif

//-----------------------------------------------------------------------------
KString KDuration::ToString(Format Format, BaseInterval Interval, uint8_t iPrecision) const
//-----------------------------------------------------------------------------
{
	KString sOut;

	using int_t = Duration::rep;

	//-----------------------------------------------------------------------------
	auto PrintInt = [&sOut](KStringView sLabel, int_t iValue, char chPlural = 's')
	//-----------------------------------------------------------------------------
	{
		if (iValue)
		{
			if (!sOut.empty() && sOut != "-")
			{
				sOut += ", ";
			}

#ifndef DEKAF2_KSTRING_IS_STD_STRING
			sOut += KString::to_string(iValue);
#else
			sOut += std::to_string(iValue);
#endif
			sOut += ' ';
			sOut += sLabel;

			if (iValue > 1 && chPlural)
			{
				sOut += chPlural;
			}
		}

	}; // PrintInt

	//-----------------------------------------------------------------------------
	auto PrintShortInt = [&sOut](KStringView sLabel, int_t iValue)
	//-----------------------------------------------------------------------------
	{
		if (iValue)
		{
#ifndef DEKAF2_KSTRING_IS_STD_STRING
			sOut += KString::to_string(iValue);
#else
			sOut += std::to_string(iValue);
#endif
			sOut += sLabel;
		}

	}; // PrintInt

	//-----------------------------------------------------------------------------
	auto PrintFloat = [&sOut,&PrintInt](KStringView sLabel, int_t iValue, int_t iDivider, uint8_t iPrecision, char chPlural = 's')
	//-----------------------------------------------------------------------------
	{
		// we use chPlural as a flag here - if it is 0, no abbreviated value representation
		// is wanted (into which we otherwise flip if there is no remainder after division)
		if (chPlural && iValue % iDivider == 0)
		{
			PrintInt(sLabel, iValue / iDivider, chPlural);
		}
		else
		{
#ifdef DEKAF2_FORMAT_NAMESPACE
			auto f = (double)iValue / (double)iDivider;

			sOut = kFormat("{:.{}f} {}", f, iPrecision, sLabel);
#else
			const char* sFormat;

			if (iPrecision >= 3)
			{
				sFormat = "%3f";
			}
			else if (iPrecision == 2)
			{
				sFormat = "%2f";
			}
			else if (iPrecision == 1)
			{
				sFormat = "%1f";
			}
			else if (iPrecision == 0)
			{
				sFormat = "%0f";
			}
			
			std::array<char, 20> sBuffer;

			auto iWrote = snprintf(sBuffer.data(), sBuffer.size(), sFormat, (double)iValue / (double)iDivider);

			if (iWrote > 0)
			{
				sOut.append(sBuffer.data(), std::min(static_cast<int>(sBuffer.size()), iWrote));
			}

			sOut += ' ';
			sOut += sLabel;
#endif
			if (chPlural)
			{
				sOut += chPlural;
			}
		}

	}; // PrintFloat

	//-----------------------------------------------------------------------------
	auto IntervalToString = [](BaseInterval Interval, bool bLong) -> KStringView
	//-----------------------------------------------------------------------------
	{
		if (!bLong)
		{
			switch (Interval)
			{
				case NanoSeconds : return "ns";
				case MicroSeconds: return "µs";
				case MilliSeconds: return "ms";
				case Seconds     : return "s";
				case Minutes     : return "m";
				case Hours       : return "h";
				case Days        : return "d";
				case Weeks       : return "w";
				case Years       : return "y";
			}
		}
		else
		{
			switch (Interval)
			{
				case NanoSeconds : return "nanosecond";
				case MicroSeconds: return "microsecond";
				case MilliSeconds: return "millisecond";
				case Seconds     : return "second";
				case Minutes     : return "minute";
				case Hours       : return "hour";
				case Days        : return "day";
				case Weeks       : return "week";
				case Years       : return "year";
			}
		}
		return ""; // gcc ..
	};

	static constexpr int_t NANOSECS_PER_MICROSEC = (1000);
	static constexpr int_t NANOSECS_PER_MILLISEC = (1000*1000);
	static constexpr int_t NANOSECS_PER_SEC      = (1000*1000*1000);
	static constexpr int_t NANOSECS_PER_MIN      = (60*NANOSECS_PER_SEC);
	static constexpr int_t NANOSECS_PER_HOUR     = (60*60*NANOSECS_PER_SEC);
	static constexpr int_t NANOSECS_PER_DAY      = (60*60*24*NANOSECS_PER_SEC);
	static constexpr int_t NANOSECS_PER_WEEK     = (60*60*24*7*NANOSECS_PER_SEC);
	static constexpr int_t NANOSECS_PER_YEAR     = (60*60*24*365*NANOSECS_PER_SEC);

	int_t iNanoSecs   = nanoseconds().count();
	bool  bIsNegative = iNanoSecs < 0;

	if (iNanoSecs == 0)
	{
		if (Format == Format::Brief)
		{
			if (iPrecision == 1) --iPrecision;
			PrintFloat(IntervalToString(Interval, false), 0, 1, iPrecision, 0);
		}
		else if (Format == Format::Condensed)
		{
			sOut  = "0";
			sOut += IntervalToString(Interval, false);;
		}
		else
		{
			sOut  = "less than a ";
			sOut += IntervalToString(Interval, true);
		}
		return sOut;
	}

	auto PrintLong = [&iNanoSecs,&PrintInt](KStringView sLabel, int_t iDivider)
	{
		auto iValue = iNanoSecs / iDivider;
		iNanoSecs  -= (iValue * iDivider);
		PrintInt (sLabel, iValue );
	};

	auto PrintShort = [&iNanoSecs,&PrintShortInt](KStringView sLabel, int_t iDivider)
	{
		auto iValue = iNanoSecs / iDivider;
		iNanoSecs  -= (iValue * iDivider);
		PrintShortInt (sLabel, iValue );
	};

	switch (Format)
	{
		// e.g. "1 yr, 2 wks, 3 days, 6 hrs, 23 min, 10 sec"
		case Format::Long:
		{
			if (bIsNegative)
			{
				sOut = "-";

				if (iNanoSecs > (std::numeric_limits<int_t>::min)())
				{
					iNanoSecs *= -1;
				}
				else
				{
					sOut = "a very long negative time";
					break;
				}
			}
			
			PrintLong("yr" , NANOSECS_PER_YEAR);
			PrintLong("wk" , NANOSECS_PER_WEEK);
			PrintLong("day", NANOSECS_PER_DAY );
			PrintLong("hr" , NANOSECS_PER_HOUR);
			PrintLong("min", NANOSECS_PER_MIN );
			PrintLong("sec", NANOSECS_PER_SEC );

			if (iNanoSecs)
			{
				PrintLong("msec", NANOSECS_PER_MILLISEC);
				PrintLong("µsec", NANOSECS_PER_MICROSEC);
				PrintInt ("nsec", iNanoSecs);
			}
		}
		break;

		// e.g. "2w3d6h23m10s" or "-2w3d6h23m10s"
		case Format::Condensed:
		{
			if (bIsNegative)
			{
				sOut = "-";

				if (iNanoSecs > (std::numeric_limits<int_t>::min)())
				{
					iNanoSecs *= -1;
				}
				else
				{
					sOut = "< -292.5 y";
					break;
				}
			}
			// how long is a year?
//			PrintShort("y", NANOSECS_PER_YEAR);
			PrintShort("w", NANOSECS_PER_WEEK);
			PrintShort("d", NANOSECS_PER_DAY );
			PrintShort("h", NANOSECS_PER_HOUR);
			PrintShort("m", NANOSECS_PER_MIN );
			PrintShort("s", NANOSECS_PER_SEC );

			if (iNanoSecs)
			{
				PrintShort("ms", NANOSECS_PER_MILLISEC);
				PrintShort("µs", NANOSECS_PER_MICROSEC);
				PrintShortInt ("ns", iNanoSecs);
			}
		}
		break;


		// short scaled format, e.g. "103.23 µs"
		case Format::Brief:
		{
			// the parens are important to avoid a clash with defined min and max macros,
			// particularly on windows
			if (iNanoSecs <= (std::numeric_limits<int_t>::min)())
			{
				sOut = "< -292.5 y";
			}
			else
			{
				std::make_unsigned<int_t>::type iAbsNanoSecs = std::abs(iNanoSecs);

				if (iAbsNanoSecs >= NANOSECS_PER_YEAR || Interval == BaseInterval::Years)
				{
					if (iPrecision == 1 && (Interval == BaseInterval::Years || iAbsNanoSecs >= 10 * NANOSECS_PER_YEAR)) --iPrecision;
					PrintFloat ("y", iNanoSecs, NANOSECS_PER_YEAR, iPrecision, 0);
				}
				else if (iAbsNanoSecs >= NANOSECS_PER_WEEK || Interval == BaseInterval::Weeks)
				{
					if (iPrecision == 1 && (Interval == BaseInterval::Weeks || iAbsNanoSecs >= 10 * NANOSECS_PER_WEEK)) --iPrecision;
					PrintFloat ("w", iNanoSecs, NANOSECS_PER_WEEK, iPrecision, 0);
				}
				else if (iAbsNanoSecs >= NANOSECS_PER_DAY || Interval == BaseInterval::Days)
				{
					if (iPrecision == 1 && (Interval == BaseInterval::Days || iAbsNanoSecs >= 10 * NANOSECS_PER_DAY)) --iPrecision;
					PrintFloat ("d", iNanoSecs, NANOSECS_PER_DAY, iPrecision, 0);
				}
				else if (iAbsNanoSecs >= NANOSECS_PER_HOUR || Interval == BaseInterval::Hours)
				{
					if (iPrecision == 1 && (Interval == BaseInterval::Hours || iAbsNanoSecs >= 10 * NANOSECS_PER_HOUR)) --iPrecision;
					PrintFloat ("h", iNanoSecs, NANOSECS_PER_HOUR, iPrecision, 0);
				}
				else if (iAbsNanoSecs >= NANOSECS_PER_MIN || Interval == BaseInterval::Minutes)
				{
					if (iPrecision == 1 && (Interval == BaseInterval::Minutes || iAbsNanoSecs >= 10 * NANOSECS_PER_MIN)) --iPrecision;
					PrintFloat ("m", iNanoSecs, NANOSECS_PER_MIN, iPrecision, 0);
				}
				else if (iAbsNanoSecs >= NANOSECS_PER_SEC || Interval == BaseInterval::Seconds)
				{
					if (iPrecision == 1 && (Interval == BaseInterval::Seconds || iAbsNanoSecs >= 10 * NANOSECS_PER_SEC)) --iPrecision;
					PrintFloat ("s", iNanoSecs, NANOSECS_PER_SEC, iPrecision, 0);
				}
				else if (iAbsNanoSecs >= NANOSECS_PER_MILLISEC || Interval == BaseInterval::MilliSeconds)
				{
					if (iPrecision == 1 && (Interval == BaseInterval::MilliSeconds || iAbsNanoSecs >= 10 * NANOSECS_PER_MILLISEC)) --iPrecision;
					PrintFloat ("ms", iNanoSecs, NANOSECS_PER_MILLISEC, iPrecision, 0);
				}
				else if (iAbsNanoSecs >= NANOSECS_PER_MICROSEC || Interval == BaseInterval::MicroSeconds)
				{
					if (iPrecision == 1 && (Interval == BaseInterval::MicroSeconds || iAbsNanoSecs >= 10 * NANOSECS_PER_MICROSEC)) --iPrecision;
					PrintFloat ("µs", iNanoSecs, NANOSECS_PER_MICROSEC, iPrecision, 0);
				}
				else
				{
					if (iPrecision == 1 && (Interval == BaseInterval::NanoSeconds || iAbsNanoSecs >= 10)) --iPrecision;
					PrintFloat ("ns", iNanoSecs, 1, iPrecision, 0);
				}
			}
		}
		break;

		// smarter, generally more useful logic: display something that makes sense
		case Format::Smart:
		{
			// the parens are important to avoid a clash with defined min and max macros,
			// particularly on windows
			if (iNanoSecs <= (std::numeric_limits<int_t>::min)())
			{
				sOut = "a very long negative time"; // < -292.5 years
			}
			else
			{
				std::make_unsigned<int_t>::type iAbsNanoSecs = std::abs(iNanoSecs);

				if (iAbsNanoSecs >= NANOSECS_PER_YEAR)
				{
					PrintFloat ("yr" , iNanoSecs, NANOSECS_PER_YEAR, 1);
				}
				else if (iAbsNanoSecs >= NANOSECS_PER_WEEK)
				{
					PrintFloat ("wk" , iNanoSecs, NANOSECS_PER_WEEK, 1);
				}
				else if (iAbsNanoSecs >= NANOSECS_PER_DAY)
				{
					PrintFloat ("day", iNanoSecs, NANOSECS_PER_DAY, 1);
				}
				else if (iAbsNanoSecs >= NANOSECS_PER_HOUR)
				{
					if (bIsNegative)
					{
						PrintFloat ("hour", iNanoSecs, NANOSECS_PER_HOUR, 1);
					}
					else
					{
						// show hours and minutes, but not seconds:
						auto iHours  = iNanoSecs / NANOSECS_PER_HOUR;
						iNanoSecs   -= iHours    * NANOSECS_PER_HOUR;
						auto iMins   = iNanoSecs / NANOSECS_PER_MIN;

						PrintInt ("hr" , iHours);
						PrintInt ("min", iMins);
					}
				}
				else if (iAbsNanoSecs >= NANOSECS_PER_MIN)
				{
					if (bIsNegative)
					{
						PrintFloat ("min", iNanoSecs, NANOSECS_PER_MIN, 1);
					}
					else
					{
						// show minutes and seconds:
						auto iMins = iNanoSecs / NANOSECS_PER_MIN;
						iNanoSecs -= iMins     * NANOSECS_PER_MIN;
						auto iSecs = iNanoSecs / NANOSECS_PER_SEC;

						PrintInt ("min", iMins);
						PrintInt ("sec", iSecs);
					}
				}
				else if (iAbsNanoSecs >= NANOSECS_PER_SEC)
				{
					PrintFloat ("sec", iNanoSecs, NANOSECS_PER_SEC, 3);
				}
				else if (iAbsNanoSecs >= NANOSECS_PER_MILLISEC)
				{
					PrintFloat ("millisec", iNanoSecs, NANOSECS_PER_MILLISEC, 3);
				}
				else if (iAbsNanoSecs >= NANOSECS_PER_MICROSEC)
				{
					PrintFloat ("microsec", iNanoSecs, NANOSECS_PER_MICROSEC, 3);
				}
				else
				{
					PrintFloat ("nanosec", iNanoSecs, 1, 3);
				}
			}
		}
		break;
	}

	return sOut;

} // ToString

//-----------------------------------------------------------------------------
KDuration::KDuration(KStringView sDuration)
//-----------------------------------------------------------------------------
: Duration(Duration::zero())
{
	// supported formats:
	// Smart
	// 75.7 yrs
	// Long
	// 75 yrs, 36 wks, 5 days, 16 hrs, 38 mins, 43 secs, 123 msecs, 874 µsecs, 534 nsecs
	// Brief
	// 76 y
	// or
	// 1.5 y
	// Condensed
	// 3947w3d16h38m43s123ms874µs534ns
	// or
	// 1d14h24m2.92s
	// and negative values

	KDuration duration;
	int8_t bIsPositive   { 1 };

	auto it = sDuration.begin();
	auto ie = sDuration.end();

	uint32_t iNumber     { 0 };
	uint32_t iFraction   { 0 };
	KStringView sUnit;

	enum State { SkipWS1, ParseNum, ParseFraction, SkipWS2, ParseUnit, SkipComma };

	State state { SkipWS1 };

	for (;it != ie;)
	{
		switch (state)
		{
			case SkipWS1:
			case SkipWS2:
#if DEKAF2_KDURATION_HAS_KCTYPE
				while (KASCII::kIsSpace(*it))
#else
				while (std::isspace(*it))
#endif
				{
					if (++it == ie)
					{
						break;
					}
				}
				if (state == SkipWS1)
				{
					if (it != ie && duration.IsZero())
					{
						if (*it == '-')
						{
							++it;
							bIsPositive = -1;
						}
						else if (*it == '+')
						{
							++it;
						}
					}
					state = ParseNum;
				}
				else
				{
					state = ParseUnit;
				}
				break;

			case ParseNum:
#if DEKAF2_KDURATION_HAS_KCTYPE
				while (KASCII::kIsDigit(*it))
#else
				while (std::isdigit(*it))
#endif
				{
					iNumber *= 10;
					iNumber += *it - '0';

					if (++it == ie)
					{
						break;
					}
				}

				if (it != ie && *it == '.')
				{
					++it;
					state = ParseFraction;
				}
				else
				{
					state = SkipWS2;
				}
				break;

			case ParseFraction:
			{
				uint16_t iMultiplier { 1000 };
#if DEKAF2_KDURATION_HAS_KCTYPE
				while (KASCII::kIsDigit(*it))
#else
				while (std::isdigit(*it))
#endif
				{
					if (iMultiplier > 1)
					{
						iMultiplier /= 10;
					}
					iFraction *= 10;
					iFraction += *it - '0';

					if (++it == ie)
					{
						break;
					}
				}
				iFraction *= iMultiplier;
				state = SkipWS2;
				break;
			}

			case ParseUnit:
			{
				sUnit = KStringView(it, ie - it);

#if DEKAF2_KDURATION_HAS_KCTYPE
				while (KASCII::kIsAlpha(*it) ||

#else
				while (std::isalpha(*it)) ||
#endif
					   *it == KStringView("µ").front() ||
					   *it == KStringView("µ")[1])
				{
					if (++it == ie)
					{
						break;
					}
				}

				if (it != ie)
				{
					sUnit.remove_suffix(ie - it);
				}

				if (sUnit.empty())
				{
					kDebug(2, "value ({}) without unit", iNumber);
					return;
				}

				if (iFraction > 999)
				{
					kDebug(3, "iFraction > 999, rounding to next integer: {}", iFraction);
					iFraction = 0;
					++iNumber;
				}

				enum Unit { none, minutes, milliseconds, microseconds };
				Unit unit { none };

				// evaluate unit partially
				switch (sUnit.front())
				{
					case 'y':
						duration += chrono::days(365 * iNumber);
						duration += chrono::seconds(iFraction * (24 * 60 * 60 * 365 / 1000));
						break;

					case 'w':
						duration += chrono::days(7 * iNumber);
						duration += chrono::milliseconds(iFraction * 24 * 60 * 60 * 7);
						break;

					case 'd':
						duration += chrono::days(iNumber);
						duration += chrono::milliseconds(iFraction * 24 * 60 * 60);
						break;

					case 'h':
						duration += chrono::hours(iNumber);
						duration += chrono::milliseconds(iFraction * 60 * 60);
						break;

					case 's':
						duration += chrono::seconds(iNumber);
						duration += chrono::milliseconds(iFraction);
						break;

					case 'n':
						duration += chrono::nanoseconds(iNumber);
						// we drop any fractional value
						break;

					case 'u':
						unit = microseconds;
						break;

					// ambiguous cases

					case 'm':
					{
						if (sUnit.size() > 1)
						{
							// ms or msecs or milliseconds or microseconds or minutes
							if (sUnit[1] == 'i')
							{
								if (sUnit.size() > 2)
								{
									if (sUnit[2] == 'l')
									{
										unit = milliseconds;
									}
									else if (sUnit[2] == 'n')
									{
										unit = minutes;
									}
									else if (sUnit[2] == 'c')
									{
										unit = microseconds;
									}
								}
								else
								{
									unit = minutes;
								}
							}
							else if (sUnit[1] == 's')
							{
								unit = milliseconds;
							}
							else
							{
								kDebug(2, "invalid unit: {}", sUnit);
								return;
							}
						}
						else
						{
							unit = minutes;
						}

						break;
					}

					// the KStringView constructor is constexpr, as well
					// as the subscript access
					case KStringView("µ").front():
						if (sUnit.size() > 1)
						{
							if (sUnit[1] == KStringView("µ")[1])
							{
								unit = microseconds;
								break;
							}
						}
						kDebug(2, "invalid unit: {}", sUnit);
						return;

					default:
						kDebug(2, "invalid unit: {}", sUnit);
						return;
				}

				switch (unit)
				{
					case minutes:
						duration += chrono::minutes(iNumber);
						duration += chrono::milliseconds(iFraction * 60);
						break;

					case milliseconds:
						duration += chrono::milliseconds(iNumber);
						duration += chrono::microseconds(iFraction);
						break;

					case microseconds:
						duration += chrono::microseconds(iNumber);
						duration += chrono::nanoseconds(iFraction);
						break;

					default:
						break;
				}

				iNumber   = 0;
				iFraction = 0;

				state = SkipComma;
				break;
			}

			case SkipComma:
				if (*it == ',')
				{
					++it;
				}

				state = SkipWS1;
				break;
		}

	}

	// assign the parsed value
	*this = duration * bIsPositive;

} // KDuration string ctor

//-----------------------------------------------------------------------------
void KMultiDuration::clear()
//-----------------------------------------------------------------------------
{
	KDuration::clear();
	m_iRounds = 0;

} // clear

//-----------------------------------------------------------------------------
void KMultiDuration::push_back(Duration duration)
//-----------------------------------------------------------------------------
{
	KDuration::operator+=(duration);
	++m_iRounds;
}

//-----------------------------------------------------------------------------
KMultiDuration KMultiDuration::operator+(const KMultiDuration& other) const
//-----------------------------------------------------------------------------
{
	KMultiDuration temp(*this);
	return temp += other;
}

//-----------------------------------------------------------------------------
KMultiDuration::self& KMultiDuration::operator+=(const KMultiDuration& other)
//-----------------------------------------------------------------------------
{
	KDuration::operator+=(other);
	m_iRounds  += other.m_iRounds;

	return *this;
}

//-----------------------------------------------------------------------------
KMultiDuration KMultiDuration::operator-(const KMultiDuration& other) const
//-----------------------------------------------------------------------------
{
	KMultiDuration temp(*this);
	return temp -= other;
}

//-----------------------------------------------------------------------------
KMultiDuration::self& KMultiDuration::operator-=(const KMultiDuration& other)
//-----------------------------------------------------------------------------
{
	KDuration::operator-=(other);
	m_iRounds  -= other.m_iRounds;

	return *this;
}

//-----------------------------------------------------------------------------
KMultiDuration KMultiDuration::operator*(size_type iMultiplier) const
//-----------------------------------------------------------------------------
{
	KMultiDuration temp(*this);
	return temp *= iMultiplier;
}

//-----------------------------------------------------------------------------
KMultiDuration::self& KMultiDuration::operator*=(size_type iMultiplier)
//-----------------------------------------------------------------------------
{
	KDuration::operator*=(iMultiplier);
	m_iRounds  *= iMultiplier;

	return *this;
}

//-----------------------------------------------------------------------------
KMultiDuration KMultiDuration::operator/(size_type iDivisor) const
//-----------------------------------------------------------------------------
{
	KMultiDuration temp(*this);
	return temp /= iDivisor;
}

//-----------------------------------------------------------------------------
KMultiDuration::self& KMultiDuration::operator/=(size_type iDivisor)
//-----------------------------------------------------------------------------
{
	if (iDivisor)
	{
		KDuration::operator/=(iDivisor);
		m_iRounds  /= iDivisor;
	}
	else
	{
		kWarning("cannot divide by 0");
	}

	return *this;
}

//-----------------------------------------------------------------------------
void KStopDuration::clear()
//-----------------------------------------------------------------------------
{
	base::clear();
	m_Timer.clear();

} // clear

//---------------------------------------------------------------------------
KStopDuration::base::Duration KStopDuration::Stop()
//---------------------------------------------------------------------------
{
	auto D = m_Timer.elapsedAndClear();

	push_back(D);

	return D;

} // StoreInterval


//-----------------------------------------------------------------------------
void KDurations::clear()
//-----------------------------------------------------------------------------
{
	m_Durations.clear();

} // clear

//-----------------------------------------------------------------------------
const KDurations::Duration& KDurations::operator[](size_type iInterval) const
//-----------------------------------------------------------------------------
{
	if (iInterval < size())
	{
		return m_Durations[iInterval];
	}
	else
	{
		static Duration s_EmptyDuration;
		kDebug(1, "subscript access out of range: {} in {}..{}", iInterval, 0, size());
		return s_EmptyDuration;
	}
}

//-----------------------------------------------------------------------------
KDurations::Duration& KDurations::operator[](size_type iInterval)
//-----------------------------------------------------------------------------
{
	if (iInterval < size())
	{
		return m_Durations[iInterval];
	}
	else
	{
		static Duration s_EmptyDuration;
		kDebug(1, "subscript access out of range: {} in {}..{}", iInterval, 0, size());
		return s_EmptyDuration;
	}
}

//-----------------------------------------------------------------------------
KDurations::DurationBase KDurations::duration(size_type iInterval) const
//-----------------------------------------------------------------------------
{
	if (iInterval < size())
	{
		return m_Durations[iInterval].duration();
	}
	else
	{
		return DurationBase::zero();
	}
}

//---------------------------------------------------------------------------
KDurations::size_type KDurations::TotalRounds() const
//---------------------------------------------------------------------------
{
	size_type Total { 0 };

	for (const auto& Duration : m_Durations)
	{
		Total += Duration.Rounds();
	}

	return Total;

} // TotalRounds

//---------------------------------------------------------------------------
KDurations::DurationBase KDurations::duration() const
//---------------------------------------------------------------------------
{
	Duration Total = Duration::zero();

	for (const auto& Duration : m_Durations)
	{
		Total += Duration;
	}

	return Total.duration();

} // TotalDuration

//-----------------------------------------------------------------------------
KDurations KDurations::operator+(const KDurations& other) const
//-----------------------------------------------------------------------------
{
	KDurations temp(*this);
	return temp += other;
}

//-----------------------------------------------------------------------------
KDurations::self& KDurations::operator+=(const KDurations& other)
//-----------------------------------------------------------------------------
{
	if (size() < other.size())
	{
		if (!empty())
		{
			kDebug(2, "resizing object to match count of other intervals from {} to {}", size(), other.size());
		}

		m_Durations.resize(other.size());
	}

	const auto iSize = std::min(size(), other.size());

	for (size_type i = 0; i < iSize; ++i)
	{
		m_Durations[i] += other.m_Durations[i];
	}

	return *this;
}

//-----------------------------------------------------------------------------
KDurations KDurations::operator-(const KDurations& other) const
//-----------------------------------------------------------------------------
{
	KDurations temp(*this);
	return temp -= other;
}

//-----------------------------------------------------------------------------
KDurations::self& KDurations::operator-=(const KDurations& other)
//-----------------------------------------------------------------------------
{
	if (size() < other.size())
	{
		if (!empty())
		{
			kDebug(2, "resizing object to match count of other intervals from {} to {}", size(), other.size());
		}

		m_Durations.resize(other.size());
	}

	const auto iSize = std::min(size(), other.size());

	for (size_type i = 0; i < iSize; ++i)
	{
		m_Durations[i] -= other.m_Durations[i];
	}

	return *this;
}

//-----------------------------------------------------------------------------
KDurations KDurations::operator*(size_type iMultiplier) const
//-----------------------------------------------------------------------------
{
	KDurations temp(*this);
	return temp *= iMultiplier;
}

//-----------------------------------------------------------------------------
KDurations::self& KDurations::operator*=(size_type iMultiplier)
//-----------------------------------------------------------------------------
{
	for (auto& it : m_Durations)
	{
		it *= iMultiplier;
	}

	return *this;
}

//-----------------------------------------------------------------------------
KDurations KDurations::operator/(size_type iDivisor) const
//-----------------------------------------------------------------------------
{
	KDurations temp(*this);
	return temp /= iDivisor;
}

//-----------------------------------------------------------------------------
KDurations::self& KDurations::operator/=(size_type iDivisor)
//-----------------------------------------------------------------------------
{
	if (iDivisor)
	{
		for (auto& it : m_Durations)
		{
			it /= iDivisor;
		}
	}
	else
	{
		kWarning("cannot divide by 0");
	}

	return *this;
}

//-----------------------------------------------------------------------------
void KStopDurations::clear()
//-----------------------------------------------------------------------------
{
	base::clear();
	m_Timer.clear();

} // clear

//---------------------------------------------------------------------------
void KStopDurations::StartNextInterval()
//---------------------------------------------------------------------------
{
	push_back(m_Timer.elapsedAndClear());

} // StartNextInterval

//---------------------------------------------------------------------------
KStopDurations::base::DurationBase KStopDurations::StoreInterval(size_type iInterval)
//---------------------------------------------------------------------------
{
	if (size() < iInterval + 1)
	{
		resize(iInterval + 1);
	}

	auto D = m_Timer.elapsedAndClear();

	operator[](iInterval) += D;

	return D;

} // StoreInterval

DEKAF2_NAMESPACE_END
