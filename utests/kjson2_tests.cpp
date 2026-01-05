#include "catch.hpp"
#include <dekaf2/kjson.h>
#include <dekaf2/krow.h>
#include <vector>

#ifndef DEKAF2_IS_WINDOWS
#if defined(DEKAF2_WRAPPED_KJSON) || !defined(DEKAF2_IS_GCC)

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

void ambiguousPar(const KJSON2&) {}
void ambiguousPar(KStringView)   {}
bool ambiguousPar2(const KJSON2&)  { return true;  }
bool ambiguousPar2(const KString&) { return false; }
bool ambiguousPar2(KStringView)    { return false; }
void stringViewPar(KStringView)  {}
KString StringReturn(KStringView s) { return s; }
}

template<class T>
struct is_test1
: std::integral_constant<
	bool,
	std::is_same<const char*, typename std::decay<T>::type>::value
> {};

template<class T,
typename std::enable_if<is_test1<T>::value, int>::type = 0>
bool Matches(T&& t) { return true;  }

template<class T,
typename std::enable_if<is_test1<T>::value == false, int>::type = 0>
bool Matches(T&& t) { return false; }


TEST_CASE("KJSON2")
{
	SECTION("front/back")
	{
		KJSON2 j;
		j.front() = "hello";
		CHECK ( j == "hello" );
		KJSON2 j2;
		j2.back() = "hello";
		CHECK ( j2 == "hello" );
		KJSON2 j3;
		j3 = "a string";
		auto it = j3.find("key");
		CHECK ( it == j3.end() );
		auto i = j3.erase("key");
		CHECK ( i == 0 );
	}

	SECTION("Split")
	{
		KJSON2 j;
		j["key"] = "token1,token2,token3,andalongtokenthatoverstepsthessolimitationsonalllibimplementations";

		for (auto sName : j["key"].String().Split(","))
		{
			CHECK ( sName != "" );
		}

		KJSON2       jv = "variable";
		const KJSON2 jc = "const";

		const KString& sConstRef1 = jv;
		const KString& sConstRef2 = jc;
		const KString& sConstRef3 = jc.String();
		const KString& sConstRef4 = jc.ConstString();
//		KString& sRef1 = jc; // this cannot work
#ifdef DEKAF2_KJSON_ALLOW_IMPLICIT_CONVERSION_IN_PLACE
		KString& sRef2 = jv;
#endif
		KString  sConstCopy = jc;
		KString  sValueCopy = jv;
		sConstCopy = jc;
		sValueCopy = jv;

		KJSON2 jm = "movable";
		KString sMoved = std::move(jm);

		KStringView sSV1 = jc;
		KStringView sSV2 = jv;

		CHECK ( sConstRef1 == "variable" );
		CHECK ( sConstRef2 == "const" );
		CHECK ( sConstRef3 == "const" );
		CHECK ( sConstRef4 == "const" );
#ifdef DEKAF2_KJSON_ALLOW_IMPLICIT_CONVERSION_IN_PLACE
		CHECK ( sRef2 == "variable" );
#endif
		CHECK ( sConstCopy == "const" );
		CHECK ( sValueCopy == "variable" );
		CHECK ( sSV1 == "const" );
		CHECK ( sSV2 == "variable" );
		CHECK ( jm.ConstString() == "" );
		CHECK ( jm == "" );

		std::string s1;
		s1 = jc;
		CHECK ( s1 == "const" );
		s1 = jv;
		CHECK ( s1 == "variable" );
		const KJSON2 j3 = 123;
		KJSON2 j4 = 123;
		s1 = j3;
		CHECK ( s1 == "" );
		s1 = j4;
		CHECK ( s1 == "123" );

		auto sv = StringReturn("abc,def,ghi").Split();
		CHECK ( sv.size() == 3 );
	}

	SECTION("chars")
	{
		LJSON l;
		l = "abc";
		KString s = "def";
		if (l == s)
		{
			CHECK ( false );
		}
		KJSON2 j;
		j = "abc";
		if (j == s)
		{
			CHECK ( false );
		}
		j = "def";
		if (j == s)
		{
			CHECK ( true );
		}
		auto b1 = Matches("hello");
		CHECK ( b1 == true );
		auto b2 = Matches("hello"_ks);
		CHECK ( b2 == false );
	}

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
		value = j1["key1"_ks];
		CHECK ( value == "val1" );
		value = j1["key2"_ksz];
		CHECK ( value == "val2" );

		value = j1["object"]["currency"];
		CHECK ( value == "USD" );
		double d = j1["object"]["value"];
		CHECK ( d == 42.99 );
		float f = j1["object"]["value"];
		CHECK ( f == 42.99f );
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

		KJSON2 j2 = j1;
		CHECK ( j2 == j1 );

		KString value = j1["key1"];
		CHECK ( value == "val1" );
		value = j1["key2"];
		CHECK ( value == "val2" );

		value = j1["object"]["currency"];
		CHECK ( value == "USD" );
		double d = j1["object"]["value"];
		CHECK ( d == 42.99 );
		j2.clear();
//		auto p(j1["object"]);
		auto p = j1["object"];
		p = j1["object"];
		j2 = p;
		j2 = j1["object"];
		value = j2["currency"];
		CHECK ( value == "USD" );
		d = j2["value"];
		CHECK ( d == 42.99 );

		auto j1Orig(j1);
		CHECK ( j1Orig == j1 );
		auto j3(j1Orig);
		CHECK ( j1Orig == j3 );
		j2 = std::move(j1);
		CHECK ( j2 == j1Orig );
		CHECK ( j1.empty() );
		KJSON2 j4(std::move(j3));
		CHECK ( j1Orig == j4 );
		CHECK ( j3.empty() );
	}

	SECTION("Array expansion")
	{
		{
			LJSON j;
			j["myObject"] = LJSON({{"some","object"}});
			auto& ref = j["myArray"] = LJSON::array();
			CHECK ( j.dump() == R"({"myArray":[],"myObject":{"some":"object"}})" );
			ref = { 1,2,3 };
			CHECK ( j.dump() == R"({"myArray":[1,2,3],"myObject":{"some":"object"}})" );
			ref = { 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33 };
			CHECK ( j.dump() == R"({"myArray":[1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33],"myObject":{"some":"object"}})" );
		}

		{
			KJSON2 j;
			j["myObject"]= KJSON2({{"some", "object"}});
			auto& ref = j["myArray"] = KJSON2::array();
			CHECK ( j.dump() == R"({"myArray":[],"myObject":{"some":"object"}})" );
			ref = { 1,2,3 };
			CHECK ( j.dump() == R"({"myArray":[1,2,3],"myObject":{"some":"object"}})" );
			ref = { 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33 };
			CHECK ( j.dump() == R"({"myArray":[1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33],"myObject":{"some":"object"}})" );
		}
	}

	SECTION("KJSON2 basic ops")
	{
		KJSON2 obj;
		obj["/one"] = 1;
		CHECK ( obj("/one") == 1 );
		{
			LJSON lobj2; // null
			LJSON::json_pointer p("/2");
			lobj2[p] = 2;
			CHECK ( lobj2[p] == 2 );
			CHECK ( lobj2.dump() == "[null,null,2]" );
			KJSON2 obj2; // null
			obj2.Select("/2") = 2;
			CHECK ( obj2.Select("/2") == 2 );
			CHECK ( obj2.dump() == "[null,null,2]" );
			CHECK ( obj2(2) == 2 );
		}
		{
			LJSON lobj2 = LJSON::object();
			LJSON::json_pointer p("/2");
			lobj2[p] = 2;
			CHECK ( lobj2[p] == 2 );
			CHECK ( lobj2.dump() == R"({"2":2})" );
			KJSON2 obj2 = KJSON2::object();
			obj2.Select("/2") = 2;
			CHECK ( obj2.Select("/2") == 2 );
			CHECK ( obj2.dump() == R"({"2":2})" );
			CHECK ( obj2(2) == nullptr );
		}
		{
			LJSON lobj2 = LJSON::array();
			LJSON::json_pointer p("/2");
			lobj2[p] = 2;
			CHECK ( lobj2[p] == 2 );
			CHECK ( lobj2.dump() == "[null,null,2]" );
			KJSON2 obj2 = KJSON2::array();
			obj2.Select("/2") = 2;
			CHECK ( obj2.Select("/2") == 2 );
			CHECK ( obj2.dump() == "[null,null,2]" );
			CHECK ( obj2(2) == 2 );
		}
		{
//			LJSON lobj2 = true; // primitive, would crash
//			LJSON::json_pointer p("/2");
//			lobj2[p] = 2;
//			CHECK ( lobj2[p] == 2 );
//			CHECK ( lobj2.dump() == "[null,null,2]" );
			KJSON2 obj2 = true; // primitive
			obj2.Select("/2") = 2;
			CHECK ( obj2.Select("/2") == 2 );
			CHECK ( obj2.dump() == "[null,null,2]" );
			CHECK ( obj2(2) == 2 );
		}

		{
			LJSON lobj2; // null
			lobj2["2"] = 2;
			CHECK ( lobj2["2"] == 2 );
			CHECK ( lobj2.dump() == R"({"2":2})" );
			KJSON2 obj2; // null
			obj2["2"] = 2;
			CHECK ( obj2("2") == 2 );
			CHECK ( obj2.dump() == R"({"2":2})" );
			CHECK ( obj2(2) == nullptr );
		}
		{
			LJSON lobj2 = LJSON::object();
			lobj2["2"] = 2;
			CHECK ( lobj2["2"] == 2 );
			CHECK ( lobj2.dump() == R"({"2":2})" );
			KJSON2 obj2 = KJSON2::object();
			obj2["2"] = 2;
			CHECK ( obj2("2") == 2 );
			CHECK ( obj2.dump() == R"({"2":2})" );
			CHECK ( obj2(2) == nullptr );
		}
		{
//			LJSON lobj2 = LJSON::array(); // would crash
//			lobj2["2"] = 2;
//			CHECK ( lobj2["2"] == 2 );
//			CHECK ( lobj2.dump() == R"({"2":2})" );
			KJSON2 obj2 = KJSON2::array();
			obj2["2"] = 2;
			CHECK ( obj2("2") == 2 );
			CHECK ( obj2.dump() == R"({"2":2})" );
			CHECK ( obj2(2) == nullptr );
		}
		{
//			LJSON lobj2 = true; // primitive, would crash
//			LJSON::json_pointer p("/2");
//			lobj2[p] = 2;
//			CHECK ( lobj2[p] == 2 );
//			CHECK ( lobj2.dump() == "[null,null,2]" );
			KJSON2 obj2 = true; // primitive
			obj2["2"] = 2;
			CHECK ( obj2("2") == 2 );
			CHECK ( obj2.dump() == R"({"2":2})" );
			CHECK ( obj2(2) == nullptr );
		}
		KJSON2 obj3 = KJSON2::object();
		obj3.Select("/array/1") = 3;
		CHECK ( obj3.Select("/array/1") == 3 );
		CHECK ( obj3.dump() == R"({"array":[null,3]})" );
		obj["two"] = 2;
		CHECK ( obj("two") == 2 );
		CHECK ( obj.dump() == R"({"/one":1,"two":2})" );
		KJSON2 child;
		child["duck"] = "donald";
		child["pig"]  = "porky";
		if (child == "str"_ks) {}
		KJSON child2;
		child["mouse"] = "mickey";
		KJSON2 arr0 = child2;
		KJSON2 arr1 = KJSON::array();
		KJSON2 arr2 = KJSON::array();
		arr1 += child;
		arr1 += child2;
		obj["three"] = arr1;
		obj["four"] = arr2;

		{
			KJSON2 j1    = KJSON2::object();
			j1["key"]    = KJSON2::object();
			KJSON2& j2   = j1["key"];
			j2["123"].push_back({ {"key1", 123 }, { "key2", true } });
			CHECK ( j2.dump() == R"({"123":[{"key1":123,"key2":true}]})" );
		}
		{
			LJSON j0;
			j0 = kjson::Parse(R"({"array":[],"boolean":false,"result":"found"})");
			const LJSON j1 = j0;
			CHECK ( j1.dump() == R"({"array":[],"boolean":false,"result":"found"})" );
			LJSON j2 = LJSON::object();
			j2["key"] = "value";
			j2["list"] = LJSON::array();
			j2["list"].push_back({
				{ "boolean", j1["boolean"] }
			});
			CHECK ( j2.dump() == R"({"key":"value","list":[{"boolean":false}]})" );

		}
		{
			KJSON2 j0;
			j0.Parse(R"({"array":[],"boolean":false,"result":"found"})");
			const KJSON2 j1 = j0;
			CHECK ( j1.dump() == R"({"array":[],"boolean":false,"result":"found"})" );
			KJSON2 j2 = KJSON2::object();
			j2["key"] = "value";
			j2["list"] = KJSON::array();
			j2["list"].push_back({
				{ "boolean1", false },
				{ "boolean2", j1["boolean"] }
			});
			CHECK ( j2.dump() == R"({"key":"value","list":[{"boolean1":false,"boolean2":false}]})" );
		}
	}

	SECTION("KJSON2 - KROW interoperability")
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
		CHECK( obj["third"] == 12345UL );

		KJSON2 obj2(row);
		CHECK( obj2["first"] == "value1" );
		CHECK( obj2["second"] == "value2" );
		CHECK( obj2["third"] == 12345UL );


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

		if (json == json2) {}
		CHECK ( json2 == json );
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

		auto& object = kjson::GetObjectRef(json, "object");

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
		CHECK ( json["val2"]          == std::numeric_limits<uint64_t>::max() );
		CHECK ( json["val2"].UInt64() == std::numeric_limits<uint64_t>::max() );
		CHECK ( json["val2"].Int64()  == static_cast<int64_t>(std::numeric_limits<uint64_t>::max()) );
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
		json = LJSON::parse("null");
		CHECK ( json.is_null() );
		CHECK ( json.dump() == "null" );
	}

	SECTION("compile time checks")
	{
		KJSON2 json = { 1,2,3,4 };
		if (json[0] == 1) {}
		if (json["abc"] == "") {}
		static_cast<void>(json.Select(0));
		static_cast<void>(json.Select("abc"));
		json = { 1,2,3,4 };

		ambiguousPar("here"_ksv);
		ambiguousPar(json);
		ambiguousPar(*json.begin());
		kDebug(1, json.dump());
		CHECK ( *json.begin() == 1 );

		for (auto& it : json)
		{
			ambiguousPar(it);
		}

		for (auto& it : json.items())
		{
			ambiguousPar(it.value());
		}

		auto it = kjson::Find(json, "test");
		if (it == json.end()) {}

		stringViewPar(json["par"]);

		ambiguousPar2(KString()); // to satisfy the compiler..
		ambiguousPar2(KStringView()); // same here ..
		CHECK ( ambiguousPar2(json) );
		const KJSON2 jconst;
		CHECK ( ambiguousPar2(jconst) );
	}

	SECTION("Implicit merge")
	{
		KJSON2 obj1, obj2;
		obj1["key1"] = 14;
		obj2["key2"] += obj1;
		CHECK ( obj2.dump() == R"({"key2":[{"key1":14}]})" );

		KJSON2  tjson;
		KJSON2& jheaders = tjson["headers"] = KJSON2::object();
		CHECK ( jheaders.dump() == R"({})" );
		jheaders += { "one", "oneval" };
		CHECK ( jheaders.dump() == R"({"one":"oneval"})" );
		CHECK_NOTHROW( ( jheaders += { 7, "oneval" } ) ); // throws, but is caught
		CHECK ( jheaders.dump() == R"({"one":"oneval"})" );
		jheaders += { "two", "twoval" };
		CHECK ( jheaders.dump() == R"({"one":"oneval","two":"twoval"})" );
		CHECK ( tjson.dump() == R"({"headers":{"one":"oneval","two":"twoval"}})" );

		KJSON2 a1, a2;
		a2 = KJSON2::array();
		a2 += "1";
		a2 += "2";
		a2 += "3";
		a1["e"] += a2;
		CHECK ( a1.dump() == R"({"e":[["1","2","3"]]})" );

		a1 = KJSON2();
		a2 = KJSON2::array();
		a2.push_back("1");
		a2.push_back("2");
		a2.push_back("3");
		a1["e"] += a2;
		CHECK ( a1.dump() == R"({"e":[["1","2","3"]]})" );

		a1 = KJSON2::object();
		a1["key"] = "value";
		CHECK_NOTHROW( ( a1 += 2 ) );
		CHECK ( a1.dump() == R"([{"key":"value"},2])" );

		KJSON2 variant;
		variant += 1;
		variant += "string";
		variant += 3.14;
		variant += { 1 , "string", 4.1 };
		int i = variant(0);
		CHECK ( i == 1 );
		KString s = variant(1);
		CHECK ( s == "string" );
		double d = variant(2);
		CHECK ( d == 3.14 );
		auto j = variant(3);
		CHECK ( j.dump() == R"([1,"string",4.1])" );
		CHECK ( variant.dump() == R"([1,"string",3.14,[1,"string",4.1]])" );
	}

	SECTION("conversions")
	{
		KJSON2 json;
		json = 123;
		CHECK ( ( json ==  123 ) );
		KString& sref = json.String();
		CHECK ( sref == "123" );
		CHECK ( json == "123" );
		CHECK ( sref == "123" );
		int i = json;
		CHECK ( i == 123 );
		KString s2 = std::move(json.String());
		CHECK ( json == "" );
		CHECK ( s2 == "123" );

		json = "string";
		KString s = json;
		CHECK ( s == "string" );
		const KString& t = json;
		CHECK ( s == "string" );
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

	SECTION("Comparisons")
	{
		LJSON  j1;
		KJSON2 j2;
		KJSON2 j22;

		// eq
		if (j2 == j1) { };
		if (j2 == j22) { };
		if (j2 == "") { };
		if ("" == j2) { };
		if (j2 ==  1) { };
		if (1 ==  j2) { };
		if (j2["test"] == "" ) { };
		if (j2("test") == "" ) { };

		if (j1 == j2) { };
		if (j1 == "") { };
		if (j1 ==  1) { };

		j2["object"] = { 1, 2, 3, 4 };

		CHECK ( j2("object") == KJSON2({ 1, 2, 3, 4 }) );
		CHECK ( j2("object") == LJSON ({ 1, 2, 3, 4 }) );
		CHECK ( KJSON2({ 1, 2, 3, 4 }) == j2("object") );
		CHECK ( LJSON ({ 1, 2, 3, 4 }) == j2("object") );

		// neq
		if (j2 != j1) { };
		if (j2 != j22) { };
		if (j2 != "") { };
		if ("" != j2) { };
		if (j2 !=  1) { };
		if (1 !=  j2) { };
		if (j2["test"] != "" ) { };
		if (j2("test") != "" ) { };

		if (j1 != j2) { };
		if (j1 != "") { };
		if (j1 !=  1) { };

		j2["object"] = { 1, 2, 3, 5 };
		if (j2("object") != KJSON2({ 1, 2, 3, 4 })) {}
		if (j2("object") != LJSON ({ 1, 2, 3, 4 })) {}
		if ( KJSON2({ 1, 2, 3, 4 }) != j2("object")) {}
		if ( LJSON ({ 1, 2, 3, 4 }) != j2("object")) {}
		CHECK ( j2("object") != KJSON2({ 1, 2, 3, 4 }) );
		CHECK ( j2("object") != LJSON ({ 1, 2, 3, 4 }) );
		CHECK ( KJSON2({ 1, 2, 3, 4 }) != j2("object") );
		CHECK ( LJSON ({ 1, 2, 3, 4 }) != j2("object") );

		// lt
		if (j2 < j1) { };
		if (j2 < j22) { };
		if (j2 < "") { };
		if ("" < j2) { };
		if (j2 <  1) { };
		if (1 <  j2) { };
		if (j2["test"] < "" ) { };
		if (j2("test") < "" ) { };

		if (j1 < j2) { };
		if (j1 < "") { };
		if (j1 <  1) { };

		j2["object"] = { 1, 2, 3, 2 };
		if (j2("object") < KJSON2({ 1, 2, 3, 4 })) {}
		if (j2("object") < LJSON ({ 1, 2, 3, 4 })) {}
		if ( KJSON2({ 1, 2, 3, 4 }) < j2("object")) {}
		if ( LJSON ({ 1, 2, 3, 4 }) < j2("object")) {}

		// these do not work on gcc/3way - TBD
//		CHECK ( j2("object") < KJSON2({ 1, 2, 3, 4 }) );
//		CHECK ( j2("object") < LJSON ({ 1, 2, 3, 4 }) );
//		CHECK ( KJSON2({ 1, 2, 3, 1 }) < j2("object") );
//		CHECK ( LJSON ({ 1, 2, 3, 1 }) < j2("object") );

		// le
		if (j2 <= j1) { };
		if (j2 <= j22) { };
		if (j2 <= "") { };
		if ("" <= j2) { };
		if (j2 <=  1) { };
		if (1 <=  j2) { };
		if (j2["test"] <= "" ) { };
		if (j2("test") <= "" ) { };

		if (j1 <= j2) { };
		if (j1 <= "") { };
		if (j1 <=  1) { };

		j2["object"] = { 1, 2, 3, 2 };
		if (j2("object") <= KJSON2({ 1, 2, 3, 4 })) {}
		if (j2("object") <= LJSON ({ 1, 2, 3, 4 })) {}
		if ( KJSON2({ 1, 2, 3, 4 }) <= j2("object")) {}
		if ( LJSON ({ 1, 2, 3, 4 }) <= j2("object")) {}

		CHECK ( j2("object") <= KJSON2({ 1, 2, 3, 4 }) );
		CHECK ( j2("object") <= LJSON ({ 1, 2, 3, 4 }) );
		CHECK ( KJSON2({ 1, 2, 3, 1 }) <= j2("object") );
		CHECK ( LJSON ({ 1, 2, 3, 1 }) <= j2("object") );

		// gt
		if (j2 > j1) { };
		if (j2 > j22) { };
		if (j2 > "") { };
		if ("" > j2) { };
		if (j2 >  1) { };
		if (1 >  j2) { };
		if (j2["test"] > "" ) { };
		if (j2("test") > "" ) { };

		if (j1 > j2) { };
		if (j1 > "") { };
		if (j1 >  1) { };

		j2["object"] = { 1, 2, 3, 6 };
		if (j2("object") > KJSON2({ 1, 2, 3, 4 })) {}
		if (j2("object") > LJSON ({ 1, 2, 3, 4 })) {}
		if ( KJSON2({ 1, 2, 3, 4 }) > j2("object")) {}
		if ( LJSON ({ 1, 2, 3, 4 }) > j2("object")) {}

		// these do not work on gcc/3way - TBD
//		CHECK ( j2("object") > KJSON2({ 1, 2, 3, 4 }) );
//		CHECK ( j2("object") > LJSON ({ 1, 2, 3, 4 }) );
//		CHECK ( KJSON2({ 1, 2, 3, 8 }) > j2("object") );
//		CHECK ( LJSON ({ 1, 2, 3, 8 }) > j2("object") );

		// ge
		if (j2 >= j1) { };
		if (j2 >= j22) { };
		if (j2 >= "") { };
		if ("" >= j2) { };
		if (j2 >=  1) { };
		if (1  >=  j2) { };
		if (j2["test"] >= "" ) { };
		if (j2("test") >= "" ) { };

		if (j1 >= j2) { };
		if (j1 >= "") { };
		if (j1 >=  1) { };

		j2["object"] = { 1, 2, 3, 6 };
		if (j2("object") >= KJSON2({ 1, 2, 3, 4 })) {}
		if (j2("object") >= LJSON ({ 1, 2, 3, 4 })) {}
		if ( KJSON2({ 1, 2, 3, 4 }) >= j2("object")) {}
		if ( LJSON ({ 1, 2, 3, 4 }) >= j2("object")) {}

		CHECK ( j2("object") >= KJSON2({ 1, 2, 3, 4 }) );
		CHECK ( j2("object") >= LJSON ({ 1, 2, 3, 4 }) );
		CHECK ( KJSON2({ 1, 2, 3, 8 }) >= j2("object") );
		CHECK ( LJSON ({ 1, 2, 3, 8 }) >= j2("object") );
	}

	SECTION("Select and struct changes")
	{
		KJSON  j1;

		KJSON2 j2 = {
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

		j2["/answer/few/1"  ] = 2;
		CHECK ( j2("/answer/few/1") == 2 );

		j2["/answer/few/2"  ] = "three";
		CHECK ( j2("/answer/few/2") == "three" );

		j2["/answer/few"  ] = "three";
		CHECK ( j2("/answer/few") == "three" );

		if (j2 == j1) { };
		if (j2 == "") { };
		if ("" == j2) { };
		if (j2 ==  1) { };
		if (1 ==  j2) { };
		if (j2["test"] == "" ) { };
		if (j2("test") == "" ) { };

		if (j1 == j2) { };
		if (j1 == "") { };
		if (j1 ==  1) { };

		j2["object"] = { 1, 2, 3, 4 };
		CHECK ( j2("object") == KJSON2({ 1, 2, 3, 4 }) );

	}

	SECTION("Select on const object")
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
		CHECK ( j1.Select(".answer.nothing" ) == "naught" );
		CHECK ( j1.Select("/answer/few/1"  ) == "two" );
		CHECK ( j1.Select(".answer.few[0]"  ) == "one" );
		CHECK ( j1.Select("/slist/0") == "one" );
		CHECK ( j1.Select("/slist/1") == "two" );
		CHECK ( j1.Select(".slist[0]") == "one" );
		CHECK ( j1.Select(".slist[1]") == "two" );
		CHECK ( j1.Select(".slist[4]") == "" );
		CHECK ( j1.Select("/answer/unknown") == "" );
		CHECK ( j1.Select("/answer/unknown") != "something" );
	}

	SECTION("SelectConst")
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

		CHECK ( j1.ConstSelect("/key2") == "val2" );
		CHECK ( j1.ConstSelect("key2" ) == "val2" );
		CHECK ( j1.ConstSelect("key3" ) == "" );
		CHECK ( j1.ConstSelect("/answer/nothing") == "naught" );
		CHECK ( j1.ConstSelect(".answer.nothing") == "naught" );
		CHECK ( j1.ConstSelect("/answer/few/1"  ) == "two" );
		CHECK ( j1.ConstSelect(".answer.few[0]" ) == "one" );
		CHECK ( j1.ConstSelect("/slist/0") == "one" );
		CHECK ( j1.ConstSelect("/slist/1") == "two" );
		CHECK ( j1.ConstSelect(".slist[0]") == "one" );
		CHECK ( j1.ConstSelect(".slist[1]") == "two" );
		CHECK ( j1.ConstSelect(".slist[4]") == "" );
		CHECK ( j1.ConstSelect("/answer/unknown") == "" );
		CHECK ( j1.ConstSelect("/answer/unknown") != "something" );
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
		CHECK ( j1.Select(".answer.nothing").String() == "naught" );
		CHECK ( j1.Select("/answer/few/1"  ).String() == "two" );
		CHECK ( j1.Select(".answer.few[0]" ).String() == "one" );
		CHECK ( j1.Select("/slist/0").String() == "one" );
		CHECK ( j1.Select("/slist/1").String() == "two" );
		CHECK ( j1.Select(".slist[0]").String() == "one" );
		CHECK ( j1.Select(".slist[1]").String() == "two" );
		CHECK ( j1.Select(".slist[4]").String() == "" );
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

	SECTION("Implicit conversion")
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

		uint64_t                 iAnswer = j1["answer"]["everything"];
		double                        pi = j1["pi"];
		bool                      bHappy = j1["happy"];
		KString                     sVal = j1["key1"];
		// these are not implicit with KJSON2, because we do not have a template operator T()
//		std::vector<int>             Vec1 = j1["ilist"].get<decltype(Vec1)>();
//		std::map<std::string, LJSON> Map1 = j1.get<decltype(Map1)>();
//		std::vector<int>             Vec2 = j1["ilist"].ToBase();
//		std::map<std::string, LJSON> Map2 = j1.ToBase();

		CHECK ( iAnswer == 42 );
		CHECK ( pi == 3.141 );
		CHECK ( bHappy == true );
		CHECK ( sVal == "val1" );
//		CHECK ( Vec1 == (std::vector<int>{ 1, 0, 2 }) );
	}

	SECTION("Assignment")
	{
		KJSON2 j1;
		j1["Key1"] = "Value1";
		j1["Key2"] = 0.2435;
		j1["Key3"] = std::numeric_limits<uint64_t>::max();
		j1.Select("/Key4") = 2435;
//		j1["/Key5"] =  { "test" };
		j1.Select("/Key5") =  {
			{"answer", {
				{"everything", 42},
				{"nothing", "naught"},
				{"few", { "one", "two", "three"}}
			}}
		};
		j1.Select("/Key6/New/Path") = "Created";
		if (j1.Select("/Key6/New/Path/String") == "") {}
		j1.Select("/Key7/New/Array/2") = "Created1";
		j1.Select("/Key7/New/Array/String") = "Created2";
		j1.Select("/Key6/New/Path/2") = "Created2";
		CHECK ( j1.Select("/Key6/New/Path/2") == "Created2" );
		CHECK ( j1["Key1"] == "Value1" );
		CHECK ( j1["Key2"] == 0.2435 );
		CHECK ( j1["Key3"] == std::numeric_limits<uint64_t>::max() );
		CHECK ( j1["Key4"] == 2435 );
		CHECK ( j1.Select("/Key5/answer/few/1") == "two" );
		CHECK ( j1["Key5"]["answer"]["few"][1] == "two" );
		CHECK ( j1["Key5"]["answer"]["few"].Select("/2") == "three" );
		CHECK ( j1["Key5"]["answer"]["few"]["2"] == nullptr );
		CHECK ( j1["Key5"]["answer"]["few"].Select("/2") == "" );
		j1.Select("/Key6/New/Path/0") = "IsArray";

		KJSON2 j2(j1.Select("Key5"));
		CHECK ( j2.dump() == R"({"answer":{"everything":42,"few":{"2":null},"nothing":"naught"}})" );
	}

	SECTION("initializer lists")
	{
		KJSON2 j1 = {{ "one", "two", "three"}};
		KJSON2 j2 	{
			{ "key"      , "val" },
			{ "array"    , j1    }
		};
		CHECK ( j2.dump() == R"({"array":[["one","two","three"]],"key":"val"})" );

		LJSON l1 = {{ "one", "two", "three"}};
		LJSON l2 	{
			{ "key"      , "val" },
			{ "array"    , l1    }
		};
		CHECK ( l2.dump() == R"({"array":[["one","two","three"]],"key":"val"})" );
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

	SECTION("Primitives")
	{
		{
			KJSON2 j = 123;
			uint64_t i = j;
			CHECK ( i == 123 );
		}
		{
			KJSON2 j = 1.23;
			double d = j;
			CHECK ( d == 1.23 );
		}
		{
			KJSON2 j = true;
			bool b = j;
			CHECK ( b == true );
		}
		{
			KJSON2 j = "string";
			KString s = j;
			CHECK ( s == "string" );
		}
	}

	SECTION("Merge")
	{
		{
			KJSON2 j1 = {
				{"answer", {
					{"everything", 42},
					{"nothing", "naught"},
					{"few", { "one", "two", "three"}}
				}}
			};
			KJSON2 j2 = {{
				"object", {
					{"currency", "USD"},
					{"value", 42.99}
				}
			}};
			j1.Merge(j2);
			CHECK (j1.dump() == R"({"answer":{"everything":42,"few":["one","two","three"],"nothing":"naught"},"object":{"currency":"USD","value":42.99}})" );
		}
		{
			KJSON2 j1;
			KJSON2 j2 = { 1, 2, 3, 4 };
			j1.Merge(j2);
			CHECK (j1.dump() == R"([1,2,3,4])" );
		}
		{
			KJSON2 j1;
			KJSON2 j2 = "hello world";
			j1.Merge(j2);
			CHECK (j1.dump() == R"("hello world")" );
		}
		{
			KJSON2 j1;
			KJSON2 j2 = true;
			j1.Merge(j2);
			CHECK (j1.dump() == R"(true)" );
		}
		{
			KJSON2 j1;
			KJSON2 j2 = 46;
			j1.Merge(j2);
			CHECK (j1.dump() == R"(46)" );
		}
		{
			KJSON2 j1 = { 1, 2, 3, 4 };
			KJSON2 j2 = { 5, 6, 7, 8 };
			j1.Merge(j2);
			CHECK (j1.dump() == R"([1,2,3,4,5,6,7,8])" );
		}
		{
			KJSON2 j1 = { 1, 2, 3, 4 };
			KJSON2 j2 = { 55 };
			j1.Merge(j2);
			CHECK (j1.dump() == R"([1,2,3,4,55])" );
		}
		{
			KJSON2 j1 = { 1, 2, 3, 4 };
			KJSON2 j2 = { "hello" };
			j1.Merge(j2);
			CHECK (j1.dump() == R"([1,2,3,4,"hello"])" );
		}
		{
			KJSON2 j1 = { 1, 2, 3, 4 };
			KJSON2 j2 = { "hello", 5, "world" };
			j1.Merge(j2);
			CHECK (j1.dump() == R"([1,2,3,4,"hello",5,"world"])" );
		}
		{
			KJSON2 j1 = { 1, 2, 3, 4 };
			KJSON2 j2 = {{
				"object", {
					{"currency", "USD"},
					{"value", 42.99}
				}
			}};
			j1.Merge(j2);
			CHECK (j1.dump() == R"([1,2,3,4,{"object":{"currency":"USD","value":42.99}}])" );
		}
		{
			KJSON2 j1 = "string";
			j1 += "string2";
			CHECK (j1.dump() == R"(["string","string2"])");
		}
	}

#if !DEKAF2_IS_GCC || DEKAF2_GCC_VERSION_MAJOR != 11 || __GNUC_MINOR__ != 3
	// disable this test for gcc 11.3 (at least) - it insists that the for loop has a dangling string
	SECTION("auto range for loop")
	{
		KJSON2 j = { { "array", { "col1", "col2", "col3" } } };
		CHECK ( j.dump() == R"({"array":["col1","col2","col3"]})" );

		auto& Array = j("array").Array();

		if (!Array.empty())
		{
			KString sValues;

			for (const KString& sEntry : Array)
			{
				if (!sValues.empty())
				{
					sValues += ',';
				}

				sValues += sEntry;
			}

			CHECK ( sValues == "col1,col2,col3" );
		}
	}
#endif

	SECTION("format")
	{
		KJSON2 j1 = "hello world";
		CHECK ( kFormat("{}", j1) == "hello world" );
		j1 = 123;
		CHECK ( kFormat("{}", j1) == "123" );
		j1 = 1.23;
		CHECK ( kFormat("{}", j1) == "1.23" );
		j1 = true;
		CHECK ( kFormat("{}", j1) == "true" );
		j1 = nullptr;
		CHECK ( kFormat("{}", j1) == "NULL" );
		j1 = { 1, 2, 3, 4,
			{
				{ "object",
					{
						{"currency", "USD"},
						{"value",    42.99}
					}
				}
			}
		};
		CHECK ( kFormat("{}", j1) == R"([1,2,3,4,{"object":{"currency":"USD","value":42.99}}])" );
	}

	SECTION("iterate")
	{
		KJSON2 j = { 1, 2, 3 };
		auto it = j.begin();
		CHECK ( *it == 1 );
		++it;
		CHECK ( *it == 2 );
		++it;
		CHECK ( *it == 3 );
		++it;
		CHECK ( it == j.end() );
	}

	SECTION("reverse iterate")
	{
		KJSON2 j = { 1, 2, 3 };
		auto it = j.rbegin();
		CHECK ( *it == 3 );
		++it;
		CHECK ( *it == 2 );
		++it;
		CHECK ( *it == 1 );
		++it;
		CHECK ( it == j.rend() );
	}
}
#endif
#endif
