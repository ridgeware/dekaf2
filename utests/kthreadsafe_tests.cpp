#include "catch.hpp"

#include <dekaf2/kthreadsafe.h>
#include <dekaf2/kstring.h>
#include <map>
#include <array>

using namespace dekaf2;

TEST_CASE("KThreadSafe")
{
	KThreadSafe<KString> String1;
	String1.unique().get() = "content1";
	CHECK ( String1.shared().get() == "content1" );

	KThreadSafe<KString> String2(std::move(String1));

	CHECK ( String1.shared().get() == "" );
	CHECK ( String2.shared().get() == "content1" );

	auto String3 = std::move(String2);

	CHECK ( String2.shared().get() == "" );
	CHECK ( String3.shared().get() == "content1" );

	auto String4 = String3;

	CHECK ( String3.shared().get() == "content1" );
	CHECK ( String4.shared().get() == "content1" );

	KThreadSafe<KString> String5;
	String5 = String4;

	CHECK ( String4.shared().get() == "content1" );
	CHECK ( String5.shared().get() == "content1" );

	KThreadSafe<KString> String6;
	String6 = std::move(String5);

	CHECK ( String5.shared().get() == "" );
	CHECK ( String6.shared().get() == "content1" );

	KThreadSafe<std::map<KString, KString>> Map1;
	Map1.unique()["Key1"] = "Value1";

	CHECK ( Map1.shared()["Key1"] == "Value1" );

	auto map = Map1.unique();
	map["Key1"] = "NewValue1";

	CHECK ( map["Key1"] == "NewValue1" );

	map->erase("Key1");

	CHECK ( map->empty() == true );

	KThreadSafe<std::array<int, 10>> Array1;
	Array1.unique()[1] = 1;

	CHECK ( Array1.shared()[1] == 1 );

	auto Array = Array1.unique();
	Array[1] = 2;

	CHECK ( Array[1] == 2 );
}
