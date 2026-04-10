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

#include <dekaf2/core/types/kcompatibility.h>
#include "kconfiguration.h"
#include <dekaf2/threading/primitives/kthreadsafe.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/time/clock/ktime.h>
#include <dekaf2/time/duration/ktimer.h>
#include <dekaf2/system/os/ksignals.h>
#include <dekaf2/system/os/ksystem.h>
#include <atomic>

/// @namespace dekaf2 The basic dekaf2 library namespace. All functions,
/// variables and classes are prefixed with this namespace.

DEKAF2_NAMESPACE_BEGIN

/// @addtogroup core_init
/// @{

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
	~Dekaf() { s_bShutdown = true; ShutDown(); }

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
	/// DEPRECATED
	/// Get current time without constantly querying the OS
	/// This is no more necessary with moden OSs all using vDSO access and the hardware timer, no kernel switch.
	/// New code should use KUnixTime::now() directly
	static KUnixTime GetCurrentTime()
	//---------------------------------------------------------------------------
	{
		return KUnixTime::now();
	}

#if DEKAF2_IS_WINDOWS && !defined(GetCurrentTime)
	// Windows has a #define GetCurrentTime() GetTickCount() in WinBase.h ..
	inline static
	KUnixTime GetTickCount() const { return (GetCurrentTime()); }
#endif

	//---------------------------------------------------------------------------
	/// DEPRECATED
	/// Get current time without constantly querying the OS
	/// see above, no more needed.
	/// New code should use KUnixTime::now() directly
	static KUnixTime GetCurrentTimepoint()
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
	/// Because threads are not copied on fork(), but may have set (then abandoned)
	/// mutexes, only ever Fork() when no other threads are started. This Fork() only
	/// cares for dekaf2's own signal and timer threads.
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
	/// Prepares for process shutdown by stopping the timer object.
	/// @param bImmediately is currently unused
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
	void StartDefaultTimer();
	//---------------------------------------------------------------------------

	//---------------------------------------------------------------------------
	void StopDefaultTimer();
	//---------------------------------------------------------------------------

	std::atomic<bool> m_bIsMultiThreading { false };
	KStringView m_sProgPath;
	KStringViewZ m_sProgName;
	std::unique_ptr<KSignals> m_Signals;
	std::unique_ptr<KTimer> m_Timer;
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

	/// set log name for the executable, will be limited to 5 chars (uppercased)
	self& SetName(KStringView sName);
	/// set debug log file name, may be stdout, stderr, or a URL
	self& SetDebugLog(KStringView sDebugLog);
	/// set file name for the debug flag file which controls the debug output in server mode
	self& SetDebugFlag(KStringView sDebugFlag);
	/// will the executable run in multithreading?
	self& SetMultiThreading(bool bYesNo = true);
	/// do not show a stack trace for JSON errors?
	self& SetOnlyShowCallerOnJsonError(bool bYesNo = true);
	/// set debug level
	self& SetLevel(int iLevel);
	/// set system locale (used for C functions, defaults to en_US.UTF-8)
	self& SetLocale(KStringViewZ sLocale = "en_US.UTF-8");
	/// keep CLI debug mode even after starting a REST server? advisable for docker
	self& KeepCLIMode(bool bYesNo = true);
	/// output debug log with microseconds resolution?
	self& SetUSecMode(bool bYesNo = true);

}; // KInit

// use like:
// KInit(true).SetName("MyName").SetMultiThreading();

//---------------------------------------------------------------------------
/// Shortcut to initialize Dekaf and KLog in one call - however, not needed for
/// default settings as those are automatically set at startup. New code should
/// use KInit() above for named parameters.
DEKAF2_PUBLIC
void kInit(KStringView sName = KStringView{},
           KStringViewZ sDebugLog = KStringViewZ{},
           KStringViewZ sDebugFlag = KStringViewZ{},
           bool bShouldDumpCore = false,
           bool bEnableMultiThreading = false,
		   bool bStartSignalHandlerThread = true);
//---------------------------------------------------------------------------


/// @}

DEKAF2_NAMESPACE_END
