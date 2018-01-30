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


#include "dekaf.h"
#include "ksql.h"

LPCTSTR g_Synopsis[] = {
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
int main_dekaf (ULONG /*iKID*/, int argc, char* argv[])
//-----------------------------------------------------------------------------
{
	kInit ("CDBC","stdout",".createdbc.dbg");

	bool  fTestConnection = TRUE;
	char* pszTarget       = NULL;
	char* pszDBType       = NULL;
	char* pszDBUser       = NULL;
	char* pszDBPass       = NULL;
	char* pszDBName       = NULL;
	char* pszDBHost       = NULL;
	char* pszDBPort       = NULL;

	for (int ii=1; ii < argc; ++ii)
	{
		if (kstrin (argv[ii], "-d,-dd,-ddd")) {
			kSetDebug (strlen(argv[ii]) - 1, "stdout");
			kDebugLog ("%s: debug now set to %u", argv[ii], kIsDebug());
		}
		else if (strmatch (argv[ii], "-n"))
		{
			fTestConnection = FALSE;
		}

		// undocumented feature (reads the DBC file and prints summary):
		else if (strmatch (argv[ii], "-r"))
		{
			KSQL tmpdb;
			if (!tmpdb.LoadConnect (argv[2]))
			{
				printf ("createdbc(ERR): %s\n", tmpdb.GetLastError());
				return (1);
			}
			else
			{
				printf ("createdbc: %s\n", tmpdb.ConnectSummary());
				return (0);
			}
		}

		// <file> <dbtype> <user> <pass> <dbname> <server>
		else if (!pszTarget) {
			pszTarget = argv[ii];
		}
		else if (!pszDBType) {
			pszDBType = argv[ii];
		}
		else if (!pszDBUser) {
			pszDBUser = argv[ii];
		}
		else if (!pszDBPass) {
			pszDBPass = argv[ii];
		}
		else if (!pszDBName) {
			pszDBName = argv[ii];
		}
		else if (!pszDBHost) {
			pszDBHost = argv[ii];
		}
		else if (!pszDBPort) {
			pszDBPort = argv[ii];
		}
	}

	if (!pszDBHost)
	{
		for (int ii=0; g_Synopsis[ii]; ++ii) {
			printf (" %s\n", g_Synopsis[ii]);
		}
		return (1);
	}

	ULONG iDBType   = atol(pszDBType);
	UINT  iDBPort   = 0;

	if (strmatch (pszDBType, "oracle"))
	{
		iDBType = KSQL::DBT_ORACLE;
	}
	else if (strmatch (pszDBType, "mysql"))
	{
		iDBType = KSQL::DBT_MYSQL;
	}
	else if (strmatch (pszDBType, "sqlserver"))
	{
		iDBType = KSQL::DBT_SQLSERVER;
	}
	else if (strmatch (pszDBType, "sybase"))
	{
		iDBType = KSQL::DBT_SYBASE;
	}

	if (pszDBPort && *pszDBPort) {
	   iDBPort = atoi(pszDBPort);
	}

	KSQL tmpdb;
	if (!tmpdb.SetConnect(iDBType, pszDBUser, pszDBPass, pszDBName, pszDBHost, iDBPort))
	{
		printf ("FAILED.\n%s\n", tmpdb.GetLastError());
		return (1);
	}

	if (fTestConnection)
	{
		printf ("testing connection to '%s' ... ", tmpdb.ConnectSummary());
		fflush (stdout);

		if (!tmpdb.OpenConnection())
		{
			printf ("FAILED.\n%s\n", tmpdb.GetLastError());
			return (1);
		}

		printf ("ok.\n");
	}

	printf ("generating file '%s' ... ", pszTarget);
	fflush (stdout);
	if (!tmpdb.SaveConnect (pszTarget))
	{
		printf ("FAILED.\n%s\n", tmpdb.GetLastError());
		return (1);
	}

	printf ("done.\n");

	return (0);

} // main

