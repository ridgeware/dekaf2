#include "catch.hpp"
#include <dekaf2/kjoin.h>
#include <dekaf2/kprops.h>
#include <dekaf2/kstring.h>
#include <vector>
#include <map>
#include <unordered_map>
#include <list>


using namespace dekaf2;

TEST_CASE("kJoin")
{
	SECTION("empty")
	{
		std::vector<KStringView> vec;

		KString sResult;
		kJoin(sResult, vec);

		CHECK (sResult == "" );
	}

	SECTION("vector")
	{
		std::vector<KStringView> vec {
			"one",
			"two",
			"three",
			"four"
		};

		KString sResult;
		kJoin(sResult, vec);

		CHECK (sResult == "one,two,three,four" );
	}

	SECTION("list")
	{
		std::list<KStringView> list {
			"one",
			"two",
			"three",
			"four"
		};

		KString sResult;
		kJoin(sResult, list);

		CHECK (sResult == "one,two,three,four" );
	}

	SECTION("map")
	{
		std::map<KStringView, KStringView> map {
			{ "1" , "one"   },
			{ "2" , "two"   },
			{ "3" , "three" },
			{ "4" , "four"  }
		};

		KString sResult;
		kJoin(sResult, map);

		CHECK (sResult == "1=one,2=two,3=three,4=four" );
	}

	SECTION("kprops")
	{
		KProps<KStringView, KStringView, true, true> map {
			{ "1" , "one"   },
			{ "2" , "two"   },
			{ "3" , "three" },
			{ "4" , "four"  }
		};

		KString sResult;
		kJoin(sResult, map);

		CHECK (sResult == "1=one,2=two,3=three,4=four" );
	}

}
