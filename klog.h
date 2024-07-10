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

#include "kdefinitions.h"
#include "kstringview.h"

#ifdef DEKAF2_WITH_KLOG
	#include "kstring.h"
	#include "kformat.h"
	#include "ktime.h"
	#include <memory>
	#include <exception>
	#include <mutex>
	#include <vector>

	#ifndef DEKAF2_IS_WINDOWS
		#define DEKAF2_HAS_SYSLOG
	#endif
#else
	#include <iostream>
#endif

DEKAF2_NAMESPACE_BEGIN

#ifdef DEKAF2_WITH_KLOG
class KLogWriter;
class KLogSerializer;
#endif

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
class DEKAF2_PUBLIC KLog
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
private:
//----------

	// private ctor
	KLog()
#ifdef DEKAF2_WITH_KLOG
	;
#else
	{}
#endif

//----------
public:
//----------

	using self = KLog;

	// generally useful constants that Joe uses all the time (please leave these alone):
	static constexpr KStringViewZ BAR  {"----------------------------------------------------------------------------------------------------------------------------------------------------------------"};
	static constexpr KStringViewZ DASH {" - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -"};

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
	~KLog()
#ifdef DEKAF2_WITH_KLOG
	;
#else
	{}
#endif

	static constexpr KStringViewZ STDOUT = "stdout";
	static constexpr KStringViewZ STDERR = "stderr";
#ifdef DEKAF2_HAS_SYSLOG
	static constexpr KStringViewZ SYSLOG = "syslog";
#endif

	enum LOGMODE { CLI, SERVER };

	//---------------------------------------------------------------------------
	/// If the logging shall not leave the initial CLI mode and switch to SERVER
	/// mode on upstart of e.g. the REST server, call with true. This is useful
	/// if a command line option like -dd was given, with redirection to stderr
	/// or stdout.
	self& KeepCLIMode(bool bYesNo)
	//---------------------------------------------------------------------------
	{
#ifdef DEKAF2_WITH_KLOG
		m_bKeepCLIMode = bYesNo;
#endif
		return *this;
	}

	//---------------------------------------------------------------------------
	/// Sets the operation mode of the KLOG to either CLI or SERVER mode. Default
	/// is CLI, but e.g. the REST server switches this to SERVER. This affects
	/// log location and error output.
	self& SetMode(LOGMODE logmode)
	//---------------------------------------------------------------------------
#ifdef DEKAF2_WITH_KLOG
	;
#else
	{ return *this; }
#endif

	//---------------------------------------------------------------------------
	/// Returns the current KLOG operation mode
	DEKAF2_NODISCARD
	LOGMODE GetMode() const
	//---------------------------------------------------------------------------
	{
#ifdef DEKAF2_WITH_KLOG
		return m_Logmode;
#else
		return CLI;
#endif
	}

	//---------------------------------------------------------------------------
	/// Gets the current log level. Any log message that has a higher level than
	/// this value is not output.
	DEKAF2_NODISCARD
	static inline int GetLevel()
	//---------------------------------------------------------------------------
	{
#ifdef DEKAF2_WITH_KLOG
		return s_iLogLevel;
#else
		return -1;
#endif
	}

	//---------------------------------------------------------------------------
	/// Sets a new log level.
	self& SetLevel(int iLevel)
	//---------------------------------------------------------------------------
#ifdef DEKAF2_WITH_KLOG
	;
#else
	{ return *this; }
#endif

	//---------------------------------------------------------------------------
	/// For long running threads, sync the per-thread log level to the global
	/// log level (it may have changed through the Dekaf() timing thread)
	static void SyncLevel()
	//---------------------------------------------------------------------------
	{
#ifdef DEKAF2_WITH_KLOG
		s_iThreadLogLevel = s_iLogLevel;
#endif
	}

	//---------------------------------------------------------------------------
	/// display microsecond timestamps, and remove fields not needed for performance debugging like
	/// year/month/day
	self& SetUSecMode(bool bYesNo)
	//---------------------------------------------------------------------------
	{
#ifdef DEKAF2_WITH_KLOG
		m_bIsUSecMode = bYesNo;
#endif
		return *this;
	}

	//---------------------------------------------------------------------------
	/// get state of microsecond mode
	DEKAF2_NODISCARD
	bool GetUSecMode()
	//---------------------------------------------------------------------------
	{
#ifdef DEKAF2_WITH_KLOG
		return m_bIsUSecMode;
#else
		return false;
#endif
	}

	//---------------------------------------------------------------------------
	/// Get level at which back traces are automatically generated.
	DEKAF2_NODISCARD
	inline int GetBackTraceLevel() const
	//---------------------------------------------------------------------------
	{
#ifdef DEKAF2_WITH_KLOG
		return m_iBackTrace;
#else
		return -2;
#endif
	}

	//---------------------------------------------------------------------------
	/// Set level at which back traces are automatically generated.
	inline int SetBackTraceLevel(int iLevel)
	//---------------------------------------------------------------------------
	{
#ifdef DEKAF2_WITH_KLOG
		m_iBackTrace = iLevel;
#endif
		return GetBackTraceLevel();
	}

	//---------------------------------------------------------------------------
	/// Set JSON trace mode:
	/// "OFF,off,FALSE,false,NO,no,0" :: switches off
	/// "CALLER,caller,SHORT,short"   :: switches to caller frame only
	///                               :: all else switches full trace on
	self& SetJSONTrace(KStringView sJSONTrace)
	//---------------------------------------------------------------------------
#ifdef DEKAF2_WITH_KLOG
	;
#else
	{ return *this; }
#endif

	//---------------------------------------------------------------------------
	/// Returns JSON trace mode, one of off,short,full
	DEKAF2_NODISCARD
	KStringView GetJSONTrace() const
	//---------------------------------------------------------------------------
#ifdef DEKAF2_WITH_KLOG
	;
#else
	{ return "off"; }
#endif

	//---------------------------------------------------------------------------
	/// Set application name for logging (will be limited to 5 characters)
	self& SetName(KStringView sName)
	//---------------------------------------------------------------------------
#ifdef DEKAF2_WITH_KLOG
	;
#else
	{ return *this; }
#endif

	//---------------------------------------------------------------------------
	/// Get the full application path name as determined by the OS (not related to SetName() )
	/// deprecated, use kGetOwnPathname() in ksystem.h
	DEKAF2_NODISCARD
	KStringViewZ GetName() const
	//---------------------------------------------------------------------------
	{
#ifdef DEKAF2_WITH_KLOG
		return m_sPathName;
#else
		return KStringViewZ{};
#endif
	}

	//---------------------------------------------------------------------------
	/// Set the output file (or tcp stream) for the log.
	bool SetDebugLog(KStringView sLogfile)
	//---------------------------------------------------------------------------
#ifdef DEKAF2_WITH_KLOG
	;
#else
	{ return true; }
#endif

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
	/// Gets the file name of the output file for the log.
	DEKAF2_NODISCARD
	inline KStringViewZ GetDebugLog() const
	//---------------------------------------------------------------------------
	{
#ifdef DEKAF2_WITH_KLOG
		return m_sLogName;
#else
		return KStringViewZ{};
#endif
	}

	//---------------------------------------------------------------------------
	/// Sets the file name of the flag file. The flag file is monitored in
	/// regular intervals, and if changed its content is read and interpreted
	/// as the new log level.
	bool SetDebugFlag(KStringView sFlagfile)
	//---------------------------------------------------------------------------
#ifdef DEKAF2_WITH_KLOG
	;
#else
	{ return true; }
#endif

	//---------------------------------------------------------------------------
	/// Gets the file name of the flag file.
	DEKAF2_NODISCARD
	inline KStringViewZ GetDebugFlag() const
	//---------------------------------------------------------------------------
	{
#ifdef DEKAF2_WITH_KLOG
		return m_sFlagfile;
#else
		return KStringViewZ{};
#endif
	}

	//---------------------------------------------------------------------------
	/// Reset configuration to default values
	self& SetDefaults()
	//---------------------------------------------------------------------------
#ifdef DEKAF2_WITH_KLOG
	;
#else
	{ return *this; }
#endif

	//---------------------------------------------------------------------------
	/// Returns true while KLog is available - indication that the executable is neiter in init or exit state
	DEKAF2_NODISCARD
	bool Available() const
	//---------------------------------------------------------------------------
	{
#ifdef DEKAF2_WITH_KLOG
		return (m_Logger && m_Serializer);
#else
		return true;
#endif
	}

#ifdef DEKAF2_WITH_KLOG
	//---------------------------------------------------------------------------
	/// this function is deprecated - use kDebug() instead!
	template<class... Args>
	inline bool debug(int iLevel, Args&&... args)
	//---------------------------------------------------------------------------
	{
		return IntDebug(iLevel, KStringView(), kFormat(std::forward<Args>(args)...));
	}

	//---------------------------------------------------------------------------
	/// this function is deprecated - use kDebug() instead!
	template<class... Args>
	inline bool debug_fun(int iLevel, KStringView sFunction, Args&&... args)
	//---------------------------------------------------------------------------
	{
		return IntDebug(iLevel, sFunction, kFormat(std::forward<Args>(args)...));
	}

	//---------------------------------------------------------------------------
	/// print first frame from a file not in sSkipFiles (comma separated basenames)
	void TraceDownCaller(int iSkipStackLines, KStringView sSkipFiles, KStringView sMessage);
	//---------------------------------------------------------------------------

	//---------------------------------------------------------------------------
	/// Logs a warning. Takes any arguments that can be formatted through the
	/// standard formatter of the library. A warning is a debug message with
	/// level -1.
	template<class... Args, typename std::enable_if<sizeof...(Args) != 1, int>::type = 0>
	inline bool warning(Args&&... args)
	//---------------------------------------------------------------------------
	{
		return IntDebug(-1, KStringView(), kFormat(std::forward<Args>(args)...));
	}

	//---------------------------------------------------------------------------
	/// Logs a warning. Takes any arguments that can be formatted through the
	/// standard formatter of the library. A warning is a debug message with
	/// level -1.
	template<class... Args, typename std::enable_if<sizeof...(Args) == 1, int>::type = 0>
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
#endif

	//---------------------------------------------------------------------------
	/// Registered with Dekaf::AddToOneSecTimer() at construction, gets
	/// called every second and reconfigures debug level, output, and format
	/// if changed
	void CheckDebugFlag (bool bForce=false)
	//---------------------------------------------------------------------------
#ifdef DEKAF2_WITH_KLOG
	;
#else
	{ }
#endif

	//---------------------------------------------------------------------------
	/// set whether or not to show stack as klog warnings when JSON parse fails
	/// - this is a thread local setting
	bool ShowStackOnJsonError (bool bNewValue)
	//---------------------------------------------------------------------------
	{
#ifdef DEKAF2_WITH_KLOG
		bool bOldValue = s_bShouldShowStackOnJsonError;
		s_bShouldShowStackOnJsonError = bNewValue;
		return bOldValue;
#else
		return false;
#endif
	}

	//---------------------------------------------------------------------------
	/// set if only the call site should be shown in case of a JSON error or the
	/// full stack - this is a per-process setting
	bool OnlyShowCallerOnJsonError (bool bNewValue)
	//---------------------------------------------------------------------------
	{
#ifdef DEKAF2_WITH_KLOG
		bool bOldValue = m_bGlobalShouldOnlyShowCallerOnJsonError;
		m_bGlobalShouldOnlyShowCallerOnJsonError = bNewValue;
		return bOldValue;
#else
		return false;
#endif
	}

	//---------------------------------------------------------------------------
	/// Log either full trace or call place for JSON
	void JSONTrace(KStringView sFunction)
	//---------------------------------------------------------------------------
#ifdef DEKAF2_WITH_KLOG
	;
#else
	{ }
#endif

	//---------------------------------------------------------------------------
	/// Log messages of this thread of execution with level <= iLevel into the
	/// current klog. This can be used to force more logging for a particular thread
	/// than for other threads. If iLevel is <= 0, the global log level is applied
	/// and this method resets all thread specific logging.
	self& LogThisThreadToKLog(int iLevel)
	//---------------------------------------------------------------------------
#ifdef DEKAF2_WITH_KLOG
	;
#else
	{ return *this; }
#endif

#ifdef DEKAF2_KLOG_WITH_TCP
	//---------------------------------------------------------------------------
	/// Log messages of this thread of execution with level <= iLevel into a
	/// HTTP Response header.
	self& LogThisThreadToResponseHeaders(int iLevel, KHTTPHeaders& Response, KStringView sHeader = "x-klog")
	//---------------------------------------------------------------------------
#ifdef DEKAF2_WITH_KLOG
	;
#else
	{ return *this; }
#endif

	//---------------------------------------------------------------------------
	/// Log messages of this thread of execution with level <= iLevel into a
	/// JSON array
	self& LogThisThreadToJSON(int iLevel, void* pjson)
	//---------------------------------------------------------------------------
#ifdef DEKAF2_WITH_KLOG
	;
#else
	{ return *this; }
#endif

#endif

	//---------------------------------------------------------------------------
	/// When per-thread logging is active, only log messages that contain the sGrepExpression,
	/// either in egrep (regular expression) modus or plain string search. The comparison is
	/// case insensitive.
	self& LogThisThreadWithGrepExpression(bool bEGrep, KStringView sGrepExpression)
	//---------------------------------------------------------------------------
#ifdef DEKAF2_WITH_KLOG
	;
#else
	{ return *this; }
#endif

	//---------------------------------------------------------------------------
	/// Only log messages that contain or do not contain (bInverted) the sGrepExpression
	/// either in egrep (regular expression) modus or plain string search. The comparison is
	/// case insensitive.
	self& LogWithGrepExpression(bool bEGrep, bool bInverted, KStringView sGrepExpression)
	//---------------------------------------------------------------------------
#ifdef DEKAF2_WITH_KLOG
	;
#else
	{ return *this; }
#endif

#ifdef DEKAF2_WITH_KLOG
	static KStringView s_sJSONSkipFiles;
	static thread_local int s_iThreadLogLevel;
#endif

//----------
private:
//----------

#ifdef DEKAF2_WITH_KLOG

	bool IntDebug (int iLevel, KStringView sFunction, KStringView sMessage);
	void IntException (KStringView sWhat, KStringView sFunction, KStringView sClass);

	//---------------------------------------------------------------------------
	/// Create a Writer of specific type
	static std::unique_ptr<KLogWriter> CreateWriter(Writer writer, KStringViewZ sLogname = KStringViewZ{});
	//---------------------------------------------------------------------------

	//---------------------------------------------------------------------------
	/// Create a Serializer of specific type
	static std::unique_ptr<KLogSerializer> CreateSerializer(Serializer serializer);
	//---------------------------------------------------------------------------

	//---------------------------------------------------------------------------
	/// Set the log writer directly instead of opening one implicitly with SetDebugLog()
	self& SetWriter(std::unique_ptr<KLogWriter> logger);
	//---------------------------------------------------------------------------

	//---------------------------------------------------------------------------
	/// Set the log writer directly instead of opening one implicitly with SetDebugLog()
	self& SetWriter(Writer writer, KStringViewZ sLogname = KStringViewZ{});
	//---------------------------------------------------------------------------

	//---------------------------------------------------------------------------
	/// Set the log serializer directly instead of opening one implicitly with SetDebugLog()
	self& SetSerializer(std::unique_ptr<KLogSerializer> serializer);
	//---------------------------------------------------------------------------

	//---------------------------------------------------------------------------
	/// Set the log serializer directly instead of opening one implicitly with SetDebugLog()
	self& SetSerializer(Serializer serializer);
	//---------------------------------------------------------------------------

	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	class DEKAF2_PRIVATE PreventRecursion
	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{

	public:

		PreventRecursion()
		: m_bOldState(s_bCalledFromInsideKlog)
		{
			s_bCalledFromInsideKlog = true;
		}
		~PreventRecursion() { s_bCalledFromInsideKlog = m_bOldState; }

		bool IsRecursive() const { return m_bOldState; }

	private:

		static thread_local bool s_bCalledFromInsideKlog;

		bool m_bOldState;

	}; // PreventRecursion

	bool IntOpenLog ();

	static thread_local std::unique_ptr<KLogSerializer> s_PerThreadSerializer;
	static thread_local std::unique_ptr<KLogWriter> s_PerThreadWriter;
	static thread_local KString s_sPerThreadGrepExpression;
	static int s_iLogLevel;
	static thread_local bool s_bShouldShowStackOnJsonError;
#ifdef DEKAF2_KLOG_WITH_TCP
	static thread_local bool s_bPrintTimeStampOnClose;
#endif
	static thread_local bool s_bPerThreadEGrep;

	KStringViewZ m_sPathName;
	KString m_sShortName;
	KString m_sLogName;
	KString m_sFlagfile;
	KString m_sDefaultLog;
	KString m_sDefaultFlag;
	KString m_sLogDir;
	KString m_sGrepExpression;

	std::recursive_mutex m_LogMutex;

	int m_iBackTrace { -2 };
	KUnixTime m_sTimestampFlagfile;
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
	bool m_bHadConfigFromFlagFile  { false };
	bool m_bLogIsRegularFile       { false };
	bool m_bIsCGI                  { false }; // we need this bool on top of m_Logmode for the constructor
	bool m_bEGrep                  { false };
	bool m_bInvertedGrep           { false };
	bool m_bKeepCLIMode            { false };
	bool m_bIsUSecMode             { false };

	LOGMODE m_Logmode              { CLI   };

#endif // of ifdef DEKAF2_WITH_KLOG

}; // KLog

#ifndef DEKAF2_WITH_KLOG
namespace detail {
template <typename Arg, typename... Args>
void IntWarning_uNuSuAlNaMe (const char* sFunction, Arg&& arg, Args&&... args)
{
	std::clog << ">> ";
	if (*sFunction) std::clog << sFunction << ": ";
	std::clog << std::forward<Arg>(arg);
#ifdef DEKAF2_HAS_CPP_17
	((std::clog << ' ' << std::forward<Args>(args)), ...);
#else
	using expander = int[];
	(void) expander { 0, (void(std::clog << ' ' << std::forward<Args>(args)), 0)... };
#endif
	std::clog << std::endl;
}

inline void IntException_uNuSuAlNaMe (const char* sWhat, const char* sFunction)
{
	std::clog << ">> " << sFunction << ": caught exception: '" << sWhat << '\'' << std::endl;
}

inline void IntException_uNuSuAlNaMe (const std::exception& e, const char* sFunction)
{
	IntException_uNuSuAlNaMe(e.what(), sFunction);
}
} // end of namespace detail
#endif

DEKAF2_NAMESPACE_END

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
#ifdef DEKAF2_WITH_KLOG
//---------------------------------------------------------------------------
/// log a debug message, automatically provide function name.
#define kDebug(iLevel, ...) \
{ \
	if (DEKAF2_UNLIKELY(iLevel <= DEKAF2_PREFIX KLog::s_iThreadLogLevel)) \
	{ \
		DEKAF2_PREFIX KLog::getInstance().debug_fun(iLevel, DEKAF2_FUNCTION_NAME, __VA_ARGS__); \
	} \
}
//---------------------------------------------------------------------------
#else
#define kDebug(iLevel, ...)
#endif

#ifdef kDebugLog
#undef kDebugLog
#endif
#ifdef DEKAF2_WITH_KLOG
//---------------------------------------------------------------------------
/// log a debug message, do NOT automatically provide function name.
#define kDebugLog(iLevel, ...) \
{ \
	if (DEKAF2_UNLIKELY(iLevel <= DEKAF2_PREFIX KLog::s_iThreadLogLevel)) \
	{ \
		DEKAF2_PREFIX KLog::getInstance().debug(iLevel, __VA_ARGS__); \
	} \
}
//---------------------------------------------------------------------------
#else
#define kDebugLog(iLevel, ...)
#endif

#ifdef kWarning
#undef kWarning
#endif
#ifdef DEKAF2_WITH_KLOG
//---------------------------------------------------------------------------
/// log a warning message, automatically provide function name.
#define kWarning(...) \
{ \
	DEKAF2_PREFIX KLog::getInstance().debug_fun(-1, DEKAF2_FUNCTION_NAME, __VA_ARGS__); \
}
//---------------------------------------------------------------------------
#else
#define kWarning(...) DEKAF2_PREFIX detail::IntWarning_uNuSuAlNaMe(DEKAF2_FUNCTION_NAME, __VA_ARGS__)
#endif

#ifdef kWarningLog
#undef kWarningLog
#endif
#ifdef DEKAF2_WITH_KLOG
//---------------------------------------------------------------------------
/// log a warning message, do NOT automatically provide function name.
#define kWarningLog(...) \
{ \
	DEKAF2_PREFIX KLog::getInstance().debug(-1, __VA_ARGS__); \
}
//---------------------------------------------------------------------------
#else
#define kWarningLog(...) DEKAF2_PREFIX detail::IntWarning_uNuSuAlNaMe("", __VA_ARGS__)
#endif

#ifdef kException
#undef kException
#endif
#ifdef DEKAF2_WITH_KLOG
//---------------------------------------------------------------------------
/// log an exception, automatically provide function name and generate a
/// stacktrace at level -2
#define kException(except) \
{ \
	DEKAF2_PREFIX KLog::getInstance().Exception(except, DEKAF2_FUNCTION_NAME); \
}
//---------------------------------------------------------------------------
#else
#define kException(except) DEKAF2_PREFIX detail::IntException_uNuSuAlNaMe(except, DEKAF2_FUNCTION_NAME)
#endif

#ifdef kUnknownException
#undef kUnknownException
#endif
#ifdef DEKAF2_WITH_KLOG
//---------------------------------------------------------------------------
/// log an unknown exception, automatically provide function name.
#define kUnknownException() \
{ \
	DEKAF2_PREFIX KLog::getInstance().Exception(DEKAF2_FUNCTION_NAME); \
}
//---------------------------------------------------------------------------
#else
#define kUnknownException() DEKAF2_PREFIX detail::IntException_uNuSuAlNaMe("unknown", DEKAF2_FUNCTION_NAME)
#endif

#ifdef kDebugTrace
#undef kDebugTrace
#endif
#ifdef DEKAF2_WITH_KLOG
//---------------------------------------------------------------------------
/// force a stack trace, automatically provide function name.
#define kDebugTrace(...) \
{ \
	DEKAF2_PREFIX KLog::getInstance().debug_fun(-2, DEKAF2_FUNCTION_NAME, __VA_ARGS__); \
}
//---------------------------------------------------------------------------
#else
#define kDebugTrace(...)
#endif

#ifdef kJSONTrace
#undef kJSONTrace
#endif
#ifdef DEKAF2_WITH_KLOG
//---------------------------------------------------------------------------
/// special stack dump handling just for KJSON (nlohmann)
#define kJSONTrace() \
{ \
	DEKAF2_PREFIX KLog::getInstance().JSONTrace(DEKAF2_FUNCTION_NAME); \
}
//---------------------------------------------------------------------------
#else
#define kJSONTrace()
#endif

#ifdef kTraceDownCaller
#undef kTraceDownCaller
#endif
#ifdef DEKAF2_WITH_KLOG
//---------------------------------------------------------------------------
/// print first frame from a file not in sSkipFiles (comma separated basenames)
#define kTraceDownCaller(iSkipStackLines, sSkipFiles, sMessage) \
{ \
	DEKAF2_PREFIX KLog::getInstance().TraceDownCaller(iSkipStackLines, sSkipFiles, sMessage); \
}
//---------------------------------------------------------------------------
#else
#define kTraceDownCaller(iSkipStackLines, sSkipFiles, sMessage)
#endif

#ifdef kWouldLog
#undef kWouldLog
#endif
#ifdef DEKAF2_WITH_KLOG
//---------------------------------------------------------------------------
/// test if a given log level would create output
#define kWouldLog(iLevel) (DEKAF2_UNLIKELY(iLevel <= DEKAF2_PREFIX KLog::s_iThreadLogLevel))
//---------------------------------------------------------------------------
#else
#define kWouldLog(iLevel) (false)
#endif
