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
#include "kfile.h" // TODO:KEEF:TEMP
#include "bits/kcppcompat.h"

namespace dekaf2
{

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
		s_kLogLevel = kFileExists("/tmp/dekaf.dbg") ? 1 : 0; // TODO:KEEF:TEMP
		return s_kLogLevel;
	}

	//---------------------------------------------------------------------------
	/// Sets a new log level.
	inline void SetLevel(int iLevel)
	//---------------------------------------------------------------------------
	{
		// TODO:KEEF:TEMP: 
		s_kLogLevel = iLevel;
	}

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
		return m_sName;
	}

	//---------------------------------------------------------------------------
	/// Set the output file for the log.
	bool SetDebugLog(KStringView sLogfile);
	//---------------------------------------------------------------------------

	//---------------------------------------------------------------------------
	/// Gets the file name of the output file for the log.
	inline KStringView GetDebugLog() const
	//---------------------------------------------------------------------------
	{
		return m_sLogfile;
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
	bool IntDebug(int level, KStringView sFunction, KStringView sMessage);
	//---------------------------------------------------------------------------

	//---------------------------------------------------------------------------
	bool IntBacktrace();
	//---------------------------------------------------------------------------

	//---------------------------------------------------------------------------
	void IntException(KStringView sWhat, KStringView sFunction, KStringView sClass);
	//---------------------------------------------------------------------------

	int m_iBackTrace{0};
	KString m_sName;
	KString m_sLogfile;
	KString m_sFlagfile;
	KOutFile m_Log;

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
	if (level <= KLog::GetLevel()) \
	{ \
		KLog().debug_fun(level, DEKAF2_FUNCTION_NAME, __VA_ARGS__); \
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
