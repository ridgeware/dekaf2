#include "catch.hpp"

#include <dekaf2/threading/primitives/kthreadsafe.h>
#include <dekaf2/core/strings/kstring.h>
#include <map>
#include <array>
#include <thread>

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

	KThreadSafe<KString> String7("content7");
	String7.reset("content8");

	CHECK ( String7.shared().get() == "content8" );

	String7.reset();

	CHECK ( String7.shared().get() == "" );

	// The self assignments go through a reference, because the compiler
	// warns about a self assignment that names the same variable twice.
	auto& String7Ref = String7;
	String7.reset("content9");
	String7 = String7Ref;

	CHECK ( String7.shared().get() == "content9" );

	String7 = std::move(String7Ref);

	CHECK ( String7.shared().get() == "content9" );

	KThreadSafe<KString> String8("content8");
	KThreadSafe<KString> String9("content9");

	// Two threads assign in opposite directions. Both have to return.
	std::thread Thread1([&]()
	{
		for (int iRound = 0; iRound < 10000; ++iRound)
		{
			String8 = String9;
		}
	});

	std::thread Thread2([&]()
	{
		for (int iRound = 0; iRound < 10000; ++iRound)
		{
			String9 = String8;
		}
	});

	Thread1.join();
	Thread2.join();

	auto sString8 = String8.shared().get();
	auto sString9 = String9.shared().get();

	CHECK ( (sString8 == "content8" || sString8 == "content9") );
	CHECK ( (sString9 == "content8" || sString9 == "content9") );
}
