/*
//
// DEKAF(tm): Lighter, Faster, Smarter(tm)
//
// Copyright (c) 2017-2019, Ridgeware, Inc.
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

#include "dekaf2.h"
#include "kcompatibility.h"
#include "kconfiguration.h"
#include "klog.h"
#include "kfilesystem.h"
#include "kcrashexit.h"
#include "ksystem.h"
#include "kchildprocess.h"
#include "kctype.h"

#include <cstdlib>
#include <cwctype>
#include <ctime>
#include <iostream>
#include <fstream>
#include <random>
#include <locale>
#ifdef DEKAF2_IS_OSX
#include <CoreFoundation/CoreFoundation.h> // for locale retrieval
#endif

DEKAF2_NAMESPACE_BEGIN

// POSIX compliant systems check the following extensions on locales when en_US.UTF-8 is used:
// en_US.UTF-8, en_US.utf8, en_US, en.UTF-8, en.utf8, and en
constexpr KStringViewZ DefaultLocale { "en_US.UTF-8" };
std::atomic<bool> Dekaf::s_bStarted  { false };
std::atomic<bool> Dekaf::s_bShutdown { false };

//---------------------------------------------------------------------------
DEKAF2_ALWAYS_INLINE DEKAF2_PRIVATE
void local_split_in_path_and_name(const KString& sFullPath, KStringView& sPath, KStringViewZ& sName)
//---------------------------------------------------------------------------
{
	// we have to do the separation of path and name manually here as kDirname() and kBasename()
	// would invoke kRFind(), which on non-Linux platforms would call into Dekaf().GetCpuId() and hence
	// into a not yet constructed instance
	auto pos = sFullPath.size();

	while (pos)
	{
		--pos;
		if (sFullPath[pos] == kDirSep)
		{
			++pos;
			break;
		}
	}

	sName = KStringViewZ(sFullPath.data() + pos);

	while (pos > 0 && sFullPath[pos-1] == kDirSep)
	{
		--pos;
	}

	sPath = KStringView(sFullPath.data(), pos);
}

//---------------------------------------------------------------------------
Dekaf::Dekaf()
//---------------------------------------------------------------------------
{
	// do not sync std i/o (cout, cerr, ...)
	std::ios::sync_with_stdio(false);

	SetUnicodeLocale();
	SetRandomSeed();

	local_split_in_path_and_name(kGetOwnPathname(), m_sProgPath, m_sProgName);

	StartDefaultTimer();

	// allow KLog() calls from dekaf now
	m_bInConstruction = false;

	// and mark Dekaf as active
	s_bStarted = true;
}

#ifdef DEKAF2_IS_OSX
//---------------------------------------------------------------------------
DEKAF2_PRIVATE
KString CFStringToKString(CFStringRef Value)
//---------------------------------------------------------------------------
{
	std::array<char, 100> buffer;

	const char* szPtr = CFStringGetCStringPtr(Value, kCFStringEncodingUTF8);

	// depending on platform or locale the fast path of calling
	// CFStringGetCStringPtr() may return null
	if (szPtr == nullptr)
	{
		// in that case call the less optimized CFStringGetCString() - which will
		// cause us two string copies..
		if (CFStringGetCString(Value, buffer.data(), buffer.size(), kCFStringEncodingUTF8))
		{
			szPtr = buffer.data();
		}
	}

	return szPtr;
}
#endif

//---------------------------------------------------------------------------
DEKAF2_PRIVATE
KString GetUserLocale()
//---------------------------------------------------------------------------
{
#ifdef DEKAF2_IS_OSX

	CFLocaleRef Locale  = CFLocaleCopyCurrent();
	CFStringRef Value   = CFLocaleGetIdentifier(Locale);
	KString     sLocale = CFStringToKString(Value);
	CFRelease(Locale);

#else

	KString sLocale = std::locale("").name();

#endif

	return sLocale;
}

//---------------------------------------------------------------------------
DEKAF2_PRIVATE
bool CTypeIsUnicodeAware()
//---------------------------------------------------------------------------
{
	return (std::iswupper(0x190) && std::towupper(0x25b) == 0x190);
}

//---------------------------------------------------------------------------
bool Dekaf::SetUnicodeLocale(KStringViewZ sName)
//---------------------------------------------------------------------------
{
	if (!sName.empty())
	{
		// set to a specific locale, return false if unknown or not unicode
		return kSetGlobalLocale(sName) && CTypeIsUnicodeAware();
	}

	// this is the standard way to set the user locale from the environment
	// or system config
	if (kSetGlobalLocale(std::locale("").name()) && CTypeIsUnicodeAware())
	{
		return true;
	}

#ifdef DEKAF2_IS_OSX

	// Unfortunately the standard way does not work on a MacOS app that
	// is not started from the CLI. Now query the system config through
	// the CoreFramework:
	if (kSetGlobalLocale(GetUserLocale()) && CTypeIsUnicodeAware())
	{
		return true;
	}

#endif

	// try to set our default locale (en_US.UTF-8)
	if (kSetGlobalLocale(DefaultLocale) && CTypeIsUnicodeAware())
	{
		return true;
	}

#ifndef DEKAF2_IS_WINDOWS

	// last resort, slow:
	// we will try to query all installed locales and
	// pick the first one with UTF-8 support..
	if (std::system("locale -a | grep -v -e ^C | grep -m 1 -i -s UTF-*8 > /tmp/dekaf2init.txt") < 0)
	{
		std::cerr << "dekaf2: call to std::system failed when selecting locales" << std::endl;
		return false;
	}

	std::array<char, 100> UnicodeLocale;
	{
		std::ifstream file("/tmp/dekaf2init.txt");
		file.getline(UnicodeLocale.data(), UnicodeLocale.size(), '\n');
		UnicodeLocale[UnicodeLocale.size() - 1] = '\0';
	}

	std::system("rm -f /tmp/dekaf2init.txt");

	if (kSetGlobalLocale(UnicodeLocale.data()) && CTypeIsUnicodeAware())
	{
		return true;
	}

#endif

	std::cerr << "dekaf2: cannot set C++ locale to Unicode" << std::endl;

	return false;
}

//---------------------------------------------------------------------------
void Dekaf::SetRandomSeed()
//---------------------------------------------------------------------------
{
	std::random_device RandDevice;

	m_Random.unique()->seed(RandDevice());

	srand(RandDevice());
#ifndef DEKAF2_IS_WINDOWS
	srand48(RandDevice());
	#ifdef DEKAF2_IS_OSX
	srandomdev();
	#else
	srandom(RandDevice());
	#endif
#endif
}

//-----------------------------------------------------------------------------
uint32_t Dekaf::GetRandomValue(uint32_t iMin, uint32_t iMax)
//-----------------------------------------------------------------------------
{
	std::uniform_int_distribution<uint32_t> uniform_dist(iMin, iMax);
	return uniform_dist(m_Random.unique().get());

} // kRandom

//---------------------------------------------------------------------------
KStringView Dekaf::GetVersionInformation()
//---------------------------------------------------------------------------
{
	constexpr KStringView sVersionInformation =
		"dekaf-" DEKAF_VERSION "-" DEKAF2_BUILD_TYPE
		" " DEKAF2_COMPILER_ID "-" DEKAF2_COMPILER_VERSION
#ifdef DEKAF2_HAS_CPP_23
		" c++23"
#elif DEKAF2_HAS_CPP_20
		" c++20"
#elif DEKAF2_HAS_CPP_17
		" c++17"
#elif DEKAF2_HAS_CPP_14
		" c++14"
#elif DEKAF2_HAS_CPP_11
		" c++11"
#endif
#ifdef DEKAF2_HAS_JEMALLOC
		" jemalloc"
#endif
#ifdef DEKAF2_USE_EXCEPTIONS
		" exceptions"
#endif
#ifdef DEKAF2_USE_OPTIMIZED_STRING_FIND
		" optimized_string_find"
#endif
		" multi_index"
#ifdef DEKAF2_HAS_LIBPROC
		" libproc"
#endif
#ifdef DEKAF2_HAS_MYSQL
		" mysql"
#endif
#ifdef DEKAF2_HAS_FREETDS
		" freetds"
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
#if (DEKAF1_INCLUDE_PATH)
		" with-dekaf1-compatibility"
#endif
#ifdef DEKAF2_HAS_LIBZSTD
		" zstd"
#endif
#ifdef DEKAF2_HAS_LIBBROTLI
		" br"
#endif
#ifdef DEKAF2_HAS_LIBLZMA
		" xz"
#endif
#ifdef DEKAF2_WITH_KLOG
		" KLog"
#endif
#ifdef DEKAF2_LINK_TIME_OPTIMIZATION
		" LTO"
#endif
	;

	return sVersionInformation;
}

//---------------------------------------------------------------------------
KStringView Dekaf::GetVersion()
//---------------------------------------------------------------------------
{
	constexpr KStringView sVersion = "dekaf-" DEKAF_VERSION "-" DEKAF2_BUILD_TYPE ;

	return sVersion;
}

//---------------------------------------------------------------------------
KUnixTime Dekaf::GetCurrentTime() const
//---------------------------------------------------------------------------
{
#if (DEKAF2_IS_GCC && DEKAF2_GCC_VERSION_MAJOR < 10) || \
	(DEKAF2_IS_CLANG && DEKAF2_CLANG_VERSION_MAJOR < 9)
	// GCC 8/9 does not accept an atomic timepoint
	return KUnixTime::now();
#else
	if (DEKAF2_UNLIKELY(!m_Timer))
	{
		return KUnixTime::now();
	}
	else
	{
		return m_iCurrentTime;
	}
#endif
}

//---------------------------------------------------------------------------
void Dekaf::StartDefaultTimer()
//---------------------------------------------------------------------------
{
	if (!m_Timer)
	{
		// make sure we have an initial value set for
		// the time keepers
		m_iCurrentTime = KUnixTime::now();

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
	m_iCurrentTime = tp;

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
#ifndef DEKAF2_IS_WINDOWS

	// we need to stop the thread with the default timer, otherwise
	// parent will not return
	StopDefaultTimer();

	// now try to become a daemon
	detail::kDaemonize();

	// and start the timer again
	StartDefaultTimer();

#else

	kWarning("not supported on Windows");

#endif

}

//---------------------------------------------------------------------------
pid_t Dekaf::Fork()
//---------------------------------------------------------------------------
{
#ifndef DEKAF2_IS_WINDOWS

	pid_t pid;

	bool bRestartTimer = (m_Timer != nullptr);

	if (bRestartTimer)
	{
		// if we would not stop the timer it might just right now be in a locked
		// state, which would block child timers forever
		StopDefaultTimer();
	}

	if ((pid = fork()))
	{
		// parent

		kDebug(2, "new pid: {}", pid);

		// restart the timer if it had been running before
		if (bRestartTimer)
		{
			StartDefaultTimer();
		}

		return pid;
	}

	// child

	// block all signals before we start the timer
	kBlockAllSignals();

	// parent's timer thread is now out of scope,
	// we have to start our own (and before we
	// start the signal thread, because we want
	// the timer to have an empty sigmask)
	if (bRestartTimer)
	{
		m_Timer.reset(); // should be nullptr anyway
		m_OneSecTimers.clear();
		StartDefaultTimer();
	}

	// parent's signal handler thread is now out of scope,
	// we have to start our own
	if (m_Signals)
	{
		m_Signals.reset();
		StartSignalHandlerThread();
	}

	return pid; // 0

#else

	kWarning("not supported on Windows");
	return 0;

#endif

} // Fork

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
KInit::KInit(bool bStartSignalHandlerThread)
//---------------------------------------------------------------------------
{
	if (bStartSignalHandlerThread)
	{
		// it is important to block all signals before
		// instantiating Dekaf(), as otherwise the timer
		// thread would run with an enabled sigmask
		kBlockAllSignals();

		Dekaf::getInstance().StartSignalHandlerThread();
	}

	// make sure KLog is instantiated
	KLog::getInstance();

} // ctor

//---------------------------------------------------------------------------
KInit& KInit::SetName(KStringView sName)
//---------------------------------------------------------------------------
{
	KLog::getInstance().SetName(sName);
	return *this;

} // SetName

//---------------------------------------------------------------------------
KInit& KInit::SetDebugLog(KStringView sDebugLog)
//---------------------------------------------------------------------------
{
	KLog::getInstance().SetDebugLog(sDebugLog);
	return *this;

} // SetDebugLog

//---------------------------------------------------------------------------
KInit& KInit::SetDebugFlag(KStringView sDebugFlag)
//---------------------------------------------------------------------------
{
	KLog::getInstance().SetDebugFlag(sDebugFlag);
	return *this;

} // SetDebugFlag

//---------------------------------------------------------------------------
KInit& KInit::SetLevel(int iLevel)
//---------------------------------------------------------------------------
{
	KLog::getInstance().SetLevel(iLevel);
	return *this;

} // SetDebugFlag

//---------------------------------------------------------------------------
KInit& KInit::SetMultiThreading(bool bYesNo)
//---------------------------------------------------------------------------
{
	Dekaf::getInstance().SetMultiThreading(bYesNo);
	return *this;

} // SetMultiThreading

//---------------------------------------------------------------------------
KInit& KInit::SetOnlyShowCallerOnJsonError(bool bYesNo)
//---------------------------------------------------------------------------
{
	KLog::getInstance().OnlyShowCallerOnJsonError(bYesNo);
	return *this;

} // SetOnlyShowCallerOnJsonError

//---------------------------------------------------------------------------
KInit& KInit::SetLocale(KStringViewZ sLocale)
//---------------------------------------------------------------------------
{
	Dekaf::getInstance().SetUnicodeLocale(sLocale);
	return *this;

} // SetLocale

//---------------------------------------------------------------------------
KInit& KInit::SetUSecMode(bool bYesNo)
//---------------------------------------------------------------------------
{
	KLog::getInstance().SetUSecMode(bYesNo);
	return *this;

} // SetUSecMode

//---------------------------------------------------------------------------
KInit& KInit::KeepCLIMode(bool bYesNo)
//---------------------------------------------------------------------------
{
	KLog::getInstance().KeepCLIMode(bYesNo);
	return *this;

} // KeepCLIMode

//---------------------------------------------------------------------------
void kInit (KStringView sName, KStringViewZ sDebugLog, KStringViewZ sDebugFlag, bool bShouldDumpCore/*=false*/, bool bEnableMultiThreading/*=false*/, bool bStartSignalHandlerThread/*=true*/)
//---------------------------------------------------------------------------
{
	KInit Init(bStartSignalHandlerThread);

	if (bEnableMultiThreading)
	{
		Init.SetMultiThreading();
	}

	if (!sDebugLog.empty())
	{
		Init.SetDebugLog(sDebugLog);
	}

	if (!sDebugFlag.empty())
	{
		Init.SetDebugFlag(sDebugFlag);
	}

	if (!sName.empty())
	{
		Init.SetName(sName);
	}

} // kInit

DEKAF2_NAMESPACE_END
