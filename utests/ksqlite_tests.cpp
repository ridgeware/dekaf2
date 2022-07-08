#include "catch.hpp"

#include <dekaf2/bits/kcppcompat.h>

#ifdef DEKAF2_HAS_SQLITE3

#include <dekaf2/ksqlite.h>
#include <dekaf2/kfilesystem.h>
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
}

#endif

