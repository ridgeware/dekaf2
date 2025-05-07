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
	// setup CLI option parsing
	KOptions Options(false, argc, argv, KLog::STDOUT, /*bThrow*/true);
	Options.SetBriefDescription("command line database client");

	KStringViewZ sDBC      = Options("dbc                 : dbc file name or hex-encoded blob",              "");
	KString      sDBType   = Options("dbtype <type>       : db type: mysql, sqlserver, sqlserver15, sybase", "");
	KStringViewZ sUser     = Options("u,user <name>       : username"                    ,          "");
	KStringViewZ sPassword = Options("p,pass <pass>       : password"                    ,          "");
	KStringViewZ sDatabase = Options("db,database <name>  : database to use"             ,          "");
	KStringViewZ sHostname = Options("host <url>          : database server hostname"    , "localhost");
	uint16_t     iDBPort   = Options("port <number>       : database server port number" ,           0);
	bool         bQuiet    = Options("q,quiet             : only show db output"         ,       false);
	KStringViewZ sFormat   = Options("f,format <format>   : output format - ascii, bold, thin, double, rounded, spaced, vertical, json, csv, html, default ascii", "ascii");
	bool         bVersion  = Options("v,version           : show version information"    ,       false);
	KDuration    Timeout   = chrono::seconds(Options("t,timeout <seconds> : connect timeout in seconds, default 5",  5));
	bool         bNoComp   = Options("nocomp              : do not attempt to compress the database connection", false);
	bool         bNoTLS    = Options("notls               : do not attempt to encrypt the database connection", false);
	bool         bForceTLS = Options("forcetls            : force encryption for the database connection, fail otherwise", false);

	// do a final check if all required options were set
	if (!Options.Check()) return 1;

	KSQL::DBT DBType = KSQL::DBT::MYSQL;
	if (!sDBType.empty()) DBType = KSQL::TxDBType (sDBType);

	KSQL SQL;

	KSQL::Transport TransportFlags = KSQL::Transport::NoFlags;

	if (bNoTLS && bForceTLS) SetError("-notls and -forcetls options are mutually exclusive");

	if (!bNoComp)     TransportFlags |= KSQL::Transport::PreferZSTD;
	if (bForceTLS)    TransportFlags |= KSQL::Transport::RequireTLS;
	else if (!bNoTLS) TransportFlags |= KSQL::Transport::PreferTLS;

	if (!sDBC.empty())
	{
		if (!SQL.EnsureConnected ("", sDBC, KSQL::IniParms{}, Timeout, TransportFlags))
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

	if (!bQuiet)
	{
		kPrintLine(":: {} v{}", s_sProjectName, s_sProjectVersion);
		if (bVersion)
		{
			kPrintLine(":: {}", Dekaf::GetVersionInformation());
		}
	}

	auto Format = KSQL::CreateOutputFormat(sFormat);

	SQL.RunInterpreter (Format, bQuiet);

	return 0;

} // Main

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
