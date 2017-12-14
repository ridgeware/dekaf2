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

#include <clocale>
#include <cstdlib>
#include <libproc.h>
#include "dekaf2.h"
#include "klog.h"
#include "kfile.h"
#include "bits/kcppcompat.h"


namespace dekaf2
{

const char DefaultLocale[] = "en_US.UTF-8";

//---------------------------------------------------------------------------
Dekaf::Dekaf()
//---------------------------------------------------------------------------
{
	SetUnicodeLocale();
	SetRandomSeed();

#ifdef DEKAF2_IS_UNIX
	// get the own executable's path and name
	char path[PROC_PIDPATHINFO_MAXSIZE];
	if (proc_pidpath(getpid(), path, sizeof(path)) > 0)
	{
		// we have to do the separation of dir and name manually here as kFileDirName() and kFileBaseName()
		// would invoke kRFind(), which on non-Linux platforms would call into Dekaf().GetCpuId() and hence
		// into a not yet constructed instance
		size_t pos = strlen(path);
		size_t name = 0;
		while (pos)
		{
			--pos;
			if (path[pos] == '/')
			{
				name = pos + 1;
				break;
			}
		}
		m_sProgName = &path[name];
		m_sProgPath = path;
		m_sProgPath.erase(m_sProgPath.size() - m_sProgName.size());
		while (m_sProgPath.size() > 1 && m_sProgPath.back() == '/')
		{
			m_sProgPath.erase(m_sProgPath.size()-1);
		}
		KLog().SetName(m_sProgName);
	}
#endif

}

//---------------------------------------------------------------------------
bool Dekaf::SetUnicodeLocale(KStringView sName)
//---------------------------------------------------------------------------
{
	m_sLocale = sName;

	try
	{
#ifdef DEKAF2_IS_OSX
		// no way to get the user's locale in OSX with C++. So simply set to en_US if not given as a parameter.
		if (m_sLocale.empty() || m_sLocale == "C" || m_sLocale == "C.UTF-8")
		{
			m_sLocale = DefaultLocale;
		}
		std::setlocale(LC_ALL, m_sLocale.c_str());
		// OSX does not use the .UTF-8 suffix (as all is UTF8)
		if (m_sLocale.EndsWith(".UTF-8"))
		{
			m_sLocale.erase(m_sLocale.end() - 6, m_sLocale.end());
		}
		std::locale::global(std::locale(m_sLocale.c_str()));
		// make the name compatible to the rest of the world
		m_sLocale += ".UTF-8";
#else
		// on other platforms, query the user's locale
		if (m_sLocale.empty())
		{
			m_sLocale = std::locale("").name();
		}
		// set to a fully defined locale if only the C locale is setup. This is also needed for C.UTF-8, as
		// that one does not permit character conversions outside the ASCII range.
		if (m_sLocale.empty() || m_sLocale == "C" || m_sLocale == "C.UTF-8")
		{
			m_sLocale = DefaultLocale;
		}
		std::setlocale(LC_ALL, m_sLocale.c_str());
		std::locale::global(std::locale(m_sLocale.c_str()));
#endif
	}
	catch (std::exception& e) {
		kException(e);
		m_sLocale.erase();
	}
	catch (...) {
		kUnknownException();
		m_sLocale.erase();
	}

	return !m_sLocale.empty();
}

//---------------------------------------------------------------------------
void Dekaf::SetRandomSeed(unsigned int iSeed)
//---------------------------------------------------------------------------
{
	if (!iSeed)
	{
		iSeed = static_cast<unsigned int>(time(0));
	}
	srand(iSeed);
	srand48(iSeed);
}


//---------------------------------------------------------------------------
class Dekaf& Dekaf()
//---------------------------------------------------------------------------
{
	static class Dekaf myDekaf;
	return myDekaf;
}

//---------------------------------------------------------------------------
void kInit(KStringView sName, KStringView sDebugLog, KStringView sDebugFlag, bool bShouldDumpCore, bool bEnableMultiThreading)
//---------------------------------------------------------------------------
{
	Dekaf().SetMultiThreading(bEnableMultiThreading);
	KLog().SetDebugLog(sDebugLog);
	KLog().SetDebugFlag(sDebugFlag);
	KLog().SetName(sName);
}

} // end of namespace dekaf2

