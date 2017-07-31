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
	int get_level() const
	//---------------------------------------------------------------------------
	{
		return m_iLevel;
	}

	//---------------------------------------------------------------------------
	int set_level(int iLevel)
	//---------------------------------------------------------------------------
	{
		m_iLevel = iLevel;
		return m_iLevel;
	}

	//---------------------------------------------------------------------------
	bool set_debuglog(KStringView sLogfile);
	//---------------------------------------------------------------------------

	//---------------------------------------------------------------------------
	KStringView get_debuglog() const
	//---------------------------------------------------------------------------
	{
		return m_sLogfile;
	}

	//---------------------------------------------------------------------------
	bool set_debugflag(KStringView sFlagfile);
	//---------------------------------------------------------------------------

	//---------------------------------------------------------------------------
	KStringView get_debugflag() const
	//---------------------------------------------------------------------------
	{
		return m_sFlagfile;
	}

	//---------------------------------------------------------------------------
	template<class... Args>
	bool debug(int level, Args&&... args)
	//---------------------------------------------------------------------------
	{
		if (level > m_iLevel)
		{
			return true;
		}
		return int_debug(level, kFormat(std::forward<Args>(args)...));
	}

	//---------------------------------------------------------------------------
	template<class... Args>
	bool warning(Args&&... args)
	//---------------------------------------------------------------------------
	{
		return debug(-1, std::forward<Args>(args)...);
	}

	//---------------------------------------------------------------------------
	/// report a known exception
	void exception(const std::exception& e, KStringView sFunction, KStringView sClass = "")
	//---------------------------------------------------------------------------
	{
		int_exception(e.what(), sFunction, sClass);
	}


	//---------------------------------------------------------------------------
	/// report an unknown exception
	void exception(KStringView sFunction, KStringView sClass = "")
	//---------------------------------------------------------------------------
	{
		int_exception("unknown", sFunction, sClass);
	}

//----------
private:
//----------
	//---------------------------------------------------------------------------
	bool int_debug(int level, KStringView sMessage);
	//---------------------------------------------------------------------------

	//---------------------------------------------------------------------------
	void int_exception(KStringView sWhat, KStringView sFunction, KStringView sClass);
	//---------------------------------------------------------------------------

	int m_iLevel{0};
	KString m_sLogfile;
	KString m_sFlagfile;
	KFileWriter m_Log;
};


//---------------------------------------------------------------------------
KLog& KLog();
//---------------------------------------------------------------------------


} // end of namespace dekaf2
