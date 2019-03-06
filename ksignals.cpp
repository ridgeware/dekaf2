/*
//-----------------------------------------------------------------------------//
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
// For documentation, try: http://www.ridgeware.com/home/dekaf/
//
//-----------------------------------------------------------------------------//
*/

#include "ksignals.h"
#include "bits/kcppcompat.h"
#include "kcrashexit.h"
#include "kparallel.h"
#include "klog.h"
#include <chrono>
#include <thread>
#ifndef DEKAF2_IS_WINDOWS
	#include <pthread.h>
#endif

namespace dekaf2
{

//-----------------------------------------------------------------------------
void kBlockAllSignals(bool bExceptSEGVandFPE)
//-----------------------------------------------------------------------------
{
#ifdef DEKAF2_IS_WINDOWS
	signal(SIGINT,  SIG_IGN);
	signal(SIGTERM, SIG_IGN);
#else
	sigset_t signal_set;
	sigfillset(&signal_set);
	pthread_sigmask(SIG_BLOCK, &signal_set, nullptr);
#endif

	if (bExceptSEGVandFPE)
	{
		signal(SIGSEGV, kCrashExit);
		signal(SIGFPE,  kCrashExit);
		signal(SIGILL,  kCrashExit);
	}

} // kBlockAllSignals

//-----------------------------------------------------------------------------
void KSignals::BlockAllSignals(bool bExceptSEGVandFPE)
//-----------------------------------------------------------------------------
{
	kBlockAllSignals(bExceptSEGVandFPE);

	std::lock_guard<std::mutex> Lock(s_SigSetMutex);
	s_SigFuncs.clear();

} // BlockAllSignals

#ifndef DEKAF2_IS_WINDOWS
//-----------------------------------------------------------------------------
void KSignals::WaitForSignals()
//-----------------------------------------------------------------------------
{
	// this is the thread that waits for signals
	// first set up the default handler

	int sig;

	sigset_t signal_set;
	sigfillset(&signal_set);
	sigdelset(&signal_set, SIGSEGV);
	sigdelset(&signal_set, SIGFPE);
	sigdelset(&signal_set, SIGILL);

	for (;;)
	{
		// wait for all signals
		sigwait(&signal_set, &sig);

		// we received a signal
		kDebug(3, "received signal {}", kTranslateSignal(sig));

		// check if we have a function to call for
		LookupFunc(sig);
	}

} // WaitForSignals
#endif

//-----------------------------------------------------------------------------
void KSignals::LookupFunc(int signal)
//-----------------------------------------------------------------------------
{
	sigmap_t callable;

	{
		std::lock_guard<std::mutex> Lock(s_SigSetMutex);
		auto it = s_SigFuncs.find(signal);
		if (it == s_SigFuncs.end())
		{
			// signal function not found.
			return;
		}
		callable = it->second;
	}

	if (callable.bAsThread)
	{
		m_Threads.CreateOne([&]()
		{
			callable.func(signal);
		});
	}
	else
	{
		callable.func(signal);
	}

} // LookupFunc

//-----------------------------------------------------------------------------
void KSignals::IntDelSignalHandler(int iSignal, signal_func_t func)
//-----------------------------------------------------------------------------
{
	{
		std::lock_guard<std::mutex> Lock(s_SigSetMutex);
		s_SigFuncs.erase(iSignal);
	}

} // IntDelSignalHandler

//-----------------------------------------------------------------------------
void KSignals::SetSignalHandler(int iSignal, std_func_t func, bool bAsThread)
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(s_SigSetMutex);
	s_SigFuncs[iSignal] = {func, bAsThread};

} // SetSignalHandler

//-----------------------------------------------------------------------------
void KSignals::SetCSignalHandler(int iSignal, signal_func_t func, bool bAsThread)
//-----------------------------------------------------------------------------
{
	if (func == SIG_IGN || func == SIG_DFL)
	{
		IntDelSignalHandler(iSignal, func);
	}
	else
	{
		std::lock_guard<std::mutex> Lock(s_SigSetMutex);
		s_SigFuncs[iSignal] = {func, bAsThread};
	}

} // SetCSignalHandler

//-----------------------------------------------------------------------------
void KSignals::SetAllSignalHandlers(std_func_t func, bool bAsThread)
//-----------------------------------------------------------------------------
{
	for (auto it : m_SettableSigs)
	{
		SetSignalHandler(it, func, bAsThread);
	}

} // SetAllSignalHandlers

//-----------------------------------------------------------------------------
KSignals::KSignals(bool bStartHandlerThread)
//-----------------------------------------------------------------------------
{
	m_Threads.StartDetached();

	if (bStartHandlerThread)
	{
		BlockAllSignals();

		// terminate gracefully on SIGINT and SIGTERM
		SetSignalHandler(SIGINT,  [](int){ std::exit(0); });
		SetSignalHandler(SIGTERM, [](int){ std::exit(0); });

#ifdef DEKAF2_IS_WINDOWS
		// On Windows place signal handlers for the settable signals
		// that simply call our lookup function
		for (auto it : m_SettableSigs)
		{
			signal(it, [](int signal){ LookupFunc(signal); } );
		}
#else
		// On Unix systems start handler thread
		m_Threads.CreateOne(&KSignals::WaitForSignals, this);
#endif
	}

} // ctor

//-----------------------------------------------------------------------------
KSignals::~KSignals()
//-----------------------------------------------------------------------------
{
	// clear the map if existing, so the signal thread
	// cannot run into a destructing vector
	std::lock_guard<std::mutex> Lock(s_SigSetMutex);
	s_SigFuncs.clear();
	// no need to clear s_SigFuncs - it is actually even
	// better to keep it hanging around as the signal
	// handling for plain function pointers also continues
	// to work after destruction..

} // dtor

std::mutex KSignals::s_SigSetMutex;
std::map<int, KSignals::sigmap_t> KSignals::s_SigFuncs;
KRunThreads KSignals::m_Threads;

const std::array<int,
#ifdef DEKAF2_IS_WINDOWS
                 5
#else
                 11
#endif
                > KSignals::m_SettableSigs
{
	{
		SIGINT,
		SIGTERM,
		SIGILL,
		SIGFPE,
		SIGSEGV,
#ifndef DEKAF2_IS_WINDOWS
		SIGQUIT,
		SIGPIPE,
		SIGHUP,
		SIGUSR1,
		SIGUSR2,
		SIGBUS
#endif
	}
};

//-----------------------------------------------------------------------------
const char* kTranslateSignal (int iSignalNum, bool bConcise/*=TRUE*/)
//-----------------------------------------------------------------------------
{
	switch (iSignalNum)
	{
	case 0:
		return ("");

	#ifdef DEKAF2_IS_WINDOWS
		// taken from WINCON.H:
		// #define CTRL_C_EVENT        0
		// #define CTRL_BREAK_EVENT    1
		// #define CTRL_CLOSE_EVENT    2
		// // 3 is reserved!
		// // 4 is reserved!
		// #define CTRL_LOGOFF_EVENT   5
		// #define CTRL_SHUTDOWN_EVENT 6

		// We have to add an offset to them so that
		// we can distinguish between 0 (internal crash detection)
		// and CTRL_BREAK_EVENT (which windows defines as 0):
		case 1000 + /* CTRL_BREAK_EVENT */ 1:
			return (bConcise ? "CTRL_BREAK_EVENT" : "CTRL_BREAK_EVENT: win32 interrupt");
		case 1000 + /* CTRL_C_EVENT */ 0:
			return (bConcise ? "CTRL_C_EVENT"     : "CTRL_C_EVENT: win32 SERVICE_CONTROL_STOP interrupt");
	#endif

		#ifdef SIGHUP
		case SIGHUP:
			return (bConcise ? "SIGHUP"  : "SIGHUP: Hangup (POSIX).");
		#endif

		#ifdef SIGINT
		case SIGINT:
			return (bConcise ? "SIGINT"  : "SIGINT: Interrupt (ANSI).");
		#endif

		#ifdef SIGQUIT
		case SIGQUIT:
			return (bConcise ? "SIGQUIT" : "SIGQUIT: Quit (POSIX).");
		#endif

		#ifdef SIGILL
		case SIGILL:
			return (bConcise ? "SIGILL"  : "SIGILL: Illegal instruction (ANSI).");
		#endif

		#ifdef SIGTRAP
		case SIGTRAP:
			return (bConcise ? "SIGTRAP" : "SIGTRAP: trace trap (not reset when caught).");
		#endif

		#ifdef SIGABRT
		case SIGABRT: // aka SIGIOT
			return (bConcise ? "SIGABRT" : "SIGABRT: used by abort, replace SIGIOT in the future.");
		#endif

		#ifdef SIGEMT
		case SIGEMT:
			return (bConcise ? "SIGEMT" : "SIGEMT: EMT instruction.");
		#endif

		#ifdef SIGFPE
		case SIGFPE:
			return (bConcise ? "SIGFPE"  : "SIGFPE: Floating-point exception (ANSI).");
		#endif

		#ifdef SIGKILL
		case SIGKILL:
			return (bConcise ? "SIGKILL" : "SIGKILL: kill (cannot be caught or ignored).");
		#endif

		#ifdef SIGBUS
		case SIGBUS:
			return (bConcise ? "SIGBUS"  : "SIGBUS: BUS error (4.2 BSD).");
		#endif

		#ifdef SIGSEGV
		case SIGSEGV:
			return (bConcise ? "SIGSEGV" : "SIGSEGV: Segmentation violation (ANSI).");
		#endif

		#ifdef SIGSYS
		case SIGSYS:
			return (bConcise ? "SIGSYS" : "SIGSYS: bad argument to system call.");
		#endif

		#ifdef SIGPIPE
		case SIGPIPE:
			return (bConcise ? "SIGPIPE" : "SIGPIPE: Broken pipe (POSIX).");
		#endif

		#ifdef SIGALRM
		case SIGALRM:
			return (bConcise ? "SIGALRM" : "SIGALRM: alarm clock.");
		#endif

		#ifdef SIGTERM
		case SIGTERM:
			return (bConcise ? "SIGTERM" : "SIGTERM: Termination (ANSI).");
		#endif

		#ifdef SIGUSR1
		case SIGUSR1:
			return (bConcise ? "SIGUSR1" : "SIGUSR1: user signal 1");
		#endif

		#ifdef SIGUSR2
		case SIGUSR2:
			return (bConcise ? "SIGUSR2" : "SIGUSR2: user signal 2");
		#endif

		#ifdef SIGCHLD
		case SIGCHLD: // aka SIGCLD
			return (bConcise ? "SIGCHLD" : "SIGCHLD: child status change alias (POSIX).");
		#endif

		#ifdef SIGPWR
		case SIGPWR:
			return (bConcise ? "SIGPWR" : "SIGPWR: power-fail restart.");
		#endif

		#ifdef SIGWINCH
		case SIGWINCH:
			return (bConcise ? "SIGWINCH" : "SIGWINCH: window size change.");
		#endif

		#ifdef SIGURG
		case SIGURG:
			return (bConcise ? "SIGURG" : "SIGURG: urgent socket condition.");
		#endif

		#ifdef SIGIO
		case SIGIO: // aka SIGPOLL:
			return (bConcise ? "SIGIO" : "SIGIO: socket I/O possible (SIGPOLL alias).");
		#endif

		#ifdef SIGSTOP
		case SIGSTOP:
			return (bConcise ? "SIGSTOP" : "SIGSTOP: stop (cannot be caught or ignored).");
		#endif

		#ifdef SIGTSTP
		case SIGTSTP:
			return (bConcise ? "SIGTSTP" : "SIGTSTP: user stop requested from tty.");
		#endif

		#ifdef SIGCONT
		case SIGCONT:
			return (bConcise ? "SIGCONT" : "SIGCONT: stopped process has been continued.");
		#endif

		#ifdef SIGTTIN
		case SIGTTIN:
			return (bConcise ? "SIGTTIN" : "SIGTTIN: background tty read attempted.");
		#endif

		#ifdef SIGTTOU
		case SIGTTOU:
			return (bConcise ? "SIGTTOU" : "SIGTTOU: background tty write attempted.");
		#endif

		#ifdef SIGVTALRM
		case SIGVTALRM:
			return (bConcise ? "SIGVTALRM" : "SIGVTALRM: virtual timer expired.");
		#endif

		#ifdef SIGPROF
		case SIGPROF:
			return (bConcise ? "SIGPROF" : "SIGPROF: profiling timer expired.");
		#endif

		#ifdef SIGXCPU
		case SIGXCPU:
			return (bConcise ? "SIGXCPU" : "SIGXCPU: exceeded cpu limit.");
		#endif

		#ifdef SIGXFSZ
		case SIGXFSZ:
			return (bConcise ? "SIGXFSZ" : "SIGXFSZ: exceeded file size limit.");
		#endif

		#ifdef SIGWAITING
		case SIGWAITING:
			return (bConcise ? "SIGWAITING" : "SIGWAITING: process's lwps are blocked.");
		#endif

		#ifdef SIGLWP
		case SIGLWP:
			return (bConcise ? "SIGLWP" : "SIGLWP: special signal used by thread library.");
		#endif

		#ifdef SIGFREEZE
		case SIGFREEZE:
			return (bConcise ? "SIGFREEZE" : "SIGFREEZE: special signal used by CPR.");
		#endif

		#ifdef SIGTHAW
		case SIGTHAW:
			return (bConcise ? "SIGTHAW" : "SIGTHAW: special signal used by CPR.");
		#endif

		#ifdef SIGCANCEL
		case SIGCANCEL:
			return (bConcise ? "SIGCANCEL" : "SIGCANCEL: thread cancellation signal used by libthread.");
		#endif

		#ifdef SIGLOST
		case SIGLOST:
			return (bConcise ? "SIGLOST" : "SIGLOST: resource lost (eg, record-lock lost).");
		#endif

	default:
		return ("UNKNOWN");
	}

} // kTranslateSignal

} // end of namespace dekaf2

