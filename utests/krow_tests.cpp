#include "catch.hpp"
#include <dekaf2/krow.h>

using namespace dekaf2;

TEST_CASE("KROW")
{
	KROW row;

	row.AddCol("col1"  , "value1");
	row.AddCol("col1"  , "string1");
	row.AddCol("\"col2", "string2\"");
	row.AddCol("col2_2", 0, KROW::NONCOLUMN);
	row.AddCol(",col3" , "string3,");
	row.AddCol("col4"  , true);
	row.AddCol("col5"  , 1);
	row.AddCol("col5"  , 123);
	row.AddCol("col6"  , 123.456);
	row.AddCol("col6"  , 123.456);
	row.AddCol("col7"  , 0xFFFFFFFFFFFFFFFF);

	CHECK ( row.size() == 8 );

	SECTION("to_json")
	{
		{
			KJSON jRow = row.to_json();
			CHECK ( jRow.dump(-1) == R"({"\"col2":"string2\"",",col3":"string3,","col1":"string1","col4":true,"col5":123,"col6":123.456,"col7":18446744073709551615})" );
		}
		{
			KJSON jRow = row.to_json(KROW::KEYS_TO_UPPER);
			CHECK ( jRow.dump(-1) == R"({"\"COL2":"string2\"",",COL3":"string3,","COL1":"string1","COL4":true,"COL5":123,"COL6":123.456,"COL7":18446744073709551615})" );
		}
		{
			// implicit conversion
			KJSON jRow = row;
			CHECK ( jRow.dump(-1) == R"({"\"col2":"string2\"",",col3":"string3,","col1":"string1","col4":true,"col5":123,"col6":123.456,"col7":18446744073709551615})" );
		}
		{
			// implicit conversion both directions
			KJSON jRow = row;
			CHECK ( jRow.dump(-1) == R"({"\"col2":"string2\"",",col3":"string3,","col1":"string1","col4":true,"col5":123,"col6":123.456,"col7":18446744073709551615})" );
			KROW NewRow = jRow;
			KJSON jNewRow = NewRow;
			CHECK ( jNewRow.dump(-1) == R"({"\"col2":"string2\"",",col3":"string3,","col1":"string1","col4":true,"col5":123,"col6":123.456,"col7":18446744073709551615})" );
		}
	}

	SECTION("to_csv")
	{
		auto sCSV = row.to_csv(true);
		CHECK ( sCSV == "col1,\"\"\"col2\",\",col3\",col4,col5,col6,col7\r\n" );

		sCSV = row.to_csv(true, KROW::KEYS_TO_UPPER);
		CHECK ( sCSV == "COL1,\"\"\"COL2\",\",COL3\",COL4,COL5,COL6,COL7\r\n" );

		sCSV = row.to_csv();
		CHECK ( sCSV == "string1,\"string2\"\"\",\"string3,\",1,123,123.456,18446744073709551615\r\n" );
	}

}
