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
#include <iostream>
#include "dekaf2.h"
#include "klog.h"
#include "kfilesystem.h"
#include "kcrashexit.h"
#include "ksystem.h"
#include "kchildprocess.h"
#include "bits/kcppcompat.h"
#include "kconfiguration.h"
#ifdef DEKAF2_HAS_LIBPROC
#include <libproc.h>
#endif


namespace dekaf2
{

const char DefaultLocale[] = "en_US.UTF-8";
bool Dekaf::s_bShutdown = false;

#if defined(DEKAF2_HAS_LIBPROC) || defined(DEKAF2_IS_UNIX)
//---------------------------------------------------------------------------
DEKAF2_ALWAYS_INLINE
void local_split_in_path_and_name(const char* sFullPath, KString& sPath, KString& sName)
//---------------------------------------------------------------------------
{
	// we have to do the separation of path and name manually here as kDirname() and kBasename()
	// would invoke kRFind(), which on non-Linux platforms would call into Dekaf().GetCpuId() and hence
	// into a not yet constructed instance
	size_t pos = strlen(sFullPath);
	while (pos)
	{
		--pos;
		if (sFullPath[pos] == '/')
		{
			++pos;
			break;
		}
	}
	sName = &sFullPath[pos];
	while (pos > 0 && sFullPath[pos-1] == '/')
	{
		--pos;
	}
	sPath.assign(sFullPath, pos);
}
#endif

//---------------------------------------------------------------------------
Dekaf::Dekaf()
//---------------------------------------------------------------------------
{
	SetUnicodeLocale();
	SetRandomSeed();

#ifdef DEKAF2_HAS_LIBPROC
	// get the own executable's path and name through the libproc abstraction
	// which has the advantage that it also works on systems without /proc file system
	char path[PROC_PIDPATHINFO_MAXSIZE+1];
	if (proc_pidpath(getpid(), path, PROC_PIDPATHINFO_MAXSIZE) > 0)
	{
		local_split_in_path_and_name(path, m_sProgPath, m_sProgName);
	}
#elif DEKAF2_IS_UNIX
	// get the own executable's path and name through the /proc file system
	char path[PATH_MAX+1];
	ssize_t len;
	if ((len = readlink("/proc/self/exe", path, PATH_MAX)) > 0)
	{
		path[len] = 0;
		local_split_in_path_and_name(path, m_sProgPath, m_sProgName);
	}
#endif

	StartDefaultTimer();

	// allow KLog() calls from dekaf now
	m_bInConstruction = false;
}

//---------------------------------------------------------------------------
bool Dekaf::SetUnicodeLocale(KStringView sName)
//---------------------------------------------------------------------------
{
	m_sLocale = sName;

	DEKAF2_TRY
	{
		if (m_sLocale.empty())
		{
			m_sLocale = std::locale().name();
		}
		if (m_sLocale.empty() || m_sLocale == "C" || m_sLocale == "C.UTF-8")
		{
			m_sLocale = DefaultLocale;
		}
		std::setlocale(LC_ALL, m_sLocale.c_str());
		std::locale::global(std::locale(m_sLocale.c_str()));
		m_sLocale = std::locale().name();
		if (!std::iswupper(0x53d))
		{
			std::cerr << "dekaf: cannot set C++ locale to Unicode" << std::endl;
			return false;
		}
	}
	DEKAF2_CATCH (std::exception& e) {
		if (m_bInConstruction)
		{
			std::cerr << e.what() << std::endl;
		}
		else
		{
			kException(e);
		}
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
		iSeed = static_cast<unsigned int>(time(nullptr));
	}
	srand(iSeed);
	srand48(iSeed);
#ifdef DEKAF2_IS_OSX
	srandomdev();
#else
	srandom(iSeed);
#endif
}

//---------------------------------------------------------------------------
KStringView Dekaf::GetVersionInformation() const
//---------------------------------------------------------------------------
{
	constexpr KStringView sVersionInformation =
		"dekaf-" DEKAF_VERSION "-" DEKAF2_BUILD_TYPE
#ifdef DEKAF2_USE_EXCEPTIONS
		" exceptions"
#endif
#ifdef DEKAF2_USE_FBSTRING_AS_KSTRING
		" fbstring"
#endif
#ifdef DEKAF2_USE_FOLLY_STRINGPIECE_AS_KSTRINGVIEW
		" fbstringview"
#endif
#ifdef DEKAF2_USE_OPTIMIZED_STRING_FIND
		" optimized_string_find"
#endif
#ifdef DEKAF2_USE_BOOST_MULTI_INDEX
		" multi_index"
#endif
#ifdef DEKAF2_HAS_LIBPROC
		" libproc"
#endif
#ifdef DEKAF2_HAS_MYSQL
		" mysql"
#endif
#ifdef DEKAF2_HAS_SQLITE3
		" sqlite"
#endif
#ifdef DEKAF2_USE_FROZEN_HASH_FOR_LARGE_MAPS
		" large_frozen_maps"
#endif
#ifdef DEKAF2_WITH_FCGI
		" fcgi"
#endif
#ifdef DEKAF2_WITH_DEPRECATED_KSTRING_MEMBER_FUNCTIONS
		" deprecated_kstring_interface"
#endif
#if (DEKAF1_INCLUDE_PATH)
		" with-dekaf1-compatibility"
#endif
	;

	return sVersionInformation;
}

//---------------------------------------------------------------------------
KStringView Dekaf::GetVersion() const
//---------------------------------------------------------------------------
{
	constexpr KStringView sVersion = "dekaf-" DEKAF_VERSION "-" DEKAF2_BUILD_TYPE ;

	return sVersion;
}

//---------------------------------------------------------------------------
time_t Dekaf::GetCurrentTime() const
//---------------------------------------------------------------------------
{
	if (DEKAF2_UNLIKELY(!m_Timer))
	{
		return std::time(nullptr);
	}
	else
	{
		return m_iCurrentTime;
	}
}

//---------------------------------------------------------------------------
/// Get current time without constantly querying the OS
KTimer::Timepoint Dekaf::GetCurrentTimepoint() const
//---------------------------------------------------------------------------
{
	if (DEKAF2_UNLIKELY(!m_Timer))
	{
		return KTimer::Clock::now();
	}
	else
	{
		return m_iCurrentTimepoint;
	}
}

//---------------------------------------------------------------------------
void Dekaf::StartDefaultTimer()
//---------------------------------------------------------------------------
{
	if (!m_Timer)
	{
		// make sure we have an initial value set for
		// the time keepers
		m_iCurrentTimepoint = KTimer::Clock::now();
		m_iCurrentTime = KTimer::ToTimeT(m_iCurrentTimepoint);

		// create a KTimer
		m_Timer = std::make_unique<KTimer>();

		if (!m_Timer)
		{
			kCrashExit();
		}

		// and start the timer that updates the time keepers
		m_OneSecTimerID = m_Timer->CallEvery(
											 std::chrono::seconds(1),
											 [this](KTimer::Timepoint tp) {
												 this->OneSecTimer(tp);
											 });
	}
}

//---------------------------------------------------------------------------
void Dekaf::StopDefaultTimer()
//---------------------------------------------------------------------------
{
	if (m_Timer)
	{
		m_Timer->DestructWithJoin();
		m_Timer.reset();
	}
}

//---------------------------------------------------------------------------
KTimer& Dekaf::GetTimer()
//---------------------------------------------------------------------------
{
	if (!m_Timer)
	{
		kCrashExit();
	}
	return *m_Timer;
}

//---------------------------------------------------------------------------
void Dekaf::OneSecTimer(KTimer::Timepoint tp)
//---------------------------------------------------------------------------
{
	m_iCurrentTime = KTimer::Clock::to_time_t(tp);
	m_iCurrentTimepoint = tp;

	if (m_OneSecTimerMutex.try_lock())
	{

		for (const auto& it : m_OneSecTimers)
		{
			it();
		}

		m_OneSecTimerMutex.unlock();
	}
}

//---------------------------------------------------------------------------
bool Dekaf::AddToOneSecTimer(OneSecCallback CB)
//---------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_OneSecTimerMutex);

	m_OneSecTimers.push_back(CB);

	return true;
}

//---------------------------------------------------------------------------
void Dekaf::Daemonize()
//---------------------------------------------------------------------------
{
	// we need to stop the thread with the default timer, otherwise
	// parent will not return
	StopDefaultTimer();

	// now try to become a daemon
	detail::kDaemonize();

	// and start the timer again
	StartDefaultTimer();
}

//---------------------------------------------------------------------------
void Dekaf::ShutDown(bool bImmediately)
//---------------------------------------------------------------------------
{
	if (m_Timer)
	{
		if (!bImmediately)
		{
			m_Timer->DestructWithJoin();
		}

		m_Timer.reset();
	}

} // ShutDown

//---------------------------------------------------------------------------
void Dekaf::StartSignalHandlerThread()
//---------------------------------------------------------------------------
{
	if (!m_Signals)
	{
		m_Signals = std::make_unique<KSignals>();
	}
}

//---------------------------------------------------------------------------
class Dekaf& Dekaf()
//---------------------------------------------------------------------------
{
	static class Dekaf myDekaf;
	return myDekaf;
}

//---------------------------------------------------------------------------
void kInit(KStringView sName, KStringViewZ sDebugLog, KStringViewZ sDebugFlag, bool bShouldDumpCore, bool bEnableMultiThreading, bool bStartSignalHandlerThread)
//---------------------------------------------------------------------------
{
	if (bStartSignalHandlerThread)
	{
		// it is important to block all signals before
		// instantiating Dekaf(), as otherwise the timer
		// thread would run with an enabled sigmask
		kBlockAllSignals();

		Dekaf().StartSignalHandlerThread();
	}
	
	Dekaf().SetMultiThreading(bEnableMultiThreading);

	// make sure KLog is instantiated
	KLog();

	if (!sDebugLog.empty())
	{
		KLog().SetDebugLog(sDebugLog);
	}

	if (!sDebugFlag.empty())
	{
		KLog().SetDebugFlag(sDebugFlag);
	}

	if (!sName.empty())
	{
		KLog().SetName(sName);
	}
}

} // end of namespace dekaf2

