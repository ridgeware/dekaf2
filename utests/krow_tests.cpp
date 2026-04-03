#include "catch.hpp"
#include <dekaf2/krow.h>
#include <dekaf2/kfilesystem.h>
#include <dekaf2/kbase64.h>

using namespace dekaf2;

namespace {

KTempDir TempDir;

}

TEST_CASE("KROW")
{
	KROW row;

	row.AddCol("col1"  , "value1");
	row.AddCol("col1"  , "string1");
	row.AddCol("\"col2", "string2\"");
	row.AddCol("col2_2", 0, KCOL::NONCOLUMN);
	row.AddCol(",col3" , "string3,");
	row.AddCol("col4"  , true);
	row.AddCol("col5"  , 1);
	row.AddCol("col5"  , 123);
	row.AddCol("col6"  , 123.456);
	row.AddCol("col6"  , 123.456);
	row.AddCol("col7"  , 0xFFFFFFFFFFFFFFFF);
	row.AddCol("col8"  , 123, KCOL::MONEY);

	CHECK ( row.size() == 9 );

#ifndef _MSC_VER // MSC has issues with \" inside R"()";
	SECTION("to_json")
	{
		{
			KJSON jRow = row.to_json();
			CHECK ( jRow.dump(-1) == R"({"\"col2":"string2\"",",col3":"string3,","col1":"string1","col4":true,"col5":123,"col6":123.456,"col7":18446744073709551615,"col8":123.0})" );
		}
		{
			KJSON jRow = row.to_json(KROW::KEYS_TO_UPPER);
			CHECK ( jRow.dump(-1) == R"({"\"COL2":"string2\"",",COL3":"string3,","COL1":"string1","COL4":true,"COL5":123,"COL6":123.456,"COL7":18446744073709551615,"COL8":123.0})" );
		}
		{
			// implicit conversion
			KJSON jRow = row;
			CHECK ( jRow.dump(-1) == R"({"\"col2":"string2\"",",col3":"string3,","col1":"string1","col4":true,"col5":123,"col6":123.456,"col7":18446744073709551615,"col8":123.0})" );
		}
		{
			// implicit conversion both directions
			KJSON jRow = row;
			CHECK ( jRow.dump(-1) == R"({"\"col2":"string2\"",",col3":"string3,","col1":"string1","col4":true,"col5":123,"col6":123.456,"col7":18446744073709551615,"col8":123.0})" );
			KROW NewRow = jRow;
			KJSON jNewRow = NewRow;
			CHECK ( jNewRow.dump(-1) == R"({"\"col2":"string2\"",",col3":"string3,","col1":"string1","col4":true,"col5":123,"col6":123.456,"col7":18446744073709551615,"col8":123.0})" );
		}
		{
			// Latin1 instead of UTF8
			KROW row2;
			row2.AddCol("col1", "plain latin: \xE4\xC4");
			KJSON jrow = row2.to_json();
			CHECK ( jrow.dump(-1) == R"({"col1":"plain latin: äÄ"})" );
		}
	}
#endif

	SECTION("to_csv")
	{
		auto sCSV = row.to_csv(true);
		CHECK ( sCSV == "col1,\"\"\"col2\",\",col3\",col4,col5,col6,col7,col8\r\n" );

		sCSV = row.to_csv(true, KROW::KEYS_TO_UPPER);
		CHECK ( sCSV == "COL1,\"\"\"COL2\",\",COL3\",COL4,COL5,COL6,COL7,COL8\r\n" );

		sCSV = row.to_csv();
		CHECK ( sCSV == "string1,\"string2\"\"\",\"string3,\",1,123,123.456,18446744073709551615,123\r\n" );
	}

	SECTION("MySQL escapes")
	{
		{
			KROW::value_type Value { "key", KCOL { "123456789' ", KCOL::NOFLAG, 11 } };
			auto sEscaped = KROW::EscapeChars(Value, "\'\"\\`\0"_ksv, '\\');
			CHECK ( sEscaped == "123456789\\'" );
			CHECK ( sEscaped.size() == 11 );
		}
		{
			KROW::value_type Value { "key", KCOL { "123456789' ", KCOL::NOFLAG, 10 } };
			auto sEscaped = KROW::EscapeChars(Value, "\'\"\\`\0"_ksv, '\\');
			CHECK ( sEscaped == "123456789" );
			CHECK ( sEscaped.size() == 9 );
		}
		{
			KROW::value_type Value { "key", KCOL { "123456|||' ", KCOL::NOFLAG, 10 } };
			auto sEscaped = KROW::EscapeChars(Value, "|\'\"\\`\0"_ksv, '|');
			CHECK ( sEscaped == "123456||||" );
			CHECK ( sEscaped.size() == 10 );
		}
		{
			KROW::value_type Value { "key", KCOL { "123456\\\\\\' ", KCOL::NOFLAG, 10 } };
			auto sEscaped = KROW::EscapeChars(Value, "\'\"\\`\0"_ksv, '\\');
			CHECK ( sEscaped == "123456\\\\\\\\" );
			CHECK ( sEscaped.size() == 10 );
		}
		{
			KROW::value_type Value { "key", KCOL { "\\\\\\\\\\\\\\\\\\\\' ", KCOL::NOFLAG, 10 } };
			auto sEscaped = KROW::EscapeChars(Value, "\'\"\\`\0"_ksv, '\\');
			CHECK ( sEscaped == "\\\\\\\\\\\\\\\\\\\\" );
			CHECK ( sEscaped.size() == 10 );
		}
		{
			KROW::value_type Value { "key", KCOL { "\\\\\\\\\\\\\\\\\\' ", KCOL::NOFLAG, 10 } };
			auto sEscaped = KROW::EscapeChars(Value, "\'\"\\`\0"_ksv, '\\');
			CHECK ( sEscaped == "\\\\\\\\\\\\\\\\\\\\" );
			CHECK ( sEscaped.size() == 10 );
		}
	}

	SECTION("MSSQL escapes")
	{
		{
			KROW::value_type Value { "key", KCOL { "123456789' ", KCOL::NOFLAG, 11 } };
			auto sEscaped = KROW::EscapeChars(Value, "\"'"_ksv, '\0');
			CHECK ( sEscaped == "123456789" );
			CHECK ( sEscaped.size() == 9 );
		}
		{
			KROW::value_type Value { "key", KCOL { "123456789' ", KCOL::NOFLAG, 10 } };
			auto sEscaped = KROW::EscapeChars(Value, "\"'"_ksv, '\0');
			CHECK ( sEscaped == "123456789" );
			CHECK ( sEscaped.size() == 9 );
		}
		{
			KROW::value_type Value { "key", KCOL { "123456|||' ", KCOL::NOFLAG, 10 } };
			auto sEscaped = KROW::EscapeChars(Value, "|\"'"_ksv, '\0');
			CHECK ( sEscaped == "123456" );
			CHECK ( sEscaped.size() == 6 );
		}
		{
			KROW::value_type Value { "key", KCOL { "123456'''' ", KCOL::NOFLAG, 10 } };
			auto sEscaped = KROW::EscapeChars(Value, "\"'"_ksv, '\0');
			CHECK ( sEscaped == "123456" );
			CHECK ( sEscaped.size() == 6 );
		}
		{
			KROW::value_type Value { "key", KCOL { "'''''''''''' ", KCOL::NOFLAG, 10 } };
			auto sEscaped = KROW::EscapeChars(Value, "\"'"_ksv, '\0');
			CHECK ( sEscaped == "" );
			CHECK ( sEscaped.size() == 0 );
		}
	}

	SECTION("StoreLoad")
	{
		KROW row;
		row.AddCol("key1", "value1");
		row.AddCol("key2", "value2");
		row.AddCol("key3", "value3");

		KString sFilename = kFormat("{}{}RowDump", TempDir.Name(), kDirSep);
		row.Store(sFilename);

		KROW row2;
		row2.Load(sFilename);

		CHECK ( row == row2 );
	}

	SECTION("BINARY BinaryToSQL")
	{
		using DBT = KROW::DBT;

		// test data with NUL byte, single quote, and high bytes (simulates binary content)
		KString sBin("\xff\xd8\x00\x27\xab"_ksv);

		{
			auto sSQL = KROW::BinaryToSQL(sBin, DBT::MYSQL);
			CHECK ( sSQL == "X'ffd80027ab'" );
		}
		{
			auto sSQL = KROW::BinaryToSQL(sBin, DBT::SQLITE3);
			CHECK ( sSQL == "X'ffd80027ab'" );
		}
		{
			auto sSQL = KROW::BinaryToSQL(sBin, DBT::SQLSERVER);
			CHECK ( sSQL == "0xffd80027ab" );
		}
		{
			auto sSQL = KROW::BinaryToSQL(sBin, DBT::SQLSERVER15);
			CHECK ( sSQL == "0xffd80027ab" );
		}
		{
			auto sSQL = KROW::BinaryToSQL(sBin, DBT::SYBASE);
			CHECK ( sSQL == "0xffd80027ab" );
		}
		{
			auto sSQL = KROW::BinaryToSQL(sBin, DBT::POSTGRESQL);
			CHECK ( sSQL == "decode('ffd80027ab','hex')" );
		}
		{
			// empty binary
			auto sSQL = KROW::BinaryToSQL("", DBT::MYSQL);
			CHECK ( sSQL == "X''" );
		}
	}

	SECTION("BINARY FormInsert")
	{
		using DBT = KROW::DBT;

		KString sBin("\xff\xd8\x00\x27"_ksv);

		{
			KROW row("mytable");
			row.AddCol("id", 1);
			row.AddCol("img", sBin, KCOL::BINARY);

			auto sSQL = row.FormInsert(DBT::MYSQL);
			CHECK ( sSQL.str().contains("X'ffd80027'") );
			CHECK ( !sSQL.str().contains("'X'") ); // no extra quoting

			sSQL = row.FormInsert(DBT::SQLSERVER);
			CHECK ( sSQL.str().contains("0xffd80027") );

			sSQL = row.FormInsert(DBT::POSTGRESQL);
			CHECK ( sSQL.str().contains("decode('ffd80027','hex')") );
		}
	}

	SECTION("BINARY FormUpdate")
	{
		using DBT = KROW::DBT;

		KString sBin("\xff\xd8"_ksv);

		KROW row("mytable");
		row.AddCol("id", 1, KCOL::PKEY);
		row.AddCol("img", sBin, KCOL::BINARY);

		auto sSQL = row.FormUpdate(DBT::MYSQL);
		CHECK ( sSQL.str().contains("X'ffd8'") );
		CHECK ( sSQL.str().contains("update mytable") );

		sSQL = row.FormUpdate(DBT::SQLSERVER);
		CHECK ( sSQL.str().contains("0xffd8") );

		sSQL = row.FormUpdate(DBT::POSTGRESQL);
		CHECK ( sSQL.str().contains("decode('ffd8','hex')") );
	}

	SECTION("BINARY FormUpdate with BINARY PKEY")
	{
		using DBT = KROW::DBT;

		KString sBinKey("\x01\x02"_ksv);

		KROW row("mytable");
		row.AddCol("hash", sBinKey, KCOL::Flags(KCOL::PKEY | KCOL::BINARY));
		row.AddCol("name", "test");

		auto sSQL = row.FormUpdate(DBT::MYSQL);
		CHECK ( sSQL.str().contains("X'0102'") );
		CHECK ( sSQL.str().contains("where") );
	}

	SECTION("BINARY FormSelect")
	{
		using DBT = KROW::DBT;

		KString sBinKey("\xab\xcd"_ksv);

		KROW row("mytable");
		row.AddCol("hash", sBinKey, KCOL::Flags(KCOL::PKEY | KCOL::BINARY));
		row.AddCol("name", "");

		auto sSQL = row.FormSelect(DBT::MYSQL);
		CHECK ( sSQL.str().contains("X'abcd'") );
		CHECK ( sSQL.str().contains("where") );

		sSQL = row.FormSelect(DBT::POSTGRESQL);
		CHECK ( sSQL.str().contains("decode('abcd','hex')") );
	}

	SECTION("BINARY FormDelete")
	{
		using DBT = KROW::DBT;

		KString sBinKey("\xab\xcd"_ksv);

		KROW row("mytable");
		row.AddCol("hash", sBinKey, KCOL::Flags(KCOL::PKEY | KCOL::BINARY));

		auto sSQL = row.FormDelete(DBT::MYSQL);
		CHECK ( sSQL.str().contains("X'abcd'") );
		CHECK ( sSQL.str().contains("delete from mytable") );
	}

	SECTION("BINARY to_json base64")
	{
		KString sBin("\xff\xd8\x00\x27\xab"_ksv);

		KROW row("mytable");
		row.AddCol("id", 1);
		row.AddCol("img", sBin, KCOL::BINARY);

		auto json = row.to_json();

		// verify it's a base64-encoded string
		CHECK ( json["img"].is_string() );
		auto sBase64 = json["img"].get<std::string>();
		CHECK ( sBase64 == KBase64::Encode(sBin, false) );

		// verify roundtrip: decode base64 back to original bytes
		CHECK ( KBase64::Decode(sBase64) == sBin );
	}

	SECTION("BINARY AddCol(LJSON) with BINARY flag - base64 decode")
	{
		KString sBin("\xff\xd8\x00\x27\xab"_ksv);
		KString sBase64 = KBase64::Encode(sBin, false);

		LJSON json;
		json["img"] = sBase64;

		KROW row("mytable");
		row.AddCol("img", json["img"], KCOL::BINARY);

		CHECK ( row["img"] == sBin );
		CHECK ( static_cast<const KCOLS&>(row)["img"].HasFlag(KCOL::BINARY) );
	}

	SECTION("BINARY full roundtrip: KROW -> JSON -> KROW -> SQL")
	{
		using DBT = KROW::DBT;

		// original binary data with NUL, single quote, and high bytes
		KString sBin("\xff\xd8\x00\x27\x01\xab"_ksv);

		// step 1: KROW with binary column
		KROW row1("images");
		row1.AddCol("id", 42);
		row1.AddCol("img", sBin, KCOL::BINARY);

		// step 2: KROW -> JSON
		auto json = row1.to_json();
		CHECK ( json["img"].is_string() );

		// step 3: JSON -> KROW (with BINARY flag to trigger base64-decode)
		KROW row2("images");
		row2.AddCol("id", json["id"]);
		row2.AddCol("img", json["img"], KCOL::BINARY);

		// verify raw bytes are restored
		CHECK ( row2["img"] == sBin );
		CHECK ( static_cast<const KCOLS&>(row2)["img"].HasFlag(KCOL::BINARY) );

		// step 4: KROW -> SQL
		auto sMySQL = row2.FormInsert(DBT::MYSQL);
		CHECK ( sMySQL.str().contains("X'ffd80027") );

		auto sMSSQL = row2.FormInsert(DBT::SQLSERVER);
		CHECK ( sMSSQL.str().contains("0xffd80027") );

		auto sPG = row2.FormInsert(DBT::POSTGRESQL);
		CHECK ( sPG.str().contains("decode('ffd80027") );
	}

	SECTION("BINARY FlagsToString")
	{
		CHECK ( KCOL::FlagsToString(KCOL::BINARY).contains("BINARY") );
		CHECK ( KCOL::FlagsToString(KCOL::Flags(KCOL::PKEY | KCOL::BINARY)).contains("PKEY") );
		CHECK ( KCOL::FlagsToString(KCOL::Flags(KCOL::PKEY | KCOL::BINARY)).contains("BINARY") );
	}
}
