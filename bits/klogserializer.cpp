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
#include "../ksystem.h"
#include "../dekaf2.h"
#include "../kgetruntimestack.h"
#include "../kstringutils.h"
#include "../klog.h" // for KLog::DASH ..
#include "../ktime.h"

namespace dekaf2
{

//---------------------------------------------------------------------------
void KLogData::Set(int iLevel, KStringView sShortName, KStringView sPathName, KStringView sFunction, KStringView sMessage)
//---------------------------------------------------------------------------
{
	m_iLevel = iLevel;
	m_Pid = kGetPid();
	m_Tid = kGetTid();
	m_Time = Dekaf::getInstance().GetCurrentTime();
	m_sFunctionName = SanitizeFunctionName(sFunction);
	m_sShortName = sShortName;
	m_sPathName = sPathName;
	m_sMessage = sMessage;
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
const KString& KLogSerializer::Get()
//---------------------------------------------------------------------------
{
	if (m_sBuffer.empty())
	{
		Serialize();
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
void KLogTTYSerializer::Serialize()
//---------------------------------------------------------------------------
{
	// desired format:
	// | WAR | MYPRO | 17202 | 12345 | 2001-08-24 10:37:04 | Function: select count(*) from foo

	KString sLevel;

	if (m_iLevel < 0)
	{
		sLevel = "WAR";
	}
	else
	{
		sLevel.Format("DB{}", m_iLevel > 3 ? 3 : m_iLevel);
	}

	auto sPrefix = PrintStatus(sLevel);

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
		AddMultiLineMessage(sPrefix.ToView(0, PrefixWithoutFunctionSize), KLog::DASH);
	}

} // Serialize

//---------------------------------------------------------------------------
KString KLogTTYSerializer::PrintStatus(KStringView sLevel)
//---------------------------------------------------------------------------
{
	// desired format:
	// | WAR | MYPRO | 17202 | 12345 | 2001-08-24 10:37:04 |

	KString sPrefix;

	sPrefix.Format("| {:3.3s} | {:5.5s} | {:5d} | {:5d} | {} | ",
				   sLevel, m_sShortName, m_Pid, m_Tid, kFormTimestamp(m_Time));

	return sPrefix;

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
KString KLogHTTPHeaderSerializer::PrintStatus(KStringView sLevel)
//---------------------------------------------------------------------------
{
	// desired format:
	// | WAR |      12345 |

	KString sPrefix;

	sPrefix.Format("| {:3.3s} | {:>10.10s} | ", sLevel, kFormNumber(m_iElapsedMicroSeconds.count()));

	return sPrefix;

} // Serialize

//---------------------------------------------------------------------------
KJSON KLogJSONSerializer::CreateObject() const
//---------------------------------------------------------------------------
{
	KJSON json;

	json["level"] = m_iLevel;
	json["pid"] = m_Pid;
	json["tid"] = m_Tid;
	json["time_t"] = m_Time;
	json["short_name"] = m_sShortName;
	json["path_name"] = m_sPathName;
	json["function_name"] = m_sFunctionName;
	json["message"] = m_sMessage;

	return json;

} // Serialize

//---------------------------------------------------------------------------
void KLogJSONSerializer::Serialize()
//---------------------------------------------------------------------------
{
	m_sBuffer.reserve(m_sShortName.size()
	                  + m_sFunctionName.size()
	                  + m_sPathName.size()
	                  + m_sMessage.size()
	                  + 50);

	m_sBuffer = CreateObject().dump();

} // Serialize

//---------------------------------------------------------------------------
void KLogJSONArraySerializer::Serialize()
//---------------------------------------------------------------------------
{
	if (m_json.is_array())
	{
		m_json.push_back(CreateObject());
		// add only once per set()
		m_sBuffer = "1";
	}

} // Serialize

#endif // of DEKAF2_KLOG_WITH_TCP

#ifdef DEKAF2_HAS_SYSLOG

//---------------------------------------------------------------------------
void KLogSyslogSerializer::Serialize()
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
		AddMultiLineMessage(sPrefix, KLog::DASH);
	}

} // Serialize

#endif // of DEKAF2_HAS_SYSLOG

} // of namespace dekaf2
