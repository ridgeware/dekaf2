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

#pragma once

/// @file klog.h
/// Logging framework

#include <memory>
#include <exception>
#include <mutex>
#include <vector>
#include "kstring.h"
#include "kformat.h"
#include "bits/kcppcompat.h"

#ifndef DEKAF2_IS_WINDOWS
	#define DEKAF2_HAS_SYSLOG
#endif

namespace dekaf2
{

class KLogWriter;
class KLogSerializer;

#ifdef DEKAF2_KLOG_WITH_TCP
class KHTTPHeaders;
#endif

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Primary logging facility for dekaf2.
/// The logging logics in dekaf historically follows the idea that the log
/// level will be set at program start, probably by a command line argument.
///
/// It can also be changed during runtime of the application by
/// changing a "flagfile" in the file system. This setting change will be
/// picked up by the running application within a second.
///
/// Log messages that have a "level" equal or lower than the value of the
/// set "debug level" will be output.
///
/// In addition to this, the present logging implementation provides automated
/// stack traces for caught exceptions and for logged messages lower than a
/// configurable level.
///
/// This implementation has taken great care that only one instruction is
/// executed at logging call places if the level is too low for the message
/// to be logged - the comparison of the log level with the importance of
/// the message.
///
/// KLog is implemented as a singleton. It is guaranteed to be
/// instantiated whenever it is called, also in the early initialization
/// phase of the program for e.g. static types.
class KLog
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	//---------------------------------------------------------------------------
	static KLog& getInstance()
	//---------------------------------------------------------------------------
	{
		static KLog myKLog;
		return myKLog;
	}

	KLog(const KLog&) = delete;
	KLog(KLog&&) = delete;
	KLog& operator=(const KLog&) = delete;
	KLog& operator=(KLog&&) = delete;
	~KLog();

	static constexpr KStringViewZ STDOUT = "stdout";
	static constexpr KStringViewZ STDERR = "stderr";
#ifdef DEKAF2_HAS_SYSLOG
	static constexpr KStringViewZ SYSLOG = "syslog";
#endif
	static constexpr KStringViewZ BAR    = "--------------------------------------------------------------------------------";
	static constexpr KStringViewZ DASH   = "- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - ";

	enum LOGMODE { CLI, SERVER };

	//---------------------------------------------------------------------------
	/// Sets the operation mode of the KLOG to either CLI or SERVER mode. Default
	/// is CLI, but e.g. the REST server switches this to SERVER. This affects
	/// log location and error output.
	void SetMode(LOGMODE logmode);
	//---------------------------------------------------------------------------

	//---------------------------------------------------------------------------
	/// Returns the current KLOG operation mode
	LOGMODE GetMode() const
	//---------------------------------------------------------------------------
	{
		return m_Logmode;
	}

	//---------------------------------------------------------------------------
	/// Gets the current log level. Any log message that has a higher level than
	/// this value is not output.
	static inline int GetLevel()
	//---------------------------------------------------------------------------
	{
		return s_iLogLevel;
	}

	//---------------------------------------------------------------------------
	/// Sets a new log level.
	void SetLevel(int iLevel);
	//---------------------------------------------------------------------------

	//---------------------------------------------------------------------------
	/// For long running threads, sync the per-thread log level to the global
	/// log level (it may have changed through the Dekaf() timing thread)
	void SyncLevel();
	//---------------------------------------------------------------------------

	//---------------------------------------------------------------------------
	/// Get level at which back traces are automatically generated.
	inline int GetBackTraceLevel() const
	//---------------------------------------------------------------------------
	{
		return m_iBackTrace;
	}

	//---------------------------------------------------------------------------
	/// Set level at which back traces are automatically generated.
	inline int SetBackTraceLevel(int iLevel)
	//---------------------------------------------------------------------------
	{
		m_iBackTrace = iLevel;
		return m_iBackTrace;
	}

	//---------------------------------------------------------------------------
	/// Set JSON trace mode:
	/// "OFF,off,FALSE,false,NO,no,0" :: switches off
	/// "CALLER,caller,SHORT,short"   :: switches to caller frame only
	///                               :: all else switches full trace on
	void SetJSONTrace(KStringView sJSONTrace);
	//---------------------------------------------------------------------------

	//---------------------------------------------------------------------------
	/// Returns JSON trace mode, one of off,short,full
	KStringView GetJSONTrace() const;
	//---------------------------------------------------------------------------

	//---------------------------------------------------------------------------
	/// Set application name for logging.
	void SetName(KStringView sName);
	//---------------------------------------------------------------------------

	//---------------------------------------------------------------------------
	/// Get application name as provided by user.
	const KString& GetName() const
	//---------------------------------------------------------------------------
	{
		return m_sPathName;
	}

	//---------------------------------------------------------------------------
	/// Set the output file (or tcp stream) for the log.
	bool SetDebugLog(KStringView sLogfile);
	//---------------------------------------------------------------------------

	enum class Writer
	{
		NONE,
		STDOUT,
		STDERR,
		FILE,
#ifdef DEKAF2_HAS_SYSLOG
		SYSLOG,
#endif
#ifdef DEKAF2_KLOG_WITH_TCP
		TCP,
		HTTP
#endif
	};

	//---------------------------------------------------------------------------
	/// Create a Writer of specific type
	static std::unique_ptr<KLogWriter> CreateWriter(Writer writer, KStringView sLogname = KStringView{});
	//---------------------------------------------------------------------------

	enum class Serializer
	{
		TTY,
#ifdef DEKAF2_HAS_SYSLOG
		SYSLOG,
#endif
#ifdef DEKAF2_KLOG_WITH_TCP
		JSON,
#endif
	};

	//---------------------------------------------------------------------------
	/// Create a Serializer of specific type
	static std::unique_ptr<KLogSerializer> CreateSerializer(Serializer serializer);
	//---------------------------------------------------------------------------

	//---------------------------------------------------------------------------
	/// Set the log writer directly instead of opening one implicitly with SetDebugLog()
	bool SetWriter(std::unique_ptr<KLogWriter> logger);
	//---------------------------------------------------------------------------

	//---------------------------------------------------------------------------
	/// Set the log serializer directly instead of opening one implicitly with SetDebugLog()
	bool SetSerializer(std::unique_ptr<KLogSerializer> serializer);
	//---------------------------------------------------------------------------

	//---------------------------------------------------------------------------
	/// Gets the file name of the output file for the log.
	inline const KString& GetDebugLog() const
	//---------------------------------------------------------------------------
	{
		return m_sLogName;
	}

	//---------------------------------------------------------------------------
	/// Sets the file name of the flag file. The flag file is monitored in
	/// regular intervals, and if changed its content is read and interpreted
	/// as the new log level.
	bool SetDebugFlag(KStringView sFlagfile);
	//---------------------------------------------------------------------------

	//---------------------------------------------------------------------------
	/// Gets the file name of the flag file.
	inline const KString& GetDebugFlag() const
	//---------------------------------------------------------------------------
	{
		return m_sFlagfile;
	}

	//---------------------------------------------------------------------------
	/// Reset configuration to default values
	void SetDefaults();
	//---------------------------------------------------------------------------

	//---------------------------------------------------------------------------
	/// this function is deprecated - use kDebug() instead!
	template<class... Args, typename std::enable_if_t<sizeof...(Args) != 1, int> = 0>
	inline bool debug(int iLevel, Args&&... args)
	//---------------------------------------------------------------------------
	{
		return IntDebug(iLevel, KStringView(), kFormat(std::forward<Args>(args)...));
	}

	//---------------------------------------------------------------------------
	/// this function is deprecated - use kDebug() instead!
	template<class... Args, typename std::enable_if_t<sizeof...(Args) == 1, int> = 0>
	inline bool debug(int iLevel, Args&&... args)
	//---------------------------------------------------------------------------
	{
		return IntDebug(iLevel, KStringView(), std::forward<Args>(args)...);
	}

	//---------------------------------------------------------------------------
	/// this function is deprecated - use kDebug() instead!
	template<class... Args, typename std::enable_if_t<sizeof...(Args) != 1, int> = 0>
	inline bool debug_fun(int iLevel, KStringView sFunction, Args&&... args)
	//---------------------------------------------------------------------------
	{
		return IntDebug(iLevel, sFunction, kFormat(std::forward<Args>(args)...));
	}

	//---------------------------------------------------------------------------
	/// this function is deprecated - use kDebug() instead!
	template<class... Args, typename std::enable_if_t<sizeof...(Args) == 1, int> = 0>
	inline bool debug_fun(int iLevel, KStringView sFunction, Args&&... args)
	//---------------------------------------------------------------------------
	{
		return IntDebug(iLevel, sFunction, std::forward<Args>(args)...);
	}

	//---------------------------------------------------------------------------
	/// print first frame from a file not in sSkipFiles (comma separated basenames)
	void TraceDownCaller(int iSkipStackLines, KStringView sSkipFiles, KStringView sMessage);
	//---------------------------------------------------------------------------

	//---------------------------------------------------------------------------
	/// Logs a warning. Takes any arguments that can be formatted through the
	/// standard formatter of the library. A warning is a debug message with
	/// level -1.
	template<class... Args, typename std::enable_if_t<sizeof...(Args) != 1, int> = 0>
	inline bool warning(Args&&... args)
	//---------------------------------------------------------------------------
	{
		return IntDebug(-1, KStringView(), kFormat(std::forward<Args>(args)...));
	}

	//---------------------------------------------------------------------------
	/// Logs a warning. Takes any arguments that can be formatted through the
	/// standard formatter of the library. A warning is a debug message with
	/// level -1.
	template<class... Args, typename std::enable_if_t<sizeof...(Args) == 1, int> = 0>
	inline bool warning(Args&&... args)
	//---------------------------------------------------------------------------
	{
		return IntDebug(-1, KStringView(), std::forward<Args>(args)...);
	}

	//---------------------------------------------------------------------------
	/// report a known exception - better use kException().
	void Exception(const std::exception& e, KStringView sFunction, KStringView sClass = "")
	//---------------------------------------------------------------------------
	{
		IntException(e.what(), sFunction, sClass);
	}

	//---------------------------------------------------------------------------
	/// report an unknown exception - better use kUnknownException().
	void Exception(KStringView sFunction, KStringView sClass = "")
	//---------------------------------------------------------------------------
	{
		IntException("unknown", sFunction, sClass);
	}

	//---------------------------------------------------------------------------
	/// Registered with Dekaf::AddToOneSecTimer() at construction, gets
	/// called every second and reconfigures debug level, output, and format
	/// if changed
	void CheckDebugFlag (bool bForce=false);
	//---------------------------------------------------------------------------

	//---------------------------------------------------------------------------
	/// set whether or not to show stack as klog warnings when JSON parse fails
	/// - this is a thread local setting
	bool ShowStackOnJsonError (bool bNewValue)
	//---------------------------------------------------------------------------
	{
		bool bOldValue = s_bShouldShowStackOnJsonError;
		s_bShouldShowStackOnJsonError = bNewValue;
		return bOldValue;
	}

	//---------------------------------------------------------------------------
	/// set if only the call site should be shown in case of a JSON error or the
	/// full stack - this is a per-process setting
	bool OnlyShowCallerOnJsonError (bool bNewValue)
	//---------------------------------------------------------------------------
	{
		bool bOldValue = m_bGlobalShouldOnlyShowCallerOnJsonError;
		m_bGlobalShouldOnlyShowCallerOnJsonError = bNewValue;
		return bOldValue;
	}

	//---------------------------------------------------------------------------
	/// Log either full trace or call place for JSON
	void JSONTrace(KStringView sFunction);
	//---------------------------------------------------------------------------

	//---------------------------------------------------------------------------
	/// Log messages of this thread of execution with level <= iLevel into the
	/// current klog. This can be used to force more logging for a particular thread
	/// than for other threads. If iLevel is <= 0, the global log level is applied
	/// and this method resets all thread specific logging.
	void LogThisThreadToKLog(int iLevel);
	//---------------------------------------------------------------------------

#ifdef DEKAF2_KLOG_WITH_TCP
	//---------------------------------------------------------------------------
	/// Log messages of this thread of execution with level <= iLevel into a
	/// HTTP Response header.
	void LogThisThreadToResponseHeaders(int iLevel, KHTTPHeaders& Response, KStringView sHeader = "x-klog");
	//---------------------------------------------------------------------------

	//---------------------------------------------------------------------------
	/// Log messages of this thread of execution with level <= iLevel into a
	/// JSON array
	void LogThisThreadToJSON(int iLevel, void* pjson);
	//---------------------------------------------------------------------------
#endif

	//---------------------------------------------------------------------------
	/// When per-thread logging is active, only log messages that contain the sGrepExpression,
	/// either in egrep (regular expression) modus or plain string search
	void LogThisThreadWithGrepExpression(bool bEGrep, KString sGrepExpression);
	//---------------------------------------------------------------------------

	//---------------------------------------------------------------------------
	/// Only log messages that contain or do not contain (bInverted) the sGrepExpression
	/// either in egrep (regular expression) modus or plain string search
	void LogWithGrepExpression(bool bEGrep, bool bInverted, KString sGrepExpression);
	//---------------------------------------------------------------------------

	static KStringView s_sJSONSkipFiles;
	static thread_local int s_iThreadLogLevel;

//----------
private:
//----------

	// private ctor
	KLog();

	bool IntDebug (int iLevel, KStringView sFunction, KStringView sMessage);
	void IntException (KStringView sWhat, KStringView sFunction, KStringView sClass);
	bool IntOpenLog ();

	static int s_iLogLevel;
	static thread_local bool s_bShouldShowStackOnJsonError;
	static thread_local bool s_bPrintTimeStampOnClose;
	static thread_local bool s_bPerThreadEGrep;
	static thread_local std::unique_ptr<KLogSerializer> s_PerThreadSerializer;
	static thread_local std::unique_ptr<KLogWriter> s_PerThreadWriter;
	static thread_local KString s_sPerThreadGrepExpression;

	KString m_sPathName;
	KString m_sShortName;
	KString m_sLogName;
	KString m_sFlagfile;
	KString m_sDefaultLog;
	KString m_sDefaultFlag;
	KString m_sLogDir;
	KString m_sGrepExpression;

	std::recursive_mutex m_LogMutex;

	int m_iBackTrace { -2 };
	time_t m_sTimestampFlagfile { 0 };
	std::unique_ptr<KLogSerializer> m_Serializer;
	std::unique_ptr<KLogWriter> m_Logger;
	// the m_Traces vector is protected by the s_LogMutex
	std::vector<KString> m_Traces;

	bool m_bBackTraceAlreadyCalled { false };
#ifdef NDEBUG
	// per default JSON stack traces are switched off in release mode
	// but they can be switched on by env var "DEKAFJSONTRACE" or through the
	// settings in the flag file
	bool m_bGlobalShouldShowStackOnJsonError { false };
#else
	bool m_bGlobalShouldShowStackOnJsonError { true };
#endif
	bool m_bGlobalShouldOnlyShowCallerOnJsonError { false };
	bool m_bHadConfigFromFlagFile { false };
	bool m_bLogIsRegularFile { false };
	bool m_bIsCGI { false }; // we need this bool on top of m_Logmode for the constructor
	bool m_bEGrep { false };
	bool m_bInvertedGrep { false };

	LOGMODE m_Logmode { CLI };

}; // KLog

// there is no way to convince gcc to inline a variadic template function
// (and as "inline" is not imperative it may happen on other compilers as well)
// - so just fall back to using a macro. Remind that this creates problems
// with namespaces, as preprocessor macros are namespace agnostic.
//
// The problem is introduced for kDebug() (and KLog().debug(), which is deprecated)
// as we do not want to evaluate all input parameters before calling the function.
// Therefore we resort to a macro here, and _only_ here (remember, macros are evil)
//
// From inside the macro we call .debug_fun() and not .warning() as we evaluate only the
// global static KLog::s_kLogLevel, which is not guaranteed to be initialized before KLog()
// has been called for the first time. .debug_fun() then re-evaluates the level and
// outputs appropriately. The only bad thing that can happen is that we miss
// a debug output in the initialization phase of the program.

#ifdef kDebug
#undef kDebug
#endif
//---------------------------------------------------------------------------
/// log a debug message, automatically provide function name.
#define kDebug(iLevel, ...) \
{ \
	if (DEKAF2_UNLIKELY(iLevel <= dekaf2::KLog::s_iThreadLogLevel)) \
	{ \
		dekaf2::KLog::getInstance().debug_fun(iLevel, DEKAF2_FUNCTION_NAME, __VA_ARGS__); \
	} \
}
//---------------------------------------------------------------------------

#ifdef kDebugLog
#undef kDebugLog
#endif
//---------------------------------------------------------------------------
/// log a debug message, do NOT automatically provide function name.
#define kDebugLog(iLevel, ...) \
{ \
	if (DEKAF2_UNLIKELY(iLevel <= dekaf2::KLog::s_iThreadLogLevel)) \
	{ \
		dekaf2::KLog::getInstance().debug(iLevel, __VA_ARGS__); \
	} \
}
//---------------------------------------------------------------------------

#ifdef kWarning
#undef kWarning
#endif
//---------------------------------------------------------------------------
/// log a warning message, automatically provide function name.
#define kWarning(...) \
{ \
	dekaf2::KLog::getInstance().debug_fun(-1, DEKAF2_FUNCTION_NAME, __VA_ARGS__); \
}
//---------------------------------------------------------------------------

#ifdef kWarningLog
#undef kWarningLog
#endif
//---------------------------------------------------------------------------
/// log a warning message, do NOT automatically provide function name.
#define kWarningLog(...) \
{ \
	dekaf2::KLog::getInstance().debug(-1, __VA_ARGS__); \
}
//---------------------------------------------------------------------------

#ifdef kException
#undef kException
#endif
//---------------------------------------------------------------------------
/// log an exception, automatically provide function name and generate a
/// stacktrace at level -2
#define kException(except) \
{ \
	dekaf2::KLog::getInstance().Exception(except, DEKAF2_FUNCTION_NAME); \
}
//---------------------------------------------------------------------------

#ifdef kUnknownException
#undef kUnknownException
#endif
//---------------------------------------------------------------------------
/// log an unknown exception, automatically provide function name.
#define kUnknownException() \
{ \
	dekaf2::KLog::getInstance().Exception(DEKAF2_FUNCTION_NAME); \
}
//---------------------------------------------------------------------------

#ifdef kDebugTrace
#undef kDebugTrace
#endif
//---------------------------------------------------------------------------
/// force a stack trace, automatically provide function name.
#define kDebugTrace(...) \
{ \
	dekaf2::KLog::getInstance().debug_fun(-2, DEKAF2_FUNCTION_NAME, __VA_ARGS__); \
}
//---------------------------------------------------------------------------

#ifdef kJSONTrace
#undef kJSONTrace
#endif
//---------------------------------------------------------------------------
/// special stack dump handling just for KJSON (nlohmann)
#define kJSONTrace() \
{ \
	dekaf2::KLog::getInstance().JSONTrace(DEKAF2_FUNCTION_NAME); \
}
//---------------------------------------------------------------------------

#ifdef kTraceDownCaller
#undef kTraceDownCaller
#endif
//---------------------------------------------------------------------------
/// print first frame from a file not in sSkipFiles (comma separated basenames)
#define kTraceDownCaller(iSkipStackLines, sSkipFiles, sMessage) \
{ \
	dekaf2::KLog::getInstance().TraceDownCaller(iSkipStackLines, sSkipFiles, sMessage); \
}
//---------------------------------------------------------------------------

#ifdef kWouldLog
#undef kWouldLog
#endif
//---------------------------------------------------------------------------
/// test if a given log level would create output
#define kWouldLog(iLevel) (DEKAF2_UNLIKELY(iLevel <= dekaf2::KLog::s_iThreadLogLevel))
//---------------------------------------------------------------------------


} // end of namespace dekaf2
