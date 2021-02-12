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

#include "klog.h"
#include "bits/klogwriter.h"
#include "bits/klogserializer.h"
#include "dekaf2.h"
#include "kstring.h"
#include "kgetruntimestack.h"
#include "kstringutils.h"
#include "ksystem.h"
#include "kfilesystem.h"
#include "kcgistream.h"
#include <mutex>
#include <iostream>

#ifdef DEKAF2_HAS_SYSLOG
	#include <syslog.h>
#endif

#ifdef DEKAF2_KLOG_WITH_TCP
	#include "kurl.h"
#endif

namespace dekaf2
{

constexpr KStringViewZ s_sEnvLog      = "DEKAFLOG";
constexpr KStringViewZ s_sEnvFlag     = "DEKAFDBG";
constexpr KStringViewZ s_sEnvTrace    = "DEKAFTRC";
constexpr KStringViewZ s_sEnvLevel    = "DEKAFLEV";
constexpr KStringViewZ s_sEnvLogDir   = "DEKAFDIR";
constexpr KStringViewZ s_sJSONTrace   = "DEKAFJSONTRACE";

constexpr KStringViewZ s_sLogName     = "dekaf.log";
constexpr KStringViewZ s_sFlagName    = "dekaf.dbg";

thread_local std::unique_ptr<KLogSerializer> KLog::s_PerThreadSerializer;
thread_local std::unique_ptr<KLogWriter> KLog::s_PerThreadWriter;
thread_local KString KLog::s_sPerThreadGrepExpression;
thread_local bool KLog::s_bShouldShowStackOnJsonError { true };
thread_local bool KLog::s_bPrintTimeStampOnClose { false };
thread_local bool KLog::s_bPerThreadEGrep { false };
thread_local bool KLog::PreventRecursion::s_bCalledFromInsideKlog { false };

// do not initialize this static var - it risks to override a value set by KLog()'s
// initialization before..
int KLog::s_iLogLevel;

// this one however has to be initialized for every new thread to the value of
// the current global log level
thread_local int KLog::s_iThreadLogLevel { s_iLogLevel };

//---------------------------------------------------------------------------
KLog::KLog()
//---------------------------------------------------------------------------
	// if we do not start up in CGI mode, per default we log into stdout
	: m_bIsCGI       (!kGetEnv(KCGIInStream::REQUEST_METHOD).empty())
	, m_Logmode      (m_bIsCGI ? SERVER : CLI)
{
	// check if we have an environment setting for the preferred output directory
	m_sLogDir = kGetEnv(s_sEnvLogDir, "/shared");
	
	if (!m_sLogDir.empty())
	{
		if (kDirExists(m_sLogDir))
		{
			// construct default name for log file
			KString sTest = m_sLogDir;
			sTest += kDirSep;
			sTest += s_sLogName;

			// test if the file already exists
			bool bHaveExistingLog = kFileExists(sTest);

			// check if we can write in this directory
			if (!kTouchFile(sTest))
			{
				// no - fall back to the temp folder
				m_sLogDir.clear();
			}
			else if (!bHaveExistingLog)
			{
				// clean up
				kRemoveFile(sTest);
			}
		}
		else
		{
			m_sLogDir.clear();
		}
	}

	if (m_sLogDir.empty())
	{
		// find temp directory (which differs among systems and OSs)
#ifdef DEKAF2_IS_OSX
		// we do not want the /var/folders/wy/lz00g9_s27b2nmyfc52pjrjh0000gn/T - style temp dir on the Mac
		m_sLogDir = "/tmp";
#else
		m_sLogDir = kGetTemp();
#endif
	}

	// construct default name for log file
	m_sDefaultLog = m_sLogDir;
	m_sDefaultLog += kDirSep;
	m_sDefaultLog += s_sLogName;

	// construct default name for flag file
	m_sDefaultFlag = m_sLogDir;
	m_sDefaultFlag += kDirSep;
	m_sDefaultFlag += s_sFlagName;

	m_sPathName =  Dekaf::getInstance().GetProgPath();
	m_sPathName += kDirSep;
	m_sPathName += Dekaf::getInstance().GetProgName();

	SetName(Dekaf::getInstance().GetProgName());

	SetDefaults();

	CheckDebugFlag();

	Dekaf::getInstance().AddToOneSecTimer([this]()
	{
		this->CheckDebugFlag();
	});

} // ctor

//---------------------------------------------------------------------------
KLog::~KLog()
//---------------------------------------------------------------------------
{
} // dtor

//---------------------------------------------------------------------------
KLog& KLog::SetDefaults()
//---------------------------------------------------------------------------
{
	// reset to defaults

	// resets to s_sDefaultLog if empty
	SetDebugLog(kGetEnv(s_sEnvLog, (m_bIsCGI || m_Logmode == LOGMODE::SERVER) ? "" : STDOUT));

	// do not use SetDebugFlag() as it forces an immediate read of the flagfile,
	// which would loop..
	m_sFlagfile = kGetEnv(s_sEnvFlag);

	if (m_sFlagfile.empty())
	{
		m_sFlagfile = m_sDefaultFlag;
	}

#ifdef NDEBUG
	SetLevel(kGetEnv(s_sEnvLevel, "-1").Int16());
	SetBackTraceLevel(kGetEnv(s_sEnvTrace, "-3").Int16());
#else
	SetLevel(kGetEnv(s_sEnvLevel, "0").Int16());
	SetBackTraceLevel(kGetEnv(s_sEnvTrace, "-2").Int16());
#endif

	SetJSONTrace(kGetEnv(s_sJSONTrace));

	std::lock_guard<std::recursive_mutex> Lock(m_LogMutex);

	m_Traces.clear();

	return *this;

} // SetDefaults

//---------------------------------------------------------------------------
KLog& KLog::LogThisThreadToKLog(int iLevel)
//---------------------------------------------------------------------------
{
	if (iLevel > 0)
	{
		s_iThreadLogLevel = iLevel;
	}
	else
	{
		s_iThreadLogLevel = s_iLogLevel;
	}

	if (s_bPrintTimeStampOnClose)
	{
		if (s_PerThreadSerializer && s_PerThreadWriter)
		{
			auto pSerializerInstance = static_cast<KLogHTTPHeaderSerializer*>(s_PerThreadSerializer.get());
			s_PerThreadWriter->Write(0, false, pSerializerInstance->GetTimeStamp("Stop"));
			s_PerThreadWriter->Write(0, true,  KLogHTTPHeaderSerializer::GetFooter());
		}
	}

	s_PerThreadWriter.reset();
	s_PerThreadSerializer.reset();

	return *this;

} // LogThisThreadToKLog

#ifdef DEKAF2_KLOG_WITH_TCP

//---------------------------------------------------------------------------
KLog& KLog::LogThisThreadToResponseHeaders(int iLevel, KHTTPHeaders& Response, KStringView sHeader)
//---------------------------------------------------------------------------
{
	if (iLevel > 0)
	{
		s_iThreadLogLevel = iLevel;
		s_PerThreadWriter = std::make_unique<KLogHTTPHeaderWriter>(Response, sHeader);
		s_PerThreadSerializer = std::make_unique<KLogHTTPHeaderSerializer>();
		auto pSerializerInstance = static_cast<KLogHTTPHeaderSerializer*>(s_PerThreadSerializer.get());
		s_PerThreadWriter->Write(0, true,  KLogHTTPHeaderSerializer::GetHeader());
		s_PerThreadWriter->Write(0, false, pSerializerInstance->GetTimeStamp("Start"));
		s_bPrintTimeStampOnClose = true;
	}
	else
	{
		s_iThreadLogLevel = s_iLogLevel;
		s_PerThreadWriter.reset();
		s_PerThreadSerializer.reset();
	}

	return *this;

} // LogThisThreadToResponseHeaders

//---------------------------------------------------------------------------
KLog& KLog::LogThisThreadToJSON(int iLevel, void* pjson)
//---------------------------------------------------------------------------
{
	if (iLevel > 0 && pjson)
	{
		KJSON* json = static_cast<KJSON*>(pjson);
		s_iThreadLogLevel = iLevel;
		s_PerThreadWriter = std::make_unique<KLogNullWriter>();
		s_PerThreadSerializer = std::make_unique<KLogJSONArraySerializer>(*json);
	}
	else
	{
		s_iThreadLogLevel = s_iLogLevel;
		s_PerThreadWriter.reset();
		s_PerThreadSerializer.reset();
	}

	return *this;

} // LogThisThreadToJSON

#endif // DEKAF2_KLOG_WITH_TCP


//---------------------------------------------------------------------------
KLog& KLog::LogThisThreadWithGrepExpression(bool bEGrep, KString sGrepExpression)
//---------------------------------------------------------------------------
{
	kDebug(2, "using {}grep expression '{}'", bEGrep ? "e" : "", sGrepExpression);
	s_sPerThreadGrepExpression = std::move(sGrepExpression);
	s_bPerThreadEGrep = bEGrep;

	return *this;

} //  LogThisThreadWithGrepExpression

//---------------------------------------------------------------------------
KLog& KLog::SetJSONTrace(KStringView sJSONTrace)
//---------------------------------------------------------------------------
{
	if (!sJSONTrace.empty())
	{
		auto sJSONTraceLower = sJSONTrace.ToLower();
		
		if (sJSONTraceLower.In("off,false,no,0"))
		{
			m_bGlobalShouldShowStackOnJsonError = false;
		}
		else
		{
			m_bGlobalShouldShowStackOnJsonError = true;

			if (sJSONTraceLower.In("caller,short"))
			{
				m_bGlobalShouldOnlyShowCallerOnJsonError = true;
			}
		}
	}

	return *this;

} // SetJSONTrace

//---------------------------------------------------------------------------
KStringView KLog::GetJSONTrace() const
//---------------------------------------------------------------------------
{
	if (!m_bGlobalShouldShowStackOnJsonError)
	{
		return "off";
	}
	else if (m_bGlobalShouldOnlyShowCallerOnJsonError)
	{
		return "short";
	}
	else
	{
		return "full";
	}

} // GetJSONTrace

//---------------------------------------------------------------------------
KLog& KLog::SetLevel(int iLevel)
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

	s_iLogLevel = iLevel;
	s_iThreadLogLevel = iLevel;

/*
 * don't write anymore to the flag file - let this be done by external
 * application code
 *
 */

	return *this;

} // SetLevel

//---------------------------------------------------------------------------
void KLog::SyncLevel()
//---------------------------------------------------------------------------
{
	s_iThreadLogLevel = s_iLogLevel;

} // SyncLevel

//---------------------------------------------------------------------------
KLog& KLog::SetName(KStringView sName)
//---------------------------------------------------------------------------
{
	if (sName.size() > 5)
	{
		sName.erase(5, KStringView::npos);
	}

	m_sShortName = kToUpper(sName);

	return *this;

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
		sLogfile = m_sDefaultLog; // restore default
	}

	if (sLogfile == m_sLogName)
	{
		return true;
	}

	m_sLogName = sLogfile;

	return IntOpenLog();

} // SetDebugLog

//---------------------------------------------------------------------------
KLog& KLog::SetWriter(std::unique_ptr<KLogWriter> logger)
//---------------------------------------------------------------------------
{
	m_Logger = std::move(logger);
	return *this;

} // SetWriter

//---------------------------------------------------------------------------
KLog& KLog::SetWriter(Writer writer, KStringView sLogname)
//---------------------------------------------------------------------------
{
	return SetWriter(CreateWriter(writer, sLogname));

} // SetWriter

//---------------------------------------------------------------------------
KLog& KLog::SetSerializer(std::unique_ptr<KLogSerializer> serializer)
//---------------------------------------------------------------------------
{
	m_Serializer = std::move(serializer);
	return *this;

} // SetSerializer

//---------------------------------------------------------------------------
KLog& KLog::SetSerializer(Serializer serializer)
//---------------------------------------------------------------------------
{
	return SetSerializer(CreateSerializer(serializer));

} // SetSerializer

//---------------------------------------------------------------------------
std::unique_ptr<KLogWriter> KLog::CreateWriter(Writer writer, KStringView sLogname)
//---------------------------------------------------------------------------
{
	switch (writer)
	{
		default:
		case Writer::NONE:
			return std::make_unique<KLogNullWriter>();
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
	m_bLogIsRegularFile = false;
	
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
		// this is a file
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
			m_bLogIsRegularFile = true;
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
KLog& KLog::SetMode(LOGMODE logmode)
//---------------------------------------------------------------------------
{
	if (m_Logmode != logmode)
	{
		m_Logmode = logmode;

		if (logmode == SERVER)
		{
			if (!m_bKeepCLIMode)
			{
				// if new mode == SERVER, first set debug log
				SetDebugLog(kGetEnv(s_sEnvLog, m_sDefaultLog));
				// then read the debug flag
				CheckDebugFlag(true);
			}
		}
		else
		{
			// if new mode == CLI set the output to stdout
			SetDebugLog(STDOUT);
		}
	}

	return *this;

} // SetMode

//---------------------------------------------------------------------------
bool KLog::SetDebugFlag(KStringView sFlagfile)
//---------------------------------------------------------------------------
{
	if (sFlagfile.empty())
	{
		sFlagfile = m_sDefaultFlag; // restore default
	}

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

	static std::mutex s_DebugFlagMutex;
	std::lock_guard<std::mutex> Lock(s_DebugFlagMutex);

	// we periodically check if our log file was removed (e.g. by the klog cli),
	// in which case we have to close and reopen it, as otherwise we would
	// continue to write into a deleted file descriptor.
	if (m_bLogIsRegularFile)
	{
		if (!kFileExists(m_sLogName))
		{
			// file was removed.. reopen
			SetWriter(CreateWriter(Writer::FILE, m_sLogName));
		}
		// check if file grew too big
		else if (kFileSize(m_sLogName) > 10 * 1024 * 1024)
		{
			if (GetLevel() > 1)
			{
				kDebug(1, "Logfile now too large, reducing log level from {} to 1", GetLevel());
				SetLevel(1);
			}
		}
	}

	// file format of the debug "flag" file:
	// line #1: "level" where level is numeric (-1 .. 3)
	// following lines are key-value pairs in ini style
	// with the keys
	// log=        :: a pathname or a domain:host or syslog, stderr, stdout
	// trace=      :: a string that, if matched in any debug message, forces a stacktrace
	// tracelevel= :: forces stacktraces for all warnings <= the numeric tracelevel
	// tracejson=  :: "OFF,off,FALSE,false,NO,no,0" :: switches off
	//             :: "CALLER,caller,SHORT,short"   :: switches to caller frame only
	//             ::                               :: all else switches full trace on

	// this format is compatible to the dekaf1 flag file format (which only reads the first line)

	time_t tTouchTime = kGetLastMod(GetDebugFlag());

	if (tTouchTime == -1)
	{
		// no flagfile (anymore)
		if (m_bHadConfigFromFlagFile)
		{
			m_bHadConfigFromFlagFile = false;
			SetDefaults();
		}
	}
	else if (bForce || (tTouchTime > m_sTimestampFlagfile))
	{
		m_sTimestampFlagfile = tTouchTime;

		SetDefaults();
		
		KInFile file(GetDebugFlag());
		
		if (file.is_open())
		{
			m_bHadConfigFromFlagFile = true;

			KString sLine;
			// read the first line
			if (file.ReadLine(sLine))
			{
				size_t pos = 0;

				for (auto it : sLine.Split(", "))
				{
					switch (pos)
					{
						case 0:
							{
								int iLvl = it.Int32();
								if (iLvl < 0)
								{
									iLvl = 0;
								}
								else if (iLvl > 3)
								{
									iLvl = 3;
								}
								SetLevel(iLvl);
							}
							break;
							
						case 1:
							// this was the old format of "level, log" in the first line
							SetDebugLog(it);
							break;
					}
					++pos;
				}

				// now read all following lines
				while (file.ReadLine(sLine))
				{
					auto it = kSplitToPair(sLine);

					if (!it.first.empty())
					{
						if (it.first == "log")
						{
							SetDebugLog(it.second);
						}

						else if (it.first == "trace")
						{
							std::lock_guard<std::recursive_mutex> Lock(m_LogMutex);

							bool bNewTrace { true };

							for (const auto& sTrace : m_Traces)
							{
								if (it.second.find(sTrace) != KStringView::npos)
								{
									// this trace pattern or parts of it are already known
									bNewTrace = false;
									break;
								}
							}

							if (bNewTrace)
							{
								m_Traces.push_back(it.second);
							}
						}

						else if (it.first == "tracelevel")
						{
							if (kIsInteger(it.second))
							{
								SetBackTraceLevel(it.second.Int16());
							}
						}

						else if (it.first == "tracejson")
						{
							SetJSONTrace(it.second);
						}
					}
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
KLog& KLog::LogWithGrepExpression(bool bEGrep, bool bInverted, KString sGrepExpression)
//---------------------------------------------------------------------------
{
	// change string values in multithreading only with a mutex
	std::lock_guard<std::recursive_mutex> Lock(m_LogMutex);
	m_bEGrep = bEGrep;
	m_bInvertedGrep = bInverted;
	m_sGrepExpression = sGrepExpression;

	return *this;

} // LogWithGrepExpression

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

	if (DEKAF2_UNLIKELY(iLevel > s_iLogLevel && iLevel > s_iThreadLogLevel))
	{
		// bail out early if this method was called without checking the
		// log levels before (which should be really rare thanks to the
		// debug macros)
		return false;
	}

	// Prevent recursive calling through internal calls to instrumented functions
	// like, e.g. kFormTimestamp()
	PreventRecursion PR;

	if (PR.IsRecursive())
	{
		return false;
	}

	// We need a lock if we run in multithreading, as the serializers
	// have data members. We use a recursive mutex because we want to
	// protect multiple entry points that eventually call this function.
	std::lock_guard<std::recursive_mutex> Lock(m_LogMutex);

	if (DEKAF2_LIKELY(iLevel <= s_iLogLevel) || iLevel <= s_iThreadLogLevel)
	{
		// this is the regular logging, with a per-thread adaptable iLevel

		m_Serializer->Set(iLevel, m_sShortName, m_sPathName, sFunction, sMessage);

		// check if we shall print a stacktrace on demand
		if (iLevel > m_iBackTrace)
		{
			for (const auto& sTrace : m_Traces)
			{
				if (sFunction.contains(sTrace) ||
					sMessage.contains(sTrace))
				{
					iLevel = m_iBackTrace;
					break;
				}
			}
		}

		// need to keep this buffer until the log record is written
		KString sStack;

		if (iLevel <= m_iBackTrace)
		{
			// we can protect the recursion without a mutex, as we
			// are already protected by a mutex..
			if (!m_bBackTraceAlreadyCalled)
			{
				m_bBackTraceAlreadyCalled = true;
				int iSkipFromStack { 2 };
				if (iLevel == -2)
				{
					// for exceptions we have to peel off one more stack frame
					// (it is of course a brittle expectation of level == -2 == exception,
					// but for now it is true)
					iSkipFromStack += 1;
				}
				sStack = kGetBacktrace(iSkipFromStack);
				m_Serializer->SetBacktrace(sStack);
				m_bBackTraceAlreadyCalled = false;
			}
		}

		if (m_Serializer->Matches(m_bEGrep, m_bInvertedGrep, m_sGrepExpression))
		{
			m_Logger->Write(iLevel, m_Serializer->IsMultiline(), m_Serializer->Get());
		}
	}

	if (DEKAF2_UNLIKELY(iLevel <= s_iThreadLogLevel &&
						s_PerThreadSerializer &&
						s_PerThreadWriter))
	{
		do
		{
			// this is the individual per-thread-logging
			s_PerThreadSerializer->Set(iLevel, m_sShortName, m_sPathName, sFunction, sMessage);

			if (s_PerThreadSerializer->Matches(s_bPerThreadEGrep, false, s_sPerThreadGrepExpression))
			{
				s_PerThreadWriter->Write(iLevel, s_PerThreadSerializer->IsMultiline(), s_PerThreadSerializer->Get());
			}
		}
		while (false);
	}

	return true;

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

//---------------------------------------------------------------------------
// This was originally written to print the caller of throwing JSON code.
// For JSON, the configuration would be:
// iSkipStackLines = 3
// sSkipFiles = "parser.hpp,json_sax.hpp,json.hpp,krow.cpp,to_json.hpp,from_json.hpp,adl_serializer.hpp,krow.h,kjson.hpp,kjson.cpp"
// sMessage = "JSON exception"
void KLog::TraceDownCaller(int iSkipStackLines, KStringView sSkipFiles, KStringView sMessage)
//---------------------------------------------------------------------------
{
	if (!m_Logger || !m_Serializer)
	{
		return;
	}

	// we need a lock if we run in multithreading, as the serializers
	// have data members
	std::lock_guard<std::recursive_mutex> Lock(m_LogMutex);

	// we can protect the recursion without a mutex, as we
	// are already protected by a mutex..
	if (!m_bBackTraceAlreadyCalled)
	{
		m_bBackTraceAlreadyCalled = true;
		auto Frame = kFilterTrace(iSkipStackLines, sSkipFiles);
		m_bBackTraceAlreadyCalled = false;
		auto sFunction = kNormalizeFunctionName(Frame.sFunction);
		if (!sFunction.empty())
		{
			sFunction += "()";
		}
		KString sFile = sMessage;
		sFile += " at ";
		sFile += Frame.sFile;
		sFile += ':';
		sFile += Frame.sLineNumber;

		m_Serializer->Set(-2, m_sShortName, m_sPathName, sFunction, sFile);
		m_Logger->Write(-2, m_Serializer->IsMultiline(), m_Serializer->Get());
	}

} // TraceDownCaller

//---------------------------------------------------------------------------
void KLog::JSONTrace(KStringView sFunction)
//---------------------------------------------------------------------------
{
	if (m_bGlobalShouldShowStackOnJsonError && s_bShouldShowStackOnJsonError)
	{
		if (m_bGlobalShouldOnlyShowCallerOnJsonError)
		{
			static constexpr KStringView s_sJSONSkipFiles {
				"kgetruntimestack.cpp,kgetruntimestack.h,klog.cpp,klog.h,"
				"parser.hpp,json_sax.hpp,json.hpp,krow.cpp,"
				"to_json.hpp,from_json.hpp,adl_serializer.hpp,krow.h,"
				"kjson.hpp,kjson.cpp"
			};

			TraceDownCaller(1, s_sJSONSkipFiles, "JSON Exception");
		}
		else
		{
			IntDebug(-2, sFunction, "JSON Exception");
		}
	}

} // JSONTrace

#ifdef DEKAF2_REPEAT_CONSTEXPR_VARIABLE

constexpr KStringViewZ KLog::STDOUT;
constexpr KStringViewZ KLog::STDERR;
#ifdef DEKAF2_HAS_SYSLOG
constexpr KStringViewZ KLog::SYSLOG;
#endif
constexpr KStringViewZ KLog::BAR;
constexpr KStringViewZ KLog::DASH;

#endif

} // of namespace dekaf2
