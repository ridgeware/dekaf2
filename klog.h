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

#include <exception>
#include <mutex>
#include <vector>
#include "kstring.h"
#include "kstream.h"
#include "kformat.h"
#include "bits/kcppcompat.h"

#ifndef DEKAF2_IS_WINDOWS
	#define DEKAF2_HAS_SYSLOG
#endif

namespace dekaf2
{

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// ABC for the LogWriter object
class KLogWriter
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
//----------
public:
//----------
	KLogWriter() {}
	virtual ~KLogWriter();
	virtual bool Write(int iLevel, bool bIsMultiline, const KString& sOut) = 0;
	virtual bool Good() const = 0;

}; // KLogWriter


//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Logwriter that instantiates around any std::ostream
class KLogStdWriter : public KLogWriter
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
//----------
public:
//----------
	KLogStdWriter(std::ostream& iostream)
	    : m_OutStream(iostream)
	{}
	virtual ~KLogStdWriter() {}
	virtual bool Write(int iLevel, bool bIsMultiline, const KString& sOut) override;
	virtual bool Good() const override { return m_OutStream.good(); }

//----------
private:
//----------
	std::ostream& m_OutStream;

}; // KLogStdWriter


//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Logwriter that opens a file
class KLogFileWriter : public KLogWriter
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
//----------
public:
//----------
	KLogFileWriter(KStringView sFileName);
	virtual ~KLogFileWriter() {}
	virtual bool Write(int iLevel, bool bIsMultiline, const KString& sOut) override;
	virtual bool Good() const override { return m_OutFile.good(); }

//----------
private:
//----------
	KOutFile m_OutFile;

}; // KLogFileWriter

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Logwriter that writes into a string, possibly with concatenation chars
class KLogStringWriter : public KLogWriter
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
//----------
public:
//----------
	/// The sConcat will be written between individual log messages.
	/// It will not be written after the last message
	KLogStringWriter(KString& sOutString, KString sConcat = "\n");
	virtual ~KLogStringWriter() {}
	virtual bool Write(int iLevel, bool bIsMultiline, const KString& sOut) override;
	virtual bool Good() const override { return true; }

//----------
private:
//----------
	KString& m_OutString;
	KString m_sConcat;

}; // KLogStringWriter



#ifdef DEKAF2_HAS_SYSLOG
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Logwriter for the syslog
class KLogSyslogWriter : public KLogWriter
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
//----------
public:
//----------
	KLogSyslogWriter() {}
	virtual ~KLogSyslogWriter() {}
	virtual bool Write(int iLevel, bool bIsMultiline, const KString& sOut) override;
	virtual bool Good() const override { return true; }

}; // KLogSyslogWriter
#endif

#ifdef DEKAF2_KLOG_WITH_TCP

class KConnection; // fwd decl - we do not want to include the kconnection header here

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Logwriter that writes to any TCP endpoint
class KLogTCPWriter : public KLogWriter
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
//----------
public:
//----------
	KLogTCPWriter(KStringView sURL);
	virtual ~KLogTCPWriter();
	virtual bool Write(int iLevel, bool bIsMultiline, const KString& sOut) override;
	virtual bool Good() const override;

//----------
protected:
//----------

	std::unique_ptr<KConnection> m_OutStream;
	KString m_sURL;

}; // KLogTCPWriter

class KHTTPClient; // fwd decl - we do not want to include the khttpclient header here

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Logwriter that writes to any TCP endpoint using the HTTP(s) protocol
class KLogHTTPWriter : public KLogWriter
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
//----------
public:
//----------
	KLogHTTPWriter(KStringView sURL);
	virtual ~KLogHTTPWriter();
	virtual bool Write(int iLevel, bool bIsMultiline, const KString& sOut) override;
	virtual bool Good() const override;

//----------
protected:
//----------

	std::unique_ptr<KHTTPClient> m_OutStream;
	KString m_sURL;

}; // KLogHTTPWriter

class KHTTPHeaders;

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Logwriter that writes into a HTTP header
class KLogHTTPHeaderWriter : public KLogWriter
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
//----------
public:
//----------
	KLogHTTPHeaderWriter(KHTTPHeaders& HTTPHeaders, KStringView sHeader = "x-klog");
	virtual ~KLogHTTPHeaderWriter();
	virtual bool Write(int iLevel, bool bIsMultiline, const KString& sOut) override;
	virtual bool Good() const override;

//----------
protected:
//----------

	KHTTPHeaders& m_Headers;
	KString m_sHeader;

}; // KLogHTTPHeaderWriter

#endif

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Base class for KLog serialization. Takes the data to be written someplace.
class KLogData
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
//----------
public:
//----------
	KLogData(int iLevel = 0,
	         KStringView sShortName = KStringView{},
	         KStringView sPathName  = KStringView{},
	         KStringView sFunction  = KStringView{},
	         KStringView sMessage   = KStringView{})
	{
		Set(iLevel, sShortName, sPathName, sFunction, sMessage);
	}

	void Set(int iLevel, KStringView sShortName, KStringView sPathName, KStringView sFunction, KStringView sMessage);
	void SetBacktrace(KStringView sBacktrace)
	{
		m_sBacktrace = sBacktrace;
	}
	int GetLevel() const
	{
		return m_iLevel;
	}

//----------
protected:
//----------
	static KStringView SanitizeFunctionName(KStringView sFunction);

	int         m_iLevel;
	pid_t       m_Pid;
	uint64_t    m_Tid; // tid is 64 bit on OSX
	time_t      m_Time;
	KStringView m_sShortName;
	KStringView m_sPathName;
	KStringView m_sFunctionName;
	KStringView m_sMessage;
	KStringView m_sBacktrace;

}; // KLogData

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Extension of the data base class. Adds the generic serialization methods
/// as virtual functions.
class KLogSerializer : public KLogData
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
//----------
public:
//----------
	KLogSerializer() {}
	virtual ~KLogSerializer() {}
	const KString& Get() const;
	virtual operator KStringView() const;
	void Set(int iLevel, KStringView sShortName, KStringView sPathName, KStringView sFunction, KStringView sMessage);
	bool IsMultiline() const { return m_bIsMultiline; }

//----------
protected:
//----------
	virtual void Serialize() const = 0;

	mutable KString m_sBuffer;
	mutable bool m_bIsMultiline;

}; // KLogSerializer

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Specialization of the serializer for TTY like output devices: creates
/// simple text lines of output
class KLogTTYSerializer : public KLogSerializer
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
//----------
public:
//----------
	KLogTTYSerializer() {}
	virtual ~KLogTTYSerializer() {}

//----------
protected:
//----------
	void AddMultiLineMessage(KStringView sPrefix, KStringView sMessage) const;
	virtual void Serialize() const;

}; // KLogTTYSerializer

#ifdef DEKAF2_HAS_SYSLOG
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Specialization of the serializer for the Syslog: creates simple text lines
/// of output, but without the prefix like timestamp and warning level
class KLogSyslogSerializer : public KLogTTYSerializer
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
//----------
public:
//----------
	KLogSyslogSerializer() {}
	virtual ~KLogSyslogSerializer() {}

//----------
protected:
//----------
	virtual void Serialize() const;

}; // KLogSyslogSerializer
#endif

#ifdef DEKAF2_KLOG_WITH_TCP

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Specialization of the serializer for JSON output: creates a serialized
/// JSON object
class KLogJSONSerializer : public KLogSerializer
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
//----------
public:
//----------
	KLogJSONSerializer() {}
	virtual ~KLogJSONSerializer() {}

//----------
protected:
//----------
	virtual void Serialize() const;

}; // KLogJSONSerializer

#endif

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Primary logging facility for dekaf2.
/// The logging logics in dekaf historically follows the idea that the log
/// level will be set at program start, probably by a command line argument.
///
/// It can also be changed during runtime of the application by
/// changing a "flagfile" in the file system. This setting change will be
/// picked up by the running application within a second.
/// log messages that have a "level" equal or lower than the value of the
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

	static constexpr KStringViewZ STDOUT = "stdout";
	static constexpr KStringViewZ STDERR = "stderr";
#ifdef DEKAF2_HAS_SYSLOG
	static constexpr KStringViewZ SYSLOG = "syslog";
#endif
	static constexpr KStringViewZ DBAR   = "================================================================================";
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
		JSON
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
	bool SetDebugFlag(KStringViewZ sFlagfile);
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
	template<class... Args>
	inline bool warning(Args&&... args)
	//---------------------------------------------------------------------------
	{
		return IntDebug(-1, KStringView(), kFormat(std::forward<Args>(args)...));
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
		bool bOldValue = s_bGlobalShouldOnlyShowCallerOnJsonError;
		s_bGlobalShouldOnlyShowCallerOnJsonError = bNewValue;
		return bOldValue;
	}

	//---------------------------------------------------------------------------
	/// Log either full trace or call place for JSON
	void JSONTrace(KStringView sFunction);
	//---------------------------------------------------------------------------

	//---------------------------------------------------------------------------
	/// Log messages of this thread of execution with level <= iLevel into the
	/// current klog. This can be used to force more logging for a particular thread
	/// than for other threads.
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
	/// JSON string buffer.
	void LogThisThreadToJSON(int iLevel, KString& sJSON);
	//---------------------------------------------------------------------------
#endif

	static KStringView s_sJSONSkipFiles;

	// do _not_ initialize s_kLoglevel - see implementation note. Also, it needs
	// to be publicly visible as gcc does _not_ optimize a static inline GetLevel()
	// into an inline. We have to test the static var directly from kDebug to
	// have it inlined
	static int s_iLogLevel;

	static thread_local int s_iThreadLogLevel;

//----------
private:
//----------

	// private ctor
	KLog();

	bool IntDebug (int iLevel, KStringView sFunction, KStringView sMessage);
	void IntException (KStringView sWhat, KStringView sFunction, KStringView sClass);
	bool IntOpenLog ();

	static std::recursive_mutex s_LogMutex;
	static bool s_bBackTraceAlreadyCalled;
	static bool s_bGlobalShouldShowStackOnJsonError;
	static bool s_bGlobalShouldOnlyShowCallerOnJsonError; 
	static thread_local bool s_bShouldShowStackOnJsonError;
	static thread_local std::unique_ptr<KLogSerializer> s_PerThreadSerializer;
	static thread_local std::unique_ptr<KLogWriter> s_PerThreadWriter;

	KString m_sPathName;
	KString m_sShortName;
	KString m_sLogName;
	KString m_sFlagfile;
	int m_iBackTrace { -2 };
	time_t m_sTimestampFlagfile { 0 };
	std::unique_ptr<KLogSerializer> m_Serializer;
	std::unique_ptr<KLogWriter> m_Logger;
	// the m_Traces vector is protected by the s_LogMutex
	std::vector<KString> m_Traces;
	bool m_bHadConfigFromFlagFile { false };
	bool m_bLogIsRegularFile { false };
	bool m_bIsCGI { false }; // we need this bool on top of m_Logmode for the constructor
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
