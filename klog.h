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
#include <fstream>
#include "kstring.h"
#include "kwriter.h"
#include "kformat.h"
#include "kfile.h"
#include "bits/kcppcompat.h"

namespace dekaf2
{

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Base class for KLog serialization. Takes the data to be written someplace.
class KLogData
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
//----------
public:
//----------
	KLogData(int level = 0,
	         KStringView sShortName = KStringView{},
	         KStringView sPathName  = KStringView{},
	         KStringView sFunction  = KStringView{},
	         KStringView sMessage   = KStringView{})
	{
		Set(level, sShortName, sPathName, sFunction, sMessage);
	}

	void Set(int level, KStringView sShortName, KStringView sPathName, KStringView sFunction, KStringView sMessage);
	void SetBacktrace(KStringView sBacktrace)
	{
		m_sBacktrace = sBacktrace;
	}
	int GetLevel() const
	{
		return m_Level;
	}

//----------
protected:
//----------
	static KStringView SanitizeFunctionName(KStringView sFunction);

	int         m_Level;
	pid_t       m_Pid;
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
	virtual operator const KString&() const;
	void Set(int level, KStringView sShortName, KStringView sPathName, KStringView sFunction, KStringView sMessage);
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
	void HandleMultiLineMessages() const;
	virtual void Serialize() const;

}; // KLogTTYSerializer

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
	virtual bool Write(const KLogSerializer& Serializer) = 0;
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
	virtual bool Write(const KLogSerializer& Serializer);
	virtual bool Good() const { return m_OutStream.good(); }

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
	KLogFileWriter(KStringView sFileName)
	    : m_OutFile(sFileName, std::ios_base::app)
	{}
	virtual ~KLogFileWriter() {}
	virtual bool Write(const KLogSerializer& Serializer);
	virtual bool Good() const { return m_OutFile.good(); }

//----------
private:
//----------
	KOutFile m_OutFile;

}; // KLogFileWriter


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
	virtual bool Write(const KLogSerializer& Serializer);
	virtual bool Good() const { return true; }

}; // KLogSyslogWriter


//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Logwriter that writes to any TCP endpoint
class KLogTCPWriter : public KLogWriter
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
//----------
public:
//----------
	KLogTCPWriter(KStringView sURL);
	virtual ~KLogTCPWriter() {}
	virtual bool Write(const KLogSerializer& Serializer);
	virtual bool Good() const { return m_OutStream != nullptr && m_OutStream->good(); }

//----------
protected:
//----------
	std::unique_ptr<KTCPStream> m_OutStream;

}; // KLogTCPWriter


//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Logwriter that writes to any TCP endpoint using the HTTP(s) protocol
class KLogHTTPWriter : public KLogTCPWriter
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
//----------
public:
//----------
	KLogHTTPWriter(KStringView sURL) : KLogTCPWriter(sURL) {}
	virtual ~KLogHTTPWriter() {}
	virtual bool Write(const KLogSerializer& Serializer);

}; // KLogHTTPWriter


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
/// KLog is implemented as sort of a singleton. It is guaranteed to be
/// instantiated whenever it is called, also in the early initialization
/// phase of the program for e.g. static types.
class KLog
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
//----------
public:
//----------
	KLog();
	KLog(const KLog&) = delete;
	KLog(KLog&&) = delete;
	KLog& operator=(const KLog&) = delete;
	KLog& operator=(KLog&&) = delete;

	static constexpr KStringView STDOUT             = "stdout";
	static constexpr KStringView STDERR             = "stderr";
	static constexpr KStringView SYSLOG             = "syslog";

	//---------------------------------------------------------------------------
	/// Gets the current log level. Any log message that has a higher level than
	/// this value is not output.
	static inline int GetLevel()
	//---------------------------------------------------------------------------
	{
		return s_kLogLevel;
	}

	//---------------------------------------------------------------------------
	/// Sets a new log level.
	void SetLevel(int iLevel);
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

	enum class Writer { STDOUT, STDERR, FILE, SYSLOG, TCP, HTTP };
	//---------------------------------------------------------------------------
	/// Create a Writer of specific type
	static std::unique_ptr<KLogWriter> CreateWriter(Writer writer, KStringView sLogname = KStringView{});
	//---------------------------------------------------------------------------

	enum class Serializer { TTY, SYSLOG, JSON };
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
	inline KStringView GetDebugLog() const
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
	inline KStringView GetDebugFlag() const
	//---------------------------------------------------------------------------
	{
		return m_sFlagfile;
	}

	//---------------------------------------------------------------------------
	/// this function is deprecated - use kDebug() instead!
	template<class... Args>
	inline bool debug(int level, Args&&... args)
	//---------------------------------------------------------------------------
	{
		return (level > s_kLogLevel) || IntDebug(level, KStringView(), kFormat(std::forward<Args>(args)...));
	}

	//---------------------------------------------------------------------------
	/// this function is deprecated - use kDebug() instead!
	template<class... Args>
	inline bool debug_fun(int level, KStringView sFunction, Args&&... args)
	//---------------------------------------------------------------------------
	{
		return (level > s_kLogLevel) || IntDebug(level, sFunction, kFormat(std::forward<Args>(args)...));
	}

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

	// do _not_ initialize s_kLoglevel - see implementation note. Also, it needs
	// to be publicly visible as gcc does _not_ optimize a static inline GetLevel()
	// into an inline. We have to test the static var directly from kDebug to
	// have it inlined
	static int s_kLogLevel;

//----------
private:
//----------

	//---------------------------------------------------------------------------
	/// Registered with Dekaf::AddToOneSecTimer() at construction, gets
	/// called every second and reconfigures debug level, output, and format
	/// if changed
	void CheckDebugFlag();
	//---------------------------------------------------------------------------

	//---------------------------------------------------------------------------
	bool IntDebug(int level, KStringView sFunction, KStringView sMessage);
	//---------------------------------------------------------------------------

	//---------------------------------------------------------------------------
	void IntException(KStringView sWhat, KStringView sFunction, KStringView sClass);
	//---------------------------------------------------------------------------

	//---------------------------------------------------------------------------
	bool IntOpenLog();
	//---------------------------------------------------------------------------

	int m_iBackTrace{-1};
	KString m_sPathName;
 	KString m_sShortName;
	KString m_sLogName;
	KString m_sFlagfile;
	time_t m_sTimestampFlagfile{0};
	std::unique_ptr<KLogSerializer> m_Serializer;
	std::unique_ptr<KLogWriter> m_Logger;

	constexpr static const char* const s_sEnvLog      = "DEKAFLOG";
	constexpr static const char* const s_sEnvFlag     = "DEKAFDBG";
	constexpr static const char* const s_sDefaultLog  = "/tmp/dekaf.log";
	constexpr static const char* const s_sDefaultFlag = "/tmp/dekaf.dbg";

}; // KLog

//---------------------------------------------------------------------------
KLog& KLog();
//---------------------------------------------------------------------------

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
// a debug output in the initialization phase of the program or have one too many.

#ifdef kDebug
	#undef kDebug
#endif
//---------------------------------------------------------------------------
/// log a debug message, automatically provide function name.
#define kDebug(level, ...) \
{ \
	if (level <= KLog::s_kLogLevel) \
	{ \
		KLog().debug_fun(level, DEKAF2_FUNCTION_NAME, __VA_ARGS__); \
	} \
}
//---------------------------------------------------------------------------

#ifdef kDebugLog
	#undef kDebugLog
#endif
//---------------------------------------------------------------------------
/// log a debug message, do NOT automatically provide function name.
#define kDebugLog(level, ...) \
{ \
	if (level <= KLog::s_kLogLevel) \
	{ \
		KLog().debug(level, __VA_ARGS__); \
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
	KLog().debug_fun(-1, DEKAF2_FUNCTION_NAME, __VA_ARGS__); \
}
//---------------------------------------------------------------------------

#ifdef kException
	#undef kException
#endif
//---------------------------------------------------------------------------
/// log an exception, automatically provide function name.
#define kException(except) \
{ \
	KLog().Exception(except, DEKAF2_FUNCTION_NAME); \
}
//---------------------------------------------------------------------------

#ifdef kUnknownException
	#undef kUnknownException
#endif
//---------------------------------------------------------------------------
/// log an unknown exception, automatically provide function name.
#define kUnknownException() \
{ \
	KLog().Exception(DEKAF2_FUNCTION_NAME); \
}
//---------------------------------------------------------------------------


} // end of namespace dekaf2
