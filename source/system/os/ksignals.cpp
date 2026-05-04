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

#include <dekaf2/system/os/ksignals.h>
#include <dekaf2/core/init/kcompatibility.h>
#include <dekaf2/core/errors/kcrashexit.h>
#include <dekaf2/threading/execution/kparallel.h>
#include <dekaf2/core/logging/klog.h>
#include <chrono>
#include <thread>
#ifndef DEKAF2_IS_WINDOWS
	#include <pthread.h>
#endif
#include <cstdlib>
#include <memory>
#include <mutex>
#include <atomic>

DEKAF2_NAMESPACE_BEGIN

namespace {

// process-wide flag to ensure crash handlers are installed only once.
// (used on every platform - protects against duplicate installs from worker
// threads, e.g. the KTimer thread re-installing process-global Windows
// signal() handlers after KSignals already overrode them with LookupFunc.)
std::once_flag s_CrashHandlersOnce;

// process-wide signal that crash handlers have been installed; checked by
// kSetupThreadSignalHandling() to decide whether the per-thread setup
// (signal mask + alt stack) is useful at all.
std::atomic<bool> s_bCrashHandlersInstalled { false };

#ifndef DEKAF2_IS_WINDOWS

// alternate signal stack for crash handlers (one per thread).
// MINSIGSTKSZ is sufficient for our crash handler (32k on macOS, ~2k on Linux).
// Allocated lazily when kSetupThreadSignalHandling() is called on a thread,
// but only if crash handlers are already installed - otherwise SA_ONSTACK is
// not in effect and the alt stack would never be used.
thread_local std::unique_ptr<std::array<char, MINSIGSTKSZ>> s_AltStack;

#endif

} // anonymous namespace

//-----------------------------------------------------------------------------
void kInstallCrashHandlers()
//-----------------------------------------------------------------------------
{
	std::call_once(s_CrashHandlersOnce, []()
	{
#ifdef DEKAF2_IS_WINDOWS
		signal(SIGSEGV, kCrashExit);
		signal(SIGFPE,  kCrashExit);
		signal(SIGILL,  kCrashExit);
		// SIGTRAP is explicitly left alone - debugger uses it for breakpoints
#else
		struct sigaction sa;
		// SA_RESETHAND = one-shot (reset to default after first call)
		// SA_ONSTACK   = use alternate stack (prevents stack overflow in handler)
		sa.sa_flags = SA_SIGINFO | SA_RESETHAND | SA_ONSTACK;
		sa.sa_sigaction = kCrashExitExt;
		sigemptyset(&sa.sa_mask);

		sigaction(SIGSEGV, &sa, nullptr);
		sigaction(SIGFPE,  &sa, nullptr);
		sigaction(SIGILL,  &sa, nullptr);
		sigaction(SIGBUS,  &sa, nullptr);
		// SIGTRAP is explicitly left alone - debugger uses it for breakpoints
#endif

		// publish state for kSetupThreadSignalHandling()
		s_bCrashHandlersInstalled.store(true, std::memory_order_release);
	});

} // kInstallCrashHandlers

//-----------------------------------------------------------------------------
void kSetupThreadSignalHandling(bool bExceptSEGVandFPE)
//-----------------------------------------------------------------------------
{
#ifndef DEKAF2_IS_WINDOWS
	// If no crash handlers were installed for this process, do nothing here.
	// Otherwise we would silently block all signals on this thread (which is
	// surprising for users who don't use KInit but do use KThreads or
	// KThreadPool, and would break their own signal handling).
	if (!s_bCrashHandlersInstalled.load(std::memory_order_acquire))
	{
		return;
	}

	sigset_t signal_set;
	sigfillset(&signal_set);

	if (bExceptSEGVandFPE)
	{
		sigdelset(&signal_set, SIGSEGV);
		sigdelset(&signal_set, SIGFPE);
		sigdelset(&signal_set, SIGILL);
		sigdelset(&signal_set, SIGBUS); // Windows does not have SIGBUS
	}

	pthread_sigmask(SIG_BLOCK, &signal_set, nullptr); // sigprocmask would set it for all threads

	if (bExceptSEGVandFPE)
	{
		// Allocate alt stack for this thread (stack overflow protection in
		// the handler). sigaltstack() is per-thread and not inherited across
		// pthread_create(), so each thread sets up its own.
		if (!s_AltStack)
		{
			s_AltStack = std::make_unique<std::array<char, MINSIGSTKSZ>>();
			stack_t ss = { s_AltStack->data(), 0, static_cast<int>(s_AltStack->size()) };
			sigaltstack(&ss, nullptr);
		}
	}
#endif

} // kSetupThreadSignalHandling

//-----------------------------------------------------------------------------
void kBlockAllSignals(bool bExceptSEGVandFPE)
//-----------------------------------------------------------------------------
{
	// Compatibility wrapper: existing callers expect that calling this on the
	// main thread also installs crash handlers (this is what the old
	// monolithic kBlockAllSignals() did, and what dekaf2 init relies on).
	//
	// Note: historically this function called signal(SIGINT, SIG_IGN) and
	// signal(SIGTERM, SIG_IGN) on Windows. That was conceptually wrong: on
	// Windows signal() is process-global (not per-thread), so any worker
	// thread (e.g. the KTimer thread) calling kBlockAllSignals() would race
	// with the KSignals ctor that installs the LookupFunc handler. If the
	// worker thread won the race the user's CLI silently ignored Ctrl-C.
	// We now leave SIGINT/SIGTERM alone here and let KSignals install the
	// handlers exactly once, on the main thread.
	if (bExceptSEGVandFPE)
	{
		kInstallCrashHandlers();
	}

	kSetupThreadSignalHandling(bExceptSEGVandFPE);

} // kBlockAllSignals

//-----------------------------------------------------------------------------
void KSignals::BlockAllSignals(bool bExceptSEGVandFPE)
//-----------------------------------------------------------------------------
{
	kBlockAllSignals(bExceptSEGVandFPE);

	s_SigFuncs.unique()->clear();

} // BlockAllSignals

#ifndef DEKAF2_IS_WINDOWS
//-----------------------------------------------------------------------------
void KSignals::WaitForSignals()
//-----------------------------------------------------------------------------
{
	// this is the thread that waits for signals
	// first set up the default handler
	kDebug(2, "new signal handler thread started");

	int sig;
#ifndef DEKAF2_IS_OSX
	siginfo_t siginfo;
#endif

	sigset_t signal_set;
	sigfillset(&signal_set);
	sigdelset(&signal_set, SIGSEGV);
	sigdelset(&signal_set, SIGFPE);
	sigdelset(&signal_set, SIGILL);
	sigdelset(&signal_set, SIGBUS);

	for (;;)
	{
		// wait for all signals
#ifdef DEKAF2_IS_OSX
		sigwait(&signal_set, &sig);
#else
		sig = sigwaitinfo(&signal_set, &siginfo);
#endif

		if (sig > 0)
		{
			// check that we are neither in init or exit
			if (KLog::getInstance().Available())
			{
				// we received a signal
				kDebug(2, "received signal {} ({})", kTranslateSignal(sig), sig);

				// check if we have a function to call for
				LookupFunc(sig);
			}
		}
	}

} // WaitForSignals
#endif

//-----------------------------------------------------------------------------
void KSignals::LookupFunc(int iSignal)
//-----------------------------------------------------------------------------
{
	sigmap_t callable;

	{
		auto SigFuncs = s_SigFuncs.shared();
		auto it = SigFuncs->find(iSignal);
		if (it == SigFuncs->end())
		{
			// signal function not found.
			kDebug(2, "no handler for {}", kTranslateSignal(iSignal));
			return;
		}
		callable = it->second;
	}

	if (callable.bAsThread)
	{
		std::thread([&]()
		{
			kDebug(2, "calling handler for {} {} thread", kTranslateSignal(iSignal), "as separate");
			callable.func(iSignal);

		}).detach();
	}
	else
	{
		kDebug(2, "calling handler for {} {} thread", kTranslateSignal(iSignal), "in signal handler");
		callable.func(iSignal);
	}

} // LookupFunc

//-----------------------------------------------------------------------------
void KSignals::IntDelSignalHandler(int iSignal, signal_func_t func)
//-----------------------------------------------------------------------------
{
	kDebug(2, "removing handler for {}", kTranslateSignal(iSignal));

	s_SigFuncs.unique()->erase(iSignal);

} // IntDelSignalHandler

//-----------------------------------------------------------------------------
void KSignals::SetSignalHandler(int iSignal, std_func_t func, bool bAsThread)
//-----------------------------------------------------------------------------
{
	kDebug(2, "registering handler for {}", kTranslateSignal(iSignal));

	s_SigFuncs.unique().get()[iSignal] = { std::move(func), bAsThread, /*bIsUserHandler*/ true };

} // SetSignalHandler

//-----------------------------------------------------------------------------
KSignals::std_func_t KSignals::GetSignalHandler(int iSignal) const
//-----------------------------------------------------------------------------
{
	auto SigFuncs = s_SigFuncs.shared();
	auto it = SigFuncs->find(iSignal);
	if (it == SigFuncs->end() || !it->second.bIsUserHandler)
	{
		return std_func_t{};
	}
	return it->second.func;

} // GetSignalHandler

//-----------------------------------------------------------------------------
void KSignals::SetCSignalHandler(int iSignal, signal_func_t func, bool bAsThread)
//-----------------------------------------------------------------------------
{
	kDebug(2, "registering handler for {}", kTranslateSignal(iSignal));

	if (func == SIG_IGN || func == SIG_DFL)
	{
		IntDelSignalHandler(iSignal, func);
	}
	else
	{
		s_SigFuncs.unique().get()[iSignal] = { func, bAsThread, /*bIsUserHandler*/ true };
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
void KSignals::SetDefaultHandler(int iSignal)
//-----------------------------------------------------------------------------
{
	auto DefaultHandler = [](int iSignal)
	{
		kDebug(2, "dekaf2 default handler for {} called, now calling exit({})", kTranslateSignal(iSignal), EXIT_SUCCESS);
		std::exit(EXIT_SUCCESS);
	};

	switch (iSignal)
	{
		case SIGINT:
		case SIGTERM:
			kDebug(2, "setting {} default handler for {}", "dekaf2", kTranslateSignal(iSignal));
			// install directly as non-user handler so that GetSignalHandler does
			// not return it to chained shutdown handlers (which would otherwise
			// trigger exit(0) before the actual shutdown code runs)
			s_SigFuncs.unique().get()[iSignal] = { std::move(DefaultHandler), false, /*bIsUserHandler*/ false };
			break;

		default:
			kDebug(2, "setting {} default handler for {}", "system", kTranslateSignal(iSignal));
			IntDelSignalHandler(iSignal, SIG_DFL);
			break;
	}

} // SetDekaf2DefaultHandler

//-----------------------------------------------------------------------------
KSignals::KSignals(bool bStartHandlerThread)
//-----------------------------------------------------------------------------
{
	if (bStartHandlerThread)
	{
		BlockAllSignals();

		// terminate gracefully on SIGINT and SIGTERM
		SetDefaultHandler(SIGINT);
		SetDefaultHandler(SIGTERM);

#ifdef DEKAF2_IS_WINDOWS
		// On Windows place signal handlers for the settable signals
		// that simply call our lookup function
		for (auto it : m_SettableSigs)
		{
			signal(it, [](int signal){ LookupFunc(signal); } );
		}
#else
		// On Unix systems start handler thread
		std::thread(&KSignals::WaitForSignals, this).detach();
#endif
	}

} // ctor

KThreadSafe<std::map<int, KSignals::sigmap_t> > KSignals::s_SigFuncs;

constexpr std::array<int,
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
KStringView kTranslateSignal (int iSignalNum, bool bConcise/*=TRUE*/)
//-----------------------------------------------------------------------------
{
	KStringView sReturn;

	switch (iSignalNum)
	{
	case 0:
		return sReturn;

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
			sReturn = "CTRL_BREAK_EVENT: win32 interrupt"_ksv;
			break;

		case 1000 + /* CTRL_C_EVENT */ 0:
			sReturn = "CTRL_C_EVENT: win32 SERVICE_CONTROL_STOP interrupt"_ksv;
			break;
	#endif

		#ifdef SIGHUP
		case SIGHUP:
			sReturn = "SIGHUP: hangup"_ksv;
			break;
		#endif

		#ifdef SIGINT
		case SIGINT:
			sReturn = "SIGINT: interrupt from keyboard"_ksv;
			break;
		#endif

		#ifdef SIGQUIT
		case SIGQUIT:
			sReturn = "SIGQUIT: quit"_ksv;
			break;
		#endif

		#ifdef SIGILL
		case SIGILL:
			sReturn = "SIGILL: illegal instruction"_ksv;
			break;
		#endif

		#ifdef SIGTRAP
		case SIGTRAP:
			sReturn = "SIGTRAP: trace trap"_ksv;
			break;
		#endif

		#ifdef SIGABRT
		case SIGABRT:
			sReturn = "SIGABRT: abort"_ksv;
			break;
		#endif

		#if defined(SIGIOT) && SIGIOT != SIGABRT
		case SIGIOT:
			sReturn = "SIGIOT: abort"_ksv;
			break;
		#endif

		#ifdef SIGEMT
		case SIGEMT:
			sReturn = "SIGEMT: emulator trap instruction"_ksv;
			break;
		#endif

		#ifdef SIGFPE
		case SIGFPE:
			sReturn = "SIGFPE: floating-point exception"_ksv;
			break;
		#endif

		#ifdef SIGKILL
		case SIGKILL:
			sReturn = "SIGKILL: kill (cannot be caught or ignored)"_ksv;
			break;
		#endif

		#ifdef SIGBUS
		case SIGBUS:
			sReturn = "SIGBUS: bus error"_ksv;
			break;
		#endif

		#ifdef SIGSEGV
		case SIGSEGV:
			sReturn = "SIGSEGV: segmentation violation"_ksv;
			break;
		#endif

		#ifdef SIGSTKFLT
		case SIGSTKFLT:
			sReturn = "SIGSTKFLT: stack fault on coprocessor";
			break;
		#endif

		#ifdef SIGSYS
		case SIGSYS:
			sReturn = "SIGSYS: bad argument to system call"_ksv;
			break;
		#endif

		#ifdef SIGPIPE
		case SIGPIPE:
			sReturn = "SIGPIPE: broken pipe"_ksv;
			break;
		#endif

		#ifdef SIGALRM
		case SIGALRM:
			sReturn = "SIGALRM: alarm clock"_ksv;
			break;
		#endif

		#ifdef SIGTERM
		case SIGTERM:
			sReturn = "SIGTERM: termination"_ksv;
			break;
		#endif

		#ifdef SIGUSR1
		case SIGUSR1:
			sReturn = "SIGUSR1: user signal 1"_ksv;
			break;
		#endif

		#ifdef SIGUSR2
		case SIGUSR2:
			sReturn = "SIGUSR2: user signal 2"_ksv;
			break;
		#endif

		#ifdef SIGCHLD
		case SIGCHLD:
			sReturn = "SIGCHLD: child status change"_ksv;
			break;
		#endif

		#ifdef SIGPWR
		case SIGPWR:
			sReturn = "SIGPWR: power-fail restart"_ksv;
			break;
		#endif

		#if defined(SIGLOST) && SIGPWR != SIGLOST
		case SIGLOST:
			sReturn = "SIGLOST: resource lost (eg, record-lock lost)"_ksv;
			break;
		#endif

		#ifdef SIGWINCH
		case SIGWINCH:
			sReturn = "SIGWINCH: terminal window size change"_ksv;
			break;
		#endif

		#ifdef SIGURG
		case SIGURG:
			sReturn = "SIGURG: urgent socket condition"_ksv;
			break;
		#endif

		#ifdef SIGPOLL
		case SIGPOLL:
			sReturn = "SIGPOLL: socket I/O possible"_ksv;
			break;
		#endif

		#if defined(SIGIO) && SIGIO != SIGPOLL
		case SIGIO:
			sReturn = "SIGIO: socket I/O possible"_ksv;
			break;
		#endif

		#ifdef SIGSTOP
		case SIGSTOP:
			sReturn = "SIGSTOP: stop (cannot be caught or ignored)"_ksv;
			break;
		#endif

		#ifdef SIGTSTP
		case SIGTSTP:
			sReturn = "SIGTSTP: user stop requested from tty"_ksv;
			break;
		#endif

		#ifdef SIGCONT
		case SIGCONT:
			sReturn = "SIGCONT: stopped process has been continued"_ksv;
			break;
		#endif

		#ifdef SIGTTIN
		case SIGTTIN:
			sReturn = "SIGTTIN: background tty read attempted"_ksv;
			break;
		#endif

		#ifdef SIGTTOU
		case SIGTTOU:
			sReturn = "SIGTTOU: background tty write attempted"_ksv;
			break;
		#endif

		#ifdef SIGVTALRM
		case SIGVTALRM:
			sReturn = "SIGVTALRM: virtual timer expired"_ksv;
			break;
		#endif

		#ifdef SIGPROF
		case SIGPROF:
			sReturn = "SIGPROF: profiling timer expired"_ksv;
			break;
		#endif

		#ifdef SIGXCPU
		case SIGXCPU:
			sReturn = "SIGXCPU: exceeded cpu limit"_ksv;
			break;
		#endif

		#ifdef SIGXFSZ
		case SIGXFSZ:
			sReturn = "SIGXFSZ: exceeded file size limit"_ksv;
			break;
		#endif

		#ifdef SIGWAITING
		case SIGWAITING:
			sReturn = "SIGWAITING: process's lwps are blocked"_ksv;
			break;
		#endif

		#ifdef SIGLWP
		case SIGLWP:
			sReturn = "SIGLWP: special signal used by thread library"_ksv;
			break;
		#endif

		#ifdef SIGFREEZE
		case SIGFREEZE:
			sReturn = "SIGFREEZE: special signal used by CPR"_ksv;
			break;
		#endif

		#ifdef SIGTHAW
		case SIGTHAW:
			sReturn = "SIGTHAW: special signal used by CPR"_ksv;
			break;
		#endif

		#ifdef SIGCANCEL
		case SIGCANCEL:
			sReturn = "SIGCANCEL: thread cancellation signal used by libthread"_ksv;
			break;
		#endif

		default:
#if defined(SIGRTMIN) && defined(SIGRTMAX)
			if (iSignalNum >= SIGRTMIN && iSignalNum <= SIGRTMAX)
			{
				sReturn = "SIGRT: real time signal"_ksv;
			}
			else
#endif
			{
				sReturn = "UNKNOWN: unknown signal"_ksv;
			}
			break;
	}

	if (bConcise)
	{
		sReturn.erase(sReturn.find(':'));
	}

	return sReturn;

} // kTranslateSignal

DEKAF2_NAMESPACE_END

