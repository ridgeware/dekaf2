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

#include "klog.h"
#include "kstring.h"
#include "kstringutils.h"
#include "kinshell.h"
#include "ktcpserver.h"
#include <iostream>  // STL

using namespace dekaf2;

KStringView g_Synopsis[] = {
	" ",
	"klog -- command line interface to DEKAF2 logging features (aka 'the KLOG')",
	" ",
	"usage: klog [...]",
	"  [-]<N>              : dump last <N> lines of log",
	"  [-]f                : 'follow' feature (continuous output as log grows). ^C to break.",
	"  [-]off              : set debug level to 0",
	"  [-]on               : set debug level to 1",
	"  [-]set <N>          : set debug level to 0, 1, 2, etc.",
	"  [-]clear            : clear the log (no change to debug level)",
	"  [-]log <localpath>  : override the default path for: {LOG}",
	"  [-]flag <localpath> : override the default path for: {FLAG}",
	"  [-]listen <port>    : start a klog listener (netcat) on given port",
	" "
};

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KlogServer : public KTCPServer
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//-------
public:
//-------
	
#if !defined(DEKAF2_NO_GCC) && DEKAF2_GCC_VERSION < 80000
	// gcc versions below 8.0 do not properly honour the constructor forwarding,
	// therefore we use argument forwarding
	template<typename... Args>
	KlogServer(Args&&... args)
	: KTCPServer(std::forward<Args>(args)...)
	{}
#else
	using KTCPServer::KTCPServer;
#endif
	
//-------
protected:
//-------

	virtual KString Request(KString& sLine, Parameters& parameters) override
	{
		m_out.WriteLine (sLine).Flush();
		return KString();
	}

	KOutStream m_out{std::cout};
};


//-----------------------------------------------------------------------------
void Synopsis ()
//-----------------------------------------------------------------------------
{
	KOutStream out(std::cout);
    auto iNumLines = sizeof(g_Synopsis)/sizeof(KStringView);
    for (size_t ii=0; ii < iNumLines; ++ii) {
		KString sLine = g_Synopsis[ii];
		sLine.Replace ("{LOG}",  KLog().GetDebugLog());
		sLine.Replace ("{FLAG}", KLog().GetDebugFlag());
		out.WriteLine (sLine);
	}

} // Synopsis

//-----------------------------------------------------------------------------
int main (int argc, char* argv[])
//-----------------------------------------------------------------------------
{
	int        iErrors      = 0;
	int        iDumpLines   = 0;
	bool       bFollowFlag  = false;
	int        iNewLevel    = -1;
	bool       bClearLog    = false;
	bool       bAbort       = false;
	KString    sLogFile     = KLog().GetDebugLog();
	KString    sFlagFile    = KLog().GetDebugFlag();
	int        iPort        = 0;
	KOutStream err(std::cerr);
	KOutStream out(std::cout);

	// just parse -dash style options:
	for (int ii=1; ii < argc; ++ii)
	{
		const char* sArg = argv[ii];

		if (kStrIn (sArg, "-off,off")) {
			iNewLevel = 0;
		}
		else if (kStrIn (sArg, "-f,f")) {
			bFollowFlag = true;
		}
		else if (kStrIn (sArg, "-on,on")) {
			iNewLevel = 1;
		}
		else if (kStrIn (sArg, "-log,log"))
		{
			if (ii < (argc-1)) {
				sLogFile = argv[++ii];
				KLog().SetDebugLog (sLogFile);
			}
			else {
				err.Format ("klog: missing argument after -log option.\n");
				bAbort = true;
			}
		}
		else if (kStrIn (sArg, "-flag,flag"))
		{
			if (ii < (argc-1)) {
				sFlagFile = argv[++ii];
				KLog().SetDebugFlag (sFlagFile);
			}
			else {
				err.Format ("klog: missing argument after -flag option.\n");
				bAbort = true;
			}
		}
		else if (kStrIn (sArg, "-set,set"))
		{
			if (ii < (argc-1)) {
				iNewLevel = atoi(argv[++ii]);
			}
			else
			{
				err.Format ("klog: missing argument after -set option.\n");
				bAbort = true;
			}
		}
		else if (kStrIn (sArg, "-clear,clear"))
		{
			bClearLog = true;
		}
		else if (kStrIn (sArg, "-listen,listen"))
		{
			if (ii < (argc-1)) {
				iPort = atoi(argv[++ii]);
			}
			else
			{
				err.Format ("klog: missing argument after -set option.\n");
				bAbort = true;
			}
		}
		else
		{
			iDumpLines = atoi (argv[ii] + 1);
			if (iDumpLines <= 0)
			{
				err.Format ("klog: argument '{}' not understood.\n", sArg);
				bAbort = true;
				break; // for
			}
		}

	} // for args

	if ((argc==1) || bAbort)
	{
		Synopsis ();
		return (++iErrors);
	}

	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	// Take actions:
	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	if (bClearLog)
	{
		if ((sLogFile == "stdout") || (sLogFile == "stderr") || (sLogFile == "null"))
			err.Format ("klog: dekaf log is set to '{}' -- nothing to clear.\n", sLogFile);
		else if (!kFileExists (sLogFile))
			err.Format ("klog: log ({}) already cleared.\n", sLogFile);
		else if (kRemoveFile (sLogFile))
			err.Format ("klog: log ({}) cleared.\n", sLogFile);
		else
			err.Format ("klog: FAILED to clear log ({}).\n", sLogFile);
	}

	if (iNewLevel >= 0)
	{
		err.Format ("klog: set new debug level to {}.\n", iNewLevel);
        KLog().SetLevel(iNewLevel);
	}

    if (sLogFile == "null")
	{
		return (iErrors);
	}
	else if ((bFollowFlag || iDumpLines) && ((sLogFile == "stdout") || (sLogFile == "stderr")))
	{
		err.Format ("klog: dekaf log already set to '{}' -- no need to use this utility to dump it.\n", sLogFile);
		return (++iErrors);
	}
	else if (bFollowFlag)
	{
		out.Format ("klog: continuous log output ({}), ^C to break...\n", sLogFile);

		KString sCmd;

		if (iDumpLines) {
			sCmd.Format ("tail -{}f {}", iDumpLines, sLogFile);
		}
		else {
			sCmd.Format ("tail -f {}", sLogFile);
		}

		KInShell pipe (sCmd);
		KString  sLine;
		while (pipe.ReadLine(sLine))
		{
			out.WriteLine (sLine);
		}
	}
	else if (iDumpLines)
	{
		if (!kFileExists (sLogFile))
		{
			err.Format ("klog: log ({}) is empty.\n", sLogFile);
		}
		else
		{
			out.Format ("klog: last {} lines of {}...\n", iDumpLines, sLogFile);

			KString sCmd;
			sCmd.Format ("tail -{} {}", iDumpLines, sLogFile);

            KInShell pipe (sCmd);
			KString  sLine;
			while (pipe.ReadLine(sLine))
			{
				out.WriteLine (sLine);
			}
		}
	}
	else if (iPort)
	{
		out.Format ("klog: listening to port {} ...\n", iPort);
		KlogServer server (iPort, /*bSSL=*/false);
		server.Start(/*iTimeoutInSeconds=*/static_cast<uint16_t>(-1), /*bBlocking=*/true);
	}

	return (iErrors);

} // main -- klog cli
