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
#include <dekaf2/ksystem.h>
#include <dekaf2/kfilesystem.h>
#include <dekaf2/kstringutils.h>

using namespace dekaf2;

enum {
	MAX_FORMVARS      = 50,
	MAXLEN_REQBUFFER  = 5000
};

typedef const char* LPCTSTR;

int64_t DumpRows (KSQL& db);
void  TestEscapes (int iTestNum, LPCTSTR pszString, LPCTSTR pszStringX, int iDBType);

KStringViewZ g_sDbcFile;

#define ORADATEFORM  "YYYYMMDDHH24MISS"

void SimulateLostConnection (KSQL& db);

//-----------------------------------------------------------------------------
KString HereDoc (KStringView sString)
//-----------------------------------------------------------------------------
{
	KString sReturnMe;
	bool    bTrim = true;

	for (auto& ch : sString)
	{
		if (bTrim && ((ch <= ' ') || (ch == '|'))) {
			/* skip it */
		}
		else if (ch == '\n') {
			sReturnMe += ch;
			bTrim = true;
		}
		else {
			sReturnMe += ch;
			bTrim = false;
		}
	}

	kDebugLog (3, "HereDoc(from):\n{}", sString);
	kDebugLog (3, "HereDoc(to):\n{}", sReturnMe);

	return (sReturnMe);

} // HereDoc

//-----------------------------------------------------------------------------
KString GenRandomString(int iMinSize, int iMaxSize)
//-----------------------------------------------------------------------------
{
	auto iSize = kRandom(iMinSize, iMaxSize);

	KString sRandom;

	for (decltype(iSize) iCt = 0; iCt < iSize; ++iCt)
	{
		// select characters from a..z plus 3 that we convert into space
		auto iCh = kRandom(97, 125);

		if (iCh > 122)
		{
			if (sRandom && sRandom.back() != ' ')
			{
				sRandom += ' ';
			}
		}
		else
		{
			sRandom += static_cast<char>(iCh);
		}
	}

	return sRandom;

} // GenRandomString

//-----------------------------------------------------------------------------
static bool SqlServerIdentityInsert (KSQL& db, LPCTSTR pszTablename, KStringView sOnOff)
//-----------------------------------------------------------------------------
{
	if (db.GetDBType() == KSQL::DBT::SQLSERVER ||
		db.GetDBType() == KSQL::DBT::SQLSERVER15)
	{
		// avoid this errors:
		// Cannot insert explicit value for identity column in table 'XXX' when IDENTITY_INSERT is set to OFF.
		// Explicit value must be specified for identity column in table 'XXX' either when IDENTITY_INSERT is set to ON or when a replication user is inserting into a NOT FOR REPLICATION identity column.
		return (db.ExecSQL ("set identity_insert {} {}", pszTablename, sOnOff));
	}
	else
	{
		return (true);
	}

} // SqlServerIdentityInsertOn

//-----------------------------------------------------------------------------
void Check_CtSend(KSQL& db)
//-----------------------------------------------------------------------------
{
	// check if this is a SQLServer connection
	if (db.GetDBType() != KSQL::DBT::SQLSERVER &&
		db.GetDBType() != KSQL::DBT::SQLSERVER15)
	{
		return;
	}

	// yes - setup the error case by issuing nonsense requests
	auto iOld = db.SetFlags (KSQL::F_IgnoreSQLErrors);

	db.ExecSQL ("drop table BOGUS_TABLE");
	db.ExecSQL ("drop table BOGUS_TABLE");

	db.ExecQuery ("select bogus from BOGUS order by fubar");
	db.ExecQuery ("select bogus from BOGUS order by fubar");

	db.ExecSQL (
		"create table TEST1_KSQL (\n"
		"    anum      int           not null,\n"
		"    astring   char(100)     null\n"
		")");

	db.ExecSQL ("insert into TEST1_KSQL (anum,astring) values (1,'row-1')");
	if (db.GetNumRowsAffected() != 1)
	{
		kDebug (1, "GetNumRowsAffected() = {}, but should be 1", db.GetNumRowsAffected());
	}

	db.ExecSQL ("update TEST1_KSQL set anum=2 where astring='row-1'");

	if (db.GetNumRowsAffected() != 1)
	{
		kDebug (1, "GetNumRowsAffected() = {}, but should be 1", db.GetNumRowsAffected());
	}

	auto iNum = db.SingleIntQuery ("select anum from TEST1_KSQL where astring='row-1'");
	if (iNum != 2)
	{
		kDebug (1, "did not get anum of 2, but got {}", iNum);
	}

	db.ExecSQL ("drop table TEST1_KSQL");

	db.SetFlags (iOld);

} // Check_CtSend

void KillConnectionTest(KSQL& db)
{
	// check if this is a MySQL connection
	if (db.GetDBType() != KSQL::DBT::MYSQL)
	{
		return;
	}

	// create a second connection
	KSQL db2(db);

	CHECK( db2.EnsureConnected() );

	if (!db2.IsConnectionOpen())
	{
		// stop here..
		return;
	}

	auto iConnectionID = db2.GetConnectionID();

	CHECK ( iConnectionID > 0 );

	CHECK ( db.KillConnection(iConnectionID) );

	// the connection is canceled, and will not be restarted at the first trial
	auto iNewConnectionID = db2.SingleIntRawQuery("SELECT CONNECTION_ID()");

	CHECK ( iNewConnectionID == -1 );

	// now the connection will be reestablished, as the ID had been removed from
	// the list of voluntarily canceled connections
	iNewConnectionID = db2.SingleIntRawQuery("SELECT CONNECTION_ID()");

	CHECK ( iNewConnectionID > 0 );
	CHECK ( iConnectionID != iNewConnectionID );
}

//-----------------------------------------------------------------------------
TEST_CASE("KSQL")
//-----------------------------------------------------------------------------
{
	KStringView QUOTES1   = "Fred's Fishing Pole";
	KStringView QUOTES2   = "Fred's `fishing` pole's \"longer\" than mine.";
	KStringView SLASHES1  = "This is a \\l\\i\\t\\t\\l\\e /s/l/a/s/h test.";
	KStringView SLASHES2  = "This <b>is</b>\\n a string\\r with s/l/a/s/h/e/s, \\g\\e\\t\\ i\\t\\???";
	KStringView ASIAN1    = "Chinese characters ñäöüß 一二三四五六七八九十";
	KStringView ASIAN2    = "Chinese characters ñäöüß 十九八七六五四三二一";

	// - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	// test KSQL class...
	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
	SECTION("KSQL::constructor and destructor (no connect info)...")
	{
		KSQL db;
	}

	SECTION("KSQL: full suite")
	{
		KSQL db; // <-- shared across the remaining tests

		if (g_sDbcFile.empty())
		{
			return;  // <--- other other tests are useless
		}

		if (!kFileExists (g_sDbcFile))
		{
			KString sErr;
			sErr.Format ("failed to find dbc file: {}", g_sDbcFile);
			FAIL_CHECK (sErr);
			return;  // <--- other other tests are useless
		}
		else if (!db.LoadConnect (g_sDbcFile))
		{
			FAIL_CHECK (db.GetLastError());
			return;  // <--- other other tests are useless
		}

		if (!db.OpenConnection())
		{
			INFO ("FAILED TO CONNECT TO: " << db.ConnectSummary());
			FAIL (db.GetLastError());  // <-- all other tests will be useless so ABORT
		}

		// check CTLIB desync case
		Check_CtSend(db);

		// establish BASE STATE by dropping tables from possible prior runs
		kDebugLog (1, "flag test: F_IgnoreSQLErrors (should only produce DBG output)...");
		db.SetFlags (KSQL::F_IgnoreSQLErrors);
		db.ExecSQL ("drop table TEST_KSQL");
		db.ExecSQL ("drop table TEST1_KSQL");
		db.ExecSQL ("drop table TEST2_KSQL");
		db.ExecSQL ("drop table TEST_KSQL_BLOB");
		db.ExecSQL ("drop table BOGUS_TABLE");
		db.ExecSQL ("drop table BOGUS_TABLE");
		db.ExecSQL ("drop table BOGUS_TABLE");
		db.SetFlags (0);
	
		kDebugLog (1, " ");
		kDebugLog (1, "ExecSQL() test (should be no errors):");

		if (!db.IsConnectionOpen())
		{
			return; // bail now --> all the rest of the tests will just blow up anyway
		}

		KillConnectionTest(db);

		{
			constexpr KStringViewZ sBefore   = "insert into FRED values ('this is {{not}} a valid {{token}}', {{NOW}})";
			KStringViewZ sExpected;
			switch (db.GetDBType())
			{
				case KSQL::DBT::MYSQL:
				case KSQL::DBT::SQLITE3:
					sExpected = "insert into FRED values ('this is {{not}} a valid {{token}}', now())";
					break;

				case KSQL::DBT::SQLSERVER:
				case KSQL::DBT::SQLSERVER15:
					sExpected = "insert into FRED values ('this is {{not}} a valid {{token}}', getdate())";
					break;

				default:
					sExpected = "unknown";
					break;
			}

			KString sSQL(sBefore);
			db.DoTranslations (sSQL);
		}

		kDebugLog (1, "NEGATIVE TEST: exception handling for bad ojbect");

		db.SetFlags (KSQL::F_IgnoreSQLErrors);
		if (db.ExecQuery ("select bogus from BOGUS order by fubar"))
		{
			FAIL_CHECK ("ExecQuery should have returned FALSE");
		}

		kDebugLog (1, "NEGATIVE TEST: exception handling for invalid sql function");

		if (db.ExecQuery ("select junk('fred')"))
		{
			FAIL_CHECK ("ExecQuery should have returned FALSE");
		}

		db.SetFlags (0);

		// simple table tests

		if (!db.ExecSQL (
			"create table TEST1_KSQL (\n"
			"    anum      int           not null,\n"
			"    astring   char(100)     null,\n"
			"    adate     {{DATETIME}}  not null default {{NOW}}\n"
			")"))
		{
			INFO (db.GetLastSQL());
			FAIL_CHECK (db.GetLastError());
		}

		if (!db.ExecSQL ("insert into TEST1_KSQL (anum,astring) values (1,'row-1')"))
		{
			INFO (db.GetLastSQL());
			FAIL_CHECK (db.GetLastError());
		}

		if (db.GetNumRowsAffected() != 1)
		{
			kWarning ("GetNumRowsAffected() = {}, but should be 1", db.GetNumRowsAffected());
		}
		CHECK (db.GetNumRowsAffected() == 1);

		if (!db.ExecSQL ("update TEST1_KSQL set anum=2 where astring='row-1'"))
		{
			INFO (db.GetLastSQL());
			FAIL_CHECK (db.GetLastError());
		}

		if (db.GetNumRowsAffected() != 1)
		{
			kWarning ("GetNumRowsAffected() = {}, but should be 1", db.GetNumRowsAffected());
		}
		CHECK (db.GetNumRowsAffected() == 1);

		auto iNum = db.SingleIntQuery ("select anum from TEST1_KSQL where astring='row-1'");
		if (iNum != 2)
		{
			kWarning ("did not get anum of 2, but got {}", iNum);
			INFO (db.GetLastError());
			FAIL_CHECK (db.GetLastError());
		}

		KROW ARow("TEST1_KSQL");
		CHECK ( db.LoadColumnLayout(ARow, "") );
		CHECK ( ARow.size() == 3 );
		CHECK ( ARow.GetName(0) == "anum" );
		CHECK ( ARow.GetName(1) == "astring" );
		CHECK ( ARow.GetName(2) == "adate" );

		KROW AnotherRow("TEST1_KSQL");
		CHECK ( db.Load(AnotherRow) );
		CHECK ( AnotherRow["anum"] == "2" );
		CHECK ( AnotherRow["astring"] == "row-1" );

		KROW AnotherRow2("TEST1_KSQL");
		AnotherRow2.AddCol("astring", "row-1", KROW::PKEY);
		CHECK ( db.Load(AnotherRow2) );
		CHECK ( AnotherRow2["anum"] == "2" );
		CHECK ( AnotherRow2["astring"] == "row-1" );
/* fails
		KROW NewRow("TEST1_KSQL");
		CHECK ( db.LoadColumnLayout(NewRow) );
		NewRow["anum"] = "42";
		NewRow["astring"] = "42";
		CHECK ( db.Insert(NewRow) );
*/
		if (!db.ExecSQL ("drop table TEST1_KSQL"))
		{
			INFO (db.GetLastError());
			FAIL_CHECK (db.GetLastError());
		}

		kDebugLog (1, "AUTO INCREMENT");

		if ((db.GetDBType() == KSQL::DBT::MYSQL)     ||
			(db.GetDBType() == KSQL::DBT::SQLSERVER) ||
			(db.GetDBType() == KSQL::DBT::SQLSERVER15))
		{

			if (!db.ExecSQL (
				"create table TEST_KSQL (\n"
				"    anum      int           not null primary key {{AUTO_INCREMENT}},\n"
				"    astring   char(100)     null,\n"
				"    bigstring {{MAXCHAR}}   null,\n"
				"    dtmnow    {{DATETIME}}  null\n"
				")"))
			{
				INFO (db.GetLastSQL());
				FAIL_CHECK (db.GetLastError());
			}
		}
		else // ORACLE
		{
			if (!db.ExecSQL (
				"create table TEST_KSQL (\n"
				"    anum      int           not null,\n"
				"    astring   char(100)     null,\n"
				"    bigstring {{MAXCHAR}}   null,\n"
				"    dtmnow    {{DATETIME}}  null\n"
				")"))
			{
				INFO (db.GetLastSQL());
				FAIL_CHECK (db.GetLastError());
			}
		}

		enum {PRESEED = 1000};

		for (uint32_t ii=1; ii<=9; ++ii)
		{
			kDebug (1, "round {}", ii);

			bool bHasAutoIncrement = ((db.GetDBType() == KSQL::DBT::MYSQL)     ||
									  (db.GetDBType() == KSQL::DBT::SQLSERVER) ||
									  (db.GetDBType() == KSQL::DBT::SQLSERVER15));
			bool bIsFirstRow       = (ii==1);

			if (!bHasAutoIncrement || bIsFirstRow)
			{
				SqlServerIdentityInsert (db, "TEST_KSQL", "ON");

				if (!db.ExecSQL ("insert into TEST_KSQL (anum,astring,bigstring,dtmnow) values ({},'row-{}','',{{NOW}})", PRESEED+ii, ii))
				{
					INFO (db.GetLastSQL());
					FAIL_CHECK (db.GetLastError());
				}
			}
			else
			{
				if (ii==2)
				{
					SqlServerIdentityInsert (db, "TEST_KSQL", "OFF");
				}
				// do NOT specify the 'anum' column:
				if (!db.ExecSQL ("insert into TEST_KSQL (astring,bigstring,dtmnow) values ('row-{}','',{{NOW}})", ii))
				{
					INFO (db.GetLastError());
					FAIL_CHECK (db.GetLastError());
				}
			}

			kDebugLog (1, ".GetNumRowsAffected()");

			if (db.GetNumRowsAffected() != 1)
			{
				kWarning ("GetNumRowsAffected() = {}, but should be 1", db.GetNumRowsAffected());
			}
			CHECK (db.GetNumRowsAffected() == 1);

			if (bHasAutoIncrement && !bIsFirstRow)
			{
				kDebugLog (1, "last insert id");

				auto iID = db.GetLastInsertID();
				if (iID != PRESEED+ii)
				{
					kWarning ("should have gotten auto_increment value of {}, but got: {}", PRESEED+ii, iID);
				}
				CHECK (iID == PRESEED+ii);
			}
		} // for

		kDebugLog (1, "see if we can get column headers from queries");

		db.ExecQuery ("select * from TEST_KSQL");

		KROW Cols;
		db.NextRow (Cols);
		if ((Cols.GetName(0) != "anum")
	     || (Cols.GetName(1) != "astring")
	     || (Cols.GetName(2) != "bigstring")
	     || (Cols.GetName(3) != "dtmnow"))
		{
			INFO (db.GetLastError());
			//Cols.DebugPairs (1);
			FAIL_CHECK ("query did *NOT* return the column headers");
		}

		kDebugLog (1, "query results (should be 1 row)");

		db.ExecQuery ("select * from TEST_KSQL where anum={}", PRESEED+5);
		auto iCount = DumpRows (db);
		if (iCount != 1)
		{
			kWarning ("did not get 1 row back (got {})", iCount);
			FAIL_CHECK (db.GetLastError());
		}
 	
		kDebugLog (1, "null query (should be NO matching rows and no errors)");

		db.ExecQuery ("select * from TEST_KSQL where anum=33165 order by 1");
		iCount = DumpRows (db);
		if (iCount != 0)
		{
			kWarning ("did not get 0 rows back (got {})", iCount);
			INFO (db.GetLastError());
			FAIL_CHECK (db.GetLastError());
		}
	
		kDebugLog (1, "single int query test (should return the number 9)");

		iCount = db.SingleIntQuery ("select count(*) from TEST_KSQL");
		if (iCount != 9)
		{
			kWarning ("did not get count(*) of 9, but got {}", iCount);
			INFO (db.GetLastError());
			FAIL_CHECK (db.GetLastError());
		}
	
		kDebugLog (1, "single int query test (should return 0)");

		iCount = db.SingleIntQuery ("select count(*) from TEST_KSQL where 1=0");
		if (iCount != 0)
		{
			kWarning ("did not get count(*) of 0, but got {}", iCount);
			INFO (db.GetLastError());
			FAIL_CHECK (db.GetLastError());
		}

		kDebugLog (1, "single int query test (should return -1 (error))");

		db.SetFlags (KSQL::F_IgnoreSQLErrors);
		iCount = db.SingleIntQuery ("select count(*) from FLUBBERNUTTER");
		if (iCount != -1)
		{
			kWarning ("did not get -1 back from a bad single-int query");
			INFO (db.GetLastError());
			FAIL_CHECK (db.GetLastError());
		}
		db.SetFlags (0);

		kDebugLog (1, "query results test (should be 9 rows)");

		db.ExecQuery ("select * from TEST_KSQL order by 1");
		iCount = DumpRows (db);
		if (iCount != 9)
		{
			kWarning ("did not get 9 rows back (got {}) after inserts", iCount);
			INFO (db.GetLastError());
			FAIL_CHECK (db.GetLastError());
		}
	
		kDebugLog (1, "making sure we can stop a query before fetching all rows");

		db.ExecQuery ("select * from TEST_KSQL order by 1");
		db.NextRow (); // should be 8 rows left
		iCount = db.SingleIntQuery ("select count(*) from TEST_KSQL");
		if (iCount != 9)
		{
			FAIL_CHECK ("could not start another query with results pending");
		}

		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		// Results Buffering
		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		#if 1
		kDebugLog (1, "note: buffered results tests are SHUT OFF");
		#else
		TEST (400, "buffered results:1");
		db.EndQuery();
		db.SetFlags (KSQL::F_BufferResults);
		if (!db.ExecQuery ("select * from TEST_KSQL order by 1"))
		{
			FAIL_CHECK (db.GetLastError());
		}

		TEST (401, "buffered results:2");
		iCount = DumpRows (db);
		if (iCount != 9)
		{
			FAIL_CHECK (db.GetLastError());
			kWarning ("did not get 9 rows back (got {}) from buffered results set", iCount);
		}

		TEST (402, "buffered results:3: reset buffer");
		db.ResetBuffer();
		iCount = DumpRows (db);
		if (iCount != 9)
		{
			FAIL_CHECK (db.GetLastError());
			kWarning ("did not get 9 rows back (got {}) after buffer rewind", iCount);
		}
		#endif

		kDebugLog (1, "testing truncate table");

		if (!db.ExecSQL ("truncate table TEST_KSQL"))
		{
			FAIL_CHECK (db.GetLastError());
		}

		kDebugLog (1, "ensuring table is empty");

		iCount = db.SingleIntQuery ("select count(*) from TEST_KSQL");
		if (iCount != 0)
		{
			kWarning ("truncate table must have failed: did not get count(*) of 0, but got {}", iCount);
			FAIL_CHECK (db.GetLastError());
		}
	
		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		// MySQL Bulk Insert
		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		if (db.GetDBType() == KSQL::DBT::MYSQL)
		{
			kDebugLog (1, "MySQL only test: BULK INSERT INTO");

			db.ExecSQL (
				"insert into TEST_KSQL (anum,astring,bigstring,dtmnow) values \n"
					"(  {},'row-1','',{{NOW}}),\n"
					"(   0,'row-2','',{{NOW}}),\n"
					"(   0,'row-3','',{{NOW}}),\n"
					"(   0,'row-4','',{{NOW}}),\n"
					"(   0,'row-5','',{{NOW}}),\n"
					"(   0,'row-6','',{{NOW}}),\n"
					"(   0,'row-7','',{{NOW}}),\n"
					"(   0,'row-8','',{{NOW}}),\n"
					"(   0,'row-9','',{{NOW}})\n", PRESEED+1);

			if (db.GetNumRowsAffected() != 9) {
				kWarning ("GetNumRowsAffected() = {}, but should be 9", db.GetNumRowsAffected());
				INFO (db.GetLastError());
				FAIL_CHECK (db.GetLastError());
			}
		} // MYSQL only

		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		// SQL Batch File
		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		kDebugLog (1, "sql batch file");

		KString sTmp1; sTmp1.Format ("testksql{}a.sql", getpid());
		KString sTmp2; sTmp2.Format ("testksql{}b.sql", getpid());

		KString sContents2 = HereDoc(R"(
		    |insert into TEST_KSQL (anum) values (30);
		    |insert into TEST_KSQL (anum) values (40);
		)");

		KString sContents1 = HereDoc(R"(
		    |delete from TEST_KSQL;
		    |//MSS|set identity_insert TEST_KSQL on;
			|//define {{TBLNAM}} TEST_KSQL
		    |insert into {{TBLNAM}} (anum) values (10);
		    |insert into TEST_KSQL (anum) values (20);
		    |#include "${INCLUDEME}"
		    |//MSS|set identity_insert TEST_KSQL off;
		    |select count(*) from TEST_KSQL;
		)");

		{
			KOutFile fp1 (sTmp1);
			KOutFile fp2 (sTmp2);
			if (!fp1.Write (sContents1).Good())
			{
				kWarning ("failed to write {} bytes to file: {}", sContents1.size(), sTmp1);
				FAIL_CHECK ("could not write temp files to cwd");
			}
			if (!fp2.Write (sContents2).Good())
			{
				kWarning ("failed to write {} bytes to file: {}", sContents2.size(), sTmp2);
				FAIL_CHECK ("could not write temp files to cwd");
			}
		}
	
		if (!kSetEnv ("INCLUDEME", sTmp2))
		{
			FAIL_CHECK("failed to set environment variable");
		}

		if (!db.ExecSQLFile (sTmp1))
		{
			INFO (db.GetLastError());
			FAIL_CHECK (db.GetLastError());
		}

		kRemoveFile (sTmp1);
		kRemoveFile (sTmp2);

		kDebugLog (1, "embedded query in sql file");

		if (!db.NextRow())
		{
			kWarning ("embedded query in sql file failed to return a row");
			INFO (db.GetLastError());
			FAIL_CHECK (db.GetLastError());
		}

		kDebugLog (1, "total rows affected by sql file");

		iCount = db.Get (1).Int32();
		if (iCount != 4)
		{
			kWarning ("count(*) after inserts = {}, but should be 4", iCount);
			INFO (db.GetLastError());
			FAIL_CHECK (db.GetLastError());
		}

		kDebugLog (1, "total rows inserted by sql file");

		db.ExecQuery ("select * from TEST_KSQL order by 1");

		iCount = DumpRows (db);
		if (iCount != 4)
		{
			kWarning ("did not get 4 rows back (got {}) after sql batch", iCount);
			INFO (db.GetLastError());
			FAIL_CHECK (db.GetLastError());
		}

		//BlobTests (&db);

		kDebugLog (1, "support for alternate SQL batch delimeters");

		KString sTmp;  sTmp.Format ("testksql{}c.sql", getpid());

		KOutFile fp (sTmp);
		fp.Write (HereDoc(R"(
			|// This is a full-line comment
			|
		)"));

		if (db.GetDBType() == KSQL::DBT::SQLSERVER ||
			db.GetDBType() == KSQL::DBT::SQLSERVER15)
		{
			fp.Write (HereDoc(R"(
				|IF OBJECT_ID('FRED', 'U') IS NOT NULL DROP TABLE FRED;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
				|create table FRED (a char(10) not null);	;	;	;	;;;	; ;
				|insert into FRED values (';'); ; ;;; ;	;	;
				|insert into FRED values (';');
				|
				|delimiter @@@
				|insert into FRED values ('@@@')@@@
				|insert into FRED values ('@@@')            @@@
				|
				|delimiter !!
				|IF OBJECT_ID('FRED', 'U') IS NOT NULL DROP TABLE FRED!!
				|create table FRED (a char(10) not null)!!
				|insert into FRED values ('!!')!!
				|insert into FRED values ('!!')!!!!!!!!
				|insert into FRED values ('!!')!! !! !!	!!
			)"));
		}
		else
		{
			fp.Write (HereDoc(R"(
				|drop table if exists FRED;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
				|create table FRED (a char(10) not null);	;	;	;	;;;	; ;
				|insert into FRED values (';'); ; ;;; ;	;	;
				|insert into FRED values (";");
				|
				|delimiter @@@
				|insert into FRED values ("@@@")@@@
				|insert into FRED values ("@@@")            @@@
				|
				|analyze table FRED@@@
				|delimiter !!
				|drop table if exists FRED!!
				|create table FRED (a char(10) not null)!!
				|insert into FRED values ('!!')!!
				|insert into FRED values ("!!")!!!!!!!!
				|insert into FRED values ("!!")!! !! !!	!!
			)"));
		}
		fp.close();

		if (!db.ExecSQLFile (sTmp))
		{
			INFO (db.GetLastError());
			FAIL_CHECK (db.GetLastError());
		}

		kRemoveFile (sTmp);

		KROW Row ("TEST_KSQL");
		Row.AddCol ("anum",      UINT64_C(102),            KROW::PKEY);
		Row.AddCol ("astring",   "krow insert");

		kDebugLog (1, "KROW insert");

		db.ExecSQL ("truncate table TEST_KSQL");
		SqlServerIdentityInsert (db, "TEST_KSQL", "ON");

		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		// KROW Operations
		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

		std::vector<KROW> Rows;

		if (!db.ExecSQL("create table if not exists TEST2_KSQL like TEST_KSQL"))
		{
			FAIL_CHECK (db.GetLastError());
		}

		// prepare a value with 0 byte
		KString sZero = "krow insert 101";
		sZero.insert(4, 1, 0); // insert a zero byte after "krow"

		{
			KROW Row1 ("TEST_KSQL");
			Row1.AddCol ("anum",      UINT64_C(100),            KROW::PKEY);
			Row1.AddCol ("astring",   "krow insert");

			Rows.push_back(std::move(Row1));

			// test missing table name - should now be kept over from last
			KROW Row2;
			Row2.AddCol ("anum",      UINT64_C(101),            KROW::PKEY);
			Row2.AddCol ("astring",   sZero);

			Rows.push_back(std::move(Row2));

			KROW Row3 ("TEST_KSQL");
			Row3.AddCol ("anum",      UINT64_C(102),            KROW::PKEY);
			Row3.AddCol ("astring",   ""); // test an empty value

			Rows.push_back(std::move(Row3));

			KROW Row4 ("TEST2_KSQL");
			Row4.AddCol ("anum",      UINT64_C(100),            KROW::PKEY);
			Row4.AddCol ("astring",   "value 1");

			Rows.push_back(std::move(Row4));

			KROW Row5 ("TEST2_KSQL");
			Row5.AddCol ("anum",      UINT64_C(101),            KROW::PKEY);
			Row5.AddCol ("astring",   "value 2");

			Rows.push_back(std::move(Row5));
		}

		if (!db.Insert (Rows))
		{
			FAIL_CHECK (db.GetLastError());
		}

		CHECK ( db.GetNumRowsAffected() == 5 );

		{
			KROW Row6 ("TEST2_KSQL");
			Row6.AddCol ("anum",      UINT64_C(102),            KROW::PKEY);
			Row6.AddCol ("asring",   "krow insert 103");

			Rows.push_back(std::move(Row6));
		}

		CHECK ( !db.Insert (Rows) );
		CHECK ( db.GetLastError() == "differing column layout in rows - abort" );

		CHECK ( db.SingleIntQuery("select count(*) from TEST2_KSQL") == 2 );

		if (!db.ExecSQL("drop table TEST2_KSQL"))
		{
			FAIL_CHECK (db.GetLastError());
		}

		kDebugLog (1, "KSQL auto range for loop");

		if (!db.ExecQuery ("select * from TEST_KSQL order by anum asc"))
		{
			INFO (db.GetLastSQL());
			FAIL_CHECK (db.GetLastError());
		}

		uint16_t iRows { 0 };

		INFO  ( "auto range for loop" );

		for (auto& row : db)
		{
			// get all rows..
			++iRows;
			if (row["anum"] == "101")
			{
				KString sString = row["astring"];
				// check that row 101 has a 0 byte
				CHECK ( sString == sZero );
			}
			else if (row["anum"] == "102")
			{
				KString sString = row["astring"];
				// check that row 102 has an empty astring
				CHECK ( sString.empty() );
			}
			else
			{
				CHECK ( row["astring"].size() > 0 );
			}
		}

		CHECK ( iRows == 3 );

		kDebugLog (1, "KROW update");

		Row.AddCol ("astring", "krow update");
		if (!db.Update (Row))
		{
			INFO (db.GetLastError());
			FAIL_CHECK (db.GetLastError());
		}

		kDebugLog (1, "KROW delete");

		Row.AddCol ("astring", "krow update");
		if (!db.Delete (Row))
		{
			INFO (db.GetLastError());
			FAIL_CHECK (db.GetLastError());
		}
		
		INFO ("UTF8 characters");
		kDebugLog (1, "KROW insert high-byte (asian) characters");

		db.SetFlags (KSQL::F_IgnoreSQLErrors);
		db.ExecSQL ("drop table TEST_ASIAN");
		db.SetFlags (0);

		if (db.GetDBType() == KSQL::DBT::SQLSERVER)
		{
			if (!db.ExecRawSQL (HereDoc (R"(
				|create table TEST_ASIAN (
				|    anum      int            not null primary key,
				|    astring   nvarchar(500)  null
				|))")))
			{
				INFO (db.GetLastSQL());
				FAIL_CHECK (db.GetLastError());
			}
		}
		else
		{
			if (!db.ExecRawSQL (HereDoc (R"(
				|create table TEST_ASIAN (
				|    anum      int            not null primary key,
				|    astring   varchar(500)   null
				|))")))
			{
				INFO (db.GetLastSQL());
				FAIL_CHECK (db.GetLastError());
			}
		}

		KROW URow ("TEST_ASIAN");
		URow.AddCol ("anum", UINT64_C(100), KROW::PKEY|KROW::NUMERIC);
		URow.AddCol ("astring", ASIAN1);
		if (!db.Insert(URow))
		{
			INFO (db.GetLastError());
			FAIL_CHECK (db.GetLastError());
		}
		if (!db.ExecQuery ("select * from TEST_ASIAN"))
		{
			INFO (db.GetLastSQL());
			FAIL_CHECK (db.GetLastError());
		}
		if (!db.NextRow (Cols))
		{
			FAIL_CHECK ("expected a row back but did not get one");
		}
		if (Cols.GetValue(1) != ASIAN1)
		{
			kWarning ("failed to preserve Asian text on insert");
			kWarning (" in: {}", ASIAN1);
			kWarning ("out: {}", Cols.GetValue(1));
			FAIL_CHECK (false);
		}

		URow.AddCol ("astring", ASIAN2);
		if (!db.Update(URow))
		{
			INFO (db.GetLastError());
			FAIL_CHECK (db.GetLastError());
		}
		if (!db.ExecQuery ("select * from TEST_ASIAN"))
		{
			INFO (db.GetLastSQL());
			FAIL_CHECK (db.GetLastError());
		}
		if (!db.NextRow (Cols))
		{
			FAIL_CHECK ("expected a row back but did not get one");
		}
		if (Cols.GetValue(1) != ASIAN2)
		{
			kWarning ("failed to preserve Asian text on update");
			kWarning (" in: {}", ASIAN2);
			kWarning ("out: {}", Cols.GetValue(1));
			FAIL_CHECK (false);
		}

		db.ExecSQL ("drop table TEST_ASIAN");

		kDebugLog (1, "KROW clipping");

		Row.AddCol ("astring",   "clip me here -- all this should be GONE", 0, static_cast<KCOL::Len>(strlen("clip me here")));
		db.Insert (Row);
		db.ExecQuery ("select astring from TEST_KSQL where anum=102");
		db.NextRow ();
		if (db.Get(1) != "clip me here")
		{
			kWarning ("string not clipped properly: '{}'", db.Get(1));
			INFO (db.GetLastError());
			FAIL_CHECK (db.GetLastError());
		}

		kDebugLog (1, "KROW smart clipping on quote");

		Row.AddCol ("astring",   "clip me here' -- all this should be GONE", 0, static_cast<KCOL::Len>(strlen("clip me here'")));
		db.Update (Row);
		db.ExecQuery ("select astring from TEST_KSQL where anum=102");
		db.NextRow ();
		if (db.Get(1) != "clip me here")
		{
			kWarning ("string not clipped properly: '{}'", db.Get(1));
			FAIL_CHECK (db.GetLastError());
		}

		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		// Retry Logic:
		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		SqlServerIdentityInsert (db, "TEST_KSQL", "OFF");

		kDebugLog (1, "Retry logic when connection dies during an ExecSQL");

		db.ExecSQL ("insert into TEST_KSQL (astring) values ('retry1')");
		SimulateLostConnection (db);

		if (!db.ExecSQL ("insert into TEST_KSQL (astring) values ('retry2')"))
		{
			INFO (db.GetLastSQL());
			FAIL_CHECK (db.GetLastError());
		}

		kDebugLog (1, "Retry logic when connection dies during an ExecQuery");

		db.ExecSQL ("insert into TEST_KSQL (astring) values ('retry3')");
		SimulateLostConnection (db);
		iCount = db.SingleIntQuery ("select count(*) from TEST_KSQL where astring like 'retry{{PCT}}'");
		if (iCount != 3)
		{
			kWarning ("got: {} rows from TEST_KSQL and expected 3", iCount);
			FAIL_CHECK (db.GetLastSQL());
		}

		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		// Single quote handling.
		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		kDebugLog (1, "KROW single quote insert");

		Row.clear();
		Row.AddCol ("astring", QUOTES1);

		if (!db.Insert (Row))
		{
			INFO (db.GetLastError());
			FAIL_CHECK (db.GetLastError());
		}

		Row.AddCol ("anum",    db.GetLastInsertID(), KROW::PKEY);

		kDebugLog (1, "KROW single quote update");

		Row.AddCol ("astring", QUOTES2, 0, 0);

		if (!db.Update (Row))
		{
			INFO (db.GetLastError());
			FAIL_CHECK (db.GetLastError());
		}

		kDebugLog (1, "KROW single quote select");

		if (!db.ExecQuery ("select * from TEST_KSQL where anum={}", Row.Get("anum").sValue.Int32()))
		{
			INFO (db.GetLastError());
			FAIL_CHECK (db.GetLastError());
		}
		else if (!db.NextRow (Row))
		{
			kWarning ("did not get back a row");
			INFO (db.GetLastError());
			FAIL_CHECK (db.GetLastError());
		}
		else if (Row.Get("astring").sValue != QUOTES2)
		{
			kWarning ("expected: {}", QUOTES2);
			kWarning ("     got: {}", Row.Get ("astring").sValue);
			INFO (db.GetLastError());
			FAIL_CHECK (db.GetLastError());
		}

		kDebugLog (1, "KROW single quote delete");

		Row.clear();
		Row.AddCol ("astring", QUOTES2, KROW::PKEY);

		if (!db.Delete (Row))
		{
			INFO (db.GetLastError());
			FAIL_CHECK (db.GetLastError());
		}
		else if (db.GetNumRowsAffected() != 1)
		{
			kWarning ("row not found");
			INFO (db.GetLastError());
			FAIL_CHECK (db.GetLastError());
		}

		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		// Slash handling.
		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		kDebugLog (1, "KROW slash test insert");

		Row.clear();
		Row.AddCol ("anum",    UINT64_C(98), KROW::PKEY);
		Row.AddCol ("astring", SLASHES1);

		SqlServerIdentityInsert (db, "TEST_KSQL", "ON");
		if (!db.Insert (Row))
		{
			INFO (db.GetLastError());
			FAIL_CHECK (db.GetLastError());
		}

		kDebugLog (1, "KROW slash test update");

		Row.AddCol ("astring", SLASHES2);

		if (!db.Update (Row))
		{
			INFO (db.GetLastError());
			FAIL_CHECK (db.GetLastError());
		}

		kDebugLog (1, "KROW slash test select");

		if (!db.ExecQuery ("select * from TEST_KSQL where anum=98"))
		{
			INFO (db.GetLastError());
			FAIL_CHECK (db.GetLastError());
		}
		else if (!db.NextRow (Row))
		{
			INFO ("did not get back a row");
			INFO (db.GetLastError());
			FAIL_CHECK (db.GetLastError());
		}
		else if (Row.Get("astring").sValue != SLASHES2)
		{
			kWarning ("expected: {}", SLASHES2);
			kWarning ("     got: {}", Row.Get ("astring").sValue);
			INFO (db.GetLastError());
			FAIL_CHECK (db.GetLastError());
		}

		kDebugLog (1, "KROW slash test delete");

		Row.clear();
		Row.AddCol ("astring", SLASHES2, KROW::PKEY);

		if (!db.Delete (Row))
		{
			INFO (db.GetLastError());
			FAIL_CHECK (db.GetLastError());
		}
		else if (db.GetNumRowsAffected() != 1)
		{
			INFO ("row not found");
			INFO (db.GetLastError());
			FAIL_CHECK (db.GetLastError());
		}

		kDebugLog (1, "cleanup");

		if (!db.ExecSQL ("drop table TEST_KSQL"))
		{
			INFO (db.GetLastError());
			FAIL_CHECK (db.GetLastError());
		}

		if (db.GetDBType() == KSQL::DBT::MYSQL)
		{
			bool b;
			{
				b = db.GetLock("TestLock", 1);
				INFO ( "GetLock()" );
				CHECK ( b );
			}

			{
				b = db.IsLocked("TestLock");
				INFO ( "IsLocked()" );
				CHECK ( b );
			}

			{
				b = db.ReleaseLock("TestLock");
				INFO ( "ReleaseLock()" );
				CHECK ( b );
			}

			{
				b = db.IsLocked("TestLock");
				INFO ( "IsLocked()" );
				CHECK ( b == false );
			}

			{
				b = db.ReleaseLock("TestLock");
				INFO ( "ReleaseLock()" );
				CHECK ( b == false );
			}

			{
				DbSemaphore Semaphore(db, "TestLock", false);
				CHECK ( Semaphore.IsCreated() );
				CHECK ( db.IsLocked("TestLock") );
				CHECK ( Semaphore.CreateSemaphore() );
				CHECK ( Semaphore.ClearSemaphore() );
				CHECK ( Semaphore.IsCreated() == false );
				CHECK ( db.IsLocked("TestLock") == false );
				CHECK ( Semaphore.CreateSemaphore() );
				CHECK ( Semaphore.CreateSemaphore() );
				auto db2 = db;
				DbSemaphore Semaphore2(db2, "TestLock", false, false, 0);
				CHECK ( Semaphore2.IsCreated() == false );
			}
			CHECK ( db.IsLocked("TestLock") == false );
		}

		// varchar vs int index
		if (false)
		{
			if (!db.ExecSQL ("drop table if exists TEST_VARCHAR_INDEX"))
			{
				INFO (db.GetLastError());
				FAIL_CHECK (db.GetLastError());
			}

			if (!db.ExecSQL ("drop table if exists TEST_INT_INDEX"))
			{
				INFO (db.GetLastError());
				FAIL_CHECK (db.GetLastError());
			}

			if (!db.ExecRawSQL (HereDoc (R"(
				|create table TEST_VARCHAR_INDEX (
				|    astring   longblob        not null,
				|    anum      bigint unsigned not null,
				|    a2num     int unsigned    not null,
				|    PRIMARY KEY idx_astring (astring(255))
				|))")))
			{
				INFO (db.GetLastSQL());
				FAIL_CHECK (db.GetLastError());
			}

			if (!db.ExecRawSQL (HereDoc (R"(
				|create table TEST_INT_INDEX (
				|    astring   longblob        not null,
				|    anum      bigint unsigned not null primary key,
				|    a2num     int unsigned    not null
				|))")))
			{
				INFO (db.GetLastSQL());
				FAIL_CHECK (db.GetLastError());
			}

			static constexpr int MAXTRIALS = 100000;

			std::set<KString> Strings;

			for (int i = 0; i < MAXTRIALS;)
			{
				if (Strings.insert(GenRandomString(5, 300)).second)
				{
					++i;
				}
			}

			KStopDurations Durations;

			uint64_t iCounter { 0 };
			for (const auto& sRandom : Strings)
			{
				db.ExecRawSQL(kFormat(
								   "insert into TEST_VARCHAR_INDEX \n"
								   "   set astring = '{}', \n"
								   "       anum    = {}, \n"
								   "       a2num   = {}",
								   sRandom,
								   ++iCounter,
								   sRandom.size()
								));
			}

			Durations.StartNextInterval();

			for (const auto& sRandom : Strings)
			{
				db.SingleIntRawQuery(kFormat(
											 "SELECT a2num \n"
											 "   from TEST_VARCHAR_INDEX \n"
											 "  where astring = '{}'",
											 sRandom
										));
			}

			Durations.StartNextInterval();

			for (const auto& sRandom : Strings)
			{
				db.ExecRawSQL(kFormat(
								   "insert into TEST_INT_INDEX \n"
								   "   set astring = '{}', \n"
								   "       anum    = {}, \n"
								   "       a2num   = {}",
								   sRandom,
								   sRandom.Hash(),
								   sRandom.size()
								));
			}

			Durations.StartNextInterval();

			for (const auto& sRandom : Strings)
			{
				db.SingleIntRawQuery(kFormat(
											 "SELECT a2num \n"
											 "   from TEST_INT_INDEX \n"
											 "  where anum = {}",
											 sRandom.Hash()
										));
			}

			Durations.StartNextInterval();

			std::vector<KString> Labels;
			Labels.push_back("insert varchar index");
			Labels.push_back("select varchar index");
			Labels.push_back("    insert int index");
			Labels.push_back("    select int index");

			for (std::size_t i = 0; i < Labels.size(); ++i)
			{
				KOut.FormatLine("{} : {:>6} ms total ({:3} us per query)",
								Labels[i],
								kFormNumber(Durations.milliseconds(i)),
								Durations.nanoseconds(i) / (MAXTRIALS * 1000)
								);
			}

			if (!db.ExecSQL ("drop table TEST_VARCHAR_INDEX"))
			{
				INFO (db.GetLastError());
				FAIL_CHECK (db.GetLastError());
			}

			if (!db.ExecSQL ("drop table TEST_INT_INDEX"))
			{
				INFO (db.GetLastError());
				FAIL_CHECK (db.GetLastError());
			}

		}

	}

} // TEST_CASE("ksql")

//-----------------------------------------------------------------------------
int64_t DumpRows (KSQL& db)
//-----------------------------------------------------------------------------
{
	kDebugLog (1, "  {:<4.4}-+-{:4.4}-+-{:<7.7}-+-{:<9.9}-+-{:20.20}-+",
		KLog::BAR, KLog::BAR, KLog::BAR, KLog::BAR, KLog::BAR);

	kDebugLog (1, "  {:<4.4} | {:4.4} | {:<7.7} | {:<9.9} | {:20.20} |",
		"row#", "anum", "astring", "bigstring", "dtmnow");

	kDebugLog (1, "  {:<4.4}-+-{:4.4}-+-{:<7.7}-+-{:<9.9}-+-{:20.20}-+",
		KLog::BAR, KLog::BAR, KLog::BAR, KLog::BAR, KLog::BAR);

	int64_t iRows = 0;
	while (db.NextRow())
	{
		++iRows;

		auto  anum      = db.Get(1).Int32();
		auto  astring   = db.Get(2);
		auto  bigstring = db.Get(3);
		auto  pszTime   = db.Get(4);

		kDebugLog (1, "  {:<4} | {:4} | {:<7.7} | {:<9.9} | {:20.20} |",
			iRows, anum, astring, bigstring, pszTime);
	}

	kDebugLog (1, "  {:<4.4}-+-{:4.4}-+-{:<7.7}-+-{:<9.9}-+-{:20.20}-+",
		KLog::BAR, KLog::BAR, KLog::BAR, KLog::BAR, KLog::BAR);

	return (iRows);

} // DumpRows

//-----------------------------------------------------------------------------
void SimulateLostConnection (KSQL& db)
//-----------------------------------------------------------------------------
{
	#if 0  // re-enable if retry logic goes into question

	if (db.GetDBType() == KSQL::DBT_SQLSERVER)
	{
		for (int ss=15; ss > 0; --ss)
		{
			int iSpid = db.SingleIntQuery ("select @@spid");
			if (iSpid < 0)
				break; // connection is killed
			KOut.FormatLine ("You have {} seconds to issue: KILL {}", ss, iSpid);
			sleep (1);
		}
	}

	#else

	kDebug(1, "closing connection to simulate a lost connection");
	db.CloseConnection (); // simulate connection being lost
	kDebug(2, "connection is closed");

	#endif

} // SimulateLostConnection

