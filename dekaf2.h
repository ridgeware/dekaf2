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

/// @file dekaf2.h
/// basic initialization of the library

#include <atomic>
#include "kconfiguration.h"
#include "kthreadsafe.h"
#ifdef DEKAF2_HAS_MINIFOLLY
#include <folly/CpuId.h>
#endif
#include "kstring.h"
#include "ktime.h"
#include "ktimer.h"
#include "ksignals.h"
#include "ksystem.h"
#include <vector>
#include <thread>
#include <random>

/// @namespace dekaf2 The basic dekaf2 library namespace. All functions,
/// variables and classes are prefixed with this namespace.
namespace dekaf2
{

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Basic initialization of the library.
class DEKAF2_PUBLIC Dekaf
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	//---------------------------------------------------------------------------
	static Dekaf& getInstance()
	//---------------------------------------------------------------------------
	{
		static Dekaf myDekaf;
		return myDekaf;
	}

	Dekaf(const Dekaf&) = delete;
	Dekaf(Dekaf&&) = delete;
	Dekaf& operator=(const Dekaf&) = delete;
	Dekaf& operator=(Dekaf&&) = delete;
	~Dekaf() { s_bShutdown = true; }

	//---------------------------------------------------------------------------
	/// Switch library to multi threaded mode.
	/// Set directly after program start if run in multithreading,
	/// otherwise libs risk to be wrongly initialized when functionality is used
	/// that relies on this setting.
	void SetMultiThreading(bool bIsMultiThreading = true)
	//---------------------------------------------------------------------------
	{
		m_bIsMultiThreading = bIsMultiThreading;
	}

	//---------------------------------------------------------------------------
	/// Shall we be prepared for multithrading?
	bool GetMultiThreading() const
	//---------------------------------------------------------------------------
	{
		return m_bIsMultiThreading.load(std::memory_order_relaxed);
	}

	//---------------------------------------------------------------------------
	/// Set the unicode locale. If empty defaults to the locale set by the current user. If that one is not unicode
	/// aware, we will try to set the locale to en_US.UTF-8, and if not found to any Unicode aware locale
	/// of the system with the primary goal to end up with a locale setting that allows for Unicode aware
	/// std::iswupper() etc calls. This method is called with an empty argument by the constructor of the class.
	bool SetUnicodeLocale(KStringViewZ name = KStringViewZ{});
	//---------------------------------------------------------------------------

	//---------------------------------------------------------------------------
	/// Set random seeds of various random number generators.
	/// If available uses a hardware random device for the seeds.
	void SetRandomSeed();
	//---------------------------------------------------------------------------

	//---------------------------------------------------------------------------
	/// Get a random value in range [iMin - iMax].
	uint32_t GetRandomValue(uint32_t iMin, uint32_t iMax);
	//---------------------------------------------------------------------------


#ifdef DEKAF2_HAS_MINIFOLLY
	//---------------------------------------------------------------------------
	/// Get the CPU ID and extensions
	const folly::CpuId& GetCpuId() const
	//---------------------------------------------------------------------------
	{
		return m_CPUID;
	}
#endif

	//---------------------------------------------------------------------------
	/// Get the true directory in which the current process binary is located
	KStringView GetProgPath() const
	//---------------------------------------------------------------------------
	{
		return m_sProgPath;
	}

	//---------------------------------------------------------------------------
	/// Get the name of the current process binary without path component
	KStringViewZ GetProgName() const
	//---------------------------------------------------------------------------
	{
		return m_sProgName;
	}

	//---------------------------------------------------------------------------
	/// Get the full path name of the current process binary
	const KString& GetProgPathName() const
	//---------------------------------------------------------------------------
	{
		return kGetOwnPathname();
	}

	//---------------------------------------------------------------------------
	/// Get current time without constantly querying the OS
	KUnixTime GetCurrentTime() const;
	//---------------------------------------------------------------------------

	//---------------------------------------------------------------------------
	/// Get current time without constantly querying the OS
	KUnixTime GetCurrentTimepoint() const
	//---------------------------------------------------------------------------
	{
		return GetCurrentTime();
	}

	//---------------------------------------------------------------------------
	/// Get verbose dekaf2 version information (build settings)
	static KStringView GetVersionInformation();
	//---------------------------------------------------------------------------

	//---------------------------------------------------------------------------
	/// Get terse dekaf2 version string
	static KStringView GetVersion();
	//---------------------------------------------------------------------------

	//---------------------------------------------------------------------------
	/// Returns Dekaf's main timing object so you can add more callbacks
	/// without having to maintain another timer object
	KTimer& GetTimer();
	//---------------------------------------------------------------------------

	using OneSecCallback = std::function<void(void)>;
	//---------------------------------------------------------------------------
	/// Add a (fast executing) callback to the general timing loop of
	/// Dekaf. Cannot be removed later.
	bool AddToOneSecTimer(OneSecCallback CB);
	//---------------------------------------------------------------------------

	//---------------------------------------------------------------------------
	/// Make a dekaf2 application a system daemon. We need to run this
	/// inside the Dekaf class because we have to stop the running timer
	/// and restart it again after becoming a daemon. If you have to call this
	/// method please call it as early as possible, best before any file or thread
	/// related activity. But in general let systemd do this for you. Today, handling
	/// daemonization from inside the executable is no more an accepted approach.
	void Daemonize();
	//---------------------------------------------------------------------------

	//---------------------------------------------------------------------------
	/// Fork a dekaf2 application. We need to run this inside the Dekaf2 class
	/// because we have to start threads like those for timer and signal again
	/// after forking. Also, we need to reset some of Dekaf's member variables.
	/// @return returns the child pid to the parent, and 0 to the child. A negative
	/// result is an error that should be retrieved with strerror(errno).
	pid_t Fork();
	//---------------------------------------------------------------------------

	//---------------------------------------------------------------------------
	/// Returns a pointer to the KSignals class, or a nullptr if the signal handler
	/// thread had not been started. Can be used to set or remove handlers.
	KSignals* Signals()
	//---------------------------------------------------------------------------
	{
		return m_Signals.get();
	}

	//---------------------------------------------------------------------------
	/// Start a dedicated signal handler thread that will accept all signals.
	/// Set handlers for the signals through Signals()
	void StartSignalHandlerThread();
	//---------------------------------------------------------------------------

	//---------------------------------------------------------------------------
	/// Prepares for process shutdown by stopping the timer object. If bImmediately
	/// is false may wait up to one second for the timer to join.
	void ShutDown(bool bImmediately = true);
	//---------------------------------------------------------------------------

	//---------------------------------------------------------------------------
	/// Check if dekaf is started / constructed
	static bool IsStarted()
	//---------------------------------------------------------------------------
	{
		return s_bStarted;
	}

	//---------------------------------------------------------------------------
	/// Check if dekaf is shutdown / deconstructed
	static bool IsShutDown()
	//---------------------------------------------------------------------------
	{
		return s_bShutdown;
	}

//----------
private:
//----------

	/// private constructor
	Dekaf();

	//---------------------------------------------------------------------------
	/// The core timer for dekaf, called every second
	void OneSecTimer(KTimer::Timepoint tp);
	//---------------------------------------------------------------------------

	//---------------------------------------------------------------------------
	void StartDefaultTimer();
	//---------------------------------------------------------------------------

	//---------------------------------------------------------------------------
	void StopDefaultTimer();
	//---------------------------------------------------------------------------

	std::atomic<bool> m_bIsMultiThreading { false };
	KStringView m_sProgPath;
	KStringViewZ m_sProgName;
#ifdef DEKAF2_HAS_MINIFOLLY
	folly::CpuId m_CPUID;
#endif
	std::unique_ptr<KSignals> m_Signals;
	std::unique_ptr<KTimer> m_Timer;
	KTimer::ID_t m_OneSecTimerID;
	std::mutex m_OneSecTimerMutex;
	std::vector<OneSecCallback> m_OneSecTimers;
	std::atomic<KUnixTime> m_iCurrentTime;
	KThreadSafe<std::default_random_engine> m_Random;
	bool m_bInConstruction { true };
	static std::atomic<bool> s_bStarted;
	static std::atomic<bool> s_bShutdown;

}; // Dekaf

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Helper class to initialize Dekaf and KLog with named parameters - just an
/// alternative to kInit with its anonymous parms..
class DEKAF2_PUBLIC KInit
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	using self = KInit;

	/// Start with own signal handler thread? Advisable for servers..
	KInit(bool bStartSignalHandlerThread = true);

	self& SetName(KStringView sName);
	self& SetDebugLog(KStringView sDebugLog);
	self& SetDebugFlag(KStringView sDebugFlag);
	self& SetMultiThreading(bool bYesNo = true);
	self& SetOnlyShowCallerOnJsonError(bool bYesNo = true);
	self& SetLevel(int iLevel);
	self& SetLocale(KStringViewZ sLocale = "en_US.UTF-8");

}; // KInit

// use like:
// KInit(true).SetName("MyName").SetMultiThreading();

//---------------------------------------------------------------------------
/// Shortcut to initialize Dekaf and KLog in one call - however, not needed for
/// default settings as those are automatically set at startup
DEKAF2_PUBLIC
void kInit(KStringView sName = KStringView{},
           KStringViewZ sDebugLog = KStringViewZ{},
           KStringViewZ sDebugFlag = KStringViewZ{},
           bool bShouldDumpCore = false,
           bool bEnableMultiThreading = false,
		   bool bStartSignalHandlerThread = true);
//---------------------------------------------------------------------------

} // end of namespace dekaf2
