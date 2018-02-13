#include "catch.hpp"

#include <dekaf2/kjson.h>
#include <vector>

using namespace dekaf2;

TEST_CASE("KJSON")
{
	SECTION("Basic construction")
	{
		KJSON j1 = R"(
				   {
				       "key1": "val1",
				       "key2": "val2",
		               "list": [1, 0, 2],
				       "object": {
		                   "currency": "USD",
		                   "value": 42.99
				       }
				   }
				   )"_json;

		KString value;
		value = j1["key1"];
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
		KString sKey;
		sKey = "key1";
		value = j1[sKey];
		CHECK ( value == "val1" );
		if (!j1.IsObject("object"))
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

		KString ks;
		std::basic_string<char> std("abcde");
		ks = std;

		CHECK( ks == "abcde" );
	}
/*
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

		std::string value = j1["key1"];
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
*/
}
