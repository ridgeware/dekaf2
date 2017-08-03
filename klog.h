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

#include <exception>
#include <fstream>
#include "kstring.h"
#include "kwriter.h"
#include "kformat.h"

namespace dekaf2
{

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
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

	//---------------------------------------------------------------------------
	inline int GetLevel() const
	//---------------------------------------------------------------------------
	{
		return m_iLevel;
	}

	//---------------------------------------------------------------------------
	inline int SetLevel(int iLevel)
	//---------------------------------------------------------------------------
	{
		m_iLevel = iLevel;
		return m_iLevel;
	}

	//---------------------------------------------------------------------------
	inline int GetBackTrace() const
	//---------------------------------------------------------------------------
	{
		return m_iBackTrace;
	}

	//---------------------------------------------------------------------------
	inline int SetBackTrace(int iLevel)
	//---------------------------------------------------------------------------
	{
		m_iBackTrace = iLevel;
		return m_iBackTrace;
	}

	//---------------------------------------------------------------------------
	bool SetDebugLog(KStringView sLogfile);
	//---------------------------------------------------------------------------

	//---------------------------------------------------------------------------
	inline KStringView GetDebugLog() const
	//---------------------------------------------------------------------------
	{
		return m_sLogfile;
	}

	//---------------------------------------------------------------------------
	bool SetDebugFlag(KStringView sFlagfile);
	//---------------------------------------------------------------------------

	//---------------------------------------------------------------------------
	inline KStringView GetDebugFlag() const
	//---------------------------------------------------------------------------
	{
		return m_sFlagfile;
	}

	//---------------------------------------------------------------------------
	template<class... Args>
	inline bool debug(int level, Args&&... args)
	//---------------------------------------------------------------------------
	{
		return (level > m_iLevel) || IntDebug(level, kFormat(std::forward<Args>(args)...));
	}

	//---------------------------------------------------------------------------
	template<class... Args>
	inline bool warning(Args&&... args)
	//---------------------------------------------------------------------------
	{
		return debug(-1, std::forward<Args>(args)...);
	}

	//---------------------------------------------------------------------------
	/// report a known exception
	void Exception(const std::exception& e, KStringView sFunction, KStringView sClass = "")
	//---------------------------------------------------------------------------
	{
		IntException(e.what(), sFunction, sClass);
	}


	//---------------------------------------------------------------------------
	/// report an unknown exception
	void Exception(KStringView sFunction, KStringView sClass = "")
	//---------------------------------------------------------------------------
	{
		IntException("unknown", sFunction, sClass);
	}

//----------
private:
//----------
	//---------------------------------------------------------------------------
	bool IntDebug(int level, KStringView sMessage);
	//---------------------------------------------------------------------------

	//---------------------------------------------------------------------------
	bool IntBacktrace();
	//---------------------------------------------------------------------------

	//---------------------------------------------------------------------------
	void IntException(KStringView sWhat, KStringView sFunction, KStringView sClass);
	//---------------------------------------------------------------------------

	int m_iLevel{0};
	int m_iBackTrace{0};
	KString m_sLogfile;
	KString m_sFlagfile;
	KOutFile m_Log;
};


//---------------------------------------------------------------------------
KLog& KLog();
//---------------------------------------------------------------------------


} // end of namespace dekaf2
