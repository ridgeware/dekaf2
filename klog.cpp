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
#include "kstring.h"
#include "kgetruntimestack.h"

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
    : m_sLogfile("/tmp/dekaf.log")
    , m_sFlagfile("/tmp/dekaf.dbg")
    , m_Log(m_sLogfile, std::ios::ate)
{
#if NDEBUG
	s_kLogLevel = -1;
#else
	s_kLogLevel = 0;
#endif
}

//---------------------------------------------------------------------------
bool KLog::SetDebugLog(KStringView sLogfile)
//---------------------------------------------------------------------------
{
	if (sLogfile.empty())
	{
		return false;
	}

	m_sLogfile = sLogfile;

	m_Log.open(m_sLogfile, std::ios::ate);
	if (!m_Log.is_open())
	{
		return false;
	}

	return true;
}

//---------------------------------------------------------------------------
bool KLog::SetDebugFlag(KStringView sFlagfile)
//---------------------------------------------------------------------------
{
	if (sFlagfile.empty())
	{
		return false;
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
	return m_Log.good();
}

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
