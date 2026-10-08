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

#include <dekaf2/core/logging/klog.h>

//===========================================================================
#ifdef DEKAF2_WITH_KLOG
//===========================================================================

#include <dekaf2/core/logging/bits/klogwriter.h>
#include <dekaf2/core/logging/bits/klogserializer.h>
#include <dekaf2/core/init/dekaf2.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/system/os/kgetruntimestack.h>
#include <dekaf2/core/strings/kstringutils.h>
#include <dekaf2/core/strings/kcaseless.h>
#include <dekaf2/system/os/ksystem.h>
#include <dekaf2/system/filesystem/kfilesystem.h>
#include <dekaf2/http/server/kcgistream.h>
#include <mutex>
#include <iostream>

#ifndef DEKAF2_IS_WINDOWS
	#include <syslog.h>
#endif

#ifdef DEKAF2_KLOG_WITH_TCP
	#include <dekaf2/web/url/kurl.h>
#endif

DEKAF2_NAMESPACE_BEGIN

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
#ifdef DEKAF2_KLOG_WITH_TCP
thread_local bool KLog::s_bPrintTimeStampOnClose { false };
#endif
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

	m_sPathName =  kGetOwnPathname();

	SetName(Dekaf::getInstance().GetProgName());

	SetDefaults();

	CheckDebugFlag();

	Dekaf::getInstance().GetTimer().CallEvery(chrono::seconds(5), [this](KUnixTime)
	{
		this->CheckDebugFlag();

	}, false);

} // ctor

//---------------------------------------------------------------------------
KLog::~KLog()
//---------------------------------------------------------------------------
{
	// we need the dtor in the cpp, as otherwise the compiler would not have
	// access to KLogWriter / KLogSerializer type information..

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

#ifdef DEKAF2_KLOG_WITH_TCP
	if (s_bPrintTimeStampOnClose)
	{
		if (s_PerThreadSerializer && s_PerThreadWriter)
		{
			auto pSerializerInstance = static_cast<KLogHTTPHeaderSerializer*>(s_PerThreadSerializer.get());
			s_PerThreadWriter->Write(0, false, pSerializerInstance->GetTimeStamp("Stop"));
			s_PerThreadWriter->Write(0, true,  KLogHTTPHeaderSerializer::GetFooter());
		}
	}
#endif

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
KLog& KLog::LogThisThreadWithGrepExpression(bool bEGrep, KStringView sGrepExpression)
//---------------------------------------------------------------------------
{
	kDebug(2, "using {}grep expression '{}'", bEGrep ? "e" : "", sGrepExpression);
	s_sPerThreadGrepExpression = sGrepExpression.ToLower();
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
	else if (iLevel > 4)
	{
		iLevel = 4;
	}

	if (s_iLogLevel > 0 || iLevel > 0)
	{
		IntDebug(0, DEKAF2_FUNCTION_NAME, kFormat("setting debug {} to {}", "level", iLevel));
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
KLog& KLog::SetName(KStringView sName)
//---------------------------------------------------------------------------
{
#if DEKAF2_IS_WINDOWS
	sName.remove_suffix(".exe");
#endif

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

	IntDebug(1, DEKAF2_FUNCTION_NAME, kFormat("setting debug {} to {}", "log", sLogfile));

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
KLog& KLog::SetMirror(std::shared_ptr<KLogWriter> Mirror)
//---------------------------------------------------------------------------
{
	std::lock_guard<std::recursive_mutex> Lock(m_LogMutex);
	m_Mirror = std::move(Mirror);
	return *this;

} // SetMirror

//---------------------------------------------------------------------------
KLog& KLog::SetWriter(Writer writer, KStringViewZ sLogname)
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
std::unique_ptr<KLogWriter> KLog::CreateWriter(Writer writer, KStringViewZ sLogname)
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
		case Writer::SYSLOG:
			return std::make_unique<KLogSyslogWriter>();
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
		case Serializer::SYSLOG:
			return std::make_unique<KLogSyslogSerializer>();
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
	if (m_sLogName == SYSLOG)
	{
		SetWriter(CreateWriter(Writer::SYSLOG));
		SetSerializer(CreateSerializer(Serializer::SYSLOG));
	}
	else
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
		KFileStat File(m_sLogName);

		if (!File.Exists())
		{
			// file was removed.. reopen
			SetWriter(CreateWriter(Writer::FILE, m_sLogName));
		}
		// check if file grew too big
		else if (File.Size() > 10 * 1024 * 1024)
		{
			if (GetLevel() > 1)
			{
				kDebug(1, "Logfile now too large, reducing log level from {} to 1", GetLevel());
				SetLevel(1);
			}
		}
	}

	// file format of the debug "flag" file:
	// line #1: "level" where level is numeric (-1 .. 4)
	// following lines are key-value pairs in ini style
	// with the keys
	// log=        :: a pathname or a domain:host or syslog, stderr, stdout
	// trace=      :: a string that, if matched in any debug message, forces a stacktrace
	// tracelevel= :: forces stacktraces for all warnings <= the numeric tracelevel
	// tracejson=  :: "OFF,off,FALSE,false,NO,no,0" :: switches off
	//             :: "CALLER,caller,SHORT,short"   :: switches to caller frame only
	//             ::                               :: all else switches full trace on

	// this format is compatible to the dekaf1 flag file format (which only reads the first line)

	KUnixTime tTouchTime = kGetLastMod(GetDebugFlag());

	if (tTouchTime == KUnixTime(-1))
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
								else if (iLvl > 4)
								{
									iLvl = 4;
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
KLog& KLog::LogWithGrepExpression(bool bEGrep, bool bInverted, KStringView sGrepExpression)
//---------------------------------------------------------------------------
{
	// change string values in multithreading only with a mutex
	std::lock_guard<std::recursive_mutex> Lock(m_LogMutex);
	m_bEGrep          = bEGrep;
	m_bInvertedGrep   = bInverted;
	m_sGrepExpression = sGrepExpression.ToLower();

	return *this;

} // LogWithGrepExpression

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
/// Is this a character that can appear inside a base64url token body?
//---------------------------------------------------------------------------
static inline bool kIsTokenChar(char ch)
//---------------------------------------------------------------------------
{
	return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z')
	    || (ch >= '0' && ch <= '9') || ch == '_' || ch == '-' || ch == '=';
}

//---------------------------------------------------------------------------
/// Finds "eyJ", the start of a JWT, from iPos on. memchr scans for the 'J', which
/// is rare in log text - a substring search would stop at nearly every 'e'.
static KStringView::size_type kFindJWT(KStringView sText, KStringView::size_type iPos = 0)
//---------------------------------------------------------------------------
{
	for (auto iHit = kFind(sText, 'J', iPos + 2); iHit != KStringView::npos; iHit = kFind(sText, 'J', iHit + 1))
	{
		if (sText[iHit - 2] == 'e' && sText[iHit - 1] == 'y')
		{
			return iHit - 2;
		}
	}

	return KStringView::npos;

} // kFindJWT

//---------------------------------------------------------------------------
/// Finds a lowercase ASCII word in any case, from iPos on. memchr scans for the
/// anchor, a letter of the word, in its two cases, and only its hits compare the
/// whole word - with a rare letter as the anchor this is faster than a substring
/// search, while a caseless search compares at every position.
static KStringView::size_type kFindAnyCase(KStringView sText, KStringView sWord, char chAnchor, KStringView::size_type iPos = 0)
//---------------------------------------------------------------------------
{
	auto iAnchor = sWord.find(chAnchor);
	auto iFound  = KStringView::npos;

	for (auto chCase : { chAnchor, KASCII::kToUpper(chAnchor) })
	{
		for (auto iHit = kFind(sText, chCase, iPos + iAnchor); iHit < iFound; iHit = kFind(sText, chCase, iHit + 1))
		{
			auto iStart = iHit - iAnchor;

			if (sText.size() - iStart < sWord.size())
			{
				break;
			}

			if (kCaselessBeginsWithLeft(sText.substr(iStart), sWord))
			{
				iFound = iStart;
				break;
			}
		}
	}

	return iFound;

} // kFindAnyCase

//---------------------------------------------------------------------------
/// Is this a character that ends a value in a log line - white space or a quote?
static inline bool kIsValueEnd(char ch)
//---------------------------------------------------------------------------
{
	return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || ch == '"' || ch == '\'';
}

//---------------------------------------------------------------------------
/// Does sName end with sWord, in any case? sWord is in lowercase. The comparison
/// runs from the back, as the last characters already differ for nearly all names
/// of parameters - this runs for each '=' of a line.
static inline bool kEndsWithAnyCase(KStringView sName, KStringView sWord)
//---------------------------------------------------------------------------
{
	if (sWord.size() > sName.size())
	{
		return false;
	}

	auto iName = sName.size();

	for (auto iWord = sWord.size(); iWord > 0; )
	{
		if (KASCII::kToLower(sName[--iName]) != sWord[--iWord])
		{
			return false;
		}
	}

	return true;

} // kEndsWithAnyCase

//---------------------------------------------------------------------------
/// Is this a character of the name of a parameter - a letter, a digit, '_', '-' or '.'?
/// This runs for each '=' of a line, and the tests are combined with '|' instead
/// of '||', which compiles without branches - the version with branches was
/// measurably slower on lines like "a=1 b=2 c=3".
static inline bool kIsNameChar(char ch)
//---------------------------------------------------------------------------
{
	auto c = static_cast<unsigned char>(ch);

	// (c | 0x20) folds the upper case letters onto the lower case ones
	return (static_cast<unsigned char>((c | 0x20) - 'a') < 26)
	     | (static_cast<unsigned char>(c - '0') < 10)
	     | (c == '_')
	     | (c == '-')
	     | (c == '.');
}

//---------------------------------------------------------------------------
/// Does the text before a '=' end with the name of a parameter that holds a
/// credential? Short names like "pass" or "key" count only in a URL, after a '?'
/// or '&', as "pass=3 fail=0" or "key=..." are ordinary output elsewhere.
///
/// This runs for each '=' of a line, so it compares from the '=' backwards and
/// never reads the whole name: the last character selects the few candidates,
/// and nearly all names of ordinary parameters fail at their last or second to
/// last character.
/// @param sBefore the text up to the '='
static bool kEndsWithSecretName(KStringView sBefore)
//---------------------------------------------------------------------------
{
	// does sBefore end with sWord as a whole name?
	auto IsName = [sBefore](KStringView sWord)
	{
		return kEndsWithAnyCase(sBefore, sWord)
		    && (sBefore.size() == sWord.size() || !kIsNameChar(sBefore[sBefore.size() - sWord.size() - 1]));
	};

	// does sBefore end with sWord as a whole name in a URL?
	auto IsNameInURL = [sBefore](KStringView sWord)
	{
		if (sBefore.size() <= sWord.size() || !kEndsWithAnyCase(sBefore, sWord))
		{
			return false;
		}

		auto chBefore = sBefore[sBefore.size() - sWord.size() - 1];

		return chBefore == '?' || chBefore == '&';
	};

	auto EndsWith = [sBefore](KStringView sSuffix)
	{
		return kEndsWithAnyCase(sBefore, sSuffix);
	};

	if (sBefore.empty())
	{
		return false;
	}

	switch (KASCII::kToLower(sBefore.back()))
	{
		case 'd':
			return IsName("password") || IsName("passwd") || EndsWith("_password") || IsNameInURL("pwd");

		case 't':
			return IsName("secret") || EndsWith("_secret");

		case 'y':
			return IsName("apikey") || IsName("api_key") || IsNameInURL("key");

		case 'n':
			return EndsWith("_token") || IsNameInURL("token");

		case 's':
			return IsNameInURL("pass");

		case 'e':
			return IsNameInURL("code") || IsNameInURL("signature");

		case 'g':
			return IsNameInURL("sig");

		default:
			return false;
	}

} // kEndsWithSecretName

//---------------------------------------------------------------------------
/// Finds the value of a parameter "name=value" whose name says that it holds a
/// credential, from iPos on. memchr scans for the '=', and only its hits compare
/// the name before it. The value runs to white space or a quote, and in a URL also
/// to the next '&' or '#' - in other text these may be part of a password.
/// @return the value, or an empty view if there is none
static KStringView kFindSecretParameter(KStringView sText, KStringView::size_type iPos = 0)
//---------------------------------------------------------------------------
{
	for (auto iHit = kFind(sText, '=', iPos); iHit != KStringView::npos; iHit = kFind(sText, '=', iHit + 1))
	{
		if (!kEndsWithSecretName(sText.substr(0, iHit)))
		{
			continue;
		}

		// only now the start of the name, to see if it is in a URL
		auto iName = iHit;

		while (iName > 0 && kIsNameChar(sText[iName - 1]))
		{
			--iName;
		}

		bool bInURL = iName > 0 && (sText[iName - 1] == '?' || sText[iName - 1] == '&');
		auto iEnd   = iHit + 1;

		while (iEnd < sText.size() && !kIsValueEnd(sText[iEnd]) && !(bInURL && (sText[iEnd] == '&' || sText[iEnd] == '#')))
		{
			++iEnd;
		}

		auto sValue = sText.substr(iHit + 1, iEnd - iHit - 1);

		// never redact a redaction - "access_token=[redacted]" from the JWT pass
		if (!sValue.empty() && !sValue.starts_with("[redacted]"))
		{
			return sValue;
		}
	}

	return {};

} // kFindSecretParameter

//---------------------------------------------------------------------------
/// Finds the password in the userinfo of a URL, "user:password@host", from iPos
/// on, also without a scheme. memchr scans for the '@', and only its hits read the
/// text before it: back to white space, a quote, a bracket or a '/'. A '/' must be
/// the one of a "//" after the scheme, else the '@' is part of a path. A ':' in
/// that run separates the password - an e-mail address has none, and "mailto:"
/// and "sip:" are schemes, not users.
/// @return the password, or an empty view if there is none
static KStringView kFindUserinfoPassword(KStringView sText, KStringView::size_type iPos = 0)
//---------------------------------------------------------------------------
{
	static constexpr KStringView Schemes[] = { "mailto", "sip", "sips", "xmpp" };

	for (auto iAt = kFind(sText, '@', iPos); iAt != KStringView::npos; iAt = kFind(sText, '@', iAt + 1))
	{
		auto iRun   = iAt;
		auto iColon = KStringView::npos;

		while (iRun > 0)
		{
			auto ch = sText[iRun - 1];

			if (kIsValueEnd(ch) || ch == '/' || ch == '<' || ch == '(' || ch == '[')
			{
				break;
			}

			if (ch == ':')
			{
				// the leftmost colon remains
				iColon = iRun - 1;
			}

			--iRun;
		}

		// an e-mail address has no colon
		if (iColon == KStringView::npos)
		{
			continue;
		}

		bool bAfterScheme = iRun > 1 && sText[iRun - 1] == '/' && sText[iRun - 2] == '/';

		if (iRun > 0 && sText[iRun - 1] == '/' && !bAfterScheme)
		{
			continue;
		}

		auto sUser     = sText.substr(iRun, iColon - iRun);
		auto sPassword = sText.substr(iColon + 1, iAt - iColon - 1);

		if (sPassword.empty() || sPassword.starts_with("[redacted]"))
		{
			continue;
		}

		if (!bAfterScheme)
		{
			bool bIsScheme { false };

			for (auto sScheme : Schemes)
			{
				if (kCaselessEqualLeft(sUser, sScheme))
				{
					bIsScheme = true;
					break;
				}
			}

			if (bIsScheme)
			{
				continue;
			}
		}

		return sPassword;
	}

	return {};

} // kFindUserinfoPassword

//---------------------------------------------------------------------------
/// Redact bearer credentials from a log message before it is written anywhere.
///
/// WHY THIS LIVES IN THE LOG WRITER rather than at each call site: in one sweep
/// of a single application SIX separate places were found
/// logging a live credential -- a full IDP access token, a full JWT on every
/// authenticated call, an OAuth token response, a Slack token into a WARNING log,
/// and, worst, two sites that logged the CONFIGURED token on a failed auth, so a
/// deliberately wrong guess made the server write the right answer into the log.
/// Those were all fixed, but the seventh will be written next month. Per-site
/// discipline has already been tried and has already failed; this is the net.
///
/// It redacts five shapes:
///   eyJ....  a JWT -- three base64url runs separated by dots. The "eyJ" prefix
///            is '{"' base64url-encoded, so every JWT begins with it.
///   Bearer <tok>          the value only, the scheme is kept
///   Authorization: <val>  the value only, the header name is kept
///   password=<val>        the value of a parameter with a name of a credential,
///                         the name is kept (see kEndsWithSecretName())
///   user:<pass>@host      the password in the userinfo of a URL, with or
///                         without a scheme
/// Header names and authentication schemes are case insensitive in HTTP, so
/// "Bearer" and "Authorization" are recognised in any case, and so are the
/// names of parameters.
///
/// THE GUARD MATTERS MORE THAN THE REDACTION. This runs on every log line at
/// every level, so the common path must be a few memchr scans for rare characters
/// that all miss. Only a line that already looks like it carries a credential
/// pays for the copy.
///
/// It is a safety net and not a guarantee: a token logged under a name this does
/// not recognise, or in a shape that is not a JWT, still gets through. Do not
/// treat it as permission to log credentials.
///
/// @param sMessage the message to log
/// @param sBuffer receives the redacted copy of the message
/// @return true if sBuffer holds the message to log, false if sMessage needs no
/// change and sBuffer is left alone - the caller owns both, so neither result
/// can outlive what it points into
//---------------------------------------------------------------------------
static bool kRedactCredentials(KStringView sMessage, KString& sBuffer)
//---------------------------------------------------------------------------
{
	// the fast path: no copy, no allocation, no scan beyond these five - the anchors
	// are the rare characters 'J', 'b', 'z', '=' and '@'
	if (DEKAF2_LIKELY(kFindJWT(sMessage)                            == KStringView::npos
	               && kFindAnyCase(sMessage, "bearer ",        'b') == KStringView::npos
	               && kFindAnyCase(sMessage, "authorization:", 'z') == KStringView::npos
	               && kFindSecretParameter(sMessage).empty()
	               && kFindUserinfoPassword(sMessage).empty()))
	{
		return false;
	}

	sBuffer = sMessage;

	// - - - JWTs - - -
	// A JWT is header.payload.signature, all base64url. We only replace when the
	// shape really is three dotted runs, so a log line that merely mentions "eyJ"
	// in prose survives intact.
	for (KString::size_type iPos = 0; (iPos = kFindJWT(sBuffer, iPos)) != KString::npos; )
	{
		// a JWT starts at the beginning of a word - "keyJar.settings.production"
		// holds "eyJ" and dots as well, but is not one. A '=' separates, as in
		// "id_token=eyJ...": it is base64url padding only at the end of a run
		if (iPos > 0)
		{
			auto chBefore = sBuffer[iPos - 1];

			if (chBefore != '=' && (kIsTokenChar(chBefore) || chBefore == '.'))
			{
				iPos += 3;
				continue;
			}
		}

		auto iRun = iPos;
		int  iDots { 0 };

		while (iRun < sBuffer.size() && (kIsTokenChar(sBuffer[iRun]) || sBuffer[iRun] == '.'))
		{
			if (sBuffer[iRun] == '.')
			{
				++iDots;
			}
			++iRun;
		}

		if (iDots >= 2 && (iRun - iPos) > 20)
		{
			sBuffer.replace(iPos, iRun - iPos, "[redacted]");
			iPos += 10;  // length of the replacement, so we do not rescan it
		}
		else
		{
			iPos += 3;
		}
	}

	// - - - "Authorization: [scheme] <value>" and a bare "Bearer <token>" - - -
	// The header name AND the scheme are kept: "which auth did it try?" is a real
	// debugging question and answering it costs nothing. All words match in any
	// case, and white space may follow the colon of the header, or none at all.
	//
	// ORDER AND SCHEME-SKIPPING BOTH MATTER, and a test caught this. Handling
	// "Bearer " first turned "Authorization: Bearer sk-live-..." into
	// "Authorization: Bearer [redacted]", and then the "Authorization: " pass took
	// the word *Bearer* as its value and ate it too. So Authorization goes first and
	// steps over a recognised scheme word before redacting what follows; the later
	// Bearer pass then finds an already-redacted value and leaves it alone.
	struct Prefix
	{
		KStringView sWord;    // in lowercase
		char        chAnchor; // a rare letter of sWord
	};

	static constexpr Prefix      Prefixes[] = { { "authorization:", 'z' }, { "bearer ", 'b' } };
	static constexpr KStringView Schemes[]  = { "bearer ", "basic ", "digest ", "token " };

	for (const auto& Prefix : Prefixes)
	{
		for (KString::size_type iPos = 0; (iPos = kFindAnyCase(sBuffer, Prefix.sWord, Prefix.chAnchor, iPos)) != KString::npos; )
		{
			auto iValue = iPos + Prefix.sWord.size();

			while (iValue < sBuffer.size() && (sBuffer[iValue] == ' ' || sBuffer[iValue] == '\t'))
			{
				++iValue;
			}

			// step over "Bearer" / "Basic" / ... so the scheme survives the redaction
			for (auto sScheme : Schemes)
			{
				if (kCaselessBeginsWithLeft(KStringView(sBuffer).substr(iValue), sScheme))
				{
					iValue += sScheme.size();
					break;
				}
			}

			// never redact a redaction -- the passes overlap by design
			if (sBuffer.compare(iValue, 10, "[redacted]", 10) == 0)
			{
				iPos = iValue + 10;
				continue;
			}

			auto iEnd = iValue;

			// the value runs to whitespace, a quote, or the end of the line
			while (iEnd < sBuffer.size()
			    && sBuffer[iEnd] != ' '  && sBuffer[iEnd] != '\t'
			    && sBuffer[iEnd] != '\n' && sBuffer[iEnd] != '\r'
			    && sBuffer[iEnd] != '"'  && sBuffer[iEnd] != '\'')
			{
				++iEnd;
			}

			if (iEnd > iValue)
			{
				sBuffer.replace(iValue, iEnd - iValue, "[redacted]");
				iPos = iValue + 10;  // length of "[redacted]"
			}
			else
			{
				iPos = iValue;
			}
		}
	}

	// - - - "password=<value>" and "user:<password>@host" - - -
	// each finder returns the secret as a view into sBuffer, from iPos on
	auto RedactAll = [&sBuffer](KStringView (*Find)(KStringView, KStringView::size_type))
	{
		for (KString::size_type iPos = 0;;)
		{
			auto sSecret = Find(sBuffer, iPos);

			if (sSecret.empty())
			{
				break;
			}

			auto iStart = static_cast<KString::size_type>(sSecret.data() - sBuffer.data());
			sBuffer.replace(iStart, sSecret.size(), "[redacted]");
			iPos = iStart + 10;  // length of "[redacted]"
		}
	};

	RedactAll(kFindSecretParameter);
	RedactAll(kFindUserinfoPassword);

	return true;

} // kRedactCredentials

bool KLog::IntDebug(int iLevel, KStringView sFunction, KStringView sMessage)
//---------------------------------------------------------------------------
{
	// Moving this check to the first place helps to avoid
	// static deinitialization races with static instances of classes that will
	// output to KLog in their destructor.
	// It also allows to call functions with potential KLog output in the
	// constructor of KLog, because the Logger and Serializer is only set at
	// the end of construction. Until then, logging is disabled.
	if (!Available())
	{
		return false;
	}

	// another guard against init / deinit races, particularly for destructors
	// that want to log in destruction of Dekaf (e.g. KTimer)
	if (!Dekaf::IsStarted() || Dekaf::IsShutDown())
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

	// Strip bearer credentials before ANY serializer sees the message. Done here,
	// after the level and recursion guards, so a line that will not be logged
	// never pays for it -- and done once, so the main logger, the mirror and the
	// per-thread log all get the redacted text rather than three chances to differ.
	KString sRedactBuffer;

	if (kRedactCredentials(sMessage, sRedactBuffer))
	{
		sMessage = sRedactBuffer;
	}

	// We need a lock if we run in multithreading, as the serializers
	// have data members. We use a recursive mutex because we want to
	// protect multiple entry points that eventually call this function.
	std::lock_guard<std::recursive_mutex> Lock(m_LogMutex);

	if (DEKAF2_LIKELY(iLevel <= s_iLogLevel))
	{
		// this is the regular logging

		if (m_Serializer)
		{
			m_Serializer->Set(iLevel, m_sShortName, m_sPathName, sFunction, sMessage);
		}

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
			auto sSerialized = m_Serializer->Get(GetUSecMode());

			m_Logger->Write(iLevel, m_Serializer->IsMultiline(), sSerialized);

			if (DEKAF2_UNLIKELY(m_Mirror != nullptr))
			{
				m_Mirror->Write(iLevel, m_Serializer->IsMultiline(), sSerialized);
			}
		}
	}

	if (DEKAF2_UNLIKELY(iLevel <= s_iThreadLogLevel &&
						s_PerThreadSerializer &&
						s_PerThreadWriter))
	{
		// this is the individual per-thread-logging
		s_PerThreadSerializer->Set(iLevel, m_sShortName, m_sPathName, sFunction, sMessage);

		if (s_PerThreadSerializer->Matches(s_bPerThreadEGrep, false, s_sPerThreadGrepExpression))
		{
			s_PerThreadWriter->Write(iLevel, s_PerThreadSerializer->IsMultiline(), s_PerThreadSerializer->Get());
		}
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

		auto sSerialized = m_Serializer->Get();

		m_Logger->Write(-2, m_Serializer->IsMultiline(), sSerialized);

		if (DEKAF2_UNLIKELY(m_Mirror != nullptr))
		{
			m_Mirror->Write(-2, m_Serializer->IsMultiline(), sSerialized);
		}
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

DEKAF2_NAMESPACE_END

//===========================================================================
#endif // of ifdef DEKAF2_WITH_KLOG
//===========================================================================

#ifdef DEKAF2_REPEAT_CONSTEXPR_VARIABLE
DEKAF2_NAMESPACE_BEGIN
constexpr KStringViewZ KLog::STDOUT;
constexpr KStringViewZ KLog::STDERR;
constexpr KStringViewZ KLog::SYSLOG;
DEKAF2_NAMESPACE_END
#endif
