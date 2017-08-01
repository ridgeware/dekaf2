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

#include "klog.h"
#include "kcppcompat.h"
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
void kCrashExit (int iSignalNum)
//-----------------------------------------------------------------------------
{
	// switch automatic backtracing off
	KLog().SetBackTrace(100);

	// and start our own stackdump

	KLog().warning ("                 *      \n");
	KLog().warning ("            *    *    * \n");
	KLog().warning ("              *  *  *   \n");

	switch (iSignalNum)
	{
#ifdef UNIX
	case SIGTERM:  // <-- [STOP] in browser causes apache to send SIGTERM to CGIs
		KLog().warning ("         * * * KILLED * * *%s\n",
			(getenv("REQUEST_METHOD")) ? "    -- web user hit [STOP] in browser" : "");
		break;

	case SIGINT:   // <-- sent from command line?
	case SIGQUIT:  // <-- sent from command line?
	case SIGHUP:   // <-- sent from command line?
	case SIGUSR1:  // <-- sent from command line?
	case SIGUSR2:  // <-- sent from command line?
		KLog().warning ("       * * * CANCELLED * * *\n");
		break;

	case SIGPIPE:
		KLog().warning ("       * * *  SIGPIPE  * * *\n");
		break;

#endif
	case 0:
	default:
		KLog().warning ("        * * * CRASHED * * *\n");
		break;
	}

	KLog().warning ("              *  *  *   \n");
	KLog().warning ("            *    *    * \n");
	KLog().warning ("                 *      \n");

	switch (iSignalNum)
	{
	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	// self-detected crash conditions:
	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	case 0:
		KLog().warning ("CRASHCODE=0: self-detected crash condition.");
		break;
	case CRASHCODE_MEMORY:
		KLog().warning ("CRASHCODE_MEMORY: self-detected dynamic allocation error.");
		break;
	case CRASHCODE_TODO:
		KLog().warning ("CRASHCODE_TODO: feature not implemented yet.");
		break;
	case CRASHCODE_DEKAFUSAGE:
		KLog().warning ("CRASHCODE_DEKAFUSAGE: invalid DEKAF framework usage.");
		break;
	case CRASHCODE_CORRUPT:
		KLog().warning ("CRASHCODE_CORRUPT: self-detected memory corruption.");
		break;
	case CRASHCODE_DBERROR:
		KLog().warning ("CRASHCODE_DBERROR: self-detected fatal database error.");
		break;
	case CRASHCODE_DBINTEGRITY:
		KLog().warning ("CRASHCODE_DBINTEGRITY: self-detected database integrity problem.");
		break;
	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	// standard UNIX signals:
	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	default:
		KLog().warning (kTranslateSignal (iSignalNum, /*fConcise=*/false));
	}

#ifndef WIN32
	if (iSignalNum != SIGINT)
	{
		KLog().warning ("attempting to print a backtrace:");

		KLog().warning(kGetRuntimeStack().s());

		#if 0
		KLog().warning ("enabling core dumps...");
		rlimit core_limit = { RLIM_INFINITY, RLIM_INFINITY };
		setrlimit (RLIMIT_CORE, &core_limit); // enable core dumps

		KLog().warning ("dumping core (assuming we can write to {0}})...", KFile::GetCWD());
		abort();
		#endif
	}
#endif

	KLog().warning ("exiting program.");
	exit (-1);

} // kCrashExit

} // of namespace dekaf2

