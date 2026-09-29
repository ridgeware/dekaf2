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
#include <dekaf2/core/init/dekaf2.h>
#include <dekaf2/io/streams/kstream.h>
#include <dekaf2/core/errors/kexception.h>
#include <dekaf2/core/format/kformat.h>
#include <dekaf2/system/filesystem/kfilesystem.h>
#include <dekaf2/io/readwrite/kreader.h>

using namespace dekaf2;

//-----------------------------------------------------------------------------
KSql::KSql ()
//-----------------------------------------------------------------------------
{
	KInit(false).SetName(s_sProjectName);
	SetThrowOnError(true);

} // ctor

//-----------------------------------------------------------------------------
/// Where does the real SQL end, if its last statement is UNTERMINATED?
/// Returns the offset to truncate at before appending a ';', or npos when the
/// SQL is already terminated (or is empty / all comments) and needs nothing.
/// Trailing whitespace and trailing whole-line "--" comments are not content:
/// a script ending in a comment still needs the ';' on the statement above it,
/// and the terminator has to land on THAT line -- RunInterpreter is line
/// oriented, so a ';' sitting on a line of its own does not terminate anything.
//-----------------------------------------------------------------------------
std::size_t SQLUnterminatedEnd (KStringView sSQL)
//-----------------------------------------------------------------------------
{
	std::size_t iEnd = sSQL.size();

	for (;;)
	{
		while (iEnd > 0 && KASCII::kIsSpace (sSQL[iEnd - 1]))
		{
			--iEnd;
		}

		if (iEnd == 0)                { return KStringView::npos; }  // nothing to terminate
		if (sSQL[iEnd - 1] == ';')    { return KStringView::npos; }  // already terminated

		// is the last line a comment? if so ignore it and look at what precedes
		auto iNL = sSQL.substr (0, iEnd).rfind ('\n');
		std::size_t iLineStart = (iNL == KStringView::npos) ? 0 : iNL + 1;

		while (iLineStart < iEnd && KASCII::kIsSpace (sSQL[iLineStart]))
		{
			++iLineStart;
		}

		if (!sSQL.substr (iLineStart, iEnd - iLineStart).starts_with ("--"))
		{
			return iEnd;   // real SQL, unterminated -- truncate here and add ';'
		}

		if (iNL == KStringView::npos) { return KStringView::npos; }  // all comment

		iEnd = iNL;
	}

} // SQLUnterminatedEnd

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

	// bare (positional) arguments are database names: collect them during the parse.
	// The ad hoc mechanism only catches unknown options (with dashes) - a leading bare
	// command would otherwise be flagged as "excess argument" by the parser.
	std::vector<KStringViewZ> Databases;

	Options.UnknownCommand([&](KOptions::ArgList& Args)
	{
		while (!Args.empty()) { Databases.push_back(Args.pop()); }
	});

	int iRetval = Options.Parse(argc, argv, KOut);

	kDebug (1, "Options about to be defined");

	KStringViewZ sDBC       = Options("dbc                   : dbc file name or hex-encoded blob",     "");
	KString      sDBType    = Options(kFormat("t,dbtype <type> : db type: {}", KSQL::GetSupportedDBTypes()), "");
	KStringViewZ sUser      = Options("u,-user <name>        : username"                    ,          "");
	KStringViewZ sPassword  = Options("p,-pass <pass>        : password"                    ,          "");
	KStringViewZ sDatabase  = Options("db,-database <name>   : database to use"             ,          "");
	KStringViewZ sHostname  = Options("host <url>            : database server hostname"    , "localhost");
	uint16_t     iDBPort    = Options("port <number>         : database server port number" ,           0);
	bool         bQuiet     = Options("q,-quiet              : only show db output"         ,       false);
	KStringViewZ sFormat    = Options(kFormat("f,-format <format> : output format: {}, default ascii", KFormTable::GetSupportedStyles()), "");
	bool         bVersion   = Options("v,-version            : show version information"    ,       false);
	KDuration    Timeout    = chrono::seconds(Options("to,timeout <seconds> : connect timeout in seconds, default 5"        ,    5));
	bool         bNoComp    = Options("nocomp                : do not attempt to compress the database connection"          , false);
	bool         bNoTLS     = Options("notls                 : do not attempt to encrypt the database connection"           , false);
	bool         bForceTLS  = Options("forcetls              : force encryption for the database connection, fail otherwise", false);
	KStringViewZ sInSQL     = Options("e,exec <file|sql>     : execute a SQL file or a quoted SQL literal"                  ,    "");

	if (!Options.Check()) return iRetval;

	if (sInSQL)
	{
		bQuiet = true;
	}

	if (!Databases.empty())
	{
		if (sDatabase.empty() && sDBC.empty() && Databases.size() == 1)
		{
			sDatabase = Databases.front();
		}
		else
		{
			SetError(kFormat("unexpected parameters: {}", kJoined(Databases)));
		}
	}

	KSQL::DBT DBType = KSQL::DBT::MYSQL;
	if (!sDBType.empty())
	{
		DBType = KSQL::TxDBType (sDBType);
	}
#ifdef DEKAF2_HAS_SQLITE3
	else if (kFileExists(sDatabase))
	{
		DBType = KSQL::DBT::SQLITE3;
	}
#endif

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

	auto Format = KSQL::CreateOutputFormat(sFormat.empty() ? "ascii" : sFormat);

	// -e takes either a literal SQL string or the name of a file. BOTH need the
	// final statement to be terminated: the interpreter silently DISCARDS a
	// trailing statement that has no ';' -- no rows, no error, exit 0. The
	// literal path always appended one; the file path did not, so
	//     ksql -dbc x.dbc -e query.sql
	// produced nothing at all when query.sql lacked a final semicolon, which
	// reads exactly like "the query returned no rows" (Joe, 2026-09-25).
	// Normalize both the same way.
	std::unique_ptr<KTempFile<>> pTempSQL;
	KString sEffectiveInfile { sInSQL };
	KString sTempFile;

	if (sInSQL)
	{
		bool    bIsFile = kFileExists (sInSQL);
		KString sSQL;

		if (bIsFile)
		{
			if (!kReadAll (sInSQL, sSQL))
			{
				return SetError (kFormat ("could not read sql file: {}", sInSQL));
			}
		}
		else
		{
			sSQL = sInSQL;
		}

		auto iEnd = SQLUnterminatedEnd (sSQL);

		if (iEnd != KStringView::npos)
		{
			// drop trailing blank lines / comments so the ';' lands on the end of
			// the statement itself, then terminate it.
			sSQL.erase (iEnd);
			sSQL += ';';
		}
		else if (bIsFile)
		{
			// already terminated and already on disk -- hand it over untouched.
			sSQL.clear();
		}

		if (!sSQL.empty())
		{
			sTempFile = kFormat ("{}/ksql-{}.sql", "/tmp", getpid());
			if (!kWriteFile (sTempFile, sSQL))
			{
				return SetError(kFormat ("could not write to temp file: {}",sTempFile));
			}

			sEffectiveInfile = sTempFile;
		}
	}

	auto bOK = SQL.RunInterpreter (Format, bQuiet, sEffectiveInfile, /*bSavedFormatAllowed=*/ sFormat.empty());

	if (sTempFile)
	{
		kRemoveFile (sTempFile);
	}

	return !bOK;

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

	KString sLeft     {kBasename (sLeftDBC)};  sLeft.TrimRight(".dbc");
	KString sRight    {kBasename (sRightDBC)}; sRight.TrimRight(".dbc");
	uint8_t iMax    = (sLeft.size() > sRight.size()) ? sLeft.size() : sRight.size();

	KSQL::DIFF::Diffs diffs;
	KString           sSummary;
	auto              iDiffs = LeftDB.DiffSchemas (LeftSchema, RightSchema, diffs, sSummary, {
	                            {KSQL::DIFF::left_schema,  "left schema"},
	                            {KSQL::DIFF::left_prefix,  kFormat (":: {:>{}.{}}:",sLeft,iMax,iMax)},
	                            {KSQL::DIFF::right_schema, "right schema"},
	                            {KSQL::DIFF::right_prefix, kFormat (":: {:>{}.{}}:",sRight,iMax,iMax)},
	                           });

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
