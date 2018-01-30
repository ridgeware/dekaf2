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
#include <dekaf2/dekaf2all.h>
#include <dekaf2/ksql.h>

using namespace dekaf2;

enum {
	MAX_FORMVARS      = 50,
	MAXLEN_REQBUFFER  = 5000
};

typedef const char* LPCTSTR;

size_t DumpRows (KSQL& db);
void  TestEscapes (int iTestNum, LPCTSTR pszString, LPCTSTR pszStringX, int iDBType);

KStringView g_sDbcFile;

#define ORADATEFORM  "YYYYMMDDHH24MISS"

void SimulateLostConnection (KSQL* pdb);

//-----------------------------------------------------------------------------
KString HereDoc (const char* szString)
//-----------------------------------------------------------------------------
{
	KString sReturnMe (szString);
	sReturnMe.ReplaceRegex ("^[\t]*|", "", /*all=*/true);
	return (sReturnMe);

} // HereDoc

//-----------------------------------------------------------------------------
static bool SqlServerIdentityInsert (KSQL& db, LPCTSTR pszTablename, KStringView sOnOff)
//-----------------------------------------------------------------------------
{
	if (db.GetDBType() == KSQL::DBT_SQLSERVER)
	{
		// avoid this errors:
		// Cannot insert explicit value for identity column in table 'XXX' when IDENTITY_INSERT is set to OFF.
		// Explicit value must be specified for identity column in table 'XXX' either when IDENTITY_INSERT is set to ON or when a replication user is inserting into a NOT FOR REPLICATION identity column.
		return (db.ExecSQL ("set identity_insert {} {}", pszTablename, sOnOff));
	}
	else {
		return (true);
	}

} // SqlServerIdentityInsertOn

//-----------------------------------------------------------------------------
TEST_CASE("KSQL")
//-----------------------------------------------------------------------------
{
	KStringView BENIGN    = "Something benign.";
	KStringView BENIGNX   = "Something benign.";
	KStringView QUOTES1   = "Fred's Fishing Pole";
	KStringView QUOTES1x  = "Fred''s Fishing Pole";
	KStringView QUOTES2   = "Fred's fishing pole's longer than mine.";
	KStringView QUOTES2x  = "Fred''s fishing pole''s longer than mine.";
	KStringView SLASHES1  = "This is a \\l\\i\\t\\t\\l\\e /s/l/a/s/h test.";
	KStringView SLASHES1x = "This is a \\\\l\\\\i\\\\t\\\\t\\\\l\\\\e /s/l/a/s/h test.";
	KStringView SLASHES2  = "This <b>is</b>\\n a string\\r with s/l/a/s/h/e/s, \\g\\e\\t\\ i\\t\\???";
	KStringView SLASHES2x = "This <b>is</b>\\\\n a string\\\\r with s/l/a/s/h/e/s, \\\\g\\\\e\\\\t\\\\ i\\\\t\\\\???";
	KStringView ASIAN1    = "Chinese characters 一二三四五六七八九十";
	KStringView ASIAN2    = "Chinese characters 十九八七六五四三二一";

	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
	// test static helper methods of KROW class:
	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
	SECTION("KROW::EscapeChars(BENIGN)")
	{
		KString sEscaped;
		KROW::EscapeChars (BENIGN, sEscaped, KSQL::DBT_MYSQL);
		CHECK (sEscaped == BENIGNX);
	}

	SECTION("KROW::EscapeChars(QUOTES1)")
	{
		KString sEscaped;
		KROW::EscapeChars (QUOTES1, sEscaped, KSQL::DBT_MYSQL);
		CHECK (sEscaped == QUOTES1x);
	}

	SECTION("KROW::EscapeChars(QUOTES2)")
	{
		KString sEscaped;
		KROW::EscapeChars (QUOTES2, sEscaped, KSQL::DBT_MYSQL);
		CHECK (sEscaped == QUOTES2x);
	}

	SECTION("KROW::EscapeChars(SLASHES1)")
	{
		KString sEscaped;
		KROW::EscapeChars (SLASHES1, sEscaped, KSQL::DBT_MYSQL);
		CHECK (sEscaped == SLASHES1x);
	}

	SECTION("KROW::EscapeChars(SLASHES2)")
	{
		KString sEscaped;
		KROW::EscapeChars (SLASHES2, sEscaped, KSQL::DBT_MYSQL);
		CHECK (sEscaped == SLASHES2x);
	}

	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
	// test KSQL class...
	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
	SECTION("KSQL::constructor and destructor (no connect info)...")
	{
		KSQL db;
	}

	KSQL db; // <-- shared across the remaining tests

	if (g_sDbcFile.empty()) {
		return;  // <--- other other tests are useless
	}

	SECTION("load DBC file")
	{
		if (!kFileExists (g_sDbcFile))
		{
			KString sErr;
			sErr.Format ("failed to file dbc file: {}", g_sDbcFile);
			FAIL (sErr);
			return;  // <--- other other tests are useless
		}
		else if (!db.LoadConnect (g_sDbcFile))
		{
			FAIL (db.GetLastError().c_str());
			return;  // <--- other other tests are useless
		}
	}

	SECTION("open connection")
	{
		if (!db.OpenConnection())
		{
			FAIL (db.GetLastError().c_str());
		}

		// establish BASE STATE by dropping tables from possibl prior runs
		kDebug (1, " ");
		kDebug (1, "flag test: F_IgnoreSQLErrors (should only produce DBG output)...");
		db.SetFlags (KSQL::F_IgnoreSQLErrors);
		db.ExecSQL ("drop table TEST_KSQL");
		db.ExecSQL ("drop table TEST_KSQL_BLOB");
		db.ExecSQL ("drop table BOGUS_TABLE");
		db.ExecSQL ("drop table BOGUS_TABLE");
		db.ExecSQL ("drop table BOGUS_TABLE");
		db.SetFlags (0);
	
		kDebug (1, " ");
		kDebug (1, "ExecSQL() test (should be no errors):");
	}

	if (!db.IsConnectionOpen())
	{
		return; // bail now --> all the rest of the tests will just blow up anyway
	}

	SECTION("NEGATIVE TEST: exception handling for bad ojbect")
	{
		db.SetFlags (KSQL::F_IgnoreSQLErrors);
		if (db.ExecQuery ("select bogus from BOGUS order by fubar")) {
			FAIL ("ExecQuery should have returned FALSE");
		}
	}

	SECTION("NEGATIVE TEST: exception handling for invalid sql function")
	{
		if (db.ExecQuery ("select junk('fred')")) {
			FAIL ("ExecQuery should have returned FALSE");
		}
	}

	db.SetFlags (0);

	if ((db.GetDBType() == KSQL::DBT_MYSQL) || (db.GetDBType() == KSQL::DBT_SQLSERVER))
	{
		SECTION ("AUTO INCREMENT")
		{
			if (!db.ExecSQL (
				"create table TEST_KSQL (\n"
				"    anum      int           not null primary key {{AUTO_INCREMENT}},\n"
				"    astring   char(100)     null,\n"
				"    bigstring {{MAXCHAR}}   null,\n"
				"    dtmnow    {{DATETIME}}  null\n"
				")"))
			{
				FAIL (db.GetLastError().c_str());
			}
		}
	}
	else // ORACLE
	{
		SECTION ("AUTO INCREMENT")
		{
			if (!db.ExecSQL (
				"create table TEST_KSQL (\n"
				"    anum      int           not null,\n"
				"    astring   char(100)     null,\n"
				"    bigstring {{MAXCHAR}}   null,\n"
				"    dtmnow    {{DATETIME}}  null\n"
				")"))
			{
				FAIL (db.GetLastError());
			}
		}
	}

	enum {PRESEED = 1000};

	for (uint32_t ii=1; ii<=9; ++ii)
	{
		bool bHasAutoIncrement = ((db.GetDBType() == KSQL::DBT_MYSQL) || (db.GetDBType() == KSQL::DBT_SQLSERVER));
		bool bIsFirstRow       = (ii==1);

		SECTION ("insert row")
		{
			if (!bHasAutoIncrement || bIsFirstRow)
			{
				SqlServerIdentityInsert (db, "TEST_KSQL", "ON");

				if (!db.ExecSQL ("insert into TEST_KSQL (anum,astring,bigstring,dtmnow) values ({},'row-{}','',{{NOW}})", PRESEED+ii, ii)) {
					FAIL (db.GetLastError());
				}
			}
			else
			{
				if (ii==2) {
					SqlServerIdentityInsert (db, "TEST_KSQL", "OFF");
				}
				// do NOT specify the 'anum' column:
				if (!db.ExecSQL ("insert into TEST_KSQL (astring,bigstring,dtmnow) values ('row-{}','',{{NOW}})", ii)) {
					FAIL (db.GetLastError());
				}
			}
		}

		SECTION (".GetNumRowsAffected()")
		{
			if (db.GetNumRowsAffected() != 1) {
				kWarning ("GetNumRowsAffected() = {}, but should be 1", db.GetNumRowsAffected());
			}
			CHECK (db.GetNumRowsAffected() != 1);
		}

		if (bHasAutoIncrement && !bIsFirstRow)
		{
			SECTION ("last insert id")
			{
				auto iID = db.GetLastInsertID();
				if (iID != PRESEED+ii) {
					kWarning ("should have gotten auto_increment value of {}, but got: {}", PRESEED+ii, iID);
				}
				CHECK (iID != PRESEED+ii);
			}
		}
	} // for

	SECTION ("see if we can get column headers from queries")
	{
		db.ExecQuery ("select * from TEST_KSQL");

		KROW Cols;
		db.NextRow (Cols);
		if ((Cols.GetName(0) != "anum")
	     || (Cols.GetName(1) != "astring")
	     || (Cols.GetName(2) != "bigstring")
	     || (Cols.GetName(3) != "dtmnow"))
		{
			kWarning ("{}", db.GetLastError());
			//Cols.DebugPairs (1);
			FAIL ("query did *NOT* return the column headers");
		}
	}

	SECTION ("query results (should be 1 row)")
	{
		db.ExecQuery ("select * from TEST_KSQL where anum={}", PRESEED+5);
		long iCount = DumpRows (db);
		if (iCount != 1)
		{
			kWarning ("did not get 1 row back (got {})", iCount);
			FAIL (db.GetLastError().c_str());
		}
	}
 	
	SECTION ("null query (should be NO matching rows and no errors)")
	{
		db.ExecQuery ("select * from TEST_KSQL where anum=33165 order by 1");
		auto iCount = DumpRows (db);
		if (iCount != 0)
		{
			kWarning ("did not get 0 rows back (got {})", iCount);
			FAIL (db.GetLastError().c_str());
		}
	}
	
	SECTION ("single int query test (should return the number 9)")
	{
		auto iCount = db.SingleIntQuery ("select count(*) from TEST_KSQL");
		if (iCount != 9)
		{
			kWarning ("did not get count(*) of 9, but got {}", iCount);
			FAIL (db.GetLastError().c_str());
		}
	}
	
	SECTION ("single int query test (should return 0)")
	{
		auto iCount = db.SingleIntQuery ("select count(*) from TEST_KSQL where 1=0");
		if (iCount != 0) {
			kWarning ("did not get count(*) of 0, but got {}", iCount);
			FAIL (db.GetLastError().c_str());
		}
	}

	SECTION ("single int query test (should return -1 (error))")
	{
		db.SetFlags (KSQL::F_IgnoreSQLErrors);
		auto iCount = db.SingleIntQuery ("select count(*) from FLUBBERNUTTER");
		if (iCount != -1) {
			kWarning ("did not get -1 back from a bad single-int query");
			FAIL (db.GetLastError().c_str());
		}
		db.SetFlags (0);
	}

	SECTION ("query results test (should be 9 rows)")
	{
		db.ExecQuery ("select * from TEST_KSQL order by 1");
		auto iCount = DumpRows (db);
		if (iCount != 9)
		{
			kWarning ("did not get 9 rows back (got {}) after inserts", iCount);
			FAIL (db.GetLastError().c_str());
		}
	}
	
	SECTION ("making sure we can stop a query before fetching all rows")
	{
		db.ExecQuery ("select * from TEST_KSQL order by 1");
		db.NextRow (); // should be 8 rows left
		auto iCount = db.SingleIntQuery ("select count(*) from TEST_KSQL");
		if (iCount != 9) {
			FAIL ("could not start another query with results pending");
		}
	}

	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	// Results Buffering
	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	#if 1
		kDebug (1, "note: buffered results tests are SHUT OFF");
	#else
		TEST (400, "buffered results:1");
		db.EndQuery();
		db.SetFlags (KSQL::F_BufferResults);
		if (!db.ExecQuery ("select * from TEST_KSQL order by 1")) {
			FAIL (db.GetLastError().c_str());
			FAILED();
		}
		else {
			PASSED();
		}

		TEST (401, "buffered results:2");
		auto iCount = DumpRows (db);
		if (iCount != 9)
		{
			FAIL (db.GetLastError().c_str());
			kWarning ("did not get 9 rows back (got {}) from buffered results set", iCount);
			FAILED();
		}
		else {
			PASSED();
		}

		TEST (402, "buffered results:3: reset buffer");
		db.ResetBuffer();
		auto iCount = DumpRows (db);
		if (iCount != 9)
		{
			FAIL (db.GetLastError().c_str());
			kWarning ("did not get 9 rows back (got {}) after buffer rewind", iCount);
			FAILED();
		}
		else {
			PASSED();
		}
	#endif

	SECTION ("testing truncate table")
	{
		if (!db.ExecSQL ("truncate table TEST_KSQL")) {
			FAIL (db.GetLastError().c_str());
		}
	}

	SECTION ("ensuring table is empty")
	{
		auto iCount = db.SingleIntQuery ("select count(*) from TEST_KSQL");
		if (iCount != 0)
		{
			kWarning ("truncate table must have failed: did not get count(*) of 0, but got {}", iCount);
			FAIL (db.GetLastError().c_str());
		}
	}
	
	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	// MySQL Bulk Insert
	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	if (db.GetDBType() == KSQL::DBT_MYSQL)
	{
		SECTION ("MySQL only test: BULK INSERT INTO")
		{
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
				FAIL (db.GetLastError().c_str());
			}
		}
	} // MYSQL only

	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	// SQL Batch File
	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	SECTION ("sql batch file")
	{
		KString sTmp1; sTmp1.Format ("testksql{}a.sql", getpid());
		KString sTmp2; sTmp2.Format ("testksql{}b.sql", getpid());

		KString sContents2 = HereDoc(R"(
		    |insert into TEST_KSQL (anum) values (30);
		    |insert into TEST_KSQL (anum) values (40);
		)");

		KString sContents1 = HereDoc(R"(
		    |delete from TEST_KSQL;
		    |//MSS|set identity_insert TEST_KSQL on;
		    |insert into TEST_KSQL (anum) values (10);
		    |insert into TEST_KSQL (anum) values (20);
		    |#include "${INCLUDEME}"
		    |//MSS|set identity_insert TEST_KSQL off
		    |select count(*) from TEST_KSQL;;
		)");

		KFile fp1 (sTmp1.c_str());
		KFile fp2 (sTmp2.c_str());
		if (!fp1.Write (sContents1).Good() || fp2.Write (sContents2).Good())
		{
			FAIL ("could not write temp files to cwd");
		}

		fp1.close();
		fp2.close();
	
		kSetEnv ("INCLUDEME", sTmp2);
		if (!db.ExecSQLFile (sTmp1)) {
			FAIL (db.GetLastError().c_str());
		}

		kRemoveFile (sTmp1);
		kRemoveFile (sTmp2);
	}

	if (db.GetDBType() == KSQL::DBT_MYSQL)
	{
		SECTION ("embedded query in sql file")
		{
			if (!db.NextRow()) {
				kWarning ("embedded query in sql file failed to return a row");
				FAIL (db.GetLastError().c_str());
			}
		}

		SECTION ("total rows affected by sql file")
		{
			auto iCount = db.Get (1).Int32();
			if (iCount != 4)
			{
				kWarning ("count(*) after inserts = {}, but should be 4", iCount);
				FAIL (db.GetLastError().c_str());
			}
		}

		SECTION ("total rows inserted by sql file")
		{
			db.ExecQuery ("select * from TEST_KSQL order by 1");

			auto iCount = DumpRows (db);
			if (iCount != 4)
			{
				kWarning ("did not get 4 rows back (got {}) after sql batch", iCount);
				FAIL (db.GetLastError().c_str());
			}
		}

		//BlobTests (&db);

	} // mysql only

	SECTION ("support for alternate SQL batch delimeters")
	{
		KString sTmp;  sTmp.Format ("testksql{}b.sql", getpid());

		KFile fp (sTmp.c_str());
		fp.Write (R"(
			// This is a full-line comment

		)");

		if (db.GetDBType() == KSQL::DBT_SQLSERVER)
		{
			fp.Write (HereDoc(R"(
				|IF OBJECT_ID('FRED', 'U') IS NOT NULL DROP TABLE FRED;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
				|create table FRED (a char(10) not null);	;	;	;	;;;	; ;
				|insert into FRED values (';'); ; ;;; ;	;	;\r	 ;
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
				|insert into FRED values (';'); ; ;;; ;	;	;\r	 ;
				|insert into FRED values (\";\
				|
				|delimiter @@@
				|insert into FRED values (\"@@@\")@@@
				|insert into FRED values (\"@@@\")            @@@
				|
				|delimiter !!
				|drop table if exists FRED!!
				|create table FRED (a char(10) not null)!!
				|insert into FRED values ('!!')!!
				|insert into FRED values (\"!!\")!!!!!!!!
				|insert into FRED values (\"!!\")!! !! !!	!!
			)"));
		}
		fp.close();

		if (!db.ExecSQLFile (sTmp)) {
			FAIL (db.GetLastError().c_str());
		}

		kRemoveFile (sTmp);
	}

	KROW Row ("TEST_KSQL");

	SECTION ("KROW insert")
	{
		db.ExecSQL ("truncate table TEST_KSQL");
		SqlServerIdentityInsert (db, "TEST_KSQL", "ON");

		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		// KROW Operations
		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

		Row.AddCol ("anum",      100ull,            KROW::PKEY);
		Row.AddCol ("astring",   "krow insert");

		if (!db.Insert (Row)) {
			FAIL (db.GetLastError().c_str());
		}
	}

	SECTION ("KROW update")
	{
		Row.AddCol ("astring", "krow update");
		if (!db.Update (Row)) {
			FAIL (db.GetLastError().c_str());
		}
	}

	SECTION ("KROW delete")
	{
		Row.AddCol ("astring", "krow update");
		if (!db.Delete (Row)) {
			FAIL (db.GetLastError().c_str());
		}
	}
		
	SECTION ("KROW insert unicode characters")
	{
		db.SetFlags (KSQL::F_IgnoreSQLErrors);
		db.ExecSQL ("drop table TEST_UNICODE");
		db.SetFlags (0);
			
		db.ExecRawSQL (HereDoc (R"(
			|create table TEST_UNICODE (
			|    anum      int              not null primary key,
			|    bigstring nvarchar(4000)   null
			|))"));

		KROW URow ("TEST_UNICODE");
		URow.AddCol ("anum", 100ull, KROW::PKEY|KROW::NUMERIC);
		URow.AddCol ("bigstring", ASIAN1);
		if (!db.Insert(URow,true)) {
			FAIL (db.GetLastError().c_str());
		}
			
		KROW Cols;
		db.ExecQuery ("select * from TEST_UNICODE");
		db.NextRow (Cols);
		if (Cols.GetValue(1) != ASIAN1)
		{
			kWarning ("query did *NOT* return the column headers");
			FAIL (db.GetLastError().c_str());
		}

		URow.AddCol ("bigstring", ASIAN2);
		if (!db.Update(URow,true)) {
			FAIL (db.GetLastError().c_str());
		}
			
		db.ExecQuery ("select * from TEST_UNICODE");
		db.NextRow (Cols);
		if (Cols.GetValue(1) == ASIAN2)
		{
			kWarning ("query did *NOT* return the column headers");
			FAIL (db.GetLastError().c_str());
		}

		if (!db.Delete(URow,true)) {
			FAIL (db.GetLastError().c_str());
		}
	}

	db.ExecSQL ("drop table TEST_UNICODE");

	SECTION ("KROW clipping")
	{
		Row.AddCol ("astring",   "clip me here -- all this should be GONE", 0, strlen("clip me here"));
		db.Insert (Row);
		db.ExecQuery ("select astring from TEST_KSQL where anum=100");
		db.NextRow ();
		if (db.Get(1) != "clip me here")
		{
			kWarning ("string not clipped properly: '{}'", db.Get(1));
			FAIL (db.GetLastError().c_str());
		}
	}

	SECTION ("KROW smart clipping on quote")
	{
		Row.AddCol ("astring",   "clip me here' -- all this should be GONE", 0, strlen("clip me here'"));
		db.Update (Row);
		db.ExecQuery ("select astring from TEST_KSQL where anum=100");
		db.NextRow ();
		if (db.Get(1) != "clip me here") {
			kWarning ("string not clipped properly: '{}'", db.Get(1));
			FAIL (db.GetLastError().c_str());
		}
	}

	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	// Retry Logic:
	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	SqlServerIdentityInsert (db, "TEST_KSQL", "OFF");

	SECTION ("Retry logic when connection dies during an ExecSQL")
	{
		db.ExecSQL ("insert into TEST_KSQL (astring) values ('retry1')");
		SimulateLostConnection (&db);

		if (!db.ExecSQL ("insert into TEST_KSQL (astring) values ('retry2')")) {
			FAIL (db.GetLastError().c_str());
		}
	}

	SECTION ("Retry logic when connection dies during an ExecQuery")
	{
		db.ExecSQL ("insert into TEST_KSQL (astring) values ('retry3')");
		SimulateLostConnection (&db);
		auto iCount = db.SingleIntQuery ("select count(*) from TEST_KSQL where astring like 'retry{{PCT}}'");
		if (iCount != 3) {
			kWarning ("got: {} instead of 3", iCount);
			FAIL (db.GetLastError().c_str());
		}		
	}

	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	// Single quote handling.
	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	SECTION ("KROW single quote insert")
	{
		Row.clear();
		Row.AddCol ("astring", QUOTES1);

		if (!db.Insert (Row)) {
			FAIL (db.GetLastError().c_str());
		}

		Row.AddCol ("anum",    db.GetLastInsertID(), KROW::PKEY);
	}

	SECTION ("KROW single quote update")
	{
		Row.AddCol ("astring", QUOTES2, 0, 0);

		if (!db.Update (Row)) {
			FAIL (db.GetLastError().c_str());
		}
	}

	SECTION ("KROW single quote select")
	{
		if (!db.ExecQuery ("select * from TEST_KSQL where anum={}", Row.Get("anum").sValue.Int32()))
		{
			FAIL (db.GetLastError().c_str());
		}
		else if (!db.NextRow (Row)) {
			kWarning ("did not get back a row");
			FAIL (db.GetLastError().c_str());
		}
		else if (Row.Get("astring").sValue != QUOTES2) {
			FAIL (db.GetLastError().c_str());
			kWarning ("expected: {}", QUOTES2);
			kWarning ("     got: {}", Row.Get ("astring").sValue);
		}
	}

	SECTION ("KROW single quote delete")
	{
		Row.clear();
		Row.AddCol ("astring", QUOTES2, KROW::PKEY);

		if (!db.Delete (Row)) {
			FAIL (db.GetLastError().c_str());
		}
		else if (db.GetNumRowsAffected() != 1) {
			kWarning ("row not found");
			FAIL (db.GetLastError().c_str());
		}
	}

	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	// Slash handling.
	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	SECTION ("KROW slash test insert")
	{
		Row.clear();
		Row.AddCol ("anum",    98ull, KROW::PKEY);
		Row.AddCol ("astring", SLASHES1);

		SqlServerIdentityInsert (db, "TEST_KSQL", "ON");
		if (!db.Insert (Row)) {
			FAIL (db.GetLastError().c_str());
		}
	}

	SECTION ("KROW slash test update")
	{
		Row.AddCol ("astring", SLASHES2);

		if (!db.Update (Row)) {
			FAIL (db.GetLastError().c_str());
		}
	}

	SECTION ("KROW slash test select")
	{
		if (!db.ExecQuery ("select * from TEST_KSQL where anum=98")) {
			FAIL (db.GetLastError().c_str());
		}
		else if (!db.NextRow (Row)) {
			kWarning ("did not get back a row");
			FAIL (db.GetLastError().c_str());
		}
		else if (Row.Get("astring").sValue != SLASHES2) {
			kWarning ("expected: {}", SLASHES2);
			kWarning ("     got: {}", Row.Get ("astring").sValue);
			FAIL (db.GetLastError().c_str());
		}
	}

	SECTION ("KROW slash test delete")
	{
		Row.clear();
		Row.AddCol ("astring", SLASHES2, KROW::PKEY);

		// { LPTSTR dszSANITY = (char*)malloc(75); kstrncpy (dszSANITY, "SANITY", 75); free (dszSANITY); }

		#if 1
		if (!db.Delete (Row)) {
			FAIL (db.GetLastError().c_str());
		}
		else if (db.GetNumRowsAffected() != 1) {
			kWarning ("row not found");
			FAIL (db.GetLastError().c_str());
		}
		#endif
	}

	SECTION ("cleanup")
	{
		if (!db.ExecSQL ("drop table TEST_KSQL")) {
			FAIL (db.GetLastError().c_str());
		}
	}

} // TEST_CASE("ksql")

//-----------------------------------------------------------------------------
size_t DumpRows (KSQL& db)
//-----------------------------------------------------------------------------
{
	kDebug (1, "  {:<4.4} | {:4.4} | {<7.7} | {<9.9} | {20.20} |",
		"row#", "anum", "astring", "bigstring", "dtmnow");

	kDebug (1, "  {<4.4}-+-{4.4}-+-{<7.7}-+-{<9.9}-+-{20.20}-+",
		KLog::BAR, KLog::BAR, KLog::BAR, KLog::BAR, KLog::BAR);

	size_t iRows = 0;
	while (db.NextRow())
	{
		++iRows;

		auto  anum      = db.Get(1).Int32();
		auto  astring   = db.Get(2);
		auto  bigstring = db.Get(3);
		auto  pszTime   = db.Get(4);

		kDebug (1, "  {:<4.4} | {:4.4} | {<7.7} | {<9.9} | {20.20} |",
			iRows, anum, astring, bigstring, pszTime);
	}

	kDebug (1, "  {<4.4}-+-{4.4}-+-{<7.7}-+-{<9.9}-+-{20.20}-+",
		KLog::BAR, KLog::BAR, KLog::BAR, KLog::BAR, KLog::BAR);

	return (iRows);

} // DumpRows

//-----------------------------------------------------------------------------
void SimulateLostConnection (KSQL* pdb)
//-----------------------------------------------------------------------------
{
	#if 0  // re-enable if retry logic goes into question

	if (pdb->GetDBType() == KSQL::DBT_SQLSERVER)
	{
		for (int ss=15; ss > 0; --ss)
		{
			int iSpid = pdb->SingleIntQuery ("select @@spid");
			if (iSpid < 0)
				break; // connection is killed
			printf ("You have %d seconds to issue: KILL %d\n", ss, iSpid);
			sleep (1);
		}
	}

	#else

	pdb->CloseConnection (); // simulate connection being lost

	#endif

} // SimulateLostConnection

