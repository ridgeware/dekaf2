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

	SECTION("KSQLInjectionSafeString")
	{
		KSQL DB;
		DB.SetDBType(KSQL::DBT::MYSQL);
		auto sAnd   = DB.FormatSQL(" and key2='{}'", "test2");
		auto sQuery = DB.FormatSQL("select * from ABC where key1='{}'{}", "test1", sAnd);
		CHECK ( sQuery == "select * from ABC where key1='test1' and key2='test2'");

		{
			KSQLInjectionSafeString sSafe = "select * from test";
			auto sSafe2 = DB.FormatSQL(sSafe);
			CHECK ( sSafe2 == "select * from test" );
		}
		{
			KSQLInjectionSafeString sSafe = "select * from test where key = '{}'";
			auto sSafe2 = DB.FormatSQL(sSafe, "something");
			CHECK ( sSafe2 == "select * from test where key = 'something'" );
		}
		{
			KSQLInjectionSafeString sSafe = "select * from test where key = {}";
			auto sSafe2 = DB.FormatSQL(sSafe, "'something'"); // note: no escaping for const data!
			CHECK ( sSafe2 == "select * from test where key = 'something'" );
		}
		{
			KSQLInjectionSafeString sSafe = "select * from test where key = {}";
			KString sWhat = "'something'";
			auto sSafe2 = DB.FormatSQL(sSafe, sWhat); // note: escaping for non-const data!
			CHECK ( sSafe2 == "select * from test where key = \\'something\\'" );
		}
		{
			KSQLInjectionSafeString sSafe = "select * from test where key = {}";
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

//		auto sNo = DB.FormatSQL(kFormat("select {}", "key1"));
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
	}

	SECTION("FormOrderBy")
	{
		KSQL DB;
		DB.SetDBType(KSQL::DBT::MYSQL);

		KSQLInjectionSafeString sOrderBy;
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
		KSQLInjectionSafeString sSQL { "update xx set date={{NOW}}, {{DATETIME}} {{MAXCHAR}} {{unknown}}{{CHAR2000}} date{{PCT}} {{AUTO_INCREMENT}}, {{UTC}} {{$$}}.{{PID}}.{{DC}}{{hostname}}}}" };
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
}
