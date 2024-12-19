
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
void KSql::Client (KSQL& SQL)
//-----------------------------------------------------------------------------
{
	SQL.SetFlag (KSQL::F_IgnoreSelectKeyword);

	if (!m_bQuiet)
	{
		kPrintLine("{} v{}", s_sProjectName, s_sProjectVersion)
			&& kPrint("{} > ", SQL.ConnectSummary())
			&& kFlush();
	}

	KString sSQL, sLine;

	for (auto& sLine : KIn)
	{
		sSQL += kFormat("{}\n", sLine);
		KString sTrimmed(sLine);
		sTrimmed.Replace('\t', ' ');
		sTrimmed.Trim();

		KString sNewDbName;
		auto    parts      = sTrimmed.Split(" ");
		KString sFirstWord = parts.size() ? parts[0] : ""; sFirstWord.MakeLower();

		if (sFirstWord.In("quit,quit;,exit,exit;"))
		{
			break; // for
		}
		else if ((parts.size() > 1) && sFirstWord.In("use,use;"))
		{
			sNewDbName = parts[1];
			sNewDbName.remove_suffix(';');
			sNewDbName.Trim();
			sSQL.Trim();
			if (!sSQL.ends_with(";"))
			{
				sSQL += ";";
			}
		}

		// flush command:
		if (sTrimmed.ends_with(";"))
		{
			SQL.EndQuery();
			sSQL.remove_suffix(';');
			sSQL.Trim();

			if (sSQL)
			{
				if (!SQL.OutputQuery (sSQL) && SQL.GetLastError())
				{
					kPrintLine (">> {}", SQL.GetLastError());
				}
				else if (SQL.GetLastError())
				{
					kPrintLine (":: {}", SQL.GetLastError());
				}

				if (!m_bQuiet && SQL.GetNumRowsAffected())
				{
					kPrintLine (":: {} rows affected.", kFormNumber(SQL.GetNumRowsAffected()));
				}

				sSQL.clear();
			}
		}

		if (sNewDbName)
		{
			SQL.SetDBName(sNewDbName);
			sNewDbName.clear();
		}

		if (!m_bQuiet)
		{
			kPrint ("{} > ", SQL.ConnectSummary()) && kFlush();
		}
	}

	kPrintLine();

} // Client

//-----------------------------------------------------------------------------
int KSql::Main(int argc, char** argv)
//-----------------------------------------------------------------------------
{
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
		             m_bQuiet  = Options("q,quiet            : only show db output"         ,       false);

		// do a final check if all required options were set
		if (!Options.Check()) return 1;

		KSQL::DBT DBType = KSQL::DBT::MYSQL;
		if (!sDBType.empty()) DBType = KSQL::TxDBType (sDBType);

		KSQL SQL;

		if (!sDBCFile.empty()) SQL.EnsureConnected ("", sDBCFile);
		else SQL.SetConnect (DBType, sUser, sPassword, sDatabase, sHostname, iDBPort);

		Client (SQL);
	}

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
