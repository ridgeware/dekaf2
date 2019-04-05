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
#include "kreader.h"
#include "kwriter.h"
#include "kfilesystem.h"
#include "ktcpserver.h"
#include "koptions.h"
#include "kprops.h"

using namespace dekaf2;

constexpr KStringView g_Synopsis[] = {
	" ",
	"klog -- command line interface to DEKAF logging features (aka 'the KLOG')",
	" ",
	"usage: klog [...]",
	"",
	"  -log <localpath>  : override the default path for: {LOG} (readonly)",
	"  -flag <localpath> : override the default path for: {FLAG}",
	"  f | follow        : 'follow' feature (continuous output as log grows). ^C to break.",
	"  <N>               : dump last <N> lines of log",
	"  off               : set debug level to 0",
	"  on                : set debug level to 1",
	"  set <N>           : set debug level to 0, 1, 2, etc.",
	"  get               : get debug level",
	"  clear             : clear the log (no change to debug level)",
	"",
	"dekaf2 extensions:",
	"",
	"  config            : print content of debug flag file",
	"  setlog            : set global debug log file",
	"  getlog            : get global debug log file",
	"  listen <port>     : start a klog listener (netcat) on given port",
	"  tracelevel <val>  : set the backtrace threshold to val",
	"  tracejson <val>   : set JSON trace to either off, short, or full",
	"  trace <val>       : set a debug message string that triggers a trace",
	"  untrace <val>     : remove a string from the trace triggers",
	"  notrace           : remove all trace trigger strings",
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
void PrintFlagFile()
//-----------------------------------------------------------------------------
{
	KInFile file(KLog::getInstance().GetDebugFlag());
	if (file.is_open())
	{
		KString sLine;
		while (file.ReadLine(sLine))
		{
			KOut.WriteLine(sLine);
		}
	}
	else
	{
		KErr.FormatLine("klog: cannot open file {}", KLog::getInstance().GetDebugFlag());
	}

} // PrintFlagFile

//-----------------------------------------------------------------------------
void RemoveFlagFile()
//-----------------------------------------------------------------------------
{
	KLog::getInstance().SetDefaults();

	if (kRemoveFile(KLog::getInstance().GetDebugFlag()))
	{
		KOut.WriteLine("logging and tracing switched off");
	}
	else
	{
		KErr.FormatLine("klog: cannot delete {}", KLog::getInstance().GetDebugFlag());
	}

} // RemoveFlagFile

using Properties = KProps<KString, KString, false, false>;

//-----------------------------------------------------------------------------
void WriteConfig(Properties& props, KStringView sKeyToList)
//-----------------------------------------------------------------------------
{
	props.Set("log", KLog::getInstance().GetDebugLog());
	props.Set("tracelevel", KString::to_string(KLog::getInstance().GetBackTraceLevel()));
	props.Set("tracejson", KLog::getInstance().GetJSONTrace());

	KOutFile outfile (KLog::getInstance().GetDebugFlag(), std::ios::trunc);

	if (outfile.is_open())
	{
		outfile.FormatLine("{}", KLog::getInstance().GetLevel());

		for (auto& it : props)
		{
			KString sOut;
			sOut = it.first;
			sOut += '=';
			sOut += it.second;

			if (it.first == sKeyToList)
			{
				KOut.WriteLine(sOut);
			}
			outfile.WriteLine(sOut);
		}
	}
	else
	{
		KErr.FormatLine("klog: cannot write to {}", KLog::getInstance().GetDebugFlag());
	}

} // WriteConfig

//-----------------------------------------------------------------------------
Properties ReadConfig()
//-----------------------------------------------------------------------------
{
	Properties props;

	{
		KInFile infile (KLog::getInstance().GetDebugFlag());

		if (infile.is_open())
		{
			KString sFirstLine;
			if (infile.ReadLine(sFirstLine))
			{
				props.Load(infile);
			}
		}
	}

	return props;

} // ReadConfig

//-----------------------------------------------------------------------------
void AddConfigKeyValue(KStringView sKey, KStringView sValue, bool bList)
//-----------------------------------------------------------------------------
{
	auto props = ReadConfig();

	// check if we have this key/value already
	bool bExists { false };
	const auto range = props.GetMulti(sKey);
	for (auto it = range.first; it != range.second; ++it)
	{
		if (it->second == sValue)
		{
			bExists = true;
			break;
		}
	}

	if (!bExists)
	{
		props.Add(sKey, sValue);
	}

	WriteConfig(props, bList ? sKey : "");

} //  AddConfigKeyValue

//-----------------------------------------------------------------------------
void DeleteConfigKeyValue(KStringView sKey, KStringView sValue, bool bList)
//-----------------------------------------------------------------------------
{
	auto props = ReadConfig();

	Properties filtered_props;

	for (auto& it : props)
	{
		if (it.first != sKey || (!sValue.empty() && it.second != sValue))
		{
			filtered_props.push_back(it);
		}
	}

	WriteConfig(filtered_props, bList ? sKey : "");

} // DeleteExtendedKeyValue

//-----------------------------------------------------------------------------
void Persist()
//-----------------------------------------------------------------------------
{
	auto props = ReadConfig();
	WriteConfig(props, "");

} // Persist

//-----------------------------------------------------------------------------
void SetLevel(uint16_t iLevel)
//-----------------------------------------------------------------------------
{
	KLog::getInstance().SetLevel(iLevel);
	KErr.FormatLine ("klog: set new debug level to {}", KLog::getInstance().GetLevel());
	Persist();

} // SetLevel

//-----------------------------------------------------------------------------
void SetBackTraceLevel(int16_t iLevel)
//-----------------------------------------------------------------------------
{
	KLog::getInstance().SetBackTraceLevel(iLevel);
	KErr.FormatLine ("klog: set new back trace level to {}", KLog::getInstance().GetBackTraceLevel());
	Persist();

} // SetBackTraceLevel

//-----------------------------------------------------------------------------
void SetJSONTraceLevel(KStringView sLevel)
//-----------------------------------------------------------------------------
{
	KLog::getInstance().SetJSONTrace(sLevel);
	KErr.FormatLine ("klog: set new JSON trace mode to \"{}\"", KLog::getInstance().GetJSONTrace());
	Persist();

} // SetJSONTraceLevel

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
			sLine.Replace ("{LOG}",  KLog::getInstance().GetDebugLog());
			sLine.Replace ("{FLAG}", KLog::getInstance().GetDebugFlag());
			KOut.WriteLine (sLine);
		}
		Actions.bCompleted = true;
	});

	Options.RegisterOption("log", "need pathname for output log", [&](KStringViewZ sPath)
	{
		if (!KLog::getInstance().SetDebugLog (sPath))
		{
			KErr.Format ("klog: error setting local debug log to {}.\n", sPath);
		}
	});

	Options.RegisterOption("flag", "need pathname for debug flag", [&](KStringViewZ sPath)
	{
		if (!KLog::getInstance().SetDebugFlag (sPath))
		{
			KErr.Format ("klog: error setting local debug flag to {}.\n", sPath);
		}
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
		KString sLogFile = KLog::getInstance().GetDebugLog();

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

	Options.RegisterCommand("config", [&]()
	{
		PrintFlagFile();
	});

	Options.RegisterCommand("off", [&]()
	{
		RemoveFlagFile();
	});

	Options.RegisterCommand("on", [&]()
	{
		SetLevel(1);
	});

	Options.RegisterCommand("get", [&]()
	{
		KErr.Format ("{}\n", KLog::getInstance().GetLevel());
	});

	Options.RegisterCommand("getlog", [&]()
	{
		KErr.Format ("{}\n", KLog::getInstance().GetDebugLog());
	});

	Options.RegisterCommand("setlog", "missing argument", [&](KStringViewZ sPath)
	{
		if (KLog::getInstance().SetDebugLog (sPath))
		{
			KErr.Format ("klog: set new debug log to {} - ", sPath);
			Persist();
		}
		else
		{
			KErr.Format ("klog: error setting new debug log to {}.\n", sPath);
		}
	});

	Options.RegisterCommand("set", "missing argument", [&](KStringViewZ sArg)
	{
		SetLevel(sArg.UInt16());
	});

	Options.RegisterOption("set", "missing argument", [&](KStringViewZ sArg)
	{
		SetLevel(sArg.UInt16());
	});

	Options.RegisterCommand("trace", "missing argument", [&](KStringViewZ sTrace)
	{
		AddConfigKeyValue("trace", sTrace, true);
	});

	Options.RegisterCommand("untrace", "missing argument - did you mean notrace?", [&](KStringViewZ sTrace)
	{
		DeleteConfigKeyValue("trace", sTrace, true);
	});

	Options.RegisterCommand("notrace", [&]()
	{
		DeleteConfigKeyValue("trace", "", true);
		KOut.WriteLine("all trace strings removed");
	});

	Options.RegisterCommand("tracelevel", "missing argument", [&](KStringViewZ sArg)
	{
		SetBackTraceLevel(sArg.Int16());
	});

	Options.RegisterCommand("tracejson", "missing argument", [&](KStringViewZ sArg)
	{
		SetJSONTraceLevel(sArg);
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
	// we have to act as a SERVER, as otherwise we would not read the
	// flag file for the current settings..
	KLog::getInstance().SetMode(KLog::SERVER);

	Actions Actions;
	KOptions Options(true);
	SetupOptions (Options, Actions);
	int iErrors = Options.Parse(argc, argv, KOut);

	if (Actions.bCompleted || iErrors)
	{
		return iErrors; // either error or completed
	}

	KString sLogFile = KLog::getInstance().GetDebugLog();

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
