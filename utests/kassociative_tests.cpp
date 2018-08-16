#include "catch.hpp"

#include <dekaf2/kassociative.h>
#include <dekaf2/kstring.h>
#include <vector>

using namespace dekaf2;

TEST_CASE("KAssociative")
{
	SECTION("KSet")
	{
		KSet<KString> set;
		set.insert("key1");
		set.insert("key1");
		set.insert("key2");
		set.insert("key3");
		set.emplace("key4");
		CHECK ( set.size() == 4 );
		CHECK ( set.find("key1") != set.end() );
		CHECK ( set.find("key2") != set.end() );
		CHECK ( set.find("key3") != set.end() );
		CHECK ( set.find("key4") != set.end() );
		CHECK ( set.find("key2"_ksv) != set.end() );
		CHECK ( set.find("key3"_ksz) != set.end() );
		CHECK ( set.find("key4"_ks ) != set.end() );
	}

	SECTION("KMultiSet")
	{
		KMultiSet<KString> set;
		set.insert("key1");
		set.insert("key1");
		set.insert("key2");
		set.insert("key3");
		set.emplace("key4");
		CHECK ( set.size() == 5 );
		CHECK ( set.find("key1") != set.end() );
		CHECK ( set.find("key2") != set.end() );
		CHECK ( set.find("key3") != set.end() );
		CHECK ( set.find("key4") != set.end() );
		CHECK ( set.find("key2"_ksv) != set.end() );
		CHECK ( set.find("key3"_ksz) != set.end() );
		CHECK ( set.find("key4"_ks ) != set.end() );
	}

	SECTION("KMap")
	{
		KMap<KString, KString> map;
		map.insert({"key1", "value1"});
		map.insert({"key1", "value11"});
		map.insert({"key2", "value2"});
		map.insert({"key3", "value3"});
		map.emplace("key4", "value4");
		CHECK ( map.size() == 4 );
		CHECK ( map.find("key1")->second == "value1" );
		CHECK ( map.find("key2")->second == "value2" );
		CHECK ( map.find("key3")->second == "value3" );
		CHECK ( map.find("key4")->second == "value4" );
		CHECK ( map.find("key2"_ksv)->second == "value2" );
		CHECK ( map.find("key3"_ksz)->second == "value3" );
		CHECK ( map.find("key4"_ks )->second == "value4" );
	}

	SECTION("KMultiMap")
	{
		KMultiMap<KString, KString> map;
		map.insert({"key1", "value1"});
		map.insert({"key1", "value11"});
		map.insert({"key2", "value2"});
		map.insert({"key3", "value3"});
		map.emplace("key4", "value4");
		CHECK ( map.size() == 5 );
		CHECK ( ((map.find("key1")->second == "value1")
			  || (map.find("key1")->second == "value11")) );
		CHECK ( map.find("key2")->second == "value2" );
		CHECK ( map.find("key3")->second == "value3" );
		CHECK ( map.find("key4")->second == "value4" );
		CHECK ( map.find("key2"_ksv)->second == "value2" );
		CHECK ( map.find("key3"_ksz)->second == "value3" );
		CHECK ( map.find("key4"_ks )->second == "value4" );
	}

	SECTION("KUnorderedSet")
	{
		KUnorderedSet<KString> set;
		set.insert("key1");
		set.insert("key1");
		set.insert("key2");
		set.insert("key3");
		set.emplace("key4");
		CHECK ( set.size() == 4 );
		CHECK ( set.find("key1") != set.end() );
		CHECK ( set.find("key2") != set.end() );
		CHECK ( set.find("key3") != set.end() );
		CHECK ( set.find("key4") != set.end() );
		CHECK ( set.find("key2"_ksv) != set.end() );
		CHECK ( set.find("key3"_ksz) != set.end() );
		CHECK ( set.find("key4"_ks ) != set.end() );
	}

	SECTION("KUnorderedMultiSet")
	{
		KUnorderedMultiSet<KString> set;
		set.insert("key1");
		set.insert("key1");
		set.insert("key2");
		set.insert("key3");
		set.emplace("key4");
		CHECK ( set.size() == 5 );
		CHECK ( set.find("key1") != set.end() );
		CHECK ( set.find("key2") != set.end() );
		CHECK ( set.find("key3") != set.end() );
		CHECK ( set.find("key4") != set.end() );
		CHECK ( set.find("key2"_ksv) != set.end() );
		CHECK ( set.find("key3"_ksz) != set.end() );
		CHECK ( set.find("key4"_ks ) != set.end() );
	}

	SECTION("KUnorderedMap")
	{
		KUnorderedMap<KString, KString> map;
		map.insert({"key1", "value1"});
		map.insert({"key1", "value11"});
		map.insert({"key2", "value2"});
		map.insert({"key3", "value3"});
		map.emplace("key4", "value4");
		CHECK ( map.size() == 4 );
		CHECK ( map.find("key1")->second == "value1" );
		CHECK ( map.find("key2")->second == "value2" );
		CHECK ( map.find("key3")->second == "value3" );
		CHECK ( map.find("key4")->second == "value4" );
		CHECK ( map.find("key2"_ksv)->second == "value2" );
		CHECK ( map.find("key3"_ksz)->second == "value3" );
		CHECK ( map.find("key4"_ks )->second == "value4" );
	}

	SECTION("KUnorderedMultiMap")
	{
		KUnorderedMultiMap<KString, KString> map;
		map.insert({"key1", "value1"});
		map.insert({"key1", "value11"});
		map.insert({"key2", "value2"});
		map.insert({"key3", "value3"});
		map.emplace("key4", "value4");
		CHECK ( map.size() == 5 );
		CHECK ( ((map.find("key1")->second == "value1")
				 || (map.find("key1")->second == "value11")) );
		CHECK ( map.find("key2")->second == "value2" );
		CHECK ( map.find("key3")->second == "value3" );
		CHECK ( map.find("key4")->second == "value4" );
		CHECK ( map.find("key2"_ksv)->second == "value2" );
		CHECK ( map.find("key3"_ksz)->second == "value3" );
		CHECK ( map.find("key4"_ks )->second == "value4" );
	}

}
