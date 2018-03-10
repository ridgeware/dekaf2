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
#include <folly/CpuId.h>
#include "kconfiguration.h"
#include "kstring.h"
#include "ktimer.h"
#include "ksignals.h"
#include <vector>
#include <thread>

/// @namespace dekaf2 The basic dekaf2 library namespace. All functions,
/// variables and classes are prefixed with this namespace.
namespace dekaf2
{

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Basic initialization of the library.
class Dekaf
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	Dekaf();
	Dekaf(const Dekaf&) = delete;
	Dekaf(Dekaf&&) = delete;
	Dekaf& operator=(const Dekaf&) = delete;
	Dekaf& operator=(Dekaf&&) = delete;

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
	inline bool GetMultiThreading() const
	//---------------------------------------------------------------------------
	{
		return m_bIsMultiThreading.load(std::memory_order_relaxed);
	}

	//---------------------------------------------------------------------------
	/// Set the unicode locale. If empty defaults to the locale set by the current user.
	bool SetUnicodeLocale(KStringView name = KStringView{});
	//---------------------------------------------------------------------------

	//---------------------------------------------------------------------------
	/// Get the unicode locale.
	const KString& GetUnicodeLocale()
	//---------------------------------------------------------------------------
	{
		return m_sLocale;
	}

	//---------------------------------------------------------------------------
	/// Set random seed. If iSeed == 0 (the default), the current time is used as the seed value.
	void SetRandomSeed(unsigned int iSeed = 0);
	//---------------------------------------------------------------------------

	//---------------------------------------------------------------------------
	/// Get the CPU ID and extensions
	const folly::CpuId& GetCpuId() const
	//---------------------------------------------------------------------------
	{
		return m_CPUID;
	}

	//---------------------------------------------------------------------------
	/// Get the true directory in which the current process binary is located
	const KString& GetProgPath() const
	//---------------------------------------------------------------------------
	{
		return m_sProgPath;
	}

	//---------------------------------------------------------------------------
	/// Get the name of the current process binary without path component
	const KString& GetProgName() const
	//---------------------------------------------------------------------------
	{
		return m_sProgName;
	}

	//---------------------------------------------------------------------------
	/// Get current time without constantly querying the OS
	time_t GetCurrentTime() const
	//---------------------------------------------------------------------------
	{
		return m_iCurrentTime;
	}

	//---------------------------------------------------------------------------
	/// Get current time without constantly querying the OS
	// TODO add updater thread
	KTimer::Timepoint GetCurrentTimepoint() const
	//---------------------------------------------------------------------------
	{
		return m_iCurrentTimepoint;
	}

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
	/// inside the Dekaf2 class because we have to stop the running timer
	/// and restart it again after becoming a daemon. If you have to call this
	/// method please call it as early as possible, best before any file or thread
	/// related activity. But in general let systemd do this for you. Today, handling
	/// daemonization from inside the executable is no more an accepted approach.
	void Daemonize();
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

//----------
private:
//----------

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

	std::atomic_bool m_bIsMultiThreading{false};
	KString m_sLocale;
	KString m_sProgPath;
	KString m_sProgName;
	folly::CpuId m_CPUID;
	std::unique_ptr<KSignals> m_Signals;
	std::unique_ptr<KTimer> m_Timer;
	KTimer::ID_t m_OneSecTimerID;
	std::mutex m_OneSecTimerMutex;
	std::vector<OneSecCallback> m_OneSecTimers;
	// we do not make this an atomic, although it rather should be
	// because we do not want the fence around it and because we
	// trust that the compiler eventually loads a new value in
	// readers
	time_t m_iCurrentTime;
	KTimer::Timepoint m_iCurrentTimepoint;

}; // Dekaf

//---------------------------------------------------------------------------
/// Get unique instance of class Dekaf()
class Dekaf& Dekaf();
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
/// Shortcut to initialize Dekaf and KLog in one call - however, not needed for
/// default settings as those are automatically set at startup
void kInit(KStringView sName = KStringView{},
           KStringView sDebugLog = KStringView{},
           KStringView sDebugFlag = KStringView{},
           bool bShouldDumpCore = false,
           bool bEnableMultiThreading = false,
		   bool bStartSignalHandlerThread = true);
//---------------------------------------------------------------------------

} // end of namespace dekaf2
