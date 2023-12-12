/*
//
// DEKAF(tm): Lighter, Faster, Smarter (tm)
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

/// @file ksignals.h
/// Provides a threaded signal handler framework. Other threads will not receive signals.

#include "kthreadsafe.h"
#include <map>
#include <array>
#include <csignal>

DEKAF2_NAMESPACE_BEGIN

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Provides a threaded signal handler framework. Other threads will not receive signals.
/// Permits to set callbacks for any signal that shall not be ignored. Can
/// start these callbacks in threads of its own, to return as fast as possible
/// from the signal handler.
/// On Windows, this class is much less useful, as Windows only has very limited
/// signal support. Only SIGTERM, SIGSEGV and SIGABRT can be used for real purposes.
class DEKAF2_PUBLIC KSignals
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	/// the type of a handler callback function using std::function
	using std_func_t = std::function<void(int)>;

	/// the type of a handler callback function using a function pointer
	using signal_func_t = void (*)(int);

	//-----------------------------------------------------------------------------
	/// Constructs the signal handler framework.
	/// @param bStartHandlerThread
	/// If false this effectively only blocks signals on the calling
	/// thread (as far as possible), and does nothing else.
	/// If true, in addition to blocking signals on the calling
	/// thread it starts a separate signal handler thread which then
	/// will receive signals and call installed callbacks.
	KSignals(bool bStartHandlerThread = true);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// dtor. Deliberately does not stop the running signal handler to avoid
	/// having it running into a destructed vector.
	~KSignals() = default;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Explicitly sets all signals to be ignored. This basically resets
	/// the signal handling to its state after construction.
	void BlockAllSignals(bool bExceptSEGVandFPE = true);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Sets all signals (that can be set) to call the signal handler in
	/// @a func.
	/// @param func
	/// The signal handler to be called
	/// @param bAsThread
	/// Whether the signal handler will be called in its own thread (true)
	/// or whether the signal's slot shall be used (false)
	void SetAllSignalHandlers(std_func_t func, bool bAsThread = false);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Sets a signal handler for one specific signal.
	/// @param iSignal
	/// The signal for which a signal handler shall be installer
	/// @param func
	/// The signal handler to be called
	/// @param bAsThread
	/// Whether the signal handler will be called in its own thread (true)
	/// or whether the signal's slot shall be used (false)
	void SetCSignalHandler(int iSignal, signal_func_t func, bool bAsThread = false);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Sets a signal handler for one specific signal.
	/// @param iSignal
	/// The signal for which a signal handler shall be installer
	/// @param func
	/// The signal handler to be called
	/// @param bAsThread
	/// Whether the signal handler will be called in its own thread (true)
	/// or whether the signal's slot shall be used (false)
	void SetSignalHandler(int iSignal, std_func_t func, bool bAsThread = false);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Sets one specific signal to be ignored from now on.
	/// @param iSignal
	/// The signal that shall be ignored
	void IgnoreSignalHandler(int iSignal)
	//-----------------------------------------------------------------------------
	{
		IntDelSignalHandler(iSignal, SIG_IGN);
	}

	//-----------------------------------------------------------------------------
	/// Reset signal to default signal handler of KSignals
	void SetDefaultHandler(int iSignal);
	//-----------------------------------------------------------------------------

//----------
private:
//----------

	//-----------------------------------------------------------------------------
	/// only call with func == SIG_IGN or SIG_DFL
	DEKAF2_PRIVATE
	void IntDelSignalHandler(int iSignal, signal_func_t func);
	//-----------------------------------------------------------------------------

#ifndef DEKAF2_IS_WINDOWS
	//-----------------------------------------------------------------------------
	DEKAF2_PRIVATE
	void WaitForSignals();
	//-----------------------------------------------------------------------------
#endif

	//-----------------------------------------------------------------------------
	DEKAF2_PRIVATE
	static void LookupFunc(int signal);
	//-----------------------------------------------------------------------------

	struct DEKAF2_PRIVATE sigmap_t
	{
		std_func_t func;
		bool bAsThread;
	};

#ifdef DEKAF2_IS_WINDOWS
	static const std::array<int, 5> m_SettableSigs;
#else
	static const std::array<int, 11> m_SettableSigs;
#endif

	static KThreadSafe<std::map<int, sigmap_t> > s_SigFuncs;

};

//-----------------------------------------------------------------------------
/// Translate a signal number into a descriptive string.
/// @param iSignalNum
/// The signal number
/// @param bConcise
/// true (default) returns signal name only. false returns a short description
/// of the signal as well.
DEKAF2_PUBLIC
KStringView kTranslateSignal (int iSignalNum, bool bConcise = true);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Block all possible signals for this thread - this is a standalone function
/// that can be applied to any running thread.
/// @param bExceptSEGVandFPE
/// true (default) installs kCrashExit (backtrace) as handler functions for
/// SIGSEGV and SIGFPE.
DEKAF2_PUBLIC
void kBlockAllSignals(bool bExceptSEGVandFPE = true);
//-----------------------------------------------------------------------------

DEKAF2_NAMESPACE_END

