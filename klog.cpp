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

#if 0
TODO: KLOG OVERHAUL NEEDED
[x] output format as a single line was completely misunderstood -- partially fixed
[x] program name was misunderstood -- partially fixed
[ ] no way to specify "stdout" "stderr" TRACE() "syslog" as log output 
[ ] with {} style formatting there is no way to format things like %03d, need multiple methods I guess
[ ] constructor is *removing* the klog when program starts (completely wrong)
[ ] not sure I like KLog as a classname.  probable KLOG.
[ ] kDebug() and kWarning() macros should probably be kDebugFormat() and kDebugPrintf()
#endif

#include "klog.h"
#include "kstring.h"
#include "kgetruntimestack.h"
#include "kstringutils.h"
#include "ksystem.h"
#include <stdio.h>

namespace dekaf2
{

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

// Ctor
//---------------------------------------------------------------------------
KLog::KLog()
//---------------------------------------------------------------------------
    : m_sLogfile  (kGetEnv(s_sEnvLog,  s_sDefaultLog))
    , m_sFlagfile (kGetEnv(s_sEnvFlag, s_sDefaultFlag))
    , m_Log       (m_sLogfile.c_str(), std::ios::ate)
{
#ifdef NDEBUG
	s_kLogLevel = -1;
#else
	s_kLogLevel = 0;
#endif
}

//---------------------------------------------------------------------------
void KLog::SetName(KStringView sName)
//---------------------------------------------------------------------------
{
	m_sName = sName;
}

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

	if (sLogfile == m_sLogfile)
	{
		return true;
	}

	m_sLogfile = sLogfile;

	if (m_Log.is_open())
	{
		m_Log.close();
	}

	m_Log.open(m_sLogfile.c_str(), std::ios::ate);
	if (!m_Log.is_open())
	{
		return false;
	}

	return true;

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
bool KLog::IntBacktrace()
//---------------------------------------------------------------------------
{
	KString sStack = kGetBacktrace();

	if (!sStack.empty())
	{
		m_Log.WriteLine("====== Backtrace follows:   ======");
		m_Log.Write(sStack);
		m_Log.WriteLine("====== Backtrace until here ======");
		m_Log.flush();
	}
	return m_Log.good();
}

//---------------------------------------------------------------------------
bool KLog::IntDebug(int level, KStringView sFunction, KStringView sMessage)
//---------------------------------------------------------------------------
{
	// be careful - use basetypes here if possible and try to avoid mallocs

	#if 1
	// desired format:
	// | WAR | MYPRO | 17202 | 2001-08-24 10:37:04 | select count(*) from foo

	enum {MAXPREF = 100};
	char szPrefix[MAXPREF+1];
	char szLevel[3+1];
	if (level < 0) {
		snprintf (szLevel, 3+1, "WAR");
	}
	else if (level > 3) {
		snprintf (szLevel, 3+1, "DB%d", 3);
	}
	else {
		snprintf (szLevel, 3+1, "DB%d", level);
	}

	snprintf (szPrefix, MAXPREF, "| %3.3s | %5.5s | %5u | %s | ", szLevel, m_sName.c_str(), getpid(), kFormTimestamp().c_str());

	bool    bCloseMe = false;
	FILE*   fp = NULL;
	KString sMultiLine(sMessage);

	if (m_sLogfile == STDOUT) {
		fp = stdout;
	}
	else if (m_sLogfile == STDERR) {
		fp = stderr;
	}
	else {
		fp = fopen (m_sLogfile.c_str(), "a");
		bCloseMe = true;
	}

    sMultiLine.TrimRight();
    if (sMultiLine.find("\n") != KStringView::npos)
	{
		KString sNewPrefix("\n"); sNewPrefix += szPrefix;
		KString sMultiLine(sMessage);
		sMultiLine.Replace ("\n", sNewPrefix, /*offset=*/0, /*all=*/true);
	}

	if (fp) {
		fprintf (fp, "%s", szPrefix);
		fprintf (fp, "%s\n", sMultiLine.c_str());
		if (bCloseMe) {
			fclose (fp);
		}
		else {
			fflush (fp);
		}
	}
	else {
		m_Log.Write(szPrefix);
		m_Log.WriteLine(sMultiLine);
		m_Log.flush();
	}

	#else
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

		if (!sFunction.empty())
		{
			m_Log.Write(sFunction);
			m_Log.Write(": ");
		}
	}

	m_Log.WriteLine(sMessage);
	m_Log.flush();

	if (level <= m_iBackTrace)
	{
		IntBacktrace();
	}
	#endif
	return m_Log.good();

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
