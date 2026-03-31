/*
//
// DEKAF(tm): Lighter, Faster, Smarter(tm)
//
// Copyright (c) 2000-2017, Ridgeware, Inc.
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

#include "catch.hpp"
#include <dekaf2/ksql.h>
#include <dekaf2/kjson.h>
#include <dekaf2/kstring.h>
#include <dekaf2/kparallel.h>

using namespace dekaf2;

//-----------------------------------------------------------------------------
TEST_CASE("KSQL")
//-----------------------------------------------------------------------------
{
	KString BENIGN    = "Something benign.";
	KString BENIGNx   = "Something benign.";
	KString QUOTES1   = "Fred's Fishing Pole";
	KString QUOTES1x  = "Fred\\'s Fishing Pole";
	KString QUOTES2   = "Fred's `fishing` pole's \"longer\" than mine.";
	KString QUOTES2x  = "Fred\\'s \\`fishing\\` pole\\'s \\\"longer\\\" than mine.";
	KString SLASHES1  = "This is a \\l\\i\\t\\t\\l\\e /s/l/a/s/h test.";
	KString SLASHES1x = "This is a \\\\l\\\\i\\\\t\\\\t\\\\l\\\\e /s/l/a/s/h test.";
	KString SLASHES2  = "This <b>is</b>\\n a string\\r with s/l/a/s/h/e/s, \\g\\e\\t\\ i\\t\\???";
	KString SLASHES2x = "This <b>is</b>\\\\n a string\\\\r with s/l/a/s/h/e/s, \\\\g\\\\e\\\\t\\\\ i\\\\t\\\\???";
	KString ASIAN1    = "Chinese characters ñäöüß 一二三四五六七八九十";
	KString ASIAN2    = "Chinese characters ñäöüß 十九八七六五四三二一";

	SECTION("KROW::EscapeChars(BENIGN)")
	{
		KSQL DB;
		DB.SetDBType(KSQL::DBT::MYSQL);
		auto sEscaped ( DB.EscapeString (BENIGN));
		CHECK (sEscaped == BENIGNx);
		auto sSQL = DB.FormatSQL("{}", BENIGN);
		CHECK (sSQL == BENIGNx);
	}

	SECTION("KROW::EscapeChars(QUOTES1)")
	{
		KSQL DB;
		DB.SetDBType(KSQL::DBT::MYSQL);
		auto sEscaped ( DB.EscapeString (QUOTES1));
		CHECK (sEscaped == QUOTES1x);
		auto sSQL = DB.FormatSQL("{}", QUOTES1);
		CHECK (sSQL == QUOTES1x);
	}

	SECTION("KROW::EscapeChars(QUOTES2)")
	{
		KSQL DB;
		DB.SetDBType(KSQL::DBT::MYSQL);
		auto sEscaped ( DB.EscapeString (QUOTES2));
		CHECK (sEscaped == QUOTES2x);
		auto sSQL = DB.FormatSQL("{}", QUOTES2);
		CHECK (sSQL == QUOTES2x);
	}

	SECTION("KROW::EscapeChars(SLASHES1)")
	{
		KSQL DB;
		DB.SetDBType(KSQL::DBT::MYSQL);
		auto sEscaped ( DB.EscapeString (SLASHES1));
		CHECK (sEscaped == SLASHES1x);
		auto sSQL = DB.FormatSQL("{}", SLASHES1);
		CHECK (sSQL == SLASHES1x);
	}

	SECTION("KROW::EscapeChars(SLASHES2)")
	{
		KSQL DB;
		DB.SetDBType(KSQL::DBT::MYSQL);
		auto sEscaped ( DB.EscapeString (SLASHES2));
		CHECK (sEscaped == SLASHES2x);
		auto sSQL = DB.FormatSQL("{}", SLASHES2);
		CHECK (sSQL == SLASHES2x);
	}

	SECTION("KROW::EscapeChars(ASIAN1)")
	{
		KSQL DB;
		DB.SetDBType(KSQL::DBT::MYSQL);
		auto sEscaped ( DB.EscapeString (ASIAN1));
		CHECK (sEscaped == ASIAN1);
		auto sSQL = DB.FormatSQL("{}", ASIAN1);
		CHECK (sSQL == ASIAN1);
	}

	SECTION("KROW::EscapeChars(ASIAN2)")
	{
		KSQL DB;
		DB.SetDBType(KSQL::DBT::MYSQL);
		auto sEscaped ( DB.EscapeString (ASIAN2));
		CHECK (sEscaped == ASIAN2);
		auto sSQL = DB.FormatSQL("{}", ASIAN2);
		CHECK (sSQL == ASIAN2);
	}

	SECTION("kFormatSQL")
	{
		CHECK (kFormatSQL("{}", 12345   ) == "12345"   );
		CHECK (kFormatSQL("{}", 123.45  ) == "123.45"  );
		CHECK (kFormatSQL("{}", BENIGN  ) == BENIGNx   );
		CHECK (kFormatSQL("{}", QUOTES1 ) == QUOTES1x  );
		CHECK (kFormatSQL("{}", QUOTES2 ) == QUOTES2x  );
		CHECK (kFormatSQL("{}", SLASHES1) == SLASHES1x );
		CHECK (kFormatSQL("{}", SLASHES2) == SLASHES2x );
		CHECK (kFormatSQL("{}", ASIAN1  ) == ASIAN1    );
		CHECK (kFormatSQL("{}", ASIAN2  ) == ASIAN2    );
	}

	SECTION("kFormatSQL")
	{
		CHECK (kFormatSQL("{}", 12345   ) == "12345"   );
		CHECK (kFormatSQL("{}", 123.45  ) == "123.45"  );
		CHECK (kFormatSQL("{}", KString(BENIGN)  ) == BENIGNx   );
		CHECK (kFormatSQL("{}", KString(QUOTES1) ) == QUOTES1x  );
		CHECK (kFormatSQL("{}", KString(QUOTES2) ) == QUOTES2x  );
		CHECK (kFormatSQL("{}", KString(SLASHES1)) == SLASHES1x );
		CHECK (kFormatSQL("{}", KString(SLASHES2)) == SLASHES2x );
		CHECK (kFormatSQL("{}", KString(ASIAN1)  ) == ASIAN1    );
		CHECK (kFormatSQL("{}", KString(ASIAN2)  ) == ASIAN2    );
	}

	SECTION("KSQLString")
	{
		KSQL DB;
		DB.SetDBType(KSQL::DBT::MYSQL);
		auto sAnd   = DB.FormatSQL(" and key2='{}'", "test2");
		auto sQuery = DB.FormatSQL("select * from ABC where key1='{}'{}", "test1", sAnd);
		CHECK ( sQuery == "select * from ABC where key1='test1' and key2='test2'");

		{
			KSQLString sSafe = "select * from test";
			auto sSafe2 = DB.FormatSQL(sSafe);
			CHECK ( sSafe2 == "select * from test" );
		}
		{
			KSQLString sSafe = "select * from test where key = '{}'";
			auto sSafe2 = DB.FormatSQL(sSafe, "something");
			CHECK ( sSafe2 == "select * from test where key = 'something'" );
		}
		{
			KSQLString sSafe = "select * from test where key = {}";
			auto sSafe2 = DB.FormatSQL(sSafe, "'something'"); // note: no escaping for const data!
			CHECK ( sSafe2 == "select * from test where key = 'something'" );
		}
		{
			KSQLString sSafe = "select * from test where key = {}";
			KString sWhat = "'something'";
			auto sSafe2 = DB.FormatSQL(sSafe, sWhat); // note: escaping for non-const data!
			CHECK ( sSafe2 == "select * from test where key = \\'something\\'" );
		}
		{
			KSQLString sSafe = "select * from test where key = {}";
			KString sWhat = "'something'";
			auto sSafe2 = DB.FormatSQL(sSafe, sWhat.c_str()); // note: escaping for non-const data!
			CHECK ( sSafe2 == "select * from test where key = \\'something\\'" );
		}
		{
			KString sBad = "select bad";
			CHECK_THROWS( DB.FormatSQL(sBad.ToView()) );
		}
		{
			KString sBad = "select bad";
			CHECK_THROWS( DB.FormatSQL(sBad.c_str()) );
		}
		{
			CHECK_NOTHROW( DB.FormatSQL("select good") );
		}
		{
			CHECK_NOTHROW( DB.FormatSQL(KStringView("select good")) );
		}
		{
			KSQLString sSQL = "drop table test; select * from test;drop table toast";
			auto Statements = sSQL.Split(';');
			CHECK ( Statements.size() == 3 );
			if (Statements.size() == 3)
			{
				CHECK ( Statements[0] == "drop table test"     );
				CHECK ( Statements[1] == "select * from test" );
				CHECK ( Statements[2] == "drop table toast"    );
			}
		}
		{
			KSQLString sSQL = "drop table test\\;; select * from '\"test;drop'; table toast";
			auto Statements = sSQL.Split(';');
			CHECK ( Statements.size() == 3 );
			if (Statements.size() == 3)
			{
				CHECK ( Statements[0] == "drop table test\\;"  );
				CHECK ( Statements[1] == "select * from '\"test;drop'" );
				CHECK ( Statements[2] == "table toast"    );
			}
		}
		{
			KSQLString sSQL = "drop table test\\;; select * from '\"test;drop'; table toast";
			CHECK_THROWS( sSQL.Split('"') );
			CHECK_THROWS( sSQL.Split('\'') );
			CHECK_THROWS( sSQL.Split('\\') );
		}
	}

	SECTION("FormAndClause")
	{
		KSQL DB;
		DB.SetDBType(KSQL::DBT::MYSQL);

		auto sResult = DB.FormAndClause("mycol", "a'a|bb|cc|dd|ee|ff|gg", KSQL::FAC_NORMAL, "|").str();
		sResult.CollapseAndTrim();
		CHECK (sResult == "and mycol in ('a'a','bb','cc','dd','ee','ff','gg')" );

		sResult = DB.FormAndClause("mycol", "a'a|bb|cc|dd|ee|ff|gg", KSQL::FAC_LIKE, "|").str();
		sResult.CollapseAndTrim();
		CHECK (sResult == "and (mycol like '%a\\'a%' or mycol like '%bb%' or mycol like '%cc%' or mycol like '%dd%' or mycol like '%ee%' or mycol like '%ff%' or mycol like '%gg%')" );

		sResult = DB.FormAndClause("if(ifnull(I.test,0) in (100,101),'val'ue1','value2')", "val'ue1", KSQL::FAC_NORMAL, ",").str();
		sResult.CollapseAndTrim();
		CHECK (sResult == "and if(ifnull(I.test,0) in (100,101),'val'ue1','value2') = 'val'ue1'" );

		// Tests for Mike
		sResult = DB.FormAndClause("mycol", "!", KSQL::FAC_NUMERIC | KSQL::FAC_BETWEEN).str();
		sResult.CollapseAndTrim();
		CHECK (sResult == "and mycol is not null" );

		sResult = DB.FormAndClause("mycol", "!17", KSQL::FAC_NUMERIC | KSQL::FAC_BETWEEN).str();
		sResult.CollapseAndTrim();
		CHECK (sResult == "and mycol <> 17" );

		sResult = DB.FormAndClause("mycol", ">17", KSQL::FAC_NUMERIC | KSQL::FAC_BETWEEN).str();
		sResult.CollapseAndTrim();
		CHECK (sResult == "and mycol > 17" );

		sResult = DB.FormAndClause("mycol", "<17", KSQL::FAC_NUMERIC | KSQL::FAC_BETWEEN).str();
		sResult.CollapseAndTrim();
		CHECK (sResult == "and mycol < 17" );

		sResult = DB.FormAndClause("mycol", "17", KSQL::FAC_NUMERIC | KSQL::FAC_BETWEEN).str();
		sResult.CollapseAndTrim();
		CHECK (sResult == "and mycol = 17" );

		sResult = DB.FormAndClause("mycol", "0,1", KSQL::FAC_NUMERIC | KSQL::FAC_BETWEEN).str();
		sResult.CollapseAndTrim();
		CHECK (sResult == "and mycol between 0 and 1" );

		sResult = DB.FormAndClause("mycol", "0|1", KSQL::FAC_NUMERIC | KSQL::FAC_BETWEEN, "|").str();
		sResult.CollapseAndTrim();
		CHECK (sResult == "and mycol between 0 and 1" );

		sResult = DB.FormAndClause("mycol", "1,0", KSQL::FAC_NUMERIC | KSQL::FAC_BETWEEN).str();
		sResult.CollapseAndTrim();
		CHECK (sResult == "and mycol between 1 and 0" );

		sResult = DB.FormAndClause("mycol", "100,-100", KSQL::FAC_SIGNED | KSQL::FAC_BETWEEN).str();
		sResult.CollapseAndTrim();
		CHECK (sResult == "and mycol between 100 and -100" );
	}

	SECTION("FormOrderBy")
	{
		KSQL DB;
		DB.SetDBType(KSQL::DBT::MYSQL);

		KSQLString sOrderBy;
		DB.FormOrderBy("Äa, BB descend , cc ascending, dd,Ee,ff desc,gg", sOrderBy, {
			{ "Äa",       "x'x"        },
			{ "bb",       "bb"         },
			{ "cc",       "cc"         },
			{ "dd",       "yy"         },
			{ "ee",       "ee"         },
			{ "ff",       "ff"         },
			{ "gg",       "gg"         },
		});
		CHECK ( DB.GetLastError() == "" );
		auto sOB = sOrderBy.str();
		sOB.CollapseAndTrim();
		CHECK ( sOB == "order by x\\'x , bb desc , cc , yy , ee , ff desc , gg" );
	}

	SECTION("IsSelect")
	{
		CHECK ( KSQL::IsSelect("  \t\n SeleCt * from table"_ksv) == true );
		CHECK ( KSQL::IsSelect(KStringView{}) == false );
		CHECK ( KSQL::IsSelect("select"_ksv ) == true  );
		CHECK ( KSQL::IsSelect("selec"_ksv  ) == false );
		CHECK ( KSQL::IsSelect("      "_ksv ) == false );
		CHECK ( KSQL::IsSelect(",    "_ksv  ) == false );
	}

	SECTION("DoTranslations")
	{
		KSQL DB;
		KSQLString sSQL { "update xx set date={{NOW}}, {{DATETIME}} {{MAXCHAR}} {{unknown}}{{CHAR2000}} date{{PCT}} {{AUTO_INCREMENT}}, {{UTC}} {{$$}}.{{PID}}.{{DC}}{{hostname}}}}" };
		DB.BuildTranslationList(DB.m_TxList, KSQL::DBT::MYSQL);
		DB.DoTranslations(sSQL);
		CHECK ( sSQL == kFormat("update xx set date=now(), timestamp text {{{{unknown}}}}text date% auto_increment, utc_timestamp() {}.{}.{{{{{}}}}}", kGetPid(), kGetPid(), kGetHostname()) );
	}

	SECTION("EscapeFromQuotedList")
	{
		KSQL DB;
		KStringView sList = "'item1','item2','item3'";
		auto sEscaped = DB.EscapeFromQuotedList(sList);
		CHECK ( sEscaped.str() == "'item1','item2','item3'" );

		sList = "'ite\"m1','item2','item3'";
		sEscaped = DB.EscapeFromQuotedList(sList);
		CHECK ( sEscaped.str() == "'ite\"m1','item2','item3'" );
	}

	SECTION("Flags")
	{
		KSQL DB;
		CHECK ( DB.GetFlags() == KSQL::Flags::F_None );
		CHECK ( DB.SetFlags(KSQL::Flags::F_IgnoreSQLErrors | KSQL::Flags::F_IgnoreSelectKeyword) == KSQL::Flags::F_None );
		CHECK ( DB.SetFlag(KSQL::Flags::F_ReadOnlyMode) == (KSQL::Flags::F_IgnoreSQLErrors | KSQL::Flags::F_IgnoreSelectKeyword) );
		CHECK ( DB.GetFlags() == (KSQL::Flags::F_IgnoreSQLErrors | KSQL::Flags::F_IgnoreSelectKeyword | KSQL::Flags::F_ReadOnlyMode) );
		CHECK ( DB.IsFlag(KSQL::Flags::F_IgnoreSelectKeyword) == true );
		CHECK ( DB.ClearFlag(KSQL::Flags::F_IgnoreSelectKeyword) == (KSQL::Flags::F_IgnoreSQLErrors | KSQL::Flags::F_IgnoreSelectKeyword | KSQL::Flags::F_ReadOnlyMode) );
		CHECK ( DB.IsFlag(KSQL::Flags::F_IgnoreSelectKeyword) == false );
		CHECK ( DB.ClearFlags() == (KSQL::Flags::F_IgnoreSQLErrors | KSQL::Flags::F_ReadOnlyMode) );
		CHECK ( DB.GetFlags() == KSQL::Flags::F_None );
	}

	SECTION("kParallel")
	{
		KSQL DB;
		kParallelForEach(DB, [](const KROW& row)
		{
		});
	}
}

#ifdef DEKAF2_HAS_SQLITE3

#include <dekaf2/kfilesystem.h>

namespace {
KTempDir g_KSQLSQLiteTempDir;
}

//-----------------------------------------------------------------------------
TEST_CASE("KSQL-SQLite3")
//-----------------------------------------------------------------------------
{
	auto sDBFile = kFormat("{}/ksql_sqlite_test.db", g_KSQLSQLiteTempDir.Name());

	SECTION("OpenConnection")
	{
		KSQL db;
		db.SetDBType(KSQL::DBT::SQLITE3);
		db.SetDBName(sDBFile);
		CHECK ( db.OpenConnection() );
		CHECK ( db.IsConnectionOpen() );
		CHECK ( db.ConnectSummary().Contains("sqlite3") );
		db.CloseConnection();
		CHECK ( !db.IsConnectionOpen() );
	}

	SECTION("OpenConnection failure")
	{
		KSQL db;
		db.SetDBType(KSQL::DBT::SQLITE3);
		// no database name set
		CHECK ( !db.OpenConnection() );
		CHECK ( db.GetLastError().Contains("SQLITE3") );
	}

	SECTION("Create table and basic SQL")
	{
		KSQL db;
		db.SetDBType(KSQL::DBT::SQLITE3);
		db.SetDBName(sDBFile);
		REQUIRE ( db.OpenConnection() );

		CHECK ( db.ExecSQL(
			"create table if not exists TEST_KSQL (\n"
			"    anum      integer       primary key,\n"
			"    astring   varchar(100)  null,\n"
			"    bigstring text          null\n"
			")") );

		CHECK ( db.ExecSQL("insert into TEST_KSQL (anum,astring,bigstring) values (1,'row-1','big-1')") );
		CHECK ( db.GetNumRowsAffected() == 1 );

		CHECK ( db.ExecSQL("insert into TEST_KSQL (anum,astring,bigstring) values (2,'row-2','big-2')") );
		CHECK ( db.GetNumRowsAffected() == 1 );

		CHECK ( db.ExecSQL("insert into TEST_KSQL (anum,astring,bigstring) values (3,'row-3','big-3')") );
		CHECK ( db.GetNumRowsAffected() == 1 );

		CHECK ( db.ExecSQL("update TEST_KSQL set astring='updated' where anum=2") );
		CHECK ( db.GetNumRowsAffected() == 1 );

		CHECK ( db.ExecSQL("delete from TEST_KSQL where anum=3") );
		CHECK ( db.GetNumRowsAffected() == 1 );
	}

	SECTION("ExecQuery and NextRow and Get")
	{
		KSQL db;
		db.SetDBType(KSQL::DBT::SQLITE3);
		db.SetDBName(sDBFile);
		REQUIRE ( db.OpenConnection() );

		db.SetFlags(KSQL::F_IgnoreSQLErrors);
		db.ExecSQL("drop table if exists TEST_QUERY");
		db.SetFlags(KSQL::F_None);

		REQUIRE ( db.ExecSQL(
			"create table TEST_QUERY (\n"
			"    id        integer       primary key,\n"
			"    name      varchar(100)  null,\n"
			"    value     integer       null\n"
			")") );

		for (int i = 1; i <= 5; ++i)
		{
			REQUIRE ( db.ExecSQL("insert into TEST_QUERY (id,name,value) values ({},'name-{}',{})", i, i, i * 100) );
		}

		REQUIRE ( db.ExecQuery("select id, name, value from TEST_QUERY order by id") );

		CHECK ( db.GetNumCols() == 3 );

		int iRow = 0;
		while (db.NextRow())
		{
			++iRow;
			CHECK ( db.Get(1).Int32() == iRow );
			CHECK ( db.Get(2) == kFormat("name-{}", iRow) );
			CHECK ( db.Get(3).Int32() == iRow * 100 );
		}
		CHECK ( iRow == 5 );
	}

	SECTION("NextRow with KROW")
	{
		KSQL db;
		db.SetDBType(KSQL::DBT::SQLITE3);
		db.SetDBName(sDBFile);
		REQUIRE ( db.OpenConnection() );

		REQUIRE ( db.ExecQuery("select id, name, value from TEST_QUERY order by id") );

		KROW row;
		int iRow = 0;
		while (db.NextRow(row))
		{
			++iRow;
			CHECK ( row["id"]    == KString::to_string(iRow) );
			CHECK ( row["name"]  == kFormat("name-{}", iRow) );
			CHECK ( row["value"] == KString::to_string(iRow * 100) );
		}
		CHECK ( iRow == 5 );
	}

	SECTION("Range-for loop")
	{
		KSQL db;
		db.SetDBType(KSQL::DBT::SQLITE3);
		db.SetDBName(sDBFile);
		REQUIRE ( db.OpenConnection() );

		REQUIRE ( db.ExecQuery("select id, name, value from TEST_QUERY order by id") );

		int iRow = 0;
		for (auto& row : db)
		{
			++iRow;
			CHECK ( row["id"]   == KString::to_string(iRow) );
			CHECK ( row["name"] == kFormat("name-{}", iRow) );
		}
		CHECK ( iRow == 5 );
	}

	SECTION("SingleIntQuery")
	{
		KSQL db;
		db.SetDBType(KSQL::DBT::SQLITE3);
		db.SetDBName(sDBFile);
		REQUIRE ( db.OpenConnection() );

		CHECK ( db.SingleIntQuery("select count(*) from TEST_QUERY") == 5 );
		CHECK ( db.SingleIntQuery("select count(*) from TEST_QUERY where 1=0") == 0 );
		CHECK ( db.SingleIntQuery("select value from TEST_QUERY where id=3") == 300 );

		db.SetFlags(KSQL::F_IgnoreSQLErrors);
		CHECK ( db.SingleIntQuery("select count(*) from NONEXISTENT_TABLE") == -1 );
		db.SetFlags(KSQL::F_None);
	}

	SECTION("SingleStringQuery")
	{
		KSQL db;
		db.SetDBType(KSQL::DBT::SQLITE3);
		db.SetDBName(sDBFile);
		REQUIRE ( db.OpenConnection() );

		CHECK ( db.SingleStringQuery("select name from TEST_QUERY where id=2") == "name-2" );
	}

	SECTION("GetLastInsertID")
	{
		KSQL db;
		db.SetDBType(KSQL::DBT::SQLITE3);
		db.SetDBName(sDBFile);
		REQUIRE ( db.OpenConnection() );

		db.ExecSQL("drop table if exists TEST_AUTOINC");
		REQUIRE ( db.ExecSQL(
			"create table TEST_AUTOINC (\n"
			"    id        integer       primary key autoincrement,\n"
			"    name      varchar(100)  null\n"
			")") );

		CHECK ( db.ExecSQL("insert into TEST_AUTOINC (name) values ('first')") );
		CHECK ( db.GetLastInsertID() == 1 );

		CHECK ( db.ExecSQL("insert into TEST_AUTOINC (name) values ('second')") );
		CHECK ( db.GetLastInsertID() == 2 );

		CHECK ( db.ExecSQL("insert into TEST_AUTOINC (name) values ('third')") );
		CHECK ( db.GetLastInsertID() == 3 );
	}

	SECTION("Column headers")
	{
		KSQL db;
		db.SetDBType(KSQL::DBT::SQLITE3);
		db.SetDBName(sDBFile);
		REQUIRE ( db.OpenConnection() );

		REQUIRE ( db.ExecQuery("select id, name, value from TEST_QUERY order by id") );

		KROW row;
		REQUIRE ( db.NextRow(row) );
		CHECK ( row.GetName(0) == "id" );
		CHECK ( row.GetName(1) == "name" );
		CHECK ( row.GetName(2) == "value" );
	}

	SECTION("Multiple queries in sequence")
	{
		KSQL db;
		db.SetDBType(KSQL::DBT::SQLITE3);
		db.SetDBName(sDBFile);
		REQUIRE ( db.OpenConnection() );

		REQUIRE ( db.ExecQuery("select id from TEST_QUERY where id=1") );
		REQUIRE ( db.NextRow() );
		CHECK   ( db.Get(1).Int32() == 1 );

		// start another query without finishing the first
		REQUIRE ( db.ExecQuery("select id from TEST_QUERY where id=2") );
		REQUIRE ( db.NextRow() );
		CHECK   ( db.Get(1).Int32() == 2 );

		// verify we can still do a SingleIntQuery after
		CHECK ( db.SingleIntQuery("select count(*) from TEST_QUERY") == 5 );
	}

	SECTION("Error handling for bad SQL")
	{
		KSQL db;
		db.SetDBType(KSQL::DBT::SQLITE3);
		db.SetDBName(sDBFile);
		REQUIRE ( db.OpenConnection() );

		db.SetFlags(KSQL::F_IgnoreSQLErrors);
		CHECK ( !db.ExecSQL("insert into NONEXISTENT_TABLE values (1)") );
		CHECK ( db.GetLastError().size() > 0 );

		auto Guard = db.ScopedFlags(KSQL::F_IgnoreSelectKeyword);
		CHECK ( !db.ExecQuery("select bogus from BOGUS_TABLE") );
		CHECK ( db.GetLastError().size() > 0 );
		db.SetFlags(KSQL::F_None);
	}

	SECTION("UTF-8 characters")
	{
		KSQL db;
		db.SetDBType(KSQL::DBT::SQLITE3);
		db.SetDBName(sDBFile);
		REQUIRE ( db.OpenConnection() );

		db.ExecSQL("drop table if exists TEST_UTF8");
		REQUIRE ( db.ExecSQL(
			"create table TEST_UTF8 (\n"
			"    id        integer       primary key,\n"
			"    utext     text          null\n"
			")") );

		KStringView sAsian = "Chinese characters ñäöüß 一二三四五六七八九十";

		REQUIRE ( db.ExecSQL("insert into TEST_UTF8 (id,utext) values (1,'{}')", sAsian) );

		REQUIRE ( db.ExecQuery("select utext from TEST_UTF8 where id=1") );
		REQUIRE ( db.NextRow() );
		CHECK   ( db.Get(1) == sAsian );
	}

	SECTION("KROW Insert")
	{
		KSQL db;
		db.SetDBType(KSQL::DBT::SQLITE3);
		db.SetDBName(sDBFile);
		REQUIRE ( db.OpenConnection() );

		db.ExecSQL("drop table if exists TEST_KROW");
		REQUIRE ( db.ExecSQL(
			"create table TEST_KROW (\n"
			"    id        integer       primary key,\n"
			"    name      varchar(100)  null\n"
			")") );

		KROW Row("TEST_KROW");
		Row.AddCol("id",   UINT64_C(1), KCOL::PKEY);
		Row.AddCol("name", "krow-insert-1");
		CHECK ( db.Insert(Row) );

		Row.AddCol("id",   UINT64_C(2), KCOL::PKEY);
		Row.AddCol("name", "krow-insert-2");
		CHECK ( db.Insert(Row) );

		CHECK ( db.SingleIntQuery("select count(*) from TEST_KROW") == 2 );
		CHECK ( db.SingleStringQuery("select name from TEST_KROW where id=1") == "krow-insert-1" );
	}

	SECTION("KROW Update")
	{
		KSQL db;
		db.SetDBType(KSQL::DBT::SQLITE3);
		db.SetDBName(sDBFile);
		REQUIRE ( db.OpenConnection() );

		KROW Row("TEST_KROW");
		Row.AddCol("id",   UINT64_C(1), KCOL::PKEY);
		Row.AddCol("name", "krow-updated");
		CHECK ( db.Update(Row) );

		CHECK ( db.SingleStringQuery("select name from TEST_KROW where id=1") == "krow-updated" );
	}

	SECTION("KROW Delete")
	{
		KSQL db;
		db.SetDBType(KSQL::DBT::SQLITE3);
		db.SetDBName(sDBFile);
		REQUIRE ( db.OpenConnection() );

		KROW Row("TEST_KROW");
		Row.AddCol("id", UINT64_C(2), KCOL::PKEY);
		CHECK ( db.Delete(Row) );

		CHECK ( db.SingleIntQuery("select count(*) from TEST_KROW") == 1 );
	}

	SECTION("ListTables")
	{
		KSQL db;
		db.SetDBType(KSQL::DBT::SQLITE3);
		db.SetDBName(sDBFile);
		REQUIRE ( db.OpenConnection() );

		REQUIRE ( db.ListTables() );

		int iCount = 0;
		while (db.NextRow())
		{
			++iCount;
		}
		CHECK ( iCount > 0 );
	}

	SECTION("DescribeTable")
	{
		KSQL db;
		db.SetDBType(KSQL::DBT::SQLITE3);
		db.SetDBName(sDBFile);
		REQUIRE ( db.OpenConnection() );

		REQUIRE ( db.DescribeTable("TEST_QUERY") );

		int iCount = 0;
		while (db.NextRow())
		{
			++iCount;
		}
		CHECK ( iCount == 3 );
	}

	SECTION("Null query (no matching rows)")
	{
		KSQL db;
		db.SetDBType(KSQL::DBT::SQLITE3);
		db.SetDBName(sDBFile);
		REQUIRE ( db.OpenConnection() );

		REQUIRE ( db.ExecQuery("select * from TEST_QUERY where id=99999") );
		CHECK   ( !db.NextRow() );
	}

	SECTION("Stop query before fetching all rows")
	{
		KSQL db;
		db.SetDBType(KSQL::DBT::SQLITE3);
		db.SetDBName(sDBFile);
		REQUIRE ( db.OpenConnection() );

		REQUIRE ( db.ExecQuery("select * from TEST_QUERY order by id") );
		CHECK   ( db.NextRow() ); // fetch only first row
		// now start a new query
		CHECK   ( db.SingleIntQuery("select count(*) from TEST_QUERY") == 5 );
	}

	SECTION("Translations")
	{
		KSQL db;
		db.SetDBType(KSQL::DBT::SQLITE3);

		KSQLString sSQL("select {{NOW}}, {{DATETIME}}, {{MAXCHAR}}");
		db.BuildTranslationList(db.m_TxList, KSQL::DBT::SQLITE3);
		db.DoTranslations(sSQL);
		CHECK ( sSQL == "select now(), timestamp, text" );
	}

	SECTION("Empty result set then reuse connection")
	{
		KSQL db;
		db.SetDBType(KSQL::DBT::SQLITE3);
		db.SetDBName(sDBFile);
		REQUIRE ( db.OpenConnection() );

		REQUIRE ( db.ExecQuery("select * from TEST_QUERY where 1=0") );
		CHECK   ( !db.NextRow() );

		// reuse connection
		CHECK ( db.SingleIntQuery("select count(*) from TEST_QUERY") == 5 );

		REQUIRE ( db.ExecQuery("select id from TEST_QUERY order by id") );
		REQUIRE ( db.NextRow() );
		CHECK   ( db.Get(1).Int32() == 1 );
	}
}

#endif
