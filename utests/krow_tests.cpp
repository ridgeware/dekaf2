#include "catch.hpp"
#include <dekaf2/krow.h>

using namespace dekaf2;

TEST_CASE("KROW")
{
	SECTION("to_csv")
	{
		KROW row;

		row.AddCol("col1"  , "string1");
		row.AddCol("\"col2", "string2\"");
		row.AddCol("col2_2", 0, KROW::NONCOLUMN);
		row.AddCol(",col3" , "string3,");
		row.AddCol("col4"  , true);
		row.AddCol("col5"  , 123);
		row.AddCol("col6"  , 123.456);

		CHECK ( row.size() == 7 );

		auto sCSV = row.to_csv(true);
		CHECK ( sCSV == "col1,\"\"\"col2\",\",col3\",col4,col5,col6\r\n" );

		sCSV = row.to_csv(true, KROW::KEYS_TO_UPPER);
		CHECK ( sCSV == "COL1,\"\"\"COL2\",\",COL3\",COL4,COL5,COL6\r\n" );

		sCSV = row.to_csv();
		CHECK ( sCSV == "string1,\"string2\"\"\",\"string3,\",1,123,123.456\r\n" );
	}

}
