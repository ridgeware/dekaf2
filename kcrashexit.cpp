/*
//
// DEKAF(tm): Lighter, Faster, Smarter(tm)
//
// Copyright (c) 2000-2017, Ridgeware, Inc.
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

#include "kcrashexit.h"
#include "kstringview.h"
#include "klog.h"
#include "kgetruntimestack.h"
#include "ksignals.h"
#ifndef DEKAF2_WITH_KLOG
#include "kwriter.h"
#endif
#include <mutex>

#ifdef UNIX
 #include <sys/resource.h>      // to allow core dumps
 #include <csignal>
#endif

namespace dekaf2
{

static KCrashCallback       g_pCrashCallback{nullptr};
thread_local static KString g_tl_sCrashContext;
static std::mutex           g_CrashMutex;

//-----------------------------------------------------------------------------
void kCrashExitExt (int iSignalNum, siginfo_t* siginfo, void* context)
//-----------------------------------------------------------------------------
{
	KString sVerb {"CRASHED"};
	KString sWarning;

	// switch automatic backtracing off
	KLog::getInstance().SetBackTraceLevel(-100);

	switch (iSignalNum)
	{
#ifdef DEKAF2_IS_UNIX
	case SIGTERM:  // <-- [STOP] in browser causes apache to send SIGTERM to CGIs
		sVerb = "KILLED";
		break;

	case SIGINT:   // <-- sent from command line?
	case SIGQUIT:  // <-- sent from command line?
	case SIGHUP:   // <-- sent from command line?
	case SIGUSR1:  // <-- sent from command line?
	case SIGUSR2:  // <-- sent from command line?
		sVerb = "CANCELLED";
		break;

	case SIGPIPE:
		sVerb = "SIGPIPE";
		sWarning += kFormat ("|       * * *  SIGPIPE  * * *\n");
		break;
#endif

	case 0:
	default:
		sVerb = "CRASHED";
		break;
	}

	// and start our own stackdump
	if (g_tl_sCrashContext)
	{
		for (int i = 0; i < 44; ++i)
		{
			sWarning += ":=";
		}
		sWarning += '\n';
		for (const auto sLine : g_tl_sCrashContext.Split("\n"))
		{
			sWarning += kFormat (">> {}: {}\n", sVerb, sLine);
		}
		for (int i = 0; i < 44; ++i)
		{
			sWarning += ":=";
		}
		sWarning += '\n';
	}
	else
	{
		sWarning += kFormat ("|{:<17}\n",      '*');
		sWarning += kFormat ("|{:<12}\n", "*    *    *");
		sWarning += kFormat ("|{:<14}\n",   "*  *  *");
		sWarning += kFormat ("|{:<14}\n",     sVerb);
		sWarning += kFormat ("|{:<14}\n",   "*  *  *");
		sWarning += kFormat ("|{:<12}\n", "*    *    *");
		sWarning += kFormat ("|{:<17}\n",      '*');
	}

	switch (iSignalNum)
	{
	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	// self-detected crash conditions:
	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	case 0:
		sWarning += kFormat ("CRASHCODE=0: self-detected crash condition\n");
		break;
	case CRASHCODE_MEMORY:
		sWarning += kFormat ("CRASHCODE_MEMORY: self-detected dynamic allocation error\n");
		break;
	case CRASHCODE_TODO:
		sWarning += kFormat ("CRASHCODE_TODO: feature not implemented yet\n");
		break;
	case CRASHCODE_DEKAFUSAGE:
		sWarning += kFormat ("CRASHCODE_DEKAFUSAGE: invalid DEKAF framework usage\n");
		break;
	case CRASHCODE_CORRUPT:
		sWarning += kFormat ("CRASHCODE_CORRUPT: self-detected memory corruption\n");
		break;
	case CRASHCODE_DBERROR:
		sWarning += kFormat ("CRASHCODE_DBERROR: self-detected fatal database error\n");
		break;
	case CRASHCODE_DBINTEGRITY:
		sWarning += kFormat ("CRASHCODE_DBINTEGRITY: self-detected database integrity problem\n");
		break;
	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	// standard UNIX signals:
	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	default:
		sWarning += kFormat (kTranslateSignal (iSignalNum, /*fConcise=*/false));
	}

	#ifndef WIN32
	if (siginfo != nullptr)
	{
		switch (iSignalNum)
		{
			case SIGSEGV:
			case SIGILL:
			case SIGBUS:
			case SIGFPE:
			case SIGTRAP:
			{
				// try to isolate the crashing line
				void* address = siginfo->si_addr;

				sWarning += kFormat("\nerror at address {}:", address);

				if (address)
				{
					sWarning += kFormat(kGetAddress2Line(address));
				}
				break;
			}

			default:
				break;
		}
	}

	if (iSignalNum != SIGINT)
	{
		sWarning += kFormat ("\nattempting to print a backtrace:\n");
		sWarning += kGetRuntimeStack(1);
	}
	#endif

	sWarning += kFormat ("exiting program.");
	kWarning (sWarning);

	// make sure all access on the global vars is protected from races
	std::lock_guard<std::mutex> Lock(g_CrashMutex);

	if (g_pCrashCallback)
	{
		g_pCrashCallback (sWarning);
	}

	// generate a core dump if enabled by the system
	abort ();

} // kCrashExitExt

//-----------------------------------------------------------------------------
void kCrashExit (int iSignalNum)
//-----------------------------------------------------------------------------
{
	kCrashExitExt(iSignalNum, nullptr, nullptr);

} // kCrashExit

//-----------------------------------------------------------------------------
void kSetCrashContext (KStringView sContext)
//-----------------------------------------------------------------------------
{
	g_tl_sCrashContext = sContext;
}

//-----------------------------------------------------------------------------
void kAppendCrashContext (KStringView sContext, KStringView sSeparator)
//-----------------------------------------------------------------------------
{
	g_tl_sCrashContext += sSeparator;
	g_tl_sCrashContext += sContext;
}

//-----------------------------------------------------------------------------
void kAppendCrashContext (KStringView sContext)
//-----------------------------------------------------------------------------
{
	g_tl_sCrashContext += "\n";
	g_tl_sCrashContext += sContext;
}

//-----------------------------------------------------------------------------
void kSetCrashCallback (KCrashCallback pFunction)
//-----------------------------------------------------------------------------
{
	// make sure all access on the global vars is protected from races
	std::lock_guard<std::mutex> Lock(g_CrashMutex);
	g_pCrashCallback = pFunction;
}

namespace detail {

//-----------------------------------------------------------------------------
void kFailedAssert (const KStringView& sCrashMessage)
//-----------------------------------------------------------------------------
{
	kWarning ("ASSERT FAILURE: {}", sCrashMessage);
	kCrashExit (0);

} // kFailedAssert

//-----------------------------------------------------------------------------
void kFailedAssert (const char* sCrashMessage)
//-----------------------------------------------------------------------------
{
	kWarning ("ASSERT FAILURE: {}", sCrashMessage);
	kCrashExit (0);

} // kFailedAssert

} // of namespace detail

} // of namespace dekaf2

