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
#include <dekaf2/klog.h>
#include <dekaf2/kwriter.h>
#include <iostream>
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

	KOutStream COut(std::cout);

	for (int ii=1; ii < argc; ++ii)
	{
		if (kStrIn (argv[ii], "-d,-dd,-ddd")) {
			KLog().SetLevel( strlen(argv[ii]) - 1 );
			KLog().SetDebugLog("stdout");
			kDebugLog (0, "{}: debug now set to {}", argv[ii], KLog().GetLevel());
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
				COut.FormatLine ("createdbc(ERR): {}", tmpdb.GetLastError());
				return (1);
			}
			else
			{
				COut.FormatLine ("createdbc: {}", tmpdb.ConnectSummary());
				return (0);
			}
		}

		// <file> <dbtype> <user> <pass> <dbname> <server>
		else if (sTarget.empty()) {
			sTarget = argv[ii];
		}
		else if (sDBType.empty()) {
			sDBType = argv[ii];
		}
		else if (sDBUser.empty()) {
			sDBUser = argv[ii];
		}
		else if (sDBPass.empty()) {
			sDBPass = argv[ii];
		}
		else if (sDBName.empty()) {
			sDBName = argv[ii];
		}
		else if (sDBHost.empty()) {
			sDBHost = argv[ii];
		}
		else if (sDBPort.empty()) {
			sDBPort = argv[ii];
		}
	}

	if (sDBHost.empty())
	{
		for (int ii=0; g_Synopsis[ii]; ++ii) {
			printf (" %s\n", g_Synopsis[ii]);
		}
		return (1);
	}

	KSQL::SQLTYPE iDBType = static_cast<KSQL::SQLTYPE>(sDBType.UInt32());
	uint16_t iDBPort = 0;

	if (sDBType == "oracle")
	{
		iDBType = KSQL::DBT_ORACLE;
	}
	else if (sDBType == "mysql")
	{
		iDBType = KSQL::DBT_MYSQL;
	}
	else if (sDBType == "sqlserver")
	{
		iDBType = KSQL::DBT_SQLSERVER;
	}
	else if (sDBType == "sybase")
	{
		iDBType = KSQL::DBT_SYBASE;
	}

	if (!sDBPort.empty()) {
	   iDBPort = sDBPort.UInt16();
	}

	KSQL tmpdb;
	if (!tmpdb.SetConnect(iDBType, sDBUser, sDBPass, sDBName, sDBHost, iDBPort))
	{
		COut.FormatLine ("FAILED.\n{}", tmpdb.GetLastError());
		return (1);
	}

	if (fTestConnection)
	{
		COut.Format ("testing connection to '{}' ... ", tmpdb.ConnectSummary());
		COut.Flush();

		if (!tmpdb.OpenConnection())
		{
			COut.FormatLine ("FAILED.\n{}", tmpdb.GetLastError());
			return (1);
		}

		COut.WriteLine ("ok.");
	}

	COut.Format ("generating file '{}' ... ", sTarget);
	COut.Flush();
	if (!tmpdb.SaveConnect (sTarget))
	{
		COut.FormatLine ("FAILED.\n{}", tmpdb.GetLastError());
		return (1);
	}

	COut.WriteLine("done.");

	return (0);

} // main

