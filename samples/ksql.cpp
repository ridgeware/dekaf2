/*
 //
 // DEKAF(tm): Lighter, Faster, Smarter (tm)
 //
 // Copyright (c) 2024, Ridgeware, Inc.
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

#include "ksql.h"
#include <dekaf2/dekaf2.h>
#include <dekaf2/kstream.h>
#include <dekaf2/kexception.h>
#include <dekaf2/kformat.h>
#include <dekaf2/kfilesystem.h>

using namespace dekaf2;

//-----------------------------------------------------------------------------
KSql::KSql ()
//-----------------------------------------------------------------------------
{
	KInit(false).SetName(s_sProjectName);
	SetThrowOnError(true);

} // ctor

//-----------------------------------------------------------------------------
int KSql::Main(int argc, char** argv)
//-----------------------------------------------------------------------------
{
	kDebug (1, "Main starting");

	// setup CLI option parsing
	KOptions Options(false);
	Options.AllowAdHocArgs ();
	Options.SetBriefDescription("command line database client");

	Options
		.Command ("diff <dbc1> <dbc2> [<table1> [...]>")
		.Help ("diff any two databases and optionally restrict to certain tables")
		.MinArgs(2)
		.MaxArgs(34463)
		.Stop()
	([&](KOptions::ArgList& ArgList)
	{
		auto    sLeftDBC  = ArgList.pop();
		auto    sRightDBC = ArgList.pop();
		KString sTableList;

		while (!ArgList.empty())
		{
			if (sTableList)
			{
				sTableList += ",";
			}
			sTableList += ArgList.pop();
		}

		return Diff (sLeftDBC, sRightDBC, sTableList);

	});

	int iRetval = Options.Parse(argc, argv, KOut);

	if (Options.Terminate() || iRetval)
	{
		return iRetval; // either error or completed
	}

	kDebug (1, "Options about to be defined");

	KStringViewZ sDBC       = Options("dbc                 : dbc file name or hex-encoded blob",              "");
	KString      sDBType    = Options("dbtype <type>       : db type: mysql, sqlserver, sqlserver15, sybase", "");
	KStringViewZ sUser      = Options("u,-user <name>      : username"                    ,          "");
	KStringViewZ sPassword  = Options("p,-pass <pass>      : password"                    ,          "");
	KStringViewZ sDatabase  = Options("db,-database <name> : database to use"             ,          "");
	KStringViewZ sHostname  = Options("host <url>          : database server hostname"    , "localhost");
	uint16_t     iDBPort    = Options("port <number>       : database server port number" ,           0);
	bool         bQuiet     = Options("q,-quiet            : only show db output"         ,       false);
	KStringViewZ sFormat    = Options("f,-format <format>  : output format - ascii, bold, thin, double, rounded, spaced, vertical, json, csv, html, default ascii", "ascii");
	bool         bVersion   = Options("v,-version          : show version information"    ,       false);
	KDuration    Timeout    = chrono::seconds(Options("t,timeout <seconds> : connect timeout in seconds, default 5"       ,    5));
	bool         bNoComp    = Options("nocomp              : do not attempt to compress the database connection"          , false);
	bool         bNoTLS     = Options("notls               : do not attempt to encrypt the database connection"           , false);
	bool         bForceTLS  = Options("forcetls            : force encryption for the database connection, fail otherwise", false);
	KStringViewZ sInfile    = Options("e,exec <file>       : execute the given SQL file"  ,          "");

	if (sInfile)
	{
		bQuiet = true;
	}

	KSQL::DBT DBType = KSQL::DBT::MYSQL;
	if (!sDBType.empty())
	{
		DBType = KSQL::TxDBType (sDBType);
	}

	KSQL SQL;

	KSQL::Transport TransportFlags = KSQL::Transport::NoFlags;

	if (bNoTLS && bForceTLS)
	{
		SetError("-notls and -forcetls options are mutually exclusive");
	}

	if (!bNoComp)
	{
		TransportFlags |= KSQL::Transport::PreferZSTD;
	}
	if (bForceTLS)
	{
		TransportFlags |= KSQL::Transport::RequireTLS;
	}
	else if (!bNoTLS)
	{
		TransportFlags |= KSQL::Transport::PreferTLS;
	}

	if (!sDBC.empty())
	{
		if (!SQL.LoadConnect (sDBC))
		{
			return SetError(SQL.GetLastError());
		}
		else if (!SQL.EnsureConnected ("", sDBC, KSQL::IniParms{}, Timeout, TransportFlags))
		{
			return SetError(SQL.GetLastError());
		}
	}
	else
	{
		SQL.SetConnect (DBType, sUser, sPassword, sDatabase, sHostname, iDBPort);
		if (!SQL.OpenConnection(Timeout, TransportFlags))
		{
			return SetError(SQL.GetLastError());
		}
	}

	if (! bQuiet)
	{
		kPrintLine(":: {} v{}", s_sProjectName, s_sProjectVersion);
		if (bVersion)
		{
			kPrintLine(":: {}", Dekaf::GetVersionInformation());
		}
	}

	auto Format = KSQL::CreateOutputFormat(sFormat);

	return !SQL.RunInterpreter (Format, bQuiet, sInfile);

} // Main

//-----------------------------------------------------------------------------
int KSql::Diff (KStringViewZ sLeftDBC, KStringViewZ sRightDBC, KStringView sTableList)
//-----------------------------------------------------------------------------
{
	kDebug (1, "...");

	KSQL LeftDB;
	if (!LeftDB.LoadConnect (sLeftDBC) /*|| !LeftDB.PingTest()*/ || !LeftDB.EnsureConnected())
	{
		KErr.FormatLine (">> dbc1: {}", LeftDB.GetLastError());
		return 1;
	}

	KSQL RightDB;
	if (!RightDB.LoadConnect (sRightDBC) /*|| !RightDB.PingTest()*/ || !RightDB.EnsureConnected())
	{
		KErr.FormatLine (">> dbc2: {}", RightDB.GetLastError());
		return 1;
	}

	KJSON options;
	if (sTableList)
	{
		options["table_list"] = sTableList;
	}

	KOut.FormatLine (":: {} : loading  left schema: {}", kFormTimestamp (kNow(), "{:%a %T}"), LeftDB.ConnectSummary());
	auto LeftSchema = LeftDB.LoadSchema (LeftDB.GetDBName(), /*sStartsWith=*/"", options);
	if (LeftDB.GetLastError() /*!LeftSchema || LeftSchema.is_null() || (LeftSchema == KJSON{})*/)
	{
		KErr.FormatLine (">> dbc1: {}", LeftDB.GetLastError());
		return 1;
	}

	KOut.FormatLine (":: {} : loading right schema: {}", kFormTimestamp (kNow(), "{:%a %T}"), RightDB.ConnectSummary());
	auto RightSchema = RightDB.LoadSchema (RightDB.GetDBName(), /*sStartsWith=*/"", options);
	if (RightDB.GetLastError() /*!RightSchema || RightSchema.is_null() || (RightSchema == KJSON{})*/)
	{
		KErr.FormatLine (">> dbc2: {}", RightDB.GetLastError());
		return 1;
	}

	KSQL::DIFF::Diffs diffs;
	KString           sSummary;
	auto              iDiffs = LeftDB.DiffSchemas (LeftSchema, RightSchema, diffs, sSummary);

	if (sSummary)
	{
		KOut.WriteLine (sSummary);
	}

	#if 0
	size_t iDiff{0};
	for (const auto& diff : diffs)
	{
		KOut.FormatLine (":: {} : diff [{:03}]:", kFormTimestamp (kNow(), "{:%a %T}"), ++iDiff);
		KOut.FormatLine (":: {} :      comment: {}", kFormTimestamp (kNow(), "{:%a %T}"), diff.sComment);
		KOut.FormatLine (":: {} :  action-left: {}", kFormTimestamp (kNow(), "{:%a %T}"), diff.sActionLeft);
		KOut.FormatLine (":: {} : action-right: {}", kFormTimestamp (kNow(), "{:%a %T}"), diff.sActionRight);
		KOut.WriteLine ("");
	} // for each diff
	#endif

	if (iDiffs > 0)
	{
		SetError (kFormat ("{} : {} diffs found.", kFormTimestamp (kNow(), "{:%a %T}"), iDiffs));
	}
	else
	{
		KOut.FormatLine (":: {} : no diffs found.", kFormTimestamp (kNow(), "{:%a %T}"));
	}

	return (iDiffs) ? 1 : 0;

} // Diff

//-----------------------------------------------------------------------------
int main (int argc, char** argv)
//-----------------------------------------------------------------------------
{
	try
	{
		return KSql().Main(argc, argv);
	}
	catch (const std::exception& ex)
	{
		kPrintLine(KErr, ">> {}: {}", kBasename(*argv), ex.what());
	}

	return 1;

} // main

#ifdef DEKAF2_REPEAT_CONSTEXPR_VARIABLE
constexpr KStringViewZ KSql::s_sProjectName;
constexpr KStringViewZ KSql::s_sProjectVersion;
#endif
