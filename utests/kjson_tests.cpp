#include "catch.hpp"
#include <dekaf2/kjson.h>
#include <dekaf2/krow.h>
#include <vector>

#ifndef DEKAF2_IS_WINDOWS
#ifndef DEKAF2_WRAPPED_KJSON

using namespace dekaf2;

namespace {
LJSON jsonAsPar(const LJSON& json = LJSON{})
{
	return json;
}
}

TEST_CASE("LJSON")
{
	SECTION("Basic construction")
	{
		LJSON j1;
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
		const KString& sref = j1["key1"];
		CHECK ( sref == "val1" );
		CHECK ( value == "val1" );
		value = j1["key2"_ksz];
		CHECK ( value == "val2" );

		value = j1["object"]["currency"];
		CHECK ( value == "USD" );
		double d = j1["object"]["value"];
		CHECK ( d == 42.99 );
		LJSON j2 = j1["object"_ksv];
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
			LJSON obj = j1["object"];
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
		LJSON j1 = {
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
		LJSON j2 = j1["object"];
		value = j2["currency"];
		CHECK ( value == "USD" );
		d = j2["value"];
		CHECK ( d == 42.99 );
	}

	SECTION("LJSON basic ops")
	{
		LJSON obj;
		obj["one"] = 1;
		obj["two"] = 2;
		LJSON child;
		child["duck"] = "donald";
		child["pig"]  = "porky";
		LJSON arr1 = LJSON::array();
		LJSON arr2 = LJSON::array();
		arr1 += child;
		arr1 += child;
		obj["three"] = arr1;
		obj["four"] = arr2;
	}

	SECTION("LJSON - KROW interoperability")
	{
		KROW row;
		row.AddCol("first", "value1");
		row.AddCol("second", "value2");
		row.AddCol("third", 12345);

		CHECK( row["first"] == "value1" );
		CHECK( row["second"] == "value2" );
		CHECK( row["third"].Int64() == 12345 );

		LJSON obj = row;
		CHECK( obj["first"] == "value1" );
		CHECK( obj["second"] == "value2" );
		CHECK( obj["third"] == 12345 );
	}

	SECTION("KROW - LJSON interoperability")
	{
		LJSON json;
		json["first"] = "value1";
		json["second"] = "value2";
		json["third"] = 12345;

		KROW row = json;
		CHECK( row["first"] == "value1" );
		CHECK( row["second"] == "value2" );
		CHECK( row["third"].Int64() == 12345 );

		LJSON json2 = row;

		CHECK ( json == json2 );

		KROW row2;
		row2.AddCol("fourth", "value4");

		row2 += json;
		CHECK( row2["first"] == "value1" );
		CHECK( row2["second"] == "value2" );
		CHECK( row2["third"].Int64() == 12345 );
		CHECK( row2["fourth"] == "value4" );
	}

	SECTION("Implicit merge")
	{
		LJSON obj1, obj2;
		obj1["key1"] = 14;
		obj2["key2"] += obj1;
		CHECK ( obj2.dump() == R"({"key2":[{"key1":14}]})" );

		LJSON  tjson;
		LJSON& jheaders = tjson["headers"] = LJSON::object();
		jheaders += { "one", "oneval" };
		jheaders += { "two", "twoval" };
		CHECK ( tjson.dump() == R"({"headers":{"one":"oneval","two":"twoval"}})" );

		LJSON a1, a2;
		a2 = LJSON::array();
		a2 += "1";
		a2 += "2";
		a2 += "3";
		a1["e"] += a2;
		CHECK ( a1.dump() == R"({"e":[["1","2","3"]]})" );

		a1 = LJSON();
		a2 = LJSON::array();
		a2.push_back("1");
		a2.push_back("2");
		a2.push_back("3");
		a1["e"] += a2;
		CHECK ( a1.dump() == R"({"e":[["1","2","3"]]})" );
	}

	SECTION("Print")
	{
		LJSON json = {
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

	SECTION("Contains (array)")
	{
		LJSON json;
		json = LJSON::array();
		json += "value1";
		json += "value2";
		json += "value3";
		json += "value4";
		json += "value6";
		json += "value7";

		CHECK ( Contains(json, "value3") == true  );
		CHECK ( Contains(json, "value9") == false );
	}

	SECTION("Contains (object)")
	{
		LJSON json;
		json = LJSON::object();
		json += { "key1", "value1" };
		json += { "key2", "value2" };
		json += { "key3", "value3" };
		json += { "key4", "value4" };
		json += { "key5", "value5" };
		json += { "key6", "value6" };
		json += { "key7", "value7" };

		CHECK ( Contains(json, "key3") == true  );
		CHECK ( Contains(json, "key9") == false );
	}

	SECTION("GetObjectRef")
	{
		LJSON json = {
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

		const LJSON& object = kjson::GetObjectRef(json, "object");

		CHECK ( object["currency"] == "USD" );
		CHECK ( object["value"]    == 42.99 );
	}

	SECTION("implicit instantiation")
	{
		KString sVal1 { "val1" };
		auto json = jsonAsPar(
							  {
								  {"pi", 3.141529},
								  {"happy", true},
								  {"key1", sVal1},
								  {"key2", "val2"},
								  {"days", 365 }
							  });

		CHECK ( json.dump() == R"({"days":365,"happy":true,"key1":"val1","key2":"val2","pi":3.141529})" );

		json = jsonAsPar(
						 {{
							 {"pi", 3.141529},
							 {"happy", true},
							 {"key1", sVal1},
							 {"key2", "val2"},
							 {"days", 365 }
						 },
							 {
								 {"pa", 3.141529},
								 {"happy", false},
								 {"key1", "val3"},
								 {"key2", "val4"},
								 {"days", 180 }
							 }});

		CHECK ( json.dump() == R"([{"days":365,"happy":true,"key1":"val1","key2":"val2","pi":3.141529},{"days":180,"happy":false,"key1":"val3","key2":"val4","pa":3.141529}])" );
	}

	SECTION("Increment")
	{
		LJSON json;
		json["val1"] = "string";
		Increment(json, "val1",  5);
		Increment(json, "val1", 10);
		Increment(json, "val1");
		CHECK ( json["val1"] == 16 );
	}

	SECTION("Decrement")
	{
		LJSON json;
		Increment(json, "val1", 10);
		Decrement(json, "val1",  2);
		Decrement(json, "val1");
		CHECK ( json["val1"] == 7 );
	}

	SECTION("null")
	{
		LJSON json;
		CHECK ( json.is_null() );
		CHECK ( json.dump(-1) == "null" );
		json = LJSON::object();
		CHECK ( json.is_object() );
		CHECK ( json.dump(-1) == "{}" );
		json = LJSON::parse("null");
		CHECK ( json.is_null() );
		CHECK ( json.dump(-1) == "null" );
	}

	SECTION("KStringView")
	{
		std::vector<KStringView> View { "one", "two", "three", "four", "five" };
		LJSON json
		{
			{ "view", View   }
		};
		CHECK ( json.dump(-1) == R"({"view":["one","two","three","four","five"]})" );
	}

	SECTION("Select")
	{
		LJSON j1 = {
			{"pi", 3.141},
			{"happy", true},
			{"key1", "val1"},
			{"key2", "val2"},
			{"nothing", nullptr},
			{"answer", {
				{"everything", 42},
				{"nothing", "naught"},
				{"few", { "one", "two", "three"}}
			}},
			{"ilist", {1, 0, 2}},
			{"slist", {"one", "two", "three"}},
			{"object", {
				{"currency", "USD"},
				{"value", 42.99}
			}}
		};

		CHECK ( kjson::Select(j1, "/key2") == "val2" );
		CHECK ( kjson::Select(j1, "key2" ) == "val2" );
		CHECK ( kjson::Select(j1, "/answer/nothing") == "naught" );
		CHECK ( kjson::Select(j1, ".answer.nothing" ) == "naught" );
		CHECK ( kjson::Select(j1, "/answer/few/1"  ) == "two" );
		CHECK ( kjson::Select(j1, ".answer.few[0]"  ) == "one" );
		CHECK ( kjson::Select(j1, "/slist/0") == "one" );
		CHECK ( kjson::Select(j1, "/slist/1") == "two" );
		CHECK ( kjson::Select(j1, ".slist[0]") == "one" );
		CHECK ( kjson::Select(j1, ".slist[1]") == "two" );
		CHECK ( kjson::Select(j1, ".slist[4]") == LJSON() );
		CHECK ( kjson::Select(j1, "/answer/unknown") == LJSON() );
		CHECK ( kjson::Select(j1, "/answer/unknown") != "something" );
	}

	SECTION("SelectString")
	{
		LJSON j1 = {
			{"pi", 3.141},
			{"happy", true},
			{"key1", "val1"},
			{"key2", "val2"},
			{"nothing", nullptr},
			{"answer", {
				{"everything", 42},
				{"nothing", "naught"},
				{"few", { "one", "two", "three"}}
			}},
			{"ilist", {1, 0, 2}},
			{"slist", {"one", "two", "three"}},
			{"object", {
				{"currency", "USD"},
				{"value", 42.99}
			}}
		};

		CHECK ( kjson::SelectString(j1, "/key2") == "val2" );
		CHECK ( kjson::SelectString(j1, "key2" ) == "val2" );
		CHECK ( kjson::SelectString(j1, "/answer/nothing") == "naught" );
		CHECK ( kjson::SelectString(j1, ".answer.nothing" ) == "naught" );
		CHECK ( kjson::SelectString(j1, "/answer/few/1"  ) == "two" );
		CHECK ( kjson::SelectString(j1, ".answer.few[0]"  ) == "one" );
		CHECK ( kjson::SelectString(j1, "/slist/0") == "one" );
		CHECK ( kjson::SelectString(j1, "/slist/1") == "two" );
		CHECK ( kjson::SelectString(j1, ".slist[0]") == "one" );
		CHECK ( kjson::SelectString(j1, ".slist[1]") == "two" );
		CHECK ( kjson::SelectString(j1, ".slist[4]") == "" );
		CHECK ( kjson::SelectString(j1, "/answer/unknown") == "" );
	}

	SECTION("SelectObject")
	{
		LJSON j1 = {
			{"pi", 3.141},
			{"happy", true},
			{"key1", "val1"},
			{"key2", "val2"},
			{"nothing", nullptr},
			{"answer", {
				{"everything", 42},
				{"nothing", "naught"},
				{"few", { "one", "two", "three"}}
			}},
			{"ilist", {1, 0, 2}},
			{"slist", {"one", "two", "three"}},
			{"object", {
				{"currency", "USD"},
				{"value", 42.99}
			}}
		};

		CHECK ( kjson::SelectObject(j1, "/answer").dump() == R"({"everything":42,"few":["one","two","three"],"nothing":"naught"})" );
		CHECK ( kjson::SelectObject(j1, "/answer/nothing") == LJSON() );
		CHECK ( kjson::SelectObject(j1, "/pi") == LJSON() );
	}

	SECTION("Implicit conversion")
	{
		LJSON j1 = {
			{"pi", 3.141},
			{"happy", true},
			{"key1", "val1"},
			{"key2", "val2"},
			{"nothing", nullptr},
			{"answer", {
				{"everything", 42},
				{"nothing", "naught"},
				{"few", { "one", "two", "three"}}
			}},
			{"ilist", {1, 0, 2}},
			{"slist", {"one", "two", "three"}},
			{"object", {
				{"currency", "USD"},
				{"value", 42.99}
			}}
		};

		uint64_t                 iAnswer = j1["answer"]["everything"];
		double                        pi = j1["pi"];
		bool                      bHappy = j1["happy"];
		KString                     sVal = j1["key1"];
#ifndef DEKAF2_WRAPPED_KJSON
		std::vector<int>             Vec = j1["ilist"];
		std::map<std::string, LJSON> Map = j1;
#endif

		CHECK ( iAnswer == 42 );
		CHECK ( pi == 3.141 );
		CHECK ( bHappy == true );
		CHECK ( sVal == "val1" );
#ifndef DEKAF2_WRAPPED_KJSON
		CHECK ( Vec == (std::vector<int>{ 1, 0, 2 }) );
#endif
	}

	SECTION("Assignment")
	{
		LJSON j1;
		j1["Key1"] = "Value1";
		j1["Key2"] = 0.2435;
		j1["Key3"] = std::numeric_limits<uint64_t>::max();
		j1["Key4"] = 2435;
		j1["Key5"] = {
			{"answer", {
				{"everything", 42},
				{"nothing", "naught"},
				{"few", { "one", "two", "three"}}
			}}
		};
		CHECK ( j1["Key1"] == "Value1" );
		CHECK ( j1["Key2"] == 0.2435 );
		CHECK ( j1["Key3"] == std::numeric_limits<uint64_t>::max() );
		CHECK ( j1["Key4"] == 2435 );
	}

	SECTION("Primitives")
	{
		{
			LJSON j = 123;
			uint64_t i = j;
			CHECK ( i == 123 );
		}
		{
			LJSON j = 1.23;
			double d = j;
			CHECK ( d == 1.23 );
		}
		{
			LJSON j = true;
			bool b = j;
			CHECK ( b == true );
		}
		{
			LJSON j = "string";
			KString s = j;
			CHECK ( s == "string" );
		}
	}

	SECTION("Merge")
	{
		{
			LJSON j1 = {
				{"answer", {
					{"everything", 42},
					{"nothing", "naught"},
					{"few", { "one", "two", "three"}}
				}}
			};
			LJSON j2 = {{
				"object", {
					{"currency", "USD"},
					{"value", 42.99}
				}
			}};
			kjson::Merge(j1, j2);
			CHECK (j1.dump() == R"({"answer":{"everything":42,"few":["one","two","three"],"nothing":"naught"},"object":{"currency":"USD","value":42.99}})" );
		}
		{
			LJSON j1;
			LJSON j2 = { 1, 2, 3, 4 };
			kjson::Merge(j1, j2);
			CHECK (j1.dump() == R"([1,2,3,4])" );
		}
		{
			LJSON j1;
			LJSON j2 = "hello world";
			kjson::Merge(j1, j2);
			CHECK (j1.dump() == R"("hello world")" );
		}
		{
			LJSON j1;
			LJSON j2 = 46;
			kjson::Merge(j1, j2);
			CHECK (j1.dump() == R"(46)" );
		}
		{
			LJSON j1 = { 1, 2, 3, 4 };
			LJSON j2 = { 5, 6, 7, 8 };
			kjson::Merge(j1, j2);
			CHECK (j1.dump() == R"([1,2,3,4,5,6,7,8])" );
		}
		{
			LJSON j1 = { 1, 2, 3, 4 };
			LJSON j2 = 55;
			kjson::Merge(j1, j2);
			CHECK (j1.dump() == R"([1,2,3,4,55])" );
		}
		{
			LJSON j1 = { 1, 2, 3, 4 };
			LJSON j2 = "hello";
			kjson::Merge(j1, j2);
			CHECK (j1.dump() == R"([1,2,3,4,"hello"])" );
		}
		{
			LJSON j1 = { 1, 2, 3, 4 };
			LJSON j2 = { "hello", 5, "world" };
			kjson::Merge(j1, j2);
			CHECK (j1.dump() == R"([1,2,3,4,"hello",5,"world"])" );
		}
		{
			LJSON j1 = { 1, 2, 3, 4 };
			LJSON j2 = {{
				"object", {
					{"currency", "USD"},
					{"value", 42.99}
				}
			}};
			kjson::Merge(j1, j2);
			CHECK (j1.dump() == R"([1,2,3,4,{"object":{"currency":"USD","value":42.99}}])" );
		}
	}
}
#endif
#endif
