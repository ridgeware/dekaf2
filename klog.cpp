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
[x] output format as a single line was completely misunderstood -- partially fixed
[x] program name was misunderstood -- partially fixed
[ ] no way to specify "stdout" "stderr" TRACE() "syslog" as log output 
[ ] with {} style formatting there is no way to format things like %03d, need multiple methods I guess
[ ] constructor is *removing* the klog when program starts (completely wrong)
[ ] not sure I like KLog as a classname.  probable KLOG.
[ ] kDebug() and kWarning() macros should probably be kDebugFormat() and kDebugPrintf()
*/

#include <iostream>
#include <syslog.h>
#include "dekaf2.h"
#include "klog.h"
#include "kstring.h"
#include "kgetruntimestack.h"
#include "kstringutils.h"
#include "ksystem.h"
#include "kurl.h"
#include "khttp.h"
#include "kjson.h"

namespace dekaf2
{


//---------------------------------------------------------------------------
void KLogData::Set(int level, KStringView sShortName, KStringView sPathName, KStringView sFunction, KStringView sMessage)
//---------------------------------------------------------------------------
{
	m_Level = level;
	m_Pid = getpid();
	m_Time = Dekaf().CurrentTime();
	m_sFunctionName = SanitizeFunctionName(sFunction);
	m_sShortName = sShortName;
	m_sPathName = sPathName;
	m_sMessage = sMessage;
	kTrim(m_sMessage);
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
KLogSerializer::operator const KString&() const
//---------------------------------------------------------------------------
{
	if (m_sBuffer.empty())
	{
		Serialize();
	}
	return m_sBuffer;

} // operator KString

//---------------------------------------------------------------------------
void KLogSerializer::Set(int level, KStringView sShortName, KStringView sPathName, KStringView sFunction, KStringView sMessage)
//---------------------------------------------------------------------------
{
	m_sBuffer.clear();
	m_bIsMultiline = false;
	KLogData::Set(level, sShortName, sPathName, sFunction, sMessage);

} // Set

//---------------------------------------------------------------------------
void KLogTTYSerializer::HandleMultiLineMessages() const
//---------------------------------------------------------------------------
{
	// keep the prefix range in a string view if we have to repeat it
	// for multi line messages
	KStringView sPrefix = m_sBuffer;
	KStringView sMessage = m_sMessage;

	auto pos = sMessage.find('\n');
	while (pos != KStringView::npos)
	{
		KStringView sLine = sMessage.substr(0, pos);
		kTrimRight(sLine);
		if (!sLine.empty())
		{
			// repeat the prefix for subsequent lines
			if (sPrefix.size() < m_sBuffer.size())
			{
				m_bIsMultiline = true;
				m_sBuffer += '\n';
				m_sBuffer += sPrefix;
			}
			// and append the new line
			m_sBuffer += sLine;
		}
		sMessage.remove_prefix(pos + 1);
		pos = sMessage.find('\n');
	}

	if (!sMessage.empty())
	{
		// repeat the prefix for subsequent lines
		if (sPrefix.size() < m_sBuffer.size())
		{
			m_bIsMultiline = true;
			m_sBuffer += '\n';
			m_sBuffer += sPrefix;
		}
		// and append the last line
		m_sBuffer += sMessage;
	}
	m_sBuffer += '\n';

} // HandleMultiLineMessages

//---------------------------------------------------------------------------
void KLogTTYSerializer::Serialize() const
//---------------------------------------------------------------------------
{
	// desired format:
	// | WAR | MYPRO | 17202 | 2001-08-24 10:37:04 | Function: select count(*) from foo

	KString sLevel;

	if (m_Level < 0)
	{
		sLevel = "WAR";
	}
	else
	{
		sLevel.Format("DB{}", m_Level > 3 ? 3 : m_Level);
	}

	m_sBuffer.Printf("| %3.3s | %5.5s | %5u | %s | ", sLevel, m_sShortName, getpid(), kFormTimestamp());

	if (!m_sFunctionName.empty())
	{
		m_sBuffer += m_sFunctionName;
		m_sBuffer += ": ";
	}

	HandleMultiLineMessages();

} // Serialize

//---------------------------------------------------------------------------
void KLogJSONSerializer::Serialize() const
//---------------------------------------------------------------------------
{
	LJSON json;
	json["level"]         = m_Level;
	json["pid"]           = m_Pid;
	json["time"]          = m_Time;
	json["short_name"]    = m_sShortName;
	json["exe_name"]      = m_sPathName;
	json["function_name"] = m_sFunctionName;
	json["message"]       = m_sMessage;
	if (!m_sBacktrace.empty())
	{
		json["stack"]     = m_sBacktrace;
	}
	// pretty print the json into our string buffer
	m_sBuffer = json.dump(1, '\t');

} // Serialize

//---------------------------------------------------------------------------
void KLogSyslogSerializer::Serialize() const
//---------------------------------------------------------------------------
{
	m_sBuffer = m_sFunctionName;
	if (!m_sBuffer.empty())
	{
		m_sBuffer += ": ";
	}

	HandleMultiLineMessages();

} // Serialize

//---------------------------------------------------------------------------
KLogWriter::~KLogWriter()
//---------------------------------------------------------------------------
{
}

//---------------------------------------------------------------------------
bool KLogStdWriter::Write(const KLogSerializer& Serializer)
//---------------------------------------------------------------------------
{
	const KString& sMessage = Serializer;
	return m_OutStream.write(sMessage.data(), static_cast<std::streamsize>(sMessage.size())).flush().good();

} // Write

//---------------------------------------------------------------------------
bool KLogFileWriter::Write(const KLogSerializer& Serializer)
//---------------------------------------------------------------------------
{
	const KString& sMessage = Serializer;
	return m_OutFile.write(sMessage.data(), static_cast<std::streamsize>(sMessage.size())).flush().good();

} // Write

//---------------------------------------------------------------------------
bool KLogSyslogWriter::Write(const KLogSerializer& Serializer)
//---------------------------------------------------------------------------
{
	int priority;
	switch (Serializer.GetLevel())
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

	const KString& sMessage = Serializer;

	if (!Serializer.IsMultiline())
	{
		syslog(priority, "%s", sMessage.c_str());
	}
	else
	{
		KStringView svMessage = sMessage;
		KString sPart;

		// the static_cast<void> is necessary to silence a warning for the use
		// of the sequence operator here
		for (KString::size_type pos;
		     static_cast<void>(pos = sMessage.find('\n')), pos != KString::npos;)
		{
			sPart = svMessage.substr(0, pos);
			syslog(priority, "%s", sPart.c_str());
			svMessage.remove_prefix(pos + 1);
		}

		if (!svMessage.empty())
		{
			sPart = svMessage;
			syslog(priority, "%s", sPart.c_str());
		}
	}

	return true;

} // Write

//---------------------------------------------------------------------------
KLogTCPWriter::KLogTCPWriter(KStringView sURL)
//---------------------------------------------------------------------------
{
	KURL url(sURL);
	KStringView sv = url.Domain.Serialize();
	std::string sd(sv.data(), sv.size());
	sv = url.Port.Serialize();
	std::string sp(sv.data(), sv.size());
	m_OutStream = std::make_unique<KTCPStream>(sd, sp);

} // ctor

//---------------------------------------------------------------------------
bool KLogTCPWriter::Write(const KLogSerializer& Serializer)
//---------------------------------------------------------------------------
{
	const KString& sMessage = Serializer;
	return m_OutStream != nullptr
	    && m_OutStream->write(sMessage.data(), static_cast<std::streamsize>(sMessage.size())).flush().good();

} // Write

//---------------------------------------------------------------------------
bool KLogHTTPWriter::Write(const KLogSerializer& Serializer)
//---------------------------------------------------------------------------
{
	// TODO add HTTP protocol handler
	const KString& sMessage = Serializer;
	return m_OutStream != nullptr
	    && m_OutStream->write(sMessage.data(), static_cast<std::streamsize>(sMessage.size())).flush().good();

} // Write



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

} // ctor

//---------------------------------------------------------------------------
void KLog::SetName(KStringView sName)
//---------------------------------------------------------------------------
{
	m_sShortName = sName;

	if (m_sShortName.size() > 5)
	{
		m_sShortName.erase(5, KString::npos);
	}

	m_sShortName.MakeUpper();

}

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
}

//---------------------------------------------------------------------------
bool KLog::SetSerializer(std::unique_ptr<KLogSerializer> serializer)
//---------------------------------------------------------------------------
{
	m_Serializer = std::move(serializer);
	return m_Serializer != nullptr;
}

//---------------------------------------------------------------------------
std::unique_ptr<KLogWriter> KLog::CreateWriter(Writer writer, KStringView sLogname)
//---------------------------------------------------------------------------
{
	switch (writer)
	{
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
}

//---------------------------------------------------------------------------
std::unique_ptr<KLogSerializer> KLog::CreateSerializer(Serializer serializer)
//---------------------------------------------------------------------------
{
	switch (serializer)
	{
		case Serializer::TTY:
			return std::make_unique<KLogTTYSerializer>();
		case Serializer::SYSLOG:
			return std::make_unique<KLogSyslogSerializer>();
		case Serializer::JSON:
			return std::make_unique<KLogJSONSerializer>();
	}
}

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
}

//---------------------------------------------------------------------------
bool KLog::IntDebug(int level, KStringView sFunction, KStringView sMessage)
//---------------------------------------------------------------------------
{
	if (!m_Logger || !m_Serializer)
	{
		return false;
	}

	m_Serializer->Set(level, m_sShortName, m_sPathName, sFunction, sMessage);

	if (level <= m_iBackTrace)
	{
		KString sStack = kGetBacktrace();
		m_Serializer->SetBacktrace(sStack);
		m_Logger->Write(*m_Serializer);
	}
	else
	{
		m_Logger->Write(*m_Serializer);
	}

	return m_Logger->Good();

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
		warning("{0}::{1}() caught exception: '{2}'", sClass, sFunction, sWhat);
	}
	else
	{
		warning("{0} caught exception: '{1}'", sFunction, sWhat);
	}
}

// TODO add a mechanism to check periodically for the flag file's set level

} // of namespace dekaf2
