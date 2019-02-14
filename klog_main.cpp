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
#include "kfilesystem.h"
#include "ktcpserver.h"
#include "koptions.h"

using namespace dekaf2;

constexpr KStringView g_Synopsis[] = {
	" ",
	"klog -- command line interface to DEKAF2 logging features (aka 'the KLOG')",
	" ",
	"usage: klog [...]",
	"  -log <localpath>  : override the default path for: {LOG}",
	"  -flag <localpath> : override the default path for: {FLAG}",
	"  f | follow        : 'follow' feature (continuous output as log grows). ^C to break.",
	"  <N>               : dump last <N> lines of log",
	"  off               : set debug level to 0",
	"  on                : set debug level to 1",
	"  set <N>           : set debug level to 0, 1, 2, etc.",
	"  clear             : clear the log (no change to debug level)",
	"  listen <port>     : start a klog listener (netcat) on given port",
	""
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

	virtual KString Request(KString& sLine, Parameters& parameters) override final
	{
		KOut.WriteLine (sLine).Flush();
		return KString();
	}

};

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
struct Actions
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	uint32_t   iDumpLines { 0 };
	uint16_t   iPort { 0 };
	bool       bFollowFlag { false };
	bool       bCompleted { false };

}; // Actions

//-----------------------------------------------------------------------------
void SetLevel(uint16_t iLevel)
//-----------------------------------------------------------------------------
{
	KErr.Format ("klog: set new debug level to {}.\n", iLevel);
	KLog().SetLevel(iLevel);

} // SetLevel

//-----------------------------------------------------------------------------
void SetupOptions (KOptions& Options, Actions& Actions)
//-----------------------------------------------------------------------------
{
	Options.RegisterOption("help", [&]()
	{
		auto iNumLines = sizeof(g_Synopsis)/sizeof(KStringView);
		for (size_t ii=0; ii < iNumLines; ++ii)
		{
			KString sLine = g_Synopsis[ii];
			sLine.Replace ("{LOG}",  KLog().GetDebugLog());
			sLine.Replace ("{FLAG}", KLog().GetDebugFlag());
			KOut.WriteLine (sLine);
		}
		Actions.bCompleted = true;
	});

	Options.RegisterOption("log", "need pathname for output log", [&](KStringViewZ sPath)
	{
		KLog().SetDebugLog (sPath);
	});

	Options.RegisterOption("flag", "need pathname for debug flag", [&](KStringViewZ sPath)
	{
		KLog().SetDebugFlag (sPath);
	});

	Options.RegisterCommand("f,follow", [&]()
	{
		Actions.bFollowFlag = true;
	});

	Options.RegisterCommand("listen", "need port number", [&](KStringViewZ sPort)
	{
		Actions.iPort = sPort.UInt16();
		if (!Actions.iPort)
		{
			throw KOptions::WrongParameterError("klog: port number has to be between 1 and 65535");
		}
	});

	Options.RegisterCommand("clear", [&]()
	{
		KString sLogFile = KLog().GetDebugLog();

		if ((sLogFile == "stdout") || (sLogFile == "stderr") || (sLogFile == "null"))
		{
			KErr.Format ("klog: dekaf log is set to '{}' -- nothing to clear.\n", sLogFile);
		}
		else if (!kFileExists (sLogFile))
		{
			KErr.Format ("klog: log ({}) already cleared.\n", sLogFile);
		}
		else if (kRemoveFile (sLogFile))
		{
			KErr.Format ("klog: log ({}) cleared.\n", sLogFile);
		}
		else
		{
			KErr.Format ("klog: FAILED to clear log ({}).\n", sLogFile);
		}
	});

	Options.RegisterCommand("off", [&]()
	{
		SetLevel(0);
	});

	Options.RegisterCommand("on", [&]()
	{
		SetLevel(1);
	});

	Options.RegisterCommand("set", "missing argument", [&](KStringViewZ sArg)
	{
		SetLevel(sArg.UInt16());
	});

	Options.RegisterOption("set", "missing argument", [&](KStringViewZ sArg)
	{
		SetLevel(sArg.UInt16());
	});

	Options.RegisterUnknownCommand([&](KOptions::ArgList& sArgs)
	{
		Actions.iDumpLines = sArgs.front().UInt32();
		if (!Actions.iDumpLines)
		{
			throw KOptions::Error(kFormat("klog: argument not understood: {}", sArgs.front()));
		}
	});
}

//-----------------------------------------------------------------------------
int main (int argc, char* argv[])
//-----------------------------------------------------------------------------
{
	Actions Actions;
	KOptions Options(true);
	SetupOptions (Options, Actions);
	int iErrors = Options.Parse(argc, argv, KOut);

	if (Actions.bCompleted || iErrors)
	{
		return iErrors; // either error or completed
	}

	KString sLogFile = KLog().GetDebugLog();

    if ((Actions.bFollowFlag || Actions.iDumpLines) && ((sLogFile == "stdout") || (sLogFile == "stderr")))
	{
		KErr.Format ("klog: dekaf log already set to '{}' -- no need to use this utility to dump it.\n", sLogFile);
		return (++iErrors);
	}

	if (Actions.bFollowFlag)
	{
		KOut.Format ("klog: continuous log output ({}), ^C to break...\n", sLogFile);

		KString sCmd;

		if (Actions.iDumpLines)
		{
			sCmd.Format ("tail -{}f {}", Actions.iDumpLines, sLogFile);
		}
		else
		{
			sCmd.Format ("tail -f {}", sLogFile);
		}

		KInShell pipe (sCmd);
		KString  sLine;
		while (pipe.ReadLine(sLine))
		{
			KOut.WriteLine (sLine);
		}
	}
	else if (Actions.iDumpLines)
	{
		if (!kFileExists (sLogFile))
		{
			KErr.Format ("klog: log ({}) is empty.\n", sLogFile);
		}
		else
		{
			KOut.Format ("klog: last {} lines of {}...\n", Actions.iDumpLines, sLogFile);

			KString sCmd;
			sCmd.Format ("tail -{} {}", Actions.iDumpLines, sLogFile);

            KInShell pipe (sCmd);
			KString  sLine;
			while (pipe.ReadLine(sLine))
			{
				KOut.WriteLine (sLine);
			}
		}
	}
	else if (Actions.iPort)
	{
		KOut.Format ("klog: listening to port {} ...\n", Actions.iPort);
		KlogServer server (Actions.iPort, /*bSSL=*/false);
		server.Start(/*iTimeoutInSeconds=*/static_cast<uint16_t>(-1), /*bBlocking=*/true);
	}

	return (iErrors);

} // main -- klog cli
