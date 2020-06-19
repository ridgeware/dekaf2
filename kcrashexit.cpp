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

#include "bits/kcppcompat.h"
#include "klog.h"
#include "kgetruntimestack.h"
#include "ksignals.h"
#include "kcrashexit.h"

#ifdef UNIX
 #include <sys/resource.h>      // to allow core dumps
 #include <csignal>
#endif

namespace dekaf2
{

//-----------------------------------------------------------------------------
void kCrashExitExt (int iSignalNum, siginfo_t* siginfo, void* context)
//-----------------------------------------------------------------------------
{
	auto& klog = KLog::getInstance();

	// switch automatic backtracing off
	klog.SetBackTraceLevel(-100);

	// and start our own stackdump

	klog.warning ("|                 *      \n");
	klog.warning ("|            *    *    * \n");
	klog.warning ("|              *  *  *   \n");

	switch (iSignalNum)
	{
#ifdef UNIX
	case SIGTERM:  // <-- [STOP] in browser causes apache to send SIGTERM to CGIs
		klog.warning ("|         * * * KILLED * * *{}\n",
			(getenv("REQUEST_METHOD")) ? "    -- web user hit [STOP] in browser" : "");
		break;

	case SIGINT:   // <-- sent from command line?
	case SIGQUIT:  // <-- sent from command line?
	case SIGHUP:   // <-- sent from command line?
	case SIGUSR1:  // <-- sent from command line?
	case SIGUSR2:  // <-- sent from command line?
		klog.warning ("|       * * * CANCELLED * * *\n");
		break;

	case SIGPIPE:
		klog.warning ("|       * * *  SIGPIPE  * * *\n");
		break;

#endif
	case 0:
	default:
		klog.warning ("|        * * * CRASHED * * *\n");
		break;
	}

	klog.warning ("|              *  *  *   \n");
	klog.warning ("|            *    *    * \n");
	klog.warning ("|                 *      \n");

	switch (iSignalNum)
	{
	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	// self-detected crash conditions:
	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	case 0:
		klog.warning ("CRASHCODE=0: self-detected crash condition.");
		break;
	case CRASHCODE_MEMORY:
		klog.warning ("CRASHCODE_MEMORY: self-detected dynamic allocation error.");
		break;
	case CRASHCODE_TODO:
		klog.warning ("CRASHCODE_TODO: feature not implemented yet.");
		break;
	case CRASHCODE_DEKAFUSAGE:
		klog.warning ("CRASHCODE_DEKAFUSAGE: invalid DEKAF framework usage.");
		break;
	case CRASHCODE_CORRUPT:
		klog.warning ("CRASHCODE_CORRUPT: self-detected memory corruption.");
		break;
	case CRASHCODE_DBERROR:
		klog.warning ("CRASHCODE_DBERROR: self-detected fatal database error.");
		break;
	case CRASHCODE_DBINTEGRITY:
		klog.warning ("CRASHCODE_DBINTEGRITY: self-detected database integrity problem.");
		break;
	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	// standard UNIX signals:
	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	default:
		klog.warning (kTranslateSignal (iSignalNum, /*fConcise=*/false));
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

				klog.warning("error at address {}:", address);

				if (address)
				{
					klog.warning("{}", kGetAddress2Line(address));
				}
				break;
			}

			default:
				break;
		}
	}

	if (iSignalNum != SIGINT)
	{
		klog.warning ("attempting to print a backtrace:");

		klog.warning(kGetRuntimeStack(1));

		#if 0
		klog.warning ("enabling core dumps...");
		rlimit core_limit = { RLIM_INFINITY, RLIM_INFINITY };
		setrlimit (RLIMIT_CORE, &core_limit); // enable core dumps

		klog.warning ("dumping core (assuming we can write to {0}})...", KFile::GetCWD());
		abort();
		#endif
	}
#endif

	klog.warning ("exiting program.");
	exit (-1);

} // kCrashExitExt

//-----------------------------------------------------------------------------
void kCrashExit (int iSignalNum)
//-----------------------------------------------------------------------------
{
	kCrashExitExt(iSignalNum, nullptr, nullptr);

} // kCrashExit

namespace detail {

//-----------------------------------------------------------------------------
void kFailedAssert (const char* sCrashMessage)
//-----------------------------------------------------------------------------
{
	if (!sCrashMessage || !*sCrashMessage)
	{
		sCrashMessage = "<empty>";
	}
	KLog::getInstance().warning ("ASSERT FAILURE: {}", sCrashMessage);
	kCrashExit (0);

} // kFailedAssert

} // of namespace detail

} // of namespace dekaf2

