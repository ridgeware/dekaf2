///////////////////////////////////////////////////////////////////////////////
//
// KSQL: Database Connectivity for DECKAF by Ridgeware
// DEKAF(tm): Lighter, Faster, Smarter (tm)
//
// Copyright (c) 2000-2015, Ridgeware, Inc.
//
// Use of this framework is permitted only under license agreement.
//
// For licensing, documentation and tech support, please see:
//              http://www.ridgeware.com
//
///////////////////////////////////////////////////////////////////////////////


#include <dekaf2/dekaf2.h>
#include <dekaf2/ksql.h>
#include <dekaf2/kstring.h>
#include <dekaf2/kstringutils.h>
#include <dekaf2/klog.h>
#include <dekaf2/kwriter.h>
#include <cinttypes>

using namespace dekaf2;

static const char* g_Synopsis[] = {
"",
"createdbc - create a KSQL database connection file",
"",
"usage: createdbc [-d[d[d]]] [-n] <file> <dbtype> <user> <pass> <dbname> <server> [<port>]",
"",
"where:",
"  -d[d[d]] : debug levels (to stdout)",
"  -n       : flags NOT to test the database connection before creating file",
"  <file>   : is the target filename (will be trumped if it already exists)",
"  <dbtype> : oracle, mysql, sqlserver, sybase or a number corresponding to ksql.h",
"  <user>   : database username",
"  <pass>   : database password",
"  <dbname> : database name (use '' for Oracle)",
"  <server> : hostname (or ORACLE_SID)",
"  <port>   : optional port override for database service",
"",
"history:",
"  Nov 2001 : keef created",
"",
NULL
};

//-----------------------------------------------------------------------------
int main (int argc, char* argv[])
//-----------------------------------------------------------------------------
{
	kInit ("CDBC", ".createdbc.dbg");

	bool  fTestConnection{true};
	KString sTarget;
	KString sDBType;
	KString sDBUser;
	KString sDBPass;
	KString sDBName;
	KString sDBHost;
	KString sDBPort;

	for (int ii=1; ii < argc; ++ii)
	{
		if (kStrIn (argv[ii], "-d,-dd,-ddd"))
		{
			KLog::getInstance().SetLevel( static_cast<int>(strlen(argv[ii])) - 1 );
			KLog::getInstance().SetDebugLog("stdout");
			kDebugLog (0, "{}: debug now set to {}", argv[ii], KLog::getInstance().GetLevel());
		}
		else if (!strcmp (argv[ii], "-n"))
		{
			fTestConnection = false;
		}

		// undocumented feature (reads the DBC file and prints summary):
		else if (!strcmp (argv[ii], "-r"))
		{
			KSQL tmpdb;
			if (!tmpdb.LoadConnect (argv[2]))
			{
				KOut.FormatLine ("createdbc(ERR): {}", tmpdb.GetLastError());
				return (1);
			}
			else
			{
				KOut.FormatLine ("createdbc: {}", tmpdb.ConnectSummary());
				return (0);
			}
		}

		// <file> <dbtype> <user> <pass> <dbname> <server>
		else if (sTarget.empty())
		{
			sTarget = argv[ii];
		}
		else if (sDBType.empty())
		{
			sDBType = argv[ii];
		}
		else if (sDBUser.empty())
		{
			sDBUser = argv[ii];
		}
		else if (sDBPass.empty())
		{
			sDBPass = argv[ii];
		}
		else if (sDBName.empty())
		{
			sDBName = argv[ii];
		}
		else if (sDBHost.empty())
		{
			sDBHost = argv[ii];
		}
		else if (sDBPort.empty())
		{
			sDBPort = argv[ii];
		}
	}

	if (sDBHost.empty())
	{
		for (int ii=0; g_Synopsis[ii]; ++ii)
		{
			KOut.FormatLine (" {}", g_Synopsis[ii]);
		}
		return (1);
	}

	KSQL tmpdb;

	if (!tmpdb.SetDBType (sDBType))
	{
		KErr.FormatLine ("createdbc: {}", tmpdb.GetLastError());
		return (1);
	}

	if (!tmpdb.SetConnect(tmpdb.GetDBType(), sDBUser, sDBPass, sDBName, sDBHost, sDBPort.UInt16()))
	{
		KOut.FormatLine ("FAILED.\n{}", tmpdb.GetLastError());
		return (1);
	}

	if (fTestConnection)
	{
		KOut.Format ("testing connection to '{}' ... ", tmpdb.ConnectSummary());
		KOut.Flush();

		if (!tmpdb.OpenConnection())
		{
			KOut.FormatLine ("FAILED.\n{}", tmpdb.GetLastError());
			return (1);
		}

		KOut.WriteLine ("ok.");
	}

	KOut.Format ("generating file '{}' ... ", sTarget);
	KOut.Flush();
	if (!tmpdb.SaveConnect (sTarget))
	{
		KOut.FormatLine ("FAILED.\n{}", tmpdb.GetLastError());
		return (1);
	}

	KOut.WriteLine("done.");

	return (0);

} // main

