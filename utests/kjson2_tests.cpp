#include "catch.hpp"
#include <dekaf2/experimental/kjson2.h>
#include <dekaf2/krow.h>
#include <vector>

#ifndef DEKAF2_IS_WINDOWS

using namespace dekaf2;

namespace {

KJSON2 jsonAsPar(const KJSON2& json = KJSON2{})
{
	return json;
}
template<class T>
const T& jsonAsConstRef(const T& value) { return value; }
template<class T>
T& jsonAsRef(T& value) { return value; }

}

TEST_CASE("KJSON2")
{
	SECTION("Basic construction")
	{
		KJSON2 j1;
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
		value = j1.Select("key1");
		value = j1["key1"];
		CHECK ( value == "val1" );
		value = j1["key2"_ksz];
		CHECK ( value == "val2" );

		value = j1["object"]["currency"];
		CHECK ( value == "USD" );
		double d = j1["object"]["value"];
		CHECK ( d == 42.99 );
		KJSON2 j2 = j1["object"_ksv];
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
			auto obj = j1["object"];
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
		KJSON2 j1 = {
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
		KJSON2 j2 = j1["object"];
		value = j2["currency"];
		CHECK ( value == "USD" );
		d = j2["value"];
		CHECK ( d == 42.99 );
	}

	SECTION("LJSON basic ops")
	{
		KJSON2 obj;
		obj["one"] = 1;
		obj["two"] = 2;
		KJSON2 child;
		child["duck"] = "donald";
		child["pig"]  = "porky";
		KJSON child2;
		child["mouse"] = "mickey";
		KJSON2 arr0 = child2;
		KJSON2 arr1 = KJSON::array();
		KJSON2 arr2 = KJSON::array();
		arr1 += child;
		arr1 += child2;
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

		KJSON2 obj = row;
		CHECK( obj["first"] == "value1" );
		CHECK( obj["second"] == "value2" );
		CHECK( obj["third"].UInt64() == 12345UL );
	}

	SECTION("KROW - KJSON interoperability")
	{
		KJSON2 json;
		json["first"] = "value1";
		json["second"] = "value2";
		json["third"] = 12345;

		KROW row = json;
		row = json;
		CHECK( row["first"] == "value1" );
		CHECK( row["second"] == "value2" );
		CHECK( row["third"].Int64() == 12345 );

		KJSON2 json2 = row;

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
		KJSON2 json = {
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
		KJSON2 json;
		json = KJSON::array();
		json += KStringView("value1");
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
		KJSON json;
		json = KJSON::object();
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
		KJSON2 json = {
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

		const KJSON& object = kjson::GetObjectRef(json, "object");

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
		KJSON2 json;
		json["val1"] = "string";
		Increment(json, "val1",  5);
		Increment(json, "val1", 10);
		Increment(json, "val1");
		CHECK ( json["val1"].Int64() == 16 );
		json["val2"] = std::numeric_limits<uint64_t>::max();
		CHECK ( json["val2"].Int64() == static_cast<int64_t>(std::numeric_limits<uint64_t>::max()) );
		CHECK ( json["val2"].UInt64() == std::numeric_limits<uint64_t>::max() );
	}

	SECTION("Decrement")
	{
		KJSON2 json;
		Increment(json, "val1", 10);
		Decrement(json, "val1",  2);
		Decrement(json, "val1");
		CHECK ( json["val1"].Int64() == 7 );
	}

	SECTION("null")
	{
		KJSON2 json;
		CHECK ( json.is_null() );
		CHECK ( json.dump() == "null" );
		json = KJSON::object();
		CHECK ( json.is_object() );
		CHECK ( json.dump() == "{}" );
		json = KJSON::parse("null");
		CHECK ( json.is_null() );
		CHECK ( json.dump() == "null" );
	}

	SECTION("KStringView")
	{
		std::vector<KStringView> View { "one", "two", "three", "four", "five" };
		KJSON2 json
		{
			{ "view", View   }
		};
		CHECK ( json.dump() == R"({"view":["one","two","three","four","five"]})" );
	}

	SECTION("Implicit Select")
	{
		KJSON2 j1 = {
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

		CHECK ( j1["/key2"] == "val2" );
		CHECK ( j1["key2" ] == "val2" );
		CHECK ( j1["key3" ] == "" );
		CHECK ( j1["/answer/nothing"] == "naught" );
		CHECK ( j1["answer.nothing" ] == "naught" );
		CHECK ( j1["/answer/few/1"  ] == "two" );
		CHECK ( j1["answer.few[0]"  ] == "one" );
		CHECK ( j1["/slist/0"] == "one" );
		CHECK ( j1["/slist/1"] == "two" );
		CHECK ( j1["slist[0]"] == "one" );
		CHECK ( j1["slist[1]"] == "two" );
		CHECK ( j1["slist[4]"] == "" );
		CHECK ( j1["/answer/unknown"] == "" );
		CHECK ( j1["/answer/unknown"] != "something" );
	}

	SECTION("const Select")
	{
		const KJSON2 j1 = {
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

		CHECK ( j1.Select("/key2") == "val2" );
		CHECK ( j1.Select("key2" ) == "val2" );
		CHECK ( j1.Select("key3" ) == "" );
		CHECK ( j1.Select("/answer/nothing") == "naught" );
		CHECK ( j1.Select("answer.nothing" ) == "naught" );
		CHECK ( j1.Select("/answer/few/1"  ) == "two" );
		CHECK ( j1.Select("answer.few[0]"  ) == "one" );
		CHECK ( j1.Select("/slist/0") == "one" );
		CHECK ( j1.Select("/slist/1") == "two" );
		CHECK ( j1.Select("slist[0]") == "one" );
		CHECK ( j1.Select("slist[1]") == "two" );
		CHECK ( j1.Select("slist[4]") == "" );
		CHECK ( j1.Select("/answer/unknown") == "" );
		CHECK ( j1.Select("/answer/unknown") != "something" );
	}

	SECTION("SelectString")
	{
		KJSON2 j1 = {
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

		CHECK ( j1.Select("/key2").String() == "val2" );
		CHECK ( j1.Select("key2" ).String() == "val2" );
		CHECK ( j1.Select("/answer/nothing").String() == "naught" );
		CHECK ( j1.Select("answer.nothing" ).String() == "naught" );
		CHECK ( j1.Select("/answer/few/1"  ).String() == "two" );
		CHECK ( j1.Select("answer.few[0]"  ).String() == "one" );
		CHECK ( j1.Select("/slist/0").String() == "one" );
		CHECK ( j1.Select("/slist/1").String() == "two" );
		CHECK ( j1.Select("slist[0]").String() == "one" );
		CHECK ( j1.Select("slist[1]").String() == "two" );
		CHECK ( j1.Select("slist[4]").String() == "" );
		CHECK ( j1.Select("/answer/unknown").String() == "" );
	}

	SECTION("SelectObject")
	{
		KJSON2 j1 = {
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
		CHECK ( kjson::SelectObject(j1, "/answer/nothing") == KJSON() );
		CHECK ( kjson::SelectObject(j1, "/pi") == KJSON() );
	}

	SECTION("Assignment")
	{
		KJSON2 j1;
		j1["Key1"] = "Value1";
		j1["Key2"] = 0.2435;
		j1["Key3"] = std::numeric_limits<uint64_t>::max();
		j1["/Key4"] = 2435;
		j1["/Key5"] = KJSON {
			{"answer", {
				{"everything", 42},
				{"nothing", "naught"},
				{"few", { "one", "two", "three"}}
			}},
		};
		j1["/Key6/New/Path"] = "Created";
//		j1["/Key6/New/Path/String"] = "Created";
		CHECK ( j1["Key1"] == "Value1" );
		CHECK ( j1["Key2"].Float() == 0.2435 );
		CHECK ( j1["Key3"].UInt64() == std::numeric_limits<uint64_t>::max() );
		CHECK ( j1["Key4"].UInt64() == 2435 );
		CHECK ( j1["/Key5/answer/few/1"] == "two" );
		CHECK ( j1["Key5"]["answer"]["few"][1] == "two" );
		CHECK ( j1["Key5"]["answer"]["few"]["2"] == "three" );
		CHECK ( j1["/Key6/New/Path"] == "Created" );
	}

	SECTION("Reference")
	{
		KJSON2 j1 = {
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

		CHECK ( jsonAsConstRef<uint64_t>(j1.Select("/answer/everything")) == 42 );
		CHECK ( jsonAsConstRef<KString>(j1.Select("key1")) == "val1" );
	}
}
#endif
