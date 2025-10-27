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

#include "klogserializer.h"

#ifdef DEKAF2_WITH_KLOG

#include "../ksystem.h"
#include "../dekaf2.h"
#include "../kgetruntimestack.h"
#include "../kstringutils.h"
#include "../kformat.h"
#include "../ktime.h"

DEKAF2_NAMESPACE_BEGIN

             uint64_t  KLogData::s_iStartThread      = kGetTid();
thread_local KUnixTime KLogData::s_TimeThreadStarted = kNow();
thread_local KUnixTime KLogData::s_TimeLastLogged;

//---------------------------------------------------------------------------
void KLogData::Set(int iLevel, KStringView sShortName, KStringView sPathName, KStringView sFunction, KStringView sMessage)
//---------------------------------------------------------------------------
{
	m_iLevel        = iLevel;
	m_Pid           = kGetPid();
	m_Tid           = kGetTid();
	m_Time          = kNow();
	m_sFunctionName = SanitizeFunctionName(sFunction);
	m_sShortName    = sShortName;
	m_sPathName     = sPathName;
	m_sMessage      = sMessage;

	m_sMessage.TrimRight();
	m_sBacktrace.clear();

} // Set

//---------------------------------------------------------------------------
KStringView KLogData::SanitizeFunctionName(KStringView sFunction)
//---------------------------------------------------------------------------
{
	if (!sFunction.empty())
	{
		if (*sFunction.rbegin() == ']')
		{
			// try to remove template arguments from function name
			auto pos = sFunction.rfind('[');
			if (pos != KStringView::npos)
			{
				if (pos > 0 && sFunction[pos-1] == ' ')
				{
					--pos;
				}
				sFunction.remove_suffix(sFunction.size() - pos);
			}
		}
	}

	return sFunction;

} // SanitizeFunctionName

//---------------------------------------------------------------------------
const KString& KLogSerializer::Get(bool bHiRes)
//---------------------------------------------------------------------------
{
	if (m_sBuffer.empty())
	{
		Serialize(bHiRes);
	}
	return m_sBuffer;

} // Get

//---------------------------------------------------------------------------
KLogSerializer::operator KStringView()
//---------------------------------------------------------------------------
{
	return Get();

} // operator KStringView

//---------------------------------------------------------------------------
void KLogSerializer::Set(int iLevel, KStringView sShortName, KStringView sPathName, KStringView sFunction, KStringView sMessage)
//---------------------------------------------------------------------------
{
	m_sBuffer.clear();
	m_bIsMultiline = false;
	KLogData::Set(iLevel, sShortName, sPathName, sFunction, sMessage);

} // Set

//---------------------------------------------------------------------------
bool KLogSerializer::Matches(bool bEgrep, bool bInverted, KStringView sGrepExpression)
//---------------------------------------------------------------------------
{
	bool bMatches { true };

	if (!sGrepExpression.empty())
	{
		if (bEgrep)
		{
			bMatches = !m_sFunctionName.ToLower().MatchRegex(sGrepExpression).empty();

			if (!bMatches)
			{
				bMatches = !m_sMessage.ToLower().MatchRegex(sGrepExpression).empty();
			}
		}
		else
		{
			bMatches = m_sFunctionName.ToLower().contains(sGrepExpression);

			if (!bMatches)
			{
				bMatches = m_sMessage.ToLower().contains(sGrepExpression);
			}
		}

		if (bInverted)
		{
			bMatches = !bMatches;
		}
	}

	return bMatches;

} // Matches

//---------------------------------------------------------------------------
void KLogTTYSerializer::AddMultiLineMessage(KStringView sPrefix, KStringView sMessage)
//---------------------------------------------------------------------------
{
	if (sMessage.empty())
	{
		return;
	}

	int iFragments{0};

	if (!m_sBuffer.empty())
	{
		++iFragments;
	}

	while (!sMessage.empty())
	{
		auto pos = sMessage.find('\n');
		KStringView sLine = sMessage.substr(0, pos);
		sLine.TrimRight();
		if (!sLine.empty())
		{
			++iFragments;
			m_sBuffer += sPrefix;
			m_sBuffer += sLine;
			m_sBuffer += '\n';
		}
		if (pos != KStringView::npos)
		{
			sMessage.remove_prefix(pos+1);
		}
		else
		{
			sMessage.clear();
		}
	}

	if (!m_bIsMultiline)
	{
		m_bIsMultiline = iFragments > 1;
	}

} // HandleMultiLineMessages

//---------------------------------------------------------------------------
void KLogTTYSerializer::Serialize(bool bHiRes)
//---------------------------------------------------------------------------
{
	// desired formats:
	// | WAR | MYPRO | 17202 | 12345 | 2001-08-24 10:37:04 | Function: select count(*) from foo
	// HiRes:
	// | 11:34:48.123456 | 55717 |     0 | +123µs | 123ms | KHTTPClient::Parse(): HTTP-200 OK

	auto sPrefix = PrintStatus(bHiRes);

	auto PrefixWithoutFunctionSize = sPrefix.size();

	if (!m_sFunctionName.empty())
	{
		if (m_iLevel < 0)
		{
			// print the full function signature only if this is a warning or exception
			sPrefix += m_sFunctionName;
			sPrefix += ": ";
			sPrefix.Replace("dekaf2::", "");
			sPrefix.Replace("::self_type", "");
			sPrefix.Replace("::self", "");
		}
		else
		{
			// print shortened function name only, no signature
			// - this is an intentional debug message that does not need the full signature
			sPrefix += kNormalizeFunctionName(m_sFunctionName);
			sPrefix += "(): ";
		}
	}

	AddMultiLineMessage(sPrefix, m_sMessage);

	if (!m_sBacktrace.empty())
	{
		AddMultiLineMessage(sPrefix.ToView(0, PrefixWithoutFunctionSize), m_sBacktrace);
		AddMultiLineMessage(sPrefix.ToView(0, PrefixWithoutFunctionSize), kFormat("{:-^100}", ""));
	}

} // Serialize

//---------------------------------------------------------------------------
KString KLogTTYSerializer::LevelAsString()
//---------------------------------------------------------------------------
{
	KString sLevel;

	if (m_iLevel < 0)
	{
		sLevel = "WAR";
	}
	else
	{
		sLevel.Format("DB{}", m_iLevel);
	}

	return sLevel;

} // LevelAsString

//---------------------------------------------------------------------------
KString KLogTTYSerializer::PrintStatus(bool bHiRes)
//---------------------------------------------------------------------------
{
	// desired formats:
	// | WAR | MYPRO | 17202 | 12345 | 2001-08-24 10:37:04 |
	// HiRes:
	// | 11:34:48.123456 | 55717 |     0 | +9.0µs | 7.8ms | KHTTPClient::Parse(): HTTP-200 OK
	// | 11:34:48.123457 | 55717 |     0 | +123µs | 123ms | KHTTPClient::Parse(): HTTP-200 OK

	if (!bHiRes)
	{
		return kFormat("| {:3.3s} | {:5.5s} | {:5d} | {:5d} | {:%Y-%m-%d %H:%M:%S} | ",
					   LevelAsString(), m_sShortName, m_Pid, m_Tid, m_Time);
	}
	else
	{
		auto totalTicks = m_Time - s_TimeThreadStarted;

		if (totalTicks.duration() < KDuration::duration::zero())
		{
			// sometimes we get small negative durations - remove
			// them to not confuse readers
			totalTicks = KDuration();
		}

		KDuration thisTicks;
		
		if (s_TimeLastLogged.time_since_epoch() > KUnixTime::time_point::duration::zero())
		{
			thisTicks  = m_Time - s_TimeLastLogged;

			if (thisTicks.duration() < KDuration::duration::zero())
			{
				// sometimes we get small negative durations - remove
				// them to not confuse readers
				thisTicks = KDuration();
			}
		}
		
		s_TimeLastLogged = m_Time;

#ifdef DEKAF2_IS_MACOS
		uint64_t iThread = m_Tid - s_iStartThread;
#else
		uint64_t iThread = m_Tid < s_iStartThread ? m_Tid + 65535 - s_iStartThread : m_Tid - s_iStartThread;
#endif

		return kFormat("|{:2}| {:%H:%M:%S}.{:06} | {:5d} | {:5d} | +{:>5.5s} | {:>5.5s} | ",
					   m_iLevel,
					   m_Time,
					   m_Time.subseconds().microseconds().count(),
					   m_Pid,
					   iThread,
					   thisTicks,
					   totalTicks);
	}

} // Serialize

#ifdef DEKAF2_KLOG_WITH_TCP

//---------------------------------------------------------------------------
void KLogHTTPHeaderSerializer::Set(int iLevel, KStringView sShortName, KStringView sPathName, KStringView sFunction, KStringView sMessage)
//---------------------------------------------------------------------------
{
	m_iElapsedMicroSeconds = m_Clock.elapsed().duration<std::chrono::microseconds>();
	KLogSerializer::Set(iLevel, sShortName, sPathName, sFunction, sMessage);

} // Set

//---------------------------------------------------------------------------
KStringViewZ KLogHTTPHeaderSerializer::GetHeader()
//---------------------------------------------------------------------------
{
	return "+-----+------------+--------------------------------------------------------------------\n"
		   "| LVL |  MicroSecs | Messsage\n"
		   "+-----+------------+--------------------------------------------------------------------\n";

} // GetHeader

//---------------------------------------------------------------------------
KStringViewZ KLogHTTPHeaderSerializer::GetFooter()
//---------------------------------------------------------------------------
{
	return "+-----+------------+--------------------------------------------------------------------\n";;

} // GetFooter

//---------------------------------------------------------------------------
KString KLogHTTPHeaderSerializer::GetTimeStamp(KStringView sWhat)
//---------------------------------------------------------------------------
{
	auto sTimestamp = kFormTimestamp();
	Set(1, "", "", sWhat, sTimestamp);
	return Get();

} // GetTimeStamp

//---------------------------------------------------------------------------
KString KLogHTTPHeaderSerializer::PrintStatus(bool bHiRes)
//---------------------------------------------------------------------------
{
	// desired format:
	// | WAR |      12345 |

	auto sMicroseconds = kFormNumber(m_iElapsedMicroSeconds.count());
	return kFormat("| {:3.3s} | {:>10.10s} | ", LevelAsString(), sMicroseconds);

} // Serialize

//---------------------------------------------------------------------------
KJSON KLogJSONSerializer::CreateObject() const
//---------------------------------------------------------------------------
{
	KJSON json;

	json["level"] = m_iLevel;
	json["pid"] = m_Pid;
	json["tid"] = m_Tid;
	auto ttime = m_Time.to_time_t();
	json["time_t"] = ttime;
	json["usecs"] = (m_Time - KUnixTime(ttime)).microseconds().count();
	json["short_name"] = m_sShortName;
	json["path_name"] = m_sPathName;
	json["function_name"] = m_sFunctionName;
	json["message"] = m_sMessage;

	return json;

} // Serialize

//---------------------------------------------------------------------------
void KLogJSONSerializer::Serialize(bool bHiRes)
//---------------------------------------------------------------------------
{
	m_sBuffer.reserve(m_sShortName.size()
	                  + m_sFunctionName.size()
	                  + m_sPathName.size()
	                  + m_sMessage.size()
	                  + 50);

	m_sBuffer = CreateObject().dump();

} // Serialize

#if defined(DEKAF2_USE_PRECOMPILED_HEADERS) && DEKAF2_IS_GCC
#pragma GCC diagnostic push
#ifdef DEKAF2_HAS_WARN_STRINGOP_OVERFLOW
#pragma GCC diagnostic ignored "-Wstringop-overflow"
#endif
#endif
//---------------------------------------------------------------------------
void KLogJSONArraySerializer::Serialize(bool bHiRes)
//---------------------------------------------------------------------------
{
	if (m_json.is_array())
	{
		m_json.push_back(CreateObject());
		// add only once per set()
		m_sBuffer = "1";
	}

} // Serialize
#if defined(DEKAF2_USE_PRECOMPILED_HEADERS) && DEKAF2_IS_GCC
#pragma GCC diagnostic pop
#endif

#endif // of DEKAF2_KLOG_WITH_TCP

#ifdef DEKAF2_HAS_SYSLOG

//---------------------------------------------------------------------------
void KLogSyslogSerializer::Serialize(bool bHiRes)
//---------------------------------------------------------------------------
{
	KString sPrefix(m_sFunctionName);
	if (!sPrefix.empty())
	{
		sPrefix += ": ";
	}

	AddMultiLineMessage(sPrefix, m_sMessage);

	if (!m_sBacktrace.empty())
	{
		AddMultiLineMessage(sPrefix, m_sBacktrace);
		AddMultiLineMessage(sPrefix, kFormat("{:-^100}", ""));
	}

} // Serialize

#endif // of DEKAF2_HAS_SYSLOG

DEKAF2_NAMESPACE_END

#endif // of DEKAF2_WITH_KLOG
