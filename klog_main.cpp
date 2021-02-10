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

#include "dekaf2.h"
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
#include "kjson.h"
#include "kmime.h"
#include "khttp_header.h"
#include "kstringstream.h"
#include "kreplacer.h"
#include "kconfiguration.h"
#include "khtmlentities.h"
#include "kcrashexit.h"
#include <csignal>

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
	"  grep <regex>      : filter output by regex",
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
	
#if defined(DEKAF2_IS_GCC) && DEKAF2_GCC_VERSION < 80000
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
	uint32_t    iDumpLines { 0 };
	uint16_t    iPort { 0 };
	bool        bFollowFlag { false };
	bool        bCompleted { false };
	KString     sGrepString;

}; // Actions

// output globals (in CGI mode we have to delay the output until after we wrote the header,
// therefore we redirect temporarily into a stringstream)
KOutStream* out = &KOut;
KOutStream* err = &KErr;
bool bIsCGI     = false;

using Properties = KProps<KString, KString, false, false>;

//-----------------------------------------------------------------------------
bool WriteConfig(Properties& props, KStringView sKeyToList)
//-----------------------------------------------------------------------------
{
	props.Set("log", KLog::getInstance().GetDebugLog());
	props.Set("tracelevel", KString::to_string(KLog::getInstance().GetBackTraceLevel()));
	props.Set("tracejson", KLog::getInstance().GetJSONTrace());

	KOutFile outfile (KLog::getInstance().GetDebugFlag(), std::ios::trunc);

	if (outfile.is_open())
	{
		// force mode 666 for the flag file ...
		kChangeMode(KLog::getInstance().GetDebugFlag(), DEKAF2_MODE_CREATE_FILE);
		
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

		if (outfile.Good())
		{
			return true;
		}
	}

	err->FormatLine("klog: cannot write to {}", KLog::getInstance().GetDebugFlag());

	return false;

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
bool Persist()
//-----------------------------------------------------------------------------
{
	auto props = ReadConfig();

	return WriteConfig(props, "");

} // Persist

//-----------------------------------------------------------------------------
void SetLevel(uint16_t iLevel)
//-----------------------------------------------------------------------------
{

	if (iLevel != KLog::getInstance().GetLevel())
	{
		KLog::getInstance().SetLevel(iLevel);

		if (!bIsCGI)
		{
			out->FormatLine ("klog: set new debug level to {}", KLog::getInstance().GetLevel());
		}

		Persist();
	}
	else if (!bIsCGI)
	{
		out->FormatLine ("klog: debug level already set to {}", iLevel);
	}

} // SetLevel

//-----------------------------------------------------------------------------
void SetBackTraceLevel(int16_t iLevel)
//-----------------------------------------------------------------------------
{
	if (iLevel != KLog::getInstance().GetLevel())
	{
		KLog::getInstance().SetBackTraceLevel(iLevel);

		if (!bIsCGI)
		{
			out->FormatLine ("klog: set new back trace level to {}", KLog::getInstance().GetBackTraceLevel());
		}

		Persist();
	}
	else if (!bIsCGI)
	{
		out->FormatLine ("klog: trace level already set to {}", iLevel);
	}

} // SetBackTraceLevel

//-----------------------------------------------------------------------------
void SetJSONTraceLevel(KStringView sLevel)
//-----------------------------------------------------------------------------
{
	if (sLevel != KLog::getInstance().GetJSONTrace())
	{
		KLog::getInstance().SetJSONTrace(sLevel);

		if (!bIsCGI)
		{
			out->FormatLine ("klog: set new JSON trace mode to \"{}\"", KLog::getInstance().GetJSONTrace());
		}

		Persist();
	}
	else if (!bIsCGI)
	{
		out->FormatLine ("klog: JSON trace mode already set to {}", sLevel);
	}

} // SetJSONTraceLevel

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
			out->WriteLine(sLine);
		}
	}
	else
	{
		err->FormatLine("klog: cannot open file {}", KLog::getInstance().GetDebugFlag());
	}

} // PrintFlagFile

//-----------------------------------------------------------------------------
void RemoveFlagFile()
//-----------------------------------------------------------------------------
{
	KLog::getInstance().SetDefaults();

	if (kRemoveFile(KLog::getInstance().GetDebugFlag()))
	{
		out->WriteLine("logging and tracing switched off");
	}
	else
	{
		// instead try to set default values
		KLog::getInstance().SetDefaults();

		// we may not have write permissions for the parent directory (for the delete),
		// but maybe we have them for the file itself, so we can write default parms
		if (Persist())
		{
			out->WriteLine("logging and tracing set to default values");
		}
		else
		{
			err->FormatLine("klog: cannot delete {}", KLog::getInstance().GetDebugFlag());
		}
	}

} // RemoveFlagFile

//-----------------------------------------------------------------------------
void TestBacktraces()
//-----------------------------------------------------------------------------
{
	// need to switch to CLI mode to print stack traces on the console
	KLog::getInstance().SetMode(KLog::CLI);
	KLog::getInstance().OnlyShowCallerOnJsonError(true);

	DEKAF2_TRY_EXCEPTION
	KJSON json;
	json["hello"] = 42;
	KString s = json["hello"];
	DEKAF2_LOG_EXCEPTION

	kSetCrashContext("just crashing..");

	kException(KException{"just kidding!"});
	kUnknownException();
	kDebug(-3, "hello");

	kAppendCrashContext("right now");

	volatile bool* bang = 0;
	if (*bang) {}

} // TestBacktraces

//-----------------------------------------------------------------------------
void SetupOptions (KOptions& Options, Actions& Actions)
//-----------------------------------------------------------------------------
{
	if (!Options.IsCGIEnvironment())
	{
		Options.Option("help")([&]()
		{
			auto iNumLines = sizeof(g_Synopsis)/sizeof(KStringView);
			for (size_t ii=0; ii < iNumLines; ++ii)
			{
				KString sLine = g_Synopsis[ii];
				sLine.Replace ("{LOG}",  KLog::getInstance().GetDebugLog());
				sLine.Replace ("{FLAG}", KLog::getInstance().GetDebugFlag());
				out->WriteLine (sLine);
			}
			Actions.bCompleted = true;
		});

		Options.Option("log", "pathname for output log").Type(KOptions::Path)
		([&](KStringViewZ sPath)
		{
			if (!KLog::getInstance().SetDebugLog (sPath))
			{
				err->Format ("klog: error setting local debug log to {}.\n", sPath);
			}
		});

		Options.Option("flag", "pathname for debug flag").Type(KOptions::Path)
		([&](KStringViewZ sPath)
		{
			if (!KLog::getInstance().SetDebugFlag (sPath))
			{
				err->Format ("klog: error setting local debug flag to {}.\n", sPath);
			}
		});

		Options.Command("f,follow")([&]()
		{
			Actions.bFollowFlag = true;
		});

		Options.Command("listen", "port number").Type(KOptions::Integer).Range(1, 65535)
		([&](KStringViewZ sPort)
		{
			Actions.iPort = sPort.UInt16();
		});

		Options.Command("crash")([&]()
		{
			TestBacktraces();
		});

	} // ! CGI environment

	Options.Command("grep", "search string").Type(KOptions::String)
	([&](KStringViewZ sRegex)
	{
		Actions.sGrepString = sRegex;
	});

	Options.Command("clear")([&]()
	{
		KString sLogFile = KLog::getInstance().GetDebugLog();

		if (sLogFile == "stdout" || sLogFile == "stderr" || sLogFile == "syslog")
		{
			out->Format ("klog: dekaf log is set to '{}' -- nothing to clear.\n", sLogFile);
		}
		else if (!kFileExists (sLogFile))
		{
			out->Format ("klog: log ({}) already cleared.\n", sLogFile);
		}
		else if (kRemoveFile (sLogFile))
		{
			out->Format ("klog: log ({}) cleared.\n", sLogFile);
		}
		else
		{
			err->Format ("klog: FAILED to clear log ({}).\n", sLogFile);
		}
	});

	Options.Command("config")([&]()
	{
		PrintFlagFile();
	});

	Options.Command("off")([&]()
	{
		RemoveFlagFile();
	});

	Options.Command("on")([&]()
	{
		SetLevel(1);
	});

	Options.Command("get")([&]()
	{
		out->Format ("{}\n", KLog::getInstance().GetLevel());
	});

	Options.Command("getlog")([&]()
	{
		out->Format ("{}\n", KLog::getInstance().GetDebugLog());
	});

	Options.Command("setlog", "debug log name")
	([&](KStringViewZ sPath)
	{
		if (KLog::getInstance().SetDebugLog (sPath))
		{
			out->Format ("klog: set new debug log to {} - ", sPath);
			Persist();
		}
		else
		{
			err->Format ("klog: error setting new debug log to {}.\n", sPath);
		}
	}).MinArgs(0);

	Options.Command("set").Type(KOptions::Unsigned).Range(0, 3)
	([&](KStringViewZ sArg)
	{
		SetLevel(sArg.UInt16());
	});

	Options.Command("trace")
	([&](KStringViewZ sTrace)
	{
		AddConfigKeyValue("trace", sTrace, true);
	});

	Options.Command("untrace", "did you mean notrace?")
	([&](KStringViewZ sTrace)
	{
		DeleteConfigKeyValue("trace", sTrace, true);
	});

	Options.Command("notrace")([&]()
	{
		DeleteConfigKeyValue("trace", "", true);
		out->WriteLine("all trace strings removed");
	});

	Options.Command("tracelevel").Type(KOptions::Integer)
	([&](KStringViewZ sArg)
	{
		SetBackTraceLevel(sArg.Int16());
	});

	Options.Command("tracejson")
	([&](KStringViewZ sArg)
	{
		SetJSONTraceLevel(sArg);
	});

	Options.Command("show", "number of lines").Type(KOptions::Unsigned)
	([&](KStringViewZ sArg)
	{
		Actions.iDumpLines = sArg.UInt32();
	});

	Options.RegisterUnknownCommand([&](KOptions::ArgList& sArgs)
	{
		Actions.iDumpLines = sArgs.front().UInt32();
		if (!Actions.iDumpLines)
		{
			throw KOptions::Error(kFormat("klog: argument not understood: {}", sArgs.front()));
		}
	});

} // SetupOptions

//-----------------------------------------------------------------------------
void PrintHTMLHeader(Actions& Actions)
//-----------------------------------------------------------------------------
{
	constexpr KStringView sHeader = R"foo(Content-Type: text/html

<html lang="en">
<head>
	<style>
	body {
		margin: 0;
		padding: 0;
		background-color: black;
		color: white;
	}
	#bar {
		overflow: hidden;
		position: fixed;
		top: 0;
		width: 100%;
		background-color: #C0C0C0;
		border: 2px outset;
		color: black;
		vertical-align: middle;
		font-family: Arial;
		font-size: 14px;
		padding: 5px 10px;
		white-space: nowrap;
   }
   #colheads {
		margin: 45px 0 10px 10px;
		padding: 0;
		overflow: hidden;
		position: fixed;
		top: 0;
		width: 100%;
		color: white:
		font-weight: bold;
		background-color: black;
	}
	#log {
		margin: 90px 10px 10px 5px;
		padding: 0;
	}
	em { /* button */
		background-color: #C0C0C0;
		color: black;
		border: 2px outset;
		margin: 4px 2px;
		padding: 2px 15px;
		font-style: normal;
	}
	em:hover, .on {
		background-color: white;
		color: black;
		cursor: pointer;
	}
	i { /* tag line */
		color: white;
		font-style: normal;
		text-shadow: rgba(0,0,0,0.5) -1px 0, rgba(0,0,0,0.3) 0 -1px, rgba(255,255,255,0.5) 0 1px, rgba(0,0,0,0.3) -1px -2px;
   	}
	input {
		background-color: white;
		color: black;
		margin: 4px 2px;
		padding: 2px 26px 2px 6px;
		font-weight: bold;
	}
	b { /* clear search */
		background-color: white;
		color: black;
		margin: 1px 6px 0 -18px;
		padding: 1px 3px 2px 3px;
	}
    b:hover {
		background-color: #C0C0C0;
		color: black;
		cursor: pointer;
	}
	span { /* spacer */
		padding: 0 6px;
	}

	</style>
	<script>
		function go(sAction)
		{
			var sNew= document.location.pathname
				+ "?show=" + document.forms[0]["show"].value;

			if (document.forms[0]["grep"].value != "") {
				sNew += "&grep=" + document.forms[0]["grep"].value;
			}

			if (sAction && (sAction != "")) {
				sNew += "&" + sAction;
			}

			console.log (":: action: " + sNew);
			document.location.href = sNew;
		}
   </script>
</head>
<body>
<form><input type="submit" style="display: none" /><div id="bar">
Last: <input size="6" name="show" value="{{show}}"/> lines
<span>&bull;</span>
Grep for: <input size="12" name="grep" placeholder="regex" value="{{grep}}"/><b onclick="document.forms[0]['grep'].value=''; go();">X</b>
<span>&bull;</span>
Set to:
	<em {{set0}} onclick="go('set=0');">0</em>
	<em {{set1}} onclick="go('set=1');">1</em>
	<em {{set2}} onclick="go('set=2');">2</em>
	<em {{set3}} onclick="go('set=3');">3</em>
<span>&bull;</span>
Trace:
	<em {{tracelevel1}} onclick="go('tracelevel=-1');">War</em>
	<em {{tracelevel2}} onclick="go('tracelevel=-2');">Exc</em>
	<em {{tracelevel3}} onclick="go('tracelevel=-3');">Off</em>
<span>&bull;</span>
	<em onclick="go('clear=');">wipe klog</em>
<span>&bull;</span>
	<em onclick="go();">refresh</em>
<span>&bull;</span>
<i>Long live the KLOG! [{{VERSION}}]</i>
</div>
</form>
<pre id="colheads">+-----+-------+-------+---------+---------------------+------------------------------------------------------------
| LVL | APP   |  PID  | THREAD  | TIMESTAMP           | MESSAGE
+-----+-------+-------+---------+---------------------+------------------------------------------------------------</pre>
<pre id="log">
)foo";

	KVariables Variables(true);

	auto& klog = KLog::getInstance();
	int iLevel = klog.GetLevel();
	int iTraceLevel = klog.GetBackTraceLevel();
	KStringView sJSON = klog.GetJSONTrace();

	Variables.insert("VERSION", "v" DEKAF_VERSION);
	Variables.insert("set0", iLevel == 0 ? "class=on" : "");
	Variables.insert("set1", iLevel == 1 ? "class=on" : "");
	Variables.insert("set2", iLevel == 2 ? "class=on" : "");
	Variables.insert("set3", iLevel == 3 ? "class=on" : "");
	Variables.insert("tracelevel1", iTraceLevel == -1 ? "class=on" : "");
	Variables.insert("tracelevel2", iTraceLevel == -2 ? "class=on" : "");
	Variables.insert("tracelevel3", iTraceLevel == -3 ? "class=on" : "");
	Variables.insert("tracejson", sJSON);
	Variables.insert("show", Actions.iDumpLines ? KString::to_string(Actions.iDumpLines) : "250");
	Variables.insert("grep", Actions.sGrepString);

	out->Write(Variables.Replace(sHeader));

} // PrintHTMLHeader

//-----------------------------------------------------------------------------
void PrintHTMLFooter()
//-----------------------------------------------------------------------------
{
	out->Write(R"(</pre></body></html>)");

} // PrintHTMLFooter

//-----------------------------------------------------------------------------
int main (int argc, char* argv[])
//-----------------------------------------------------------------------------
{
	kInit("KLOG");
	
	// we have to act as a SERVER, as otherwise we would not read the
	// flag file for the current settings..
	KLog::getInstance().SetMode(KLog::SERVER);

	Actions Actions;
	KOptions Options(true);
	int iErrors { 0 };

	if (Options.IsCGIEnvironment())
	{
		KString sOutput;
		KOutStringStream OSS(sOutput);

		// redirect all output into the stringstream
		out = &OSS;
		err = &OSS;
		bIsCGI = true;

		SetupOptions (Options, Actions);
		iErrors = Options.ParseCGI(argv[0], OSS);

		// redirect output into std again
		out = &KOut;
		err = &KErr;

		// print page start, which is affected by the settings above
		PrintHTMLHeader(Actions);

		// only now print all additional output that might have occured during parsing of the options
		out->Write(sOutput);

		if (iErrors)
		{
			out->Write(R"(</pre><hr/><em style="margin-left: 50px;" onclick='go()'>Refresh</em><hr/><pre>)");
		}
	}
	else
	{
		SetupOptions (Options, Actions);
		iErrors = Options.Parse(argc, argv, KOut);
	}

	if (Actions.bCompleted || iErrors)
	{
		if (bIsCGI)
		{
			PrintHTMLFooter();
		}
		return iErrors; // either error or completed
	}

	KString sLogFile = KLog::getInstance().GetDebugLog();

    if ((Actions.bFollowFlag || Actions.iDumpLines) && ((sLogFile == "stdout") || (sLogFile == "stderr")))
	{
		err->Format ("klog: dekaf log already set to '{}' -- no need to use this utility to dump it.\n", sLogFile);

		if (bIsCGI)
		{
			PrintHTMLFooter();
		}

		return (++iErrors);
	}

	if (Actions.iDumpLines || Actions.bFollowFlag)
	{
		if (!kFileExists (sLogFile) && !Actions.bFollowFlag)
		{
			err->Format ("klog: log ({}) is empty.\n", sLogFile);
		}
		else
		{
			if (Actions.bFollowFlag)
			{
				out->Format ("klog: continuous log output ({}), ^C to break...\n", sLogFile);
			}

			KString sCmd;

			if (Actions.sGrepString.empty())
			{
				sCmd.Format ("tail {}-n {} {}", Actions.bFollowFlag ? "-f " : "", Actions.iDumpLines, sLogFile);
			}
			else
			{
				sCmd.Format ("grep -iE \"{}\" {} | tail {}-n {}", Actions.sGrepString, sLogFile, Actions.bFollowFlag ? "-f " : "", Actions.iDumpLines);
			}

			KInShell Shell (sCmd);
			KString  sLine;

			if (!bIsCGI)
			{
				out->Format ("klog: last {} lines of {}...\n", Actions.iDumpLines, sLogFile);

				while (Shell.ReadLine(sLine))
				{
					out->WriteLine (sLine);
				}
			}
			else
			{
				while (Shell.ReadLine(sLine))
				{
					KStringView style = "color: #E0E0E0";

					if (sLine.starts_with("| DB2 |  KLOG |"))
					{
 						style = "color: #505050";
					}
					else if (sLine.starts_with("| DB3 |"))
					{
						style = "color: #808080";
					}
					else if (sLine.starts_with("| DB2 |"))
					{
						style = "color: #C0C0C0";
					}
					else if (sLine.starts_with("| DB1 |"))
					{
						style = "color: white";
					}
					else if (sLine.starts_with("| DBG |"))
					{
						style = "color: white";
					}
					else if (sLine.starts_with("| WAR |"))
					{
						style = "background-color: red; color: white";
					}
					else if (sLine.starts_with("| ERR |"))
					{
						style = "background-color: red; color: white";
					}

					out->FormatLine("<span style='{}'>{}</span>", style, KHTMLEntity::Encode(sLine));
				}
			}
		}
	}
	else if (Actions.iPort)
	{
		out->Format ("klog: listening to port {} ...\n", Actions.iPort);
		KlogServer server (Actions.iPort, /*bSSL=*/false);
		server.RegisterShutdownWithSignal(SIGTERM);
		server.RegisterShutdownWithSignal(SIGINT);
		server.Start(/*iTimeoutInSeconds=*/static_cast<uint16_t>(-1), /*bBlocking=*/true);
	}

	if (bIsCGI)
	{
		PrintHTMLFooter();
	}

	return (iErrors);

} // main -- klog cli
