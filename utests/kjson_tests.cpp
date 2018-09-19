#include "catch.hpp"

#include <dekaf2/kjson.h>
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

}
