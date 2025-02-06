
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

} // ctor

//-----------------------------------------------------------------------------
int KSql::Main(int argc, char** argv)
//-----------------------------------------------------------------------------
{
	// setup CLI option parsing
	KOptions Options(false, argc, argv, KLog::STDOUT, /*bThrow*/true);
	Options.SetBriefDescription("command line database client");

	KStringViewZ sDBCFile  = Options("dbc                : dbc file name"               ,          "");
	KString      sDBType   = Options("dbtype <type>      : db type - mysql, sqlserver, sqlserver15, sybase", "");
	KStringViewZ sUser     = Options("u,user <name>      : username"                    ,          "");
	KStringViewZ sPassword = Options("p,pass <pass>      : password"                    ,          "");
	KStringViewZ sDatabase = Options("db,database <name> : database to use"             ,          "");
	KStringViewZ sHostname = Options("host <url>         : database server hostname"    , "localhost");
	uint16_t     iDBPort   = Options("port <number>      : database server port number" ,           0);
	bool         bQuiet    = Options("q,quiet            : only show db output"         ,       false);
	KStringViewZ sFormat   = Options("f,format <format>  : output format - ascii, vertical, json, csv, html, default ascii", "ascii");
	bool         bVersion  = Options("v,version          : show version information"    ,       false);

	// do a final check if all required options were set
	if (!Options.Check()) return 1;

	KSQL::DBT DBType = KSQL::DBT::MYSQL;
	if (!sDBType.empty()) DBType = KSQL::TxDBType (sDBType);

	KSQL SQL;

	if (!sDBCFile.empty()) SQL.EnsureConnected ("", sDBCFile);
	else SQL.SetConnect (DBType, sUser, sPassword, sDatabase, sHostname, iDBPort);

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
