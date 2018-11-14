#include "catch.hpp"

#include <dekaf2/kjson.h>
#include <dekaf2/krow.h>
#include <vector>

using namespace dekaf2;

TEST_CASE("KJSON")
{
	SECTION("Basic construction")
	{
		KJSON j1;
		kjson::Parse(j1, R"(
				   {
				       "key1": "val1",
				       "key2": "val2",
		               "list": [1, 0, 2],
				       "object": {
		                   "currency": "USD",
		                   "value": 42.99
				       }
				   }
				   )");

		KString value;
		value = j1["key1"];
		CHECK ( value == "val1" );
		value = j1["key2"_ksz];
		CHECK ( value == "val2" );

		value = j1["object"]["currency"];
		CHECK ( value == "USD" );
		double d = j1["object"]["value"];
		CHECK ( d == 42.99 );
		KJSON j2 = j1["object"_ksv];
		value = j2["currency"];
		CHECK ( value == "USD" );
		d = j2["value"];
		CHECK ( d == 42.99 );
		KString sKey;
		sKey = "key1";
		value = j1[sKey];
		CHECK ( value == "val1" );
		if (!kjson::IsObject(j1, "object"))
		{
			CHECK ( false );
		}
		if (!kjson::IsObject(j1, "object"_ks))
		{
			CHECK ( false );
		}
		if (!kjson::IsObject(j1, "object"_ksv))
		{
			CHECK ( false );
		}
		if (!kjson::IsObject(j1, "object"_ksz))
		{
			CHECK ( false );
		}
		if (j1.is_object())
		{
			KJSON obj = j1["object"];
			if (!obj.is_object())
			{
				CHECK ( false );
			}
			value = obj["currency"];
			CHECK ( value == "USD" );
		}
		else
		{
			CHECK ( false );
		}

		value = kjson::GetString(j1, "key2");
		CHECK ( value == "val2" );

		value = kjson::GetString(j1, "not existing");
		CHECK ( value == "" );
	}

	SECTION("Initializer list construction")
	{
		KJSON j1 = {
		    {"pi", 3.141},
		    {"happy", true},
		    {"key1", "val1"},
		    {"key2", "val2"},
		    {"nothing", nullptr},
		    {"answer", {
		         {"everything", 42}
		     }},
		    {"list", {1, 0, 2}},
		    {"object", {
		         {"currency", "USD"},
		         {"value", 42.99}
		     }}
		};

		KString value = j1["key1"];
		CHECK ( value == "val1" );
		value = j1["key2"];
		CHECK ( value == "val2" );

		value = j1["object"]["currency"];
		CHECK ( value == "USD" );
		double d = j1["object"]["value"];
		CHECK ( d == 42.99 );
		KJSON j2 = j1["object"];
		value = j2["currency"];
		CHECK ( value == "USD" );
		d = j2["value"];
		CHECK ( d == 42.99 );
	}

	SECTION("LJSON basic ops")
	{
		KJSON obj;
		obj["one"] = 1;
		obj["two"] = 2;
		KJSON child;
		child["duck"] = "donald";
		child["pig"]  = "porky";
		KJSON arr1 = KJSON::array();
		KJSON arr2 = KJSON::array();
		arr1 += child;
		obj["three"] = arr1;
		obj["four"] = arr2;
	}

	SECTION("KJSON - KROW interoperability")
	{
		KROW row;
		row.AddCol("first", "value1");
		row.AddCol("second", "value2");
		row.AddCol("third", 12345);

		CHECK( row["first"] == "value1" );
		CHECK( row["second"] == "value2" );
		CHECK( row["third"].Int64() == 12345 );

		KJSON obj = row;
		CHECK( obj["first"] == "value1" );
		CHECK( obj["second"] == "value2" );
		CHECK( obj["third"] == 12345 );
	}

	SECTION("KROW - KJSON interoperability")
	{
		KJSON json;
		json["first"] = "value1";
		json["second"] = "value2";
		json["third"] = 12345;

		KROW row = json;
		CHECK( row["first"] == "value1" );
		CHECK( row["second"] == "value2" );
		CHECK( row["third"].Int64() == 12345 );

		KJSON json2 = row;

		CHECK ( json == json2 );

		KROW row2;
		row2.AddCol("fourth", "value4");

		row2 += json;
		CHECK( row2["first"] == "value1" );
		CHECK( row2["second"] == "value2" );
		CHECK( row2["third"].Int64() == 12345 );
		CHECK( row2["fourth"] == "value4" );
	}

	SECTION("Print")
	{
		KJSON json = {
			{"pi", 3.141529},
			{"happy", true},
			{"key1", "val1"},
			{"key2", "val2"},
			{"days", 365 },
			{"nothing", nullptr},
			{"answer", {
				{"everything", 42}
			}},
			{"list", {1, 0, 2}},
			{"object", {
				{"currency", "USD"},
				{"value", 42.99}
			}}
		};

		KString sString = Print(json);
		static constexpr KStringView sExpected1 = R"({"answer":{"everything":42},"days":365,"happy":true,"key1":"val1","key2":"val2","list":[1,0,2],"nothing":null,"object":{"currency":"USD","value":42.99},"pi":3.141529})";
		CHECK ( sString == sExpected1 );

		sString = Print(json["key1"]);
		static constexpr KStringView sExpected2 = "val1";
		CHECK ( sString == sExpected2 );

		sString = Print(json["days"]);
		static constexpr KStringView sExpected3 = "365";
		CHECK ( sString == sExpected3 );

		sString = Print(json["pi"]);
		static constexpr KStringView sExpected4 = "3.141529";
		CHECK ( sString == sExpected4 );

		sString = Print(json["wrong"]);
		static constexpr KStringView sExpected5 = "NULL";
		CHECK ( sString == sExpected5 );

		sString = Print(json["nothing"]);
		static constexpr KStringView sExpected6 = "NULL";
		CHECK ( sString == sExpected6 );

		sString = Print(json["list"]);
		static constexpr KStringView sExpected7 = "[1,0,2]";
		CHECK ( sString == sExpected7 );

		sString = Print(json["answer"]);
		static constexpr KStringView sExpected8 = "{\"everything\":42}";
		CHECK ( sString == sExpected8 );

	}

/*
		KJSON obj2;
		obj2["fourth"] = 87654;
 		// this does not work because KJSON does not know how to merge two objects
		obj2 += row;
		CHECK( obj2["first"] == "value1" );
		CHECK( obj2["second"] == "value2" );
		CHECK( obj2["third"] == 12345 );
		CHECK( obj2["fourth"] == 87654 );
*/

}
