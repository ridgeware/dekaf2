#include "catch.hpp"

#include <dekaf2/core/init/kcompatibility.h>

#ifdef DEKAF2_HAS_SQLITE3

#include <dekaf2/data/sql/ksqlite.h>
#include <dekaf2/system/filesystem/kfilesystem.h>
#include <vector>

using namespace dekaf2;

namespace {
KTempDir TempDir;
}

TEST_CASE("KSQLite")
{
	SECTION("Basic creation")
	{
		auto sFilename = kFormat("{}/sqlite_test.db", TempDir.Name());

		KSQLite::Database db(sFilename, KSQLite::Mode::READWRITECREATE);

		if (!db.ExecuteVoid (
						 "create table TEST_KSQL (\n"
						 "    anum      integer       primary key,\n"
						 "    astring   varchar(100)  null,\n"
						 "    bigstring BLOB          null\n"
						 ")"))
		{
			FAIL_CHECK (db.Error());
		}

		auto Insert = db.Prepare("insert into TEST_KSQL (anum,astring,bigstring) values (NULL,?1,?2)");

		for (int ct = 1; ct < 1000; ++ct)
		{
			bool bOK = Insert.Bind(1, KString::to_string(ct));
			CHECK ( bOK );
			bOK = Insert.Bind(2, KString::to_string(ct+1000));
			CHECK ( bOK );
			if (!Insert.Execute())
			{
				FAIL_CHECK (db.Error());
			}
			bOK = Insert.Reset();
			CHECK ( bOK );
		}

		{
			auto Search = db.Prepare("select anum,astring,bigstring from TEST_KSQL where astring=?1");
			CHECK ( Search.Bind(1, 91) == true );
			CHECK ( Search.NextRow() == true );
			auto Row = Search.GetRow();
			CHECK ( Row.size() == 3 );
			CHECK ( Row.empty() == false );
			CHECK ( Row.GetColIndex("bigstring") == 3 );
			CHECK ( Row.Col(2).Int32() == 91 );
			CHECK ( Row.Col(1).Int32() == 91 );
			CHECK ( Row.Col(3).Int32() == 1091 );
			CHECK ( Row.Col(10).Int32() == 0 );
		}

		{
			auto Range = db.Prepare("select anum,astring,bigstring from TEST_KSQL where anum > 100 and anum < 110");
			auto Row = Range.GetRow();

			Range.Execute();

			CHECK ( Row["astring"].IsText() );
			CHECK ( Row["anum"].IsInteger() );
			CHECK ( Row["bigstring"].IsText() );

			int iCt = 100;
			for (auto it : Range)
			{
				++iCt;
				CHECK ( it.size() == 3 );
				CHECK ( it.empty() == false );
				CHECK ( it.Col(2).Int32() == iCt );
				CHECK ( it.Col(1).Int32() == iCt );
				CHECK ( it.Col(3).Int32() == iCt + 1000 );
				CHECK ( it[2].Int32() == iCt );
				CHECK ( it[1].Int32() == iCt );
				CHECK ( it[3].Int32() == iCt + 1000 );
				CHECK ( it["astring"].Int32() == iCt );
				CHECK ( it["anum"].Int32() == iCt );
				CHECK ( it["bigstring"].Int32() == iCt + 1000 );
			}
		}

		{
			KSQLite::Statement Null;
			Null.Execute();
			auto Row = Null.GetRow();
			CHECK ( Row.Col(10).Int32() == 0 );
		}
	}

	SECTION("Multi-statement ExecuteVoid")
	{
		auto sFilename = kFormat("{}/sqlite_multi_exec.db", TempDir.Name());
		KSQLite::Database db(sFilename, KSQLite::Mode::READWRITECREATE);

		// multiple statements in a single call
		bool bOK = db.ExecuteVoid(
			"CREATE TABLE multi1 (id INTEGER PRIMARY KEY, val TEXT);"
			"INSERT INTO multi1 (id, val) VALUES (1, 'one');"
			"INSERT INTO multi1 (id, val) VALUES (2, 'two');"
			"INSERT INTO multi1 (id, val) VALUES (3, 'three');"
		);
		CHECK ( bOK );

		auto Result = db.Execute("SELECT val FROM multi1 ORDER BY id");
		REQUIRE ( Result.size() == 3 );
		CHECK ( Result[0]["val"] == "one"   );
		CHECK ( Result[1]["val"] == "two"   );
		CHECK ( Result[2]["val"] == "three" );
	}

	SECTION("Multi-statement Execute")
	{
		auto sFilename = kFormat("{}/sqlite_multi_query.db", TempDir.Name());
		KSQLite::Database db(sFilename, KSQLite::Mode::READWRITECREATE);

		db.ExecuteVoid("CREATE TABLE mq (id INTEGER PRIMARY KEY, val TEXT);"
		               "INSERT INTO mq VALUES (1, 'a');"
		               "INSERT INTO mq VALUES (2, 'b');");

		// multiple SELECTs in one Execute call - results from all statements are collected
		auto Result = db.Execute("SELECT val FROM mq WHERE id=1; SELECT val FROM mq WHERE id=2");
		REQUIRE ( Result.size() == 2 );
		CHECK ( Result[0]["val"] == "a" );
		CHECK ( Result[1]["val"] == "b" );
	}

	SECTION("Binary-safe Bind roundtrip with NUL bytes")
	{
		auto sFilename = kFormat("{}/sqlite_binary.db", TempDir.Name());
		KSQLite::Database db(sFilename, KSQLite::Mode::READWRITECREATE);

		db.ExecuteVoid("CREATE TABLE bintest (id INTEGER PRIMARY KEY, data BLOB)");

		KStringView sWithNul("hello\0world", 11);
		CHECK ( sWithNul.size() == 11 );

		auto Insert = db.Prepare("INSERT INTO bintest (id, data) VALUES (1, ?1)");
		CHECK ( Insert.Bind(1, static_cast<void*>(const_cast<char*>(sWithNul.data())), sWithNul.size(), true) );
		CHECK ( Insert.Execute() );

		auto Select = db.Prepare("SELECT data FROM bintest WHERE id=1");
		CHECK ( Select.NextRow() );
		auto Row = Select.GetRow();
		auto col = Row.Col(1);
		CHECK ( col.size() == 11 );
		CHECK ( KStringView(col.String().data(), col.size()) == sWithNul );
	}

	SECTION("Ad-hoc Execute reads back BLOB with NUL bytes")
	{
		auto sFilename = kFormat("{}/sqlite_nul_adhoc.db", TempDir.Name());
		KSQLite::Database db(sFilename, KSQLite::Mode::READWRITECREATE);

		db.ExecuteVoid("CREATE TABLE nultest (id INTEGER PRIMARY KEY, data BLOB)");

		// insert BLOB with NUL byte via Bind (the only correct way)
		KStringView sWithNul("before\0after", 12);
		auto Insert = db.Prepare("INSERT INTO nultest (id, data) VALUES (1, ?1)");
		CHECK ( Insert.Bind(1, static_cast<void*>(const_cast<char*>(sWithNul.data())), sWithNul.size(), true) );
		CHECK ( Insert.Execute() );

		// verify ad-hoc Execute can read it back (uses sqlite3_column_text internally)
		auto Result = db.Execute("SELECT data FROM nultest WHERE id=1");
		REQUIRE ( Result.size() == 1 );
		// ad-hoc Execute returns strings via sqlite3_column_text which truncates at NUL,
		// so the value will be "before" - this is a known limitation of the ad-hoc API.
		// For binary data, use Prepare + Bind + Column.
		CHECK ( Result[0]["data"] == "before" );
	}
}

#endif

