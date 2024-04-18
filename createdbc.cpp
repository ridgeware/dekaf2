/*
 //
 // DEKAF(tm): Lighter, Faster, Smarter (tm)
 //
 // Copyright (c) 2016, Ridgeware, Inc.
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
 */

#include <dekaf2/kdefinitions.h>
#include <dekaf2/dekaf2.h>
#include <dekaf2/ksql.h>
#include <dekaf2/kstring.h>
#include <dekaf2/klog.h>
#include <dekaf2/kwriter.h>
#include <dekaf2/koptions.h>
#include <dekaf2/kjoin.h>
#include <cinttypes>
#include <algorithm> // std::reverse

using namespace DEKAF2_NAMESPACE_NAME;

//-----------------------------------------------------------------------------
int Main (int argc, char* argv[])
//-----------------------------------------------------------------------------
{
	KInit(false)
		.SetName("CDBC")
		.SetDebugFlag(".createdbc.dbg")
		.SetMultiThreading(false);

	bool    fTestConnection{true};
	KString sTarget;
	KString sDBType;
	KString sDBUser;
	KString sDBPass;
	KString sDBName;
	KString sDBHost;
	KString sDBPort;

	KOptions CLI(true);

	CLI
		.Throw()
		.SetBriefDescription("create a KSQL database connection file")
		.SetAdditionalArgDescription(
			"<file> <dbtype> <user> <pass> <dbname> <server> [<port>]\n"
			"\n"
			"   <file>   : is the target filename (will be trumped if it already exists)\n"
			"   <dbtype> : oracle, mysql, sqlserver, sybase or a number corresponding to ksql.h\n"
			"   <user>   : database username\n"
			"   <pass>   : database password\n"
			"   <dbname> : database name (use '' for Oracle)\n"
			"   <server> : hostname (or ORACLE_SID)\n"
			"   <port>   : optional port override for database service")
		.SetAdditionalHelp(
			"history:\n"
			"  Nov 2001 : keef created");

	CLI
		.Option("n")
		.Set(fTestConnection, false)
		.Help("flags NOT to test the database connection before creating file");

	// undocumented feature (reads the DBC file and prints summary):
	CLI
		.Option("r")
//		.Type(KOptions::ArgTypes::File)
		.Hidden()
		.Final()
	([](KStringViewZ sFilename)
	{
		KSQL tmpdb;
		if (!tmpdb.LoadConnect (sFilename))
		{
			throw KError(tmpdb.GetLastError());
		}
		else
		{
			kPrintLine ("createdbc: {}", tmpdb.ConnectSummary());
			return (0);
		}
	});

	CLI
		.UnknownCommand
	([&](KOptions::ArgList& Commands)
	{
		while (!Commands.empty())
		{
			// <file> <dbtype> <user> <pass> <dbname> <server>
			if (sTarget.empty())
			{
				sTarget = Commands.pop();
			}
			else if (sDBType.empty())
			{
				sDBType = Commands.pop();
			}
			else if (sDBUser.empty())
			{
				sDBUser = Commands.pop();
			}
			else if (sDBPass.empty())
			{
				sDBPass = Commands.pop();
			}
			else if (sDBName.empty())
			{
				sDBName = Commands.pop();
			}
			else if (sDBHost.empty())
			{
				sDBHost = Commands.pop();
			}
			else if (sDBPort.empty())
			{
				sDBPort = Commands.pop();
			}
			else
			{
				std::reverse(Commands.begin(), Commands.end());
				throw KOptions::Error("excess parameters: " + kJoined(Commands, " "));
			}
		}

	});

	auto iRetVal = CLI.Parse(argc, argv);

	if (iRetVal	|| CLI.Terminate())
	{
		// either error or completed
		return iRetVal;
	}

	if (sDBHost.empty())
	{
		CLI.Help(KOut);
		return (1);
	}

	KSQL tmpdb;

	if (!tmpdb.SetDBType (sDBType))
	{
		throw KError (tmpdb.GetLastError());
	}

	if (!tmpdb.SetConnect(tmpdb.GetDBType(), sDBUser, sDBPass, sDBName, sDBHost, sDBPort.UInt16()))
	{
		throw KError(kFormat("FAILED.\n{}", tmpdb.GetLastError()));
	}

	if (fTestConnection)
	{
		kPrint ("testing connection to '{}' ... ", tmpdb.ConnectSummary());
		KOut.Flush();

		if (!tmpdb.OpenConnection())
		{
			kPrintLine ("FAILED.\n{}", tmpdb.GetLastError());
			return (1);
		}

		kPrintLine ("ok.");
	}

	kPrint ("generating file '{}' ... ", sTarget);
	KOut.Flush();

	if (!tmpdb.SaveConnect (sTarget))
	{
		kPrintLine ("FAILED.\n{}", tmpdb.GetLastError());
		return (1);
	}

	kPrintLine("done.");

	return (0);

} // Main

//-----------------------------------------------------------------------------
int main (int argc, char** argv)
//-----------------------------------------------------------------------------
{
	try
	{
		return Main (argc, argv);
	}
	catch (const std::exception& ex)
	{
		KErr.FormatLine(">> createdbc: {}", ex.what());
	}
	return 1;

} // main

