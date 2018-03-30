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

/*
TODO: KLOG OVERHAUL NEEDED
[ ] kDebug() and kWarning() macros should probably be kDebugFormat() and kDebugPrintf()
*/

#include <iostream>
#include <mutex>
#include <syslog.h>
#include "dekaf2.h"
#include "klog.h"
#include "kstring.h"
#include "kgetruntimestack.h"
#include "kstringutils.h"
#include "ksystem.h"
#include "kurl.h"
#include "khttpclient.h"
#include "kjson.h"
#include "ksplit.h"
#include "kconnection.h"
#include "khttpclient.h"
#include "kmime.h"

namespace dekaf2
{

KOutStream KErr(std::cerr);
KOutStream KOut(std::cout);
KInStream  KIn(std::cin);
KStream    KInOut(std::cin, std::cout);

//---------------------------------------------------------------------------
KLogWriter::~KLogWriter()
//---------------------------------------------------------------------------
{
}

//---------------------------------------------------------------------------
bool KLogStdWriter::Write(int iLevel, bool bIsMultiline, const KString& sOut)
//---------------------------------------------------------------------------
{
	return m_OutStream.write(sOut.data(), static_cast<std::streamsize>(sOut.size())).flush().good();

} // Write

//---------------------------------------------------------------------------
KLogFileWriter::KLogFileWriter(KStringView sFileName)
//---------------------------------------------------------------------------
    : m_OutFile(sFileName, std::ios_base::app)
{
#ifdef DEKAF2_IS_UNIX
	KString sBuffer(sFileName);
	// force mode 666 for the log file ...
	::chmod(sBuffer.c_str(), 0666);
#endif
} // ctor

//---------------------------------------------------------------------------
bool KLogFileWriter::Write(int iLevel, bool bIsMultiline, const KString& sOut)
//---------------------------------------------------------------------------
{
	return m_OutFile.Write(sOut).Flush().Good();

} // Write

//---------------------------------------------------------------------------
bool KLogSyslogWriter::Write(int iLevel, bool bIsMultiline, const KString& sOut)
//---------------------------------------------------------------------------
{
	int priority;
	switch (iLevel)
	{
		case -1:
			priority = LOG_ERR;
			break;
		case 0:
			priority = LOG_WARNING;
			break;
		case 1:
			priority = LOG_NOTICE;
			break;
		case 2:
			priority = LOG_INFO;
			break;
		default:
			priority = LOG_DEBUG;
			break;
	}

	if (!bIsMultiline)
	{
		syslog(priority, "%s", sOut.c_str());
	}
	else
	{
		KStringView svMessage = sOut;
		KString sPart;

		while (!svMessage.empty())
		{
			auto pos = svMessage.find('\n');
			sPart = svMessage.substr(0, pos);
			if (!sPart.empty())
			{
				syslog(priority, "%s", sPart.c_str());
			}
			svMessage.remove_prefix(pos);
			if (pos != KStringView::npos)
			{
				svMessage.remove_prefix(1);
			}
		}
	}

	return true;

} // Write

//---------------------------------------------------------------------------
KLogTCPWriter::KLogTCPWriter(KStringView sURL)
//---------------------------------------------------------------------------
    : m_sURL(sURL)
{
} // ctor

//---------------------------------------------------------------------------
KLogTCPWriter::~KLogTCPWriter()
//---------------------------------------------------------------------------
{
} // dtor

//---------------------------------------------------------------------------
bool KLogTCPWriter::Good() const
//---------------------------------------------------------------------------
{
	// this is special: we always return true if we have not (yet) created
	// the tcp client
	return !m_OutStream || m_OutStream->Good();

} // Good

//---------------------------------------------------------------------------
bool KLogTCPWriter::Write(int iLevel, bool bIsMultiline, const KString& sOut)
//---------------------------------------------------------------------------
{
	if (!Good())
	{
		// we try one reconnect should the connection have gone stale
		m_OutStream.reset();
	}

	if (!m_OutStream)
	{
		m_OutStream = std::make_unique<KConnection>(KConnection::Create(m_sURL));

		if (!m_OutStream->Good())
		{
			return false;
		}
	}

	return m_OutStream != nullptr
	        && m_OutStream->Stream().Write(sOut).Flush().Good();

} // Write

//---------------------------------------------------------------------------
KLogHTTPWriter::KLogHTTPWriter(KStringView sURL)
//---------------------------------------------------------------------------
    : m_sURL(sURL)
{
} // ctor

//---------------------------------------------------------------------------
KLogHTTPWriter::~KLogHTTPWriter()
//---------------------------------------------------------------------------
{
} // dtor

//---------------------------------------------------------------------------
bool KLogHTTPWriter::Good() const
//---------------------------------------------------------------------------
{
	// this is special: we always return true if we have not (yet) created
	// the http client
	return !m_OutStream || m_OutStream->Good();

} // Good

//---------------------------------------------------------------------------
bool KLogHTTPWriter::Write(int iLevel, bool bIsMultiline, const KString& sOut)
//---------------------------------------------------------------------------
{
	if (!Good())
	{
		// we try one reconnect should the connection have gone stale
		m_OutStream.reset();
	}

	if (!m_OutStream)
	{
		m_OutStream = std::make_unique<KHTTPClient>(m_sURL);

		if (!m_OutStream->Good())
		{
			return false;
		}
	}

	m_OutStream->Post(m_sURL, sOut, KMIME::JSON_UTF8);

	return m_OutStream->Good();

} // Write



//---------------------------------------------------------------------------
void KLogData::Set(int level, KStringView sShortName, KStringView sPathName, KStringView sFunction, KStringView sMessage)
//---------------------------------------------------------------------------
{
	m_Level = level;
	m_Pid = kGetPid();
	m_Tid = kGetTid();
	m_Time = Dekaf().GetCurrentTime();
	m_sFunctionName = SanitizeFunctionName(sFunction);
	m_sShortName = sShortName;
	m_sPathName = sPathName;
	m_sMessage = sMessage;
	kTrimRight(m_sMessage);
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
const KString& KLogSerializer::Get() const
//---------------------------------------------------------------------------
{
	if (m_sBuffer.empty())
	{
		Serialize();
	}
	return m_sBuffer;

} // Get

//---------------------------------------------------------------------------
KLogSerializer::operator KStringView() const
//---------------------------------------------------------------------------
{
	return Get();

} // operator KStringView

//---------------------------------------------------------------------------
void KLogSerializer::Set(int level, KStringView sShortName, KStringView sPathName, KStringView sFunction, KStringView sMessage)
//---------------------------------------------------------------------------
{
	m_sBuffer.clear();
	m_bIsMultiline = false;
	KLogData::Set(level, sShortName, sPathName, sFunction, sMessage);

} // Set

//---------------------------------------------------------------------------
void KLogTTYSerializer::AddMultiLineMessage(KStringView sPrefix, KStringView sMessage) const
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
		kTrimRight(sLine);
		if (!sLine.empty())
		{
			++iFragments;
			m_sBuffer += sPrefix;
			m_sBuffer += sLine;
			m_sBuffer += '\n';
		}
		sMessage.remove_prefix(pos);
		if (pos != KStringView::npos)
		{
			sMessage.remove_prefix(1);
		}
	}

	m_bIsMultiline = iFragments > 1;

} // HandleMultiLineMessages

//---------------------------------------------------------------------------
void KLogTTYSerializer::Serialize() const
//---------------------------------------------------------------------------
{
	// desired format:
	// | WAR | MYPRO | 17202 | 12345 | 2001-08-24 10:37:04 | Function: select count(*) from foo

	KString sLevel;

	if (m_Level < 0)
	{
		sLevel = "WAR";
	}
	else
	{
		sLevel.Format("DB{}", m_Level > 3 ? 3 : m_Level);
	}

	KString sPrefix;

	sPrefix.Printf("| %3.3s | %5.5s | %5u | %5u | %s | ",
	               sLevel, m_sShortName, m_Pid, m_Tid, kFormTimestamp());

	auto PrefixWithoutFunctionSize = sPrefix.size();

	// print the function name only if this is a warning or exception
	// - otherwise this is an intentional debug message that does not need it
	if (!m_sFunctionName.empty() && m_Level < 0)
	{
		sPrefix += m_sFunctionName;
		sPrefix += ": ";
	}

	AddMultiLineMessage(sPrefix, m_sMessage);

	if (!m_sBacktrace.empty())
	{
		AddMultiLineMessage(sPrefix.ToView(0, PrefixWithoutFunctionSize), m_sBacktrace);
		AddMultiLineMessage(sPrefix.ToView(0, PrefixWithoutFunctionSize), KLog::DASH);
	}

} // Serialize

//---------------------------------------------------------------------------
void KLogJSONSerializer::Serialize() const
//---------------------------------------------------------------------------
{
	m_sBuffer.reserve(m_sShortName.size()
	                  + m_sFunctionName.size()
	                  + m_sPathName.size()
	                  + m_sMessage.size()
	                  + 50);
	m_sBuffer  = '{';
	m_sBuffer += KJSON::EscWrap("level", m_Level);
	m_sBuffer += KJSON::EscWrap("pid", m_Pid);
	m_sBuffer += KJSON::EscWrap("tid", m_Tid);
	m_sBuffer += KJSON::EscWrap("time_t", m_Time);
	m_sBuffer += KJSON::EscWrap("short_name", m_sShortName);
	m_sBuffer += KJSON::EscWrap("path_name", m_sPathName);
	m_sBuffer += KJSON::EscWrap("function_name", m_sFunctionName);
	m_sBuffer += KJSON::EscWrap("message", m_sMessage);
	m_sBuffer += '}';

} // Serialize

//---------------------------------------------------------------------------
void KLogSyslogSerializer::Serialize() const
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



// "singleton"
//---------------------------------------------------------------------------
class KLog& KLog()
//---------------------------------------------------------------------------
{
	static class KLog myKLog;
	return myKLog;
}

// do not initialize this static var - it risks to override a value set by KLog()'s
// initialization before..
int KLog::s_kLogLevel;

//---------------------------------------------------------------------------
KLog::KLog()
//---------------------------------------------------------------------------
    : m_sLogName     (kGetEnv(s_sEnvLog,  s_sDefaultLog))
    , m_sFlagfile    (kGetEnv(s_sEnvFlag, s_sDefaultFlag))
    #ifdef NDEBUG
    , m_iBackTrace   (std::atoi(kGetEnv(s_sEnvTrace, "-3")))
    #else
    , m_iBackTrace   (std::atoi(kGetEnv(s_sEnvTrace, "-2")))
    #endif
{
#ifdef NDEBUG
	s_kLogLevel = -1;
#else
	s_kLogLevel = 0;
#endif
	m_sPathName =  Dekaf().GetProgPath();
	m_sPathName += '/';
	m_sPathName += Dekaf().GetProgName();

	SetName(Dekaf().GetProgName());

	IntOpenLog();

	CheckDebugFlag();

	Dekaf().AddToOneSecTimer([this]() {
		this->CheckDebugFlag();
	});

} // ctor

//---------------------------------------------------------------------------
void KLog::SetLevel(int iLevel)
//---------------------------------------------------------------------------
{
	if (iLevel <= 0)
	{
		iLevel = 0;
	}
	else if (iLevel > 3)
	{
		iLevel = 3;
	}

	s_kLogLevel = iLevel;

	if (iLevel)
	{
		KOutFile file (GetDebugFlag());
		file.FormatLine("{}", iLevel);
	}
	else
	{
		kRemoveFile (GetDebugFlag());
	}

} // SetLevel

//---------------------------------------------------------------------------
void KLog::SetName(KStringView sName)
//---------------------------------------------------------------------------
{
	if (sName.size() > 5)
	{
		sName.erase(5, KStringView::npos);
	}

	m_sShortName = kToUpper(sName);

} // SetName

// name schemes:
// http://host.name/path
// https://host.name/path
// host.name:port
// path
//---------------------------------------------------------------------------
bool KLog::SetDebugLog(KStringView sLogfile)
//---------------------------------------------------------------------------
{
	if (sLogfile.empty()) {
		sLogfile = s_sDefaultLog; // restore default
	}

	// env values always override programmatic values, and at construction of
	// KLog() we would have fetched the env value already if it is non-zero
	const char* sEnv(kGetEnv(s_sEnvLog, nullptr));
	if (sEnv)
	{
		kDebug(0, "prevented setting the debug log file to '{}' because the environment variable '{}' is set to '{}'",
		       sLogfile,
		       s_sEnvLog,
		       sEnv);
		return false;
	}

	if (sLogfile == m_sLogName)
	{
		return true;
	}

	m_sLogName = sLogfile;

	return IntOpenLog();

} // SetDebugLog

//---------------------------------------------------------------------------
bool KLog::SetWriter(std::unique_ptr<KLogWriter> logger)
//---------------------------------------------------------------------------
{
	m_Logger = std::move(logger);
	return m_Logger != nullptr && m_Logger->Good();

} // SetWriter

//---------------------------------------------------------------------------
bool KLog::SetSerializer(std::unique_ptr<KLogSerializer> serializer)
//---------------------------------------------------------------------------
{
	m_Serializer = std::move(serializer);
	return m_Serializer != nullptr;

} // SetSerializer

//---------------------------------------------------------------------------
std::unique_ptr<KLogWriter> KLog::CreateWriter(Writer writer, KStringView sLogname)
//---------------------------------------------------------------------------
{
	switch (writer)
	{
		default:
		case Writer::STDOUT:
			return std::make_unique<KLogStdWriter>(std::cout);
		case Writer::STDERR:
			return std::make_unique<KLogStdWriter>(std::cerr);
		case Writer::FILE:
			return std::make_unique<KLogFileWriter>(sLogname);
		case Writer::SYSLOG:
			return std::make_unique<KLogSyslogWriter>();
		case Writer::TCP:
			return std::make_unique<KLogTCPWriter>(sLogname);
		case Writer::HTTP:
			return std::make_unique<KLogHTTPWriter>(sLogname);
	}

} // CreateWriter

//---------------------------------------------------------------------------
std::unique_ptr<KLogSerializer> KLog::CreateSerializer(Serializer serializer)
//---------------------------------------------------------------------------
{
	switch (serializer)
	{
		default:
		case Serializer::TTY:
			return std::make_unique<KLogTTYSerializer>();
		case Serializer::SYSLOG:
			return std::make_unique<KLogSyslogSerializer>();
		case Serializer::JSON:
			return std::make_unique<KLogJSONSerializer>();
	}

} // Create Serializer

//---------------------------------------------------------------------------
bool KLog::IntOpenLog()
//---------------------------------------------------------------------------
{
	KURL url(m_sLogName);
	if (url.IsHttpURL())
	{
		SetWriter(CreateWriter(Writer::HTTP, m_sLogName));
		SetSerializer(CreateSerializer(Serializer::JSON));
	}
	else if (!url.Port.empty() && !url.Domain.empty())
	{
		// this is a simple domain:port TCP connection
		SetWriter(CreateWriter(Writer::TCP, m_sLogName));
		SetSerializer(CreateSerializer(Serializer::TTY));
	}
	else if (m_sLogName == SYSLOG)
	{
		SetWriter(CreateWriter(Writer::SYSLOG));
		SetSerializer(CreateSerializer(Serializer::SYSLOG));
	}
	else
	{
		// this is a regular file
		if (m_sLogName == STDOUT)
		{
			SetWriter(CreateWriter(Writer::STDOUT));
		}
		else if (m_sLogName == STDERR)
		{
			SetWriter(CreateWriter(Writer::STDERR));
		}
		else
		{
			SetWriter(CreateWriter(Writer::FILE, m_sLogName));
		}
		SetSerializer(CreateSerializer(Serializer::TTY));
	}

	return m_Logger && m_Logger->Good();

} // SetDebugLog

//---------------------------------------------------------------------------
bool KLog::SetDebugFlag(KStringView sFlagfile)
//---------------------------------------------------------------------------
{
	if (sFlagfile.empty()) {
		sFlagfile = s_sDefaultFlag; // restore default
	}

	// env values always override programmatic values, and at construction of
	// KLog() we would have fetched the env value already if it is non-zero
	const char* sEnv(kGetEnv(s_sEnvFlag, nullptr));
	if (sEnv)
	{
		kDebug(0, "prevented setting the debug flag file to '{}' because the environment variable '{}' is set to '{}'",
		       sFlagfile,
		       s_sEnvFlag,
		       sEnv);
		return false;
	}

	if (sFlagfile == m_sFlagfile)
	{
		return true;
	}

	m_sFlagfile = sFlagfile;

	return true;

} // SetDebugFlag

//---------------------------------------------------------------------------
void KLog::CheckDebugFlag()
//---------------------------------------------------------------------------
{
	// file format of the debug "flag" file:
	// "level, target" where level is numeric (-1 .. 3) and target can be
	// anything like a pathname or a domain:host or syslog, stderr, stdout

	time_t TouchTime = kGetLastMod(GetDebugFlag());

	if (TouchTime == -1)
	{
		// no flagfile (anymore)
		if (GetLevel() > 0)
		{
			SetLevel(0);
		}
	}
	else if (TouchTime > m_sTimestampFlagfile)
	{
		m_sTimestampFlagfile = TouchTime;

		KInFile file(GetDebugFlag());
		if (file.is_open())
		{
			KString sLine;
			if (file.ReadLine(sLine))
			{
				std::vector<KStringView> parts;
				kSplit(parts, sLine, ", ");
				size_t pos = 0;
				for (auto it : parts)
				{
					switch (pos)
					{
						case 0:
							{
								int iLvl = it.Int32();
								if (iLvl < 1)
								{
									iLvl = 1;
								}
								else if (iLvl > 3)
								{
									iLvl = 3;
								}
								SetLevel(iLvl);
							}
							break;
						case 1:
							SetDebugLog(it);
							break;
					}
					++pos;
				}
			}
			else
			{
				// empty file, set level to 1
				SetLevel(1);
			}
		}
	}

} // CheckDebugFlag

//---------------------------------------------------------------------------
bool KLog::IntDebug(int level, KStringView sFunction, KStringView sMessage)
//---------------------------------------------------------------------------
{
	// we need a lock if we run in multithreading, as the serializers
	// have data members
	static std::recursive_mutex mutex;
	std::lock_guard<std::recursive_mutex> Lock(mutex);

	if (!m_Logger || !m_Serializer)
	{
		return false;
	}

	m_Serializer->Set(level, m_sShortName, m_sPathName, sFunction, sMessage);

	static bool s_bBackTraceAlreadyCalled = false;

	if (level <= m_iBackTrace)
	{
		// we can protect the recursion without a mutex, as we
		// are already protected by a mutex..
		if (!s_bBackTraceAlreadyCalled)
		{
			s_bBackTraceAlreadyCalled = true;
			int iSkipFromStack{4};
			if (level == -2)
			{
				// for exceptions we have to peel off one more stack frame
				// (it is of course a brittle expectation of level == -2 == exception,
				// but for now it is true)
				iSkipFromStack += 1;
			}
			KString sStack = kGetBacktrace(iSkipFromStack);
			m_Serializer->SetBacktrace(sStack);
			s_bBackTraceAlreadyCalled = false;

			return m_Logger->Write(level, m_Serializer->IsMultiline(), m_Serializer->Get());
		}
	}

	return m_Logger->Write(level, m_Serializer->IsMultiline(), m_Serializer->Get());

} // IntDebug

//---------------------------------------------------------------------------
void KLog::IntException(KStringView sWhat, KStringView sFunction, KStringView sClass)
//---------------------------------------------------------------------------
{
	if (sFunction.empty())
	{
		sFunction = "(unknown)";
	}
	if (!sClass.empty())
	{
		IntDebug(-2, kFormat("{0}::{1}()", sClass, sFunction), kFormat("caught exception: '{0}'", sWhat));
	}
	else
	{
		IntDebug(-2, sFunction, kFormat("caught exception: '{0}'", sWhat));
	}

} // IntException

} // of namespace dekaf2
