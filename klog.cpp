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

#include "dekaf2.h"
#include "klog.h"
#include "kstring.h"
#include "kgetruntimestack.h"
#include "kstringutils.h"
#include "ksystem.h"
#include "ksplit.h"
#include "kfilesystem.h"
#include "kcgistream.h"
#include <mutex>
#include <iostream>

#ifdef DEKAF2_HAS_SYSLOG
	#include <syslog.h>
#endif

#ifdef DEKAF2_KLOG_WITH_TCP
	#include "kurl.h"
	#include "khttpclient.h"
	#include "kconnection.h"
	#include "kmime.h"
	#include "kjson.h"
#endif

namespace dekaf2
{

constexpr KStringViewZ s_sEnvLog      = "DEKAFLOG";
constexpr KStringViewZ s_sEnvFlag     = "DEKAFDBG";
constexpr KStringViewZ s_sEnvTrace    = "DEKAFTRC";
constexpr KStringViewZ s_sEnvLevel    = "DEKAFLEV";

constexpr KStringViewZ s_sLogName     = "dekaf.log";
constexpr KStringViewZ s_sFlagName    = "dekaf.dbg";

KString s_sDefaultLog;
KString s_sDefaultFlag;
KString s_sTempDir;

std::recursive_mutex KLog::s_LogMutex;
bool KLog::s_bBackTraceAlreadyCalled = false ;

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
	KString sBuffer(sFileName);
	// force mode 666 for the log file ...
	kChangeMode(sBuffer, 0666);

} // ctor

//---------------------------------------------------------------------------
bool KLogFileWriter::Write(int iLevel, bool bIsMultiline, const KString& sOut)
//---------------------------------------------------------------------------
{
	return m_OutFile.Write(sOut).Flush().Good();

} // Write

#ifdef DEKAF2_HAS_SYSLOG
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
#endif

#ifdef DEKAF2_KLOG_WITH_TCP

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
		m_OutStream = KConnection::Create(m_sURL);

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

	m_OutStream->Post(m_sURL, sOut, KMIME::JSON);

	return m_OutStream->Good();

} // Write

#endif // of DEKAF2_KLOG_WITH_TCP

//---------------------------------------------------------------------------
void KLogData::Set(int iLevel, KStringView sShortName, KStringView sPathName, KStringView sFunction, KStringView sMessage)
//---------------------------------------------------------------------------
{
	m_iLevel = iLevel;
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
void KLogSerializer::Set(int iLevel, KStringView sShortName, KStringView sPathName, KStringView sFunction, KStringView sMessage)
//---------------------------------------------------------------------------
{
	m_sBuffer.clear();
	m_bIsMultiline = false;
	KLogData::Set(iLevel, sShortName, sPathName, sFunction, sMessage);

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
		if (pos != KStringView::npos)
		{
			sMessage.remove_prefix(pos+1);
		}
		else
		{
			sMessage.clear();
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

	if (m_iLevel < 0)
	{
		sLevel = "WAR";
	}
	else
	{
		sLevel.Format("DB{}", m_iLevel > 3 ? 3 : m_iLevel);
	}

	KString sPrefix;

	sPrefix.Printf("| %3.3s | %5.5s | %5u | %5u | %s | ",
	               sLevel, m_sShortName, m_Pid, m_Tid, kFormTimestamp());

	auto PrefixWithoutFunctionSize = sPrefix.size();

	if (!m_sFunctionName.empty())
	{
		if (m_iLevel < 0)
		{
			// print the full function signature only if this is a warning or exception
			sPrefix += m_sFunctionName;
			sPrefix += ": ";
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

#ifdef DEKAF2_KLOG_WITH_TCP

//---------------------------------------------------------------------------
void KLogJSONSerializer::Serialize() const
//---------------------------------------------------------------------------
{
	m_sBuffer.reserve(m_sShortName.size()
	                  + m_sFunctionName.size()
	                  + m_sPathName.size()
	                  + m_sMessage.size()
	                  + 50);
	KJSON json;
	json["level"] = m_iLevel;
	json["pid"] = m_Pid;
	json["tid"] = m_Tid;
	json["time_t"] = m_Time;
	json["short_name"] = m_sShortName;
	json["path_name"] = m_sPathName;
	json["function_name"] = m_sFunctionName;
	json["message"] = m_sMessage;
	m_sBuffer = json.dump();

} // Serialize

#endif // of DEKAF2_KLOG_WITH_TCP

#ifdef DEKAF2_HAS_SYSLOG

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

#endif

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
	// if we do not start up in CGI mode, per default we log into stdout
	: m_bIsCGI       (!kGetEnv(KCGIInStream::REQUEST_METHOD).empty())
	, m_sLogName     (kGetEnv(s_sEnvLog, m_bIsCGI ? "" : STDOUT))
	, m_sFlagfile    (kGetEnv(s_sEnvFlag))
#ifdef NDEBUG
    , m_iBackTrace   (kGetEnv(s_sEnvTrace, "-3").Int16())
#else
    , m_iBackTrace   (kGetEnv(s_sEnvTrace, "-2").Int16())
#endif
	, m_Logmode      (m_bIsCGI ? SERVER : CLI)

{
#ifdef NDEBUG
	s_kLogLevel = kGetEnv(s_sEnvLevel, "-1").Int16();
#else
	s_kLogLevel = kGetEnv(s_sEnvLevel, "0").Int16();
#endif

	// find temp directory (which differs among systems and OSs)
#ifdef DEKAF2_IS_OSX
	// we do not want the /var/folders/wy/lz00g9_s27b2nmyfc52pjrjh0000gn/T - style temp dir on the Mac
	s_sTempDir = "/tmp";
#else
	s_sTempDir = kGetTemp();
#endif

	// construct default name for log file
	s_sDefaultLog = s_sTempDir;
	s_sDefaultLog += kDirSep;
	s_sDefaultLog += s_sLogName;

	// construct default name for flag file
	s_sDefaultFlag = s_sTempDir;
	s_sDefaultFlag += kDirSep;
	s_sDefaultFlag += s_sFlagName;

	if (m_sLogName.empty())
	{
		m_sLogName = s_sDefaultLog;
	}

	if (m_sFlagfile.empty())
	{
		m_sFlagfile = s_sDefaultFlag;
	}

	m_sPathName =  Dekaf().GetProgPath();
	m_sPathName += '/';
	m_sPathName += Dekaf().GetProgName();

	SetName(Dekaf().GetProgName());

	IntOpenLog();

	CheckDebugFlag();

	Dekaf().AddToOneSecTimer([this]()
	{
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

/*
 * don't write anymore to the flag file - let this be done by external
 * application code
 *
 */

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
	if (sLogfile.empty())
	{
		sLogfile = s_sDefaultLog; // restore default
	}

	// env values always override programmatic values, and at construction of
	// KLog() we would have fetched the env value already if it is non-zero
	KStringViewZ sEnv(kGetEnv(s_sEnvLog));
	if (!sEnv.empty() && (sEnv != sLogfile))
	{
		kDebug (0, "prevented setting the debug {} file to '{}' because the environment variable '{}' is set to '{}'",
		       "log",
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
#ifdef DEKAF2_HAS_SYSLOG
		case Writer::SYSLOG:
			return std::make_unique<KLogSyslogWriter>();
#endif
#ifdef DEKAF2_KLOG_WITH_TCP
		case Writer::TCP:
			return std::make_unique<KLogTCPWriter>(sLogname);
		case Writer::HTTP:
			return std::make_unique<KLogHTTPWriter>(sLogname);
#endif
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
#ifdef DEKAF2_HAS_SYSLOG
		case Serializer::SYSLOG:
			return std::make_unique<KLogSyslogSerializer>();
#endif
#ifdef DEKAF2_KLOG_WITH_TCP
		case Serializer::JSON:
			return std::make_unique<KLogJSONSerializer>();
#endif
	}

} // Create Serializer

//---------------------------------------------------------------------------
bool KLog::IntOpenLog()
//---------------------------------------------------------------------------
{
#ifdef DEKAF2_KLOG_WITH_TCP
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
	else
#endif
#ifdef DEKAF2_HAS_SYSLOG
	if (m_sLogName == SYSLOG)
	{
		SetWriter(CreateWriter(Writer::SYSLOG));
		SetSerializer(CreateSerializer(Serializer::SYSLOG));
	}
	else
#endif
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

/*
 * - new CLI MODE flag dekaf2::Klog() either has this set or not
 * - users of the Klog() like KREST can alter the mode
 * - application code should also have access to the mode switcher
 * - if CLI mode, klog flag was taken from CLI and ignores global flag file
 * - if SERVER mode, klog flag is taken from global flag file and managed with the timer
 * - if CLI mode, log file is: stderr for kWarning[Log] and stdout for all else
 * - if SERVER mode, log file is global setting (could be syslog api, a netcat socket or flat file, etc)
 */

//---------------------------------------------------------------------------
void KLog::SetMode(LOGMODE logmode)
//---------------------------------------------------------------------------
{
	if (m_Logmode != logmode)
	{
		m_Logmode = logmode;

		if (logmode == SERVER)
		{
			// if new mode == SERVER, first set debug log
			m_sLogName = kGetEnv(s_sEnvLog, s_sDefaultLog);
			// then read the debug flag
			CheckDebugFlag(true);
		}
		else
		{
			// if new mode == CLI set the output to stdout
			SetDebugLog(STDOUT);
		}
	}

} // SetMode

//---------------------------------------------------------------------------
bool KLog::SetDebugFlag(KStringViewZ sFlagfile)
//---------------------------------------------------------------------------
{
	if (sFlagfile.empty())
	{
		sFlagfile = s_sDefaultFlag; // restore default
	}

	#if 0
	// KEEF removed all this optimization logic. Just trust the application programmer.
	// If someone called KLog().SetDebugFlag() they are doing so for a reason.

	// env values always override programmatic values, and at construction of
	// KLog() we would have fetched the env value already if it is non-zero
	KStringViewZ sEnv(kGetEnv(s_sEnvFlag));
	if (!sEnv.empty() && (sEnv != sFlagfile))
	{
		kDebug(0, "prevented setting the debug {} file to '{}' because the environment variable '{}' is set to '{}'",
		       "flag",
			   sFlagfile,
		       s_sEnvFlag,
		       sEnv);
		return false;
	}

	if (sFlagfile == m_sFlagfile)
	{
		return true;
	}
	#endif

	m_sFlagfile = sFlagfile;

	// Because the debug flag file might have changed, we must FORCE a refresh of the log level,
	// even if it now points to an OLD file (the timestamp on the file is not relevant if we just
	// switched to it):
	CheckDebugFlag (/*bForce=*/true);

	return true;

} // SetDebugFlag

//---------------------------------------------------------------------------
void KLog::CheckDebugFlag(bool bForce/*=false*/)
//---------------------------------------------------------------------------
{
	if (m_Logmode == CLI)
	{
		return;
	}

	// file format of the debug "flag" file:
	// "level, target" where level is numeric (-1 .. 3) and target can be
	// anything like a pathname or a domain:host or syslog, stderr, stdout

	time_t tTouchTime = kGetLastMod(GetDebugFlag());

	if (tTouchTime == -1)
	{
		// no flagfile (anymore)
		if (GetLevel() > 0)
		{
			SetLevel(0);
		}
	}
	else if (bForce || (tTouchTime > m_sTimestampFlagfile))
	{
		m_sTimestampFlagfile = tTouchTime;

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
bool KLog::IntDebug(int iLevel, KStringView sFunction, KStringView sMessage)
//---------------------------------------------------------------------------
{
	// Moving this check to the first place helps avoiding
	// static deinitialization races with static instances of classes that will
	// output to KLog in their destructor.
	// It also allows to call functions with potential KLog output in the
	// constructor of KLog, because the Logger and Serializer is only set at
	// the end of construction. Until then, logging is disabled.
	if (!m_Logger || !m_Serializer)
	{
		return false;
	}

	// we need a lock if we run in multithreading, as the serializers
	// have data members
	std::lock_guard<std::recursive_mutex> Lock(s_LogMutex);

	m_Serializer->Set(iLevel, m_sShortName, m_sPathName, sFunction, sMessage);

	if (iLevel <= m_iBackTrace)
	{
		// we can protect the recursion without a mutex, as we
		// are already protected by a mutex..
		if (!s_bBackTraceAlreadyCalled)
		{
			s_bBackTraceAlreadyCalled = true;
			int iSkipFromStack{4};
			if (iLevel == -2)
			{
				// for exceptions we have to peel off one more stack frame
				// (it is of course a brittle expectation of level == -2 == exception,
				// but for now it is true)
				iSkipFromStack += 1;
			}
			KString sStack = kGetBacktrace(iSkipFromStack);
			m_Serializer->SetBacktrace(sStack);
			s_bBackTraceAlreadyCalled = false;

			return m_Logger->Write(iLevel, m_Serializer->IsMultiline(), m_Serializer->Get());
		}
	}

	return m_Logger->Write(iLevel, m_Serializer->IsMultiline(), m_Serializer->Get());

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

#if 0 // no longer used
//---------------------------------------------------------------------------
void KLog::trace_json()
//---------------------------------------------------------------------------
{
	if (!m_Logger || !m_Serializer)
	{
		return;
	}

	// we need a lock if we run in multithreading, as the serializers
	// have data members
	std::lock_guard<std::recursive_mutex> Lock(s_LogMutex);

	// we can protect the recursion without a mutex, as we
	// are already protected by a mutex..
	if (!s_bBackTraceAlreadyCalled)
	{
		s_bBackTraceAlreadyCalled = true;
		auto Frame = kFilterTrace(5, "parser.hpp,json_sax.hpp,json.hpp,krow.cpp,to_json.hpp,from_json.hpp,adl_serializer.hpp,krow.h,kjson.hpp,kjson.cpp");
		s_bBackTraceAlreadyCalled = false;
		auto sFunction = kNormalizeFunctionName(Frame.sFunction);
		if (!sFunction.empty())
		{
			sFunction += "()";
		}
		KString sFile = "JSON exception at ";
		sFile += Frame.sFile;
		sFile += ':';
		sFile += Frame.sLineNumber;
		m_Serializer->Set(-2, m_sShortName, m_sPathName, sFunction, sFile);

		m_Logger->Write(-2, m_Serializer->IsMultiline(), m_Serializer->Get());
	}

} // trace_json()
#endif

#ifdef DEKAF2_REPEAT_CONSTEXPR_VARIABLE

constexpr KStringViewZ KLog::STDOUT;
constexpr KStringViewZ KLog::STDERR;
constexpr KStringViewZ KLog::SYSLOG;
constexpr KStringViewZ KLog::DBAR;
constexpr KStringViewZ KLog::BAR;
constexpr KStringViewZ KLog::DASH;

#endif

} // of namespace dekaf2
