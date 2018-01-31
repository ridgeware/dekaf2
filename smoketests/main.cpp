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

#define CATCH_CONFIG_RUNNER
#include "catch.hpp"
#include <dekaf2/dekaf2.h>
#include <dekaf2/klog.h>
#include <dekaf2/kstringutils.h>
#include <dekaf2/kcrashexit.h>

#include <string>
#include <iostream>

using namespace dekaf2;

extern KStringView g_sDbcFile;

KStringView g_Synopsis[] = {
"",
"dekaf2-smoketests - slight less trivial tests for DEKAF2 frameworks (unit tests are instant)",
"",
"usage: dekaf2-smoketests [-d[d[d]]] [-dbc <dbcfile>] ...see standard args below...",
"",
"where:",
"  -d[d[d]]        : debug levels (to stdout)",
"  -dbc <dbcfile>  : trigger KSQL smoketests on given database"
};

//-----------------------------------------------------------------------------
int main( int argc, char* const argv[] )
//-----------------------------------------------------------------------------
{
	dekaf2::Dekaf().SetMultiThreading();
	KLog().SetDebugFlag(".smoketest.dbg");
	KLog().SetLevel(0);
	KLog().SetDebugLog(KLog::STDOUT);

	signal (SIGILL,  &kCrashExit);
	signal (SIGFPE,  &kCrashExit);
	signal (SIGBUS,  &kCrashExit);
	signal (SIGSEGV, &kCrashExit);

	bool bSynopsis{false};
	int  iLast{0};

	for (int ii=1; ii < argc; ++ii)
	{
		if (kStrIn (argv[ii], "-d,-dd,-ddd"))
		{
			iLast = ii;
			KLog().SetLevel( static_cast<int>(strlen(argv[ii]) - 1));
			kDebugLog (0, "{}: debug now set to {}", argv[ii], KLog().GetLevel());
		}
		else if (!strcmp(argv[ii], "-dbc"))
		{
			if (++ii < argc)
			{
				iLast = ii;
				g_sDbcFile = argv[ii];
			}
			else
			{
				kWarning("missing file name argument to -dbc");
			}
		}

		// part of the generic CATCH framework:
		//   -?, -h, --help                display usage information
		//   -l, --list-tests              list all/matching test cases
		//   -t, --list-tags               list all/matching tags
		else if (KASCII::kstrin (argv[ii], "-?,-h,--help"))
		{
			bSynopsis = true;
		}
	}

	if (bSynopsis)
	{
		KOutStream out(std::cout);
		for (unsigned long jj=0; jj < std::extent<decltype(g_Synopsis)>::value; ++jj)
		{
			out.WriteLine (g_Synopsis[jj]);
		}
	}

	auto iResult = Catch::Session().run( argc - iLast, &argv[iLast] );

	// global clean-up...

	return iResult;

} // main
