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

// Ctor
//---------------------------------------------------------------------------
KLog::KLog()
//---------------------------------------------------------------------------
    : m_sLogfile("/tmp/dekaf.log")
    , m_sFlagfile("/tmp/dekaf.dbg")
    , m_Log(m_sLogfile, KFile::APPEND)
{
}

//---------------------------------------------------------------------------
//	filename set/get debuglog("stderr/stdout/syslog" special files) /tmp/dekaf.log
bool KLog::set_debuglog(const KString& sLogfile)
//---------------------------------------------------------------------------
{
	if (sLogfile.empty())
	{
		return false;
	}

	m_sLogfile = sLogfile;

	if (!m_Log.Open(m_sLogfile))
	{
		return false;
	}

	return true;
}

//---------------------------------------------------------------------------
//	set/get debugflag() /tmp/dekaf.dbg
bool KLog::set_debugflag(const KString& sFlagfile)
//---------------------------------------------------------------------------
{
	if (sFlagfile.empty())
	{
		return false;
	}

	m_sFlagfile = sFlagfile;
}

//---------------------------------------------------------------------------
bool KLog::int_debug(int level, const KString& sMessage)
//---------------------------------------------------------------------------
{
	if (level > m_iLevel)
	{
		return true;
	}
	else
	{
		return m_Log.WriteLine(sMessage) && m_Log.Flush();
	}
}

//---------------------------------------------------------------------------
void KLog::int_exception(const char* sWhat, const char* sFunction, const char* sClass)
//---------------------------------------------------------------------------
{
	const char* sF(sFunction);
	if (!sF || !*sF)
	{
		sF = "(unknown)";
	}
	if (sClass && *sClass)
	{
		warning("{0}::{1}() caught exception: '{2}'", sClass, sF, sWhat);
	}
	else
	{
		warning("{0} caught exception: '{1}'", sF, sWhat);
	}
}

} // of namespace dekaf2
