#include "catch.hpp"

#include <dekaf2/core/init/kcompatibility.h>

#ifdef DEKAF2_HAS_SQLITE3

#include <dekaf2/data/sql/ksqlite.h>
#include <dekaf2/system/filesystem/kfilesystem.h>
#include <atomic>
#include <thread>
#include <tuple>
#include <vector>

using namespace dekaf2;

namespace {
KTempDir TempDir;
}

TEST_CASE("KSQLite")
{
	SECTION("basic execution and cursor")
	{
		auto sFilename = kFormat("{}/sqlite_basic.db", TempDir.Name());

		KSQLite::Database db(sFilename, KSQLite::Mode::READWRITECREATE);
		REQUIRE ( db.IsOpen() );

		REQUIRE ( db.ExecSQL(
						 "create table TEST_KSQL (\n"
						 "    anum      integer       primary key,\n"
						 "    astring   varchar(100)  null,\n"
						 "    bigstring BLOB          null\n"
						 ")") );

		for (int ct = 1; ct < 1000; ++ct)
		{
			auto Result = db.ExecSQL("insert into TEST_KSQL (anum,astring,bigstring) values (NULL,?1,?2)",
			                         KString::to_string(ct), KString::to_string(ct+1000));
			if (!Result)
			{
				FAIL_CHECK (Result.Error());
			}
			CHECK ( Result.AffectedRows() == 1   );
			CHECK ( Result.LastInsertID() == std::size_t(ct) );
		}

		CHECK ( db.SingleIntQuery("select count(*) from TEST_KSQL") == 999 );
		CHECK ( db.HasTable("TEST_KSQL")   == true  );
		CHECK ( db.HasTable("NO_SUCH_ONE") == false );

		{
			auto Query = db.ExecQuery("select anum,astring,bigstring from TEST_KSQL where astring=?1", 91);
			REQUIRE ( Query.Next() );
			auto& Row = Query.GetRow();
			CHECK ( Row.size() == 3 );
			CHECK ( Row.empty() == false );
			CHECK ( Row.GetColIndex("bigstring") == 3 );
			CHECK ( Row.GetColName(2) == "astring" );
			CHECK ( Row.Get<int32_t>(2)  == 91   );
			CHECK ( Row.Get<int64_t>(1)  == 91   );
			CHECK ( Row.Get<uint64_t>(3) == 1091 );
			CHECK ( Row.Get<KString>(2)  == "91" );
			CHECK ( Row.Get<int32_t>(10) == 0    );
			CHECK ( Row.Get<KString>("astring")     == "91" );
			CHECK ( Row.Get<int32_t>("bigstring")   == 1091 );
			CHECK ( Row.Get<KString>("no_such_col") == ""   );

			// Get<T> takes any integer or floating point type and enums
			enum class TestEnum : int16_t { Off = 0, On = 91 };
			CHECK ( Row.Get<short>(2)         == 91 );
			CHECK ( Row.Get<unsigned long>(2) == 91 );
			CHECK ( Row.Get<float>(2)         == 91.0f );
			CHECK ( Row.Get<time_t>(2)        == 91 );
			CHECK ( Row.Get<TestEnum>(2)      == TestEnum::On );

			// explicit conversion through the Column accessor
			CHECK ( Row.Col(2).Int32()            == 91   );
			CHECK ( Row.Col(1).Int64()            == 91   );
			CHECK ( Row.Col(3).UInt64()           == 1091 );
			CHECK ( Row.Col("astring").String()   == "91" );
			CHECK ( Row.Col("astring").IsNull()   == false );
			CHECK ( Row.Col(10).Int64()           == 0    );
			CHECK ( Row[2].Int32()                == 91   );
			CHECK ( Row["astring"].String()       == "91" );
			CHECK ( Row["bigstring"].UInt64()     == 1091 );
			CHECK ( Query.Next() == false );
			CHECK ( Query.Done() );
		}

		{
			int iCt = 100;
			for (auto& Row : db.ExecQuery("select anum,astring,bigstring from TEST_KSQL where anum > 100 and anum < 110"))
			{
				++iCt;
				CHECK ( Row.size() == 3 );
				CHECK ( Row.Get<int32_t>(1) == iCt );
				CHECK ( Row.Get<int32_t>(2) == iCt );
				CHECK ( Row.Get<int32_t>(3) == iCt + 1000 );
			}
			CHECK ( iCt == 109 );
		}
	}

	SECTION("statement cache admission on second sight")
	{
		auto sFilename = kFormat("{}/sqlite_cache.db", TempDir.Name());

		KSQLite::Database db(sFilename, KSQLite::Mode::READWRITECREATE);
		REQUIRE ( db.ExecSQL("create table c (n integer)") );

		auto iCompiled = db.Compilations();
		auto iHits     = db.CacheHits();
		auto iCached   = db.CachedStatements();

		// first sight compiles, second sight compiles and admits, third hits
		CHECK ( db.ExecSQL("insert into c (n) values (?1)", 1) );
		CHECK ( db.Compilations()     == iCompiled + 1 );
		CHECK ( db.CachedStatements() == iCached       );

		CHECK ( db.ExecSQL("insert into c (n) values (?1)", 2) );
		CHECK ( db.Compilations()     == iCompiled + 2 );
		CHECK ( db.CachedStatements() == iCached + 1   );

		CHECK ( db.ExecSQL("insert into c (n) values (?1)", 3) );
		CHECK ( db.Compilations()     == iCompiled + 2 );
		CHECK ( db.CacheHits()        == iHits + 1     );
		CHECK ( db.CachedStatements() == iCached + 1   );

		CHECK ( db.SingleIntQuery("select count(*) from c") == 3 );

		// distinct one-off texts never enter the cache
		iCached = db.CachedStatements();
		CHECK ( !db.ExecSQL("insert into c (n) values (?1, ?1)", 4) ); // wrong arity, and..
		CHECK ( db.ExecSQL("insert into c (n) values (4)")  );  // ..no-parameter calls bypass the cache
		CHECK ( db.ExecSQL("insert into c (n) values (?1) -- one-off A", 5) );
		CHECK ( db.ExecSQL("insert into c (n) values (?1) -- one-off B", 6) );
		CHECK ( db.CachedStatements() == iCached );

		// the same SQL nested in an open cursor compiles a second instance
		{
			static constexpr KStringView sSelect = "select n from c order by n";
			auto Q1 = db.ExecQuery(sSelect);
			auto Q2 = db.ExecQuery(sSelect);
			REQUIRE ( Q1.Next() );
			REQUIRE ( Q2.Next() );
			CHECK ( Q1.GetRow().Get<int64_t>(1) == Q2.GetRow().Get<int64_t>(1) );
		}
	}

	SECTION("statement cache disabled")
	{
		auto sFilename = kFormat("{}/sqlite_nocache.db", TempDir.Name());

		KSQLite::Options Opts;
		Opts.iMode           = KSQLite::Mode::READWRITECREATE;
		Opts.iStatementCache = 0;

		KSQLite::Database db(sFilename, Opts);
		REQUIRE ( db.ExecSQL("create table c (n integer)") );

		for (int i = 0; i < 4; ++i)
		{
			CHECK ( db.ExecSQL("insert into c (n) values (?1)", i) );
		}

		CHECK ( db.CachedStatements() == 0 );
		CHECK ( db.SingleIntQuery("select count(*) from c") == 4 );
	}

	SECTION("ExecResult errors and SetThrow")
	{
		auto sFilename = kFormat("{}/sqlite_errors.db", TempDir.Name());

		KSQLite::Database db(sFilename, KSQLite::Mode::READWRITECREATE);

		auto Result = db.ExecSQL("this is not sql");
		CHECK ( !Result );
		CHECK ( Result.ErrorCode() != 0 );
		CHECK ( Result.Error().empty() == false );

		db.SetThrow(true);
		CHECK_THROWS_AS ( db.ExecSQL("this is not sql"), KSQLite::Exception );
		CHECK_THROWS_AS ( db.ExecQuery("neither is this"), KSQLite::Exception );
		db.SetThrow(false);

		auto Query = db.ExecQuery("still not sql");
		CHECK ( !Query );
		CHECK ( Query.ErrorCode() != 0 );
		CHECK ( Query.Next() == false );
	}

	SECTION("NULL handling and empty string binds")
	{
		auto sFilename = kFormat("{}/sqlite_null.db", TempDir.Name());

		KSQLite::Database db(sFilename, KSQLite::Mode::READWRITECREATE);
		REQUIRE ( db.ExecSQL("create table nn (id integer primary key, txt text not null, opt text null)") );

		// a default constructed (null-data) string view binds the empty string
		// and satisfies the NOT NULL constraint
		CHECK ( db.ExecSQL("insert into nn (id, txt, opt) values (1, ?1, ?2)", KStringView{}, nullptr) );
		CHECK ( db.SingleIntQuery("select txt is null from nn where id=1") == 0 );
		CHECK ( db.SingleIntQuery("select length(txt) from nn where id=1") == 0 );

		// binding nullptr binds SQL NULL - which the NOT NULL column rejects
		CHECK ( !db.ExecSQL("insert into nn (id, txt) values (2, ?1)", nullptr) );

		auto Query = db.ExecQuery("select opt from nn where id=1");
		REQUIRE ( Query.Next() );
		auto& Row = Query.GetRow();
		CHECK ( Row.IsNull(1) );
		CHECK ( Row.IsNull("opt") );
		CHECK ( Row.Get<KString>(1) == "" );
		CHECK ( Row.GetOr<int64_t>(1, 42) == 42 );
		CHECK ( Row.GetOr<int64_t>("opt", 42) == 42 );
#ifdef KSQLITE_HAS_OPTIONAL
		CHECK ( Row.GetOptional<int64_t>(1).has_value() == false );
		CHECK ( Row.GetOptional<int64_t>("opt").has_value() == false );
#endif
	}

	SECTION("multi statement SQL without parameters")
	{
		auto sFilename = kFormat("{}/sqlite_multi.db", TempDir.Name());

		KSQLite::Database db(sFilename, KSQLite::Mode::READWRITECREATE);

		CHECK ( db.ExecSQL(
			"CREATE TABLE multi1 (id INTEGER PRIMARY KEY, val TEXT);"
			"INSERT INTO multi1 (id, val) VALUES (1, 'one');"
			"INSERT INTO multi1 (id, val) VALUES (2, 'two');"
			"INSERT INTO multi1 (id, val) VALUES (3, 'three');"
		) );

		CHECK ( db.SingleIntQuery   ("SELECT count(*) FROM multi1") == 3 );
		CHECK ( db.SingleStringQuery("SELECT val FROM multi1 WHERE id=2") == "two" );
	}

	SECTION("binary safe blob roundtrip")
	{
		auto sFilename = kFormat("{}/sqlite_blob.db", TempDir.Name());

		KSQLite::Database db(sFilename, KSQLite::Mode::READWRITECREATE);
		REQUIRE ( db.ExecSQL("CREATE TABLE bintest (id INTEGER PRIMARY KEY, data BLOB)") );

		KStringView sWithNul("hello\0world", 11);
		CHECK ( sWithNul.size() == 11 );

		CHECK ( db.ExecSQL("INSERT INTO bintest (id, data) VALUES (1, ?1)",
		                   KSQLite::Blob(sWithNul.data(), sWithNul.size())) );

		auto Query = db.ExecQuery("SELECT data FROM bintest WHERE id=1");
		REQUIRE ( Query.Next() );
		auto sData = Query.GetRow().Get<KString>(1);
		CHECK ( sData.size() == 11 );
		CHECK ( KStringView(sData) == sWithNul );
	}

	SECTION("transactions and savepoints")
	{
		auto sFilename = kFormat("{}/sqlite_txn.db", TempDir.Name());

		KSQLite::Database db(sFilename, KSQLite::Mode::READWRITECREATE);
		REQUIRE ( db.ExecSQL("create table t (n integer)") );

		// rollback on destruction
		{
			KSQLite::Transaction Txn(db);
			CHECK ( Txn.IsNested() == false );
			CHECK ( Txn.ExecSQL("insert into t values (1)") );
		}
		CHECK ( db.SingleIntQuery("select count(*) from t") == 0 );

		// commit persists
		{
			KSQLite::Transaction Txn(db);
			CHECK ( Txn.ExecSQL("insert into t values (1)") );
			CHECK ( Txn.Commit() );
		}
		CHECK ( db.SingleIntQuery("select count(*) from t") == 1 );

		// a nested transaction becomes a savepoint - inner rollback keeps
		// the outer writes
		{
			KSQLite::Transaction Outer(db);
			CHECK ( Outer.ExecSQL("insert into t values (2)") );
			{
				KSQLite::Transaction Inner(db);
				CHECK ( Inner.IsNested() );
				CHECK ( Inner.ExecSQL("insert into t values (3)") );
			}
			CHECK ( Outer.Commit() );
		}
		CHECK ( db.SingleIntQuery("select count(*) from t")            == 2 );
		CHECK ( db.SingleIntQuery("select count(*) from t where n=2")  == 1 );
		CHECK ( db.SingleIntQuery("select count(*) from t where n=3")  == 0 );
	}

	SECTION("ExecMany")
	{
		auto sFilename = kFormat("{}/sqlite_many.db", TempDir.Name());

		KSQLite::Database db(sFilename, KSQLite::Mode::READWRITECREATE);
		REQUIRE ( db.ExecSQL("create table em (n integer unique, s text)") );

		std::vector<std::tuple<int64_t, KString>> Params;
		Params.push_back(std::make_tuple(1, "one"));
		Params.push_back(std::make_tuple(2, "two"));
		Params.push_back(std::make_tuple(3, "three"));

		auto Result = db.ExecMany("insert into em (n, s) values (?1, ?2)", Params);
		CHECK ( Result );
		CHECK ( Result.AffectedRows() == 3 );
		CHECK ( db.SingleIntQuery("select count(*) from em") == 3 );

		// a failing row rolls back the whole batch
		std::vector<std::tuple<int64_t, KString>> Bad;
		Bad.push_back(std::make_tuple(10, "ten"));
		Bad.push_back(std::make_tuple(1,  "duplicate"));
		Bad.push_back(std::make_tuple(11, "eleven"));

		Result = db.ExecMany("insert into em (n, s) values (?1, ?2)", Bad);
		CHECK ( !Result );
		CHECK ( db.SingleIntQuery("select count(*) from em") == 3 );
	}

	SECTION("foreign keys are enforced by default")
	{
		auto sFilename = kFormat("{}/sqlite_fk.db", TempDir.Name());

		{
			KSQLite::Database db(sFilename, KSQLite::Mode::READWRITECREATE);
			REQUIRE ( db.ExecSQL("create table parent (id integer primary key);"
			                     "create table child (pid integer references parent(id));") );

			CHECK ( !db.ExecSQL("insert into child values (?1)", 42) );
			CHECK ( db.ExecSQL("insert into parent values (?1)", 42) );
			CHECK ( db.ExecSQL("insert into child values (?1)", 42) );
		}

		{
			KSQLite::Options Opts;
			Opts.iMode        = KSQLite::Mode::READWRITE;
			Opts.bForeignKeys = false;

			KSQLite::Database db(sFilename, Opts);
			CHECK ( db.ExecSQL("insert into child values (?1)", 4711) );
			CHECK ( db.ExecSQL("delete from child where pid=?1", 4711) );
		}
	}

	SECTION("concurrent use of one connection throws")
	{
		auto sFilename = kFormat("{}/sqlite_conc.db", TempDir.Name());

		KSQLite::Database db(sFilename, KSQLite::Mode::READWRITECREATE);
		REQUIRE ( db.ExecSQL("create table t (n integer)") );

		std::atomic<int> iCaught { 0 };
		std::atomic<int> iPassed { 0 };

		// another thread must not slip into an open transaction
		{
			KSQLite::Transaction Txn(db);
			CHECK ( Txn.ExecSQL("insert into t values (1)") );

			std::thread Intruder([&]()
			{
				try
				{
					db.ExecSQL("insert into t values (2)");
					++iPassed;
				}
				catch (const KSQLite::ConcurrencyError&)
				{
					++iCaught;
				}
			});
			Intruder.join();

			CHECK ( Txn.Commit() );
		}

		CHECK ( iCaught == 1 );
		CHECK ( iPassed == 0 );
		CHECK ( db.SingleIntQuery("select count(*) from t") == 1 );

		// the same for an open cursor
		{
			auto Query = db.ExecQuery("select n from t");
			REQUIRE ( Query.Next() );

			std::thread Intruder([&]()
			{
				try
				{
					db.ExecSQL("insert into t values (3)");
					++iPassed;
				}
				catch (const KSQLite::ConcurrencyError&)
				{
					++iCaught;
				}
			});
			Intruder.join();
		}

		CHECK ( iCaught == 2 );
		CHECK ( iPassed == 0 );

		// handover between threads is legal as long as use is not simultaneous
		std::thread Sequential([&]()
		{
			CHECK ( db.ExecSQL("insert into t values (4)") );
		});
		Sequential.join();

		CHECK ( db.SingleIntQuery("select count(*) from t") == 2 );
	}

	SECTION("one connection per thread")
	{
		auto sFilename = kFormat("{}/sqlite_threads.db", TempDir.Name());

		KSQLite::Options Opts;
		Opts.iMode = KSQLite::Mode::READWRITECREATE;
		Opts.bWAL  = true;

		KSQLite::Database Prototype(sFilename, Opts);
		REQUIRE ( Prototype.ExecSQL("create table many (tid integer, n integer)") );

		constexpr int iThreads = 8;
		constexpr int iRows    = 50;

		std::atomic<int> iFailed { 0 };

		std::vector<std::thread> Threads;

		for (int tid = 0; tid < iThreads; ++tid)
		{
			Threads.push_back(std::thread([&, tid]()
			{
				// the copy constructor opens a new connection to the same file
				KSQLite::Database db(Prototype);

				if (!db.IsOpen())
				{
					++iFailed;
					return;
				}

				for (int n = 0; n < iRows; ++n)
				{
					if (!db.ExecSQL("insert into many (tid, n) values (?1, ?2)", tid, n))
					{
						++iFailed;
					}
				}
			}));
		}

		for (auto& Thread : Threads)
		{
			Thread.join();
		}

		CHECK ( iFailed == 0 );
		CHECK ( Prototype.SingleIntQuery("select count(*) from many") == iThreads * iRows );
	}
}

#endif
