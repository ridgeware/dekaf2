#include "catch.hpp"
#include <dekaf2/kjoin.h>
#include <dekaf2/kprops.h>
#include <dekaf2/kstring.h>
#include <dekaf2/koutstringstream.h>
#include <vector>
#include <map>
#include <unordered_map>
#include <list>


using namespace dekaf2;

TEST_CASE("kJoin")
{
	SECTION("empty/string")
	{
		std::vector<KStringView> vec;

		KString sResult;
		kJoin(sResult, vec);

		CHECK (sResult == "" );
	}

	SECTION("vector/string")
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

	SECTION("list/string")
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

	SECTION("map/string")
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

	SECTION("kprops/string")
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

	SECTION("empty/stream")
	{
		std::vector<KStringView> vec;

		KString sResult;
		KOutStringStream oss(sResult);
		kJoin(oss, vec);

		CHECK (sResult == "" );
	}

	SECTION("vector/stream")
	{
		std::vector<KStringView> vec {
			"one",
			"two",
			"three",
			"four"
		};

		KString sResult;
		KOutStringStream oss(sResult);
		kJoin(oss, vec);

		CHECK (sResult == "one,two,three,four" );
	}

	SECTION("list/stream")
	{
		std::list<KStringView> list {
			"one",
			"two",
			"three",
			"four"
		};

		KString sResult;
		KOutStringStream oss(sResult);
		kJoin(oss, list);

		CHECK (sResult == "one,two,three,four" );
	}

	SECTION("map/stream")
	{
		std::map<KStringView, KStringView> map {
			{ "1" , "one"   },
			{ "2" , "two"   },
			{ "3" , "three" },
			{ "4" , "four"  }
		};

		KString sResult;
		KOutStringStream oss(sResult);
		kJoin(oss, map);

		CHECK (sResult == "1=one,2=two,3=three,4=four" );
	}

	SECTION("kprops/stream")
	{
		KProps<KStringView, KStringView, true, true> map {
			{ "1" , "one"   },
			{ "2" , "two"   },
			{ "3" , "three" },
			{ "4" , "four"  }
		};

		KString sResult;
		KOutStringStream oss(sResult);
		kJoin(oss, map);

		CHECK (sResult == "1=one,2=two,3=three,4=four" );
	}

	SECTION("empty/string/kFormat")
	{
		std::vector<uint64_t> vec;

		KString sResult;
		kJoin(sResult, vec);

		CHECK (sResult == "" );
	}

	SECTION("vector/string/kFormat")
	{
		std::vector<uint64_t> vec {
			1,
			2,
			3,
			4
		};

		KString sResult;
		kJoin(sResult, vec);

		CHECK (sResult == "1,2,3,4" );
	}

	SECTION("list/string/kFormat")
	{
		std::list<uint64_t> list {
			1,
			2,
			3,
			4
		};

		KString sResult;
		kJoin(sResult, list);

		CHECK (sResult == "1,2,3,4" );
	}

	SECTION("map/string/kFormat")
	{
		std::map<uint64_t, KStringView> map {
			{ 1 , "one"   },
			{ 2 , "two"   },
			{ 3 , "three" },
			{ 4 , "four"  }
		};

		KString sResult;
		kJoin(sResult, map);

		CHECK (sResult == "1=one,2=two,3=three,4=four" );
	}

	SECTION("map/string/kFormat/2")
	{
		std::map<KStringView, double> map {
			{ "1" , 1        },
			{ "2" , 2.42     },
			{ "3" , 3.141528 },
			{ "4" , 4.0      }
		};

		KString sResult;
		kJoin(sResult, map);

		CHECK (sResult == "1=1,2=2.42,3=3.141528,4=4" );
	}

	SECTION("kprops/string/kFormat")
	{
		KProps<uint64_t, KStringView, true, true> map {
			{ 1 , "one"   },
			{ 2 , "two"   },
			{ 3 , "three" },
			{ 4 , "four"  }
		};

		KString sResult;
		kJoin(sResult, map);

		CHECK (sResult == "1=one,2=two,3=three,4=four" );
	}

	SECTION("empty/stream/kFormat")
	{
		std::vector<uint64_t> vec;

		KString sResult;
		KOutStringStream oss(sResult);
		kJoin(oss, vec);

		CHECK (sResult == "" );
	}

	SECTION("vector/stream/kFormat")
	{
		std::vector<uint64_t> vec {
			1,
			2,
			3,
			4
		};

		KString sResult;
		KOutStringStream oss(sResult);
		kJoin(oss, vec);

		CHECK (sResult == "1,2,3,4" );
	}

	SECTION("list/stream/kFormat")
	{
		std::list<uint64_t> list {
			1,
			2,
			3,
			4
		};

		KString sResult;
		KOutStringStream oss(sResult);
		kJoin(oss, list);

		CHECK (sResult == "1,2,3,4" );
	}

	SECTION("map/stream/kFormat")
	{
		std::map<uint64_t, KStringView> map {
			{ 1 , "one"   },
			{ 2 , "two"   },
			{ 3 , "three" },
			{ 4 , "four"  }
		};

		KString sResult;
		KOutStringStream oss(sResult);
		kJoin(oss, map);

		CHECK (sResult == "1=one,2=two,3=three,4=four" );
	}

	SECTION("map/stream/kFormat/2")
	{
		std::map<KStringView, double> map {
			{ "1" , 1        },
			{ "2" , 2.42     },
			{ "3" , 3.141528 },
			{ "4" , 4.0      }
		};

		KString sResult;
		KOutStringStream oss(sResult);
		kJoin(oss, map);

		CHECK (sResult == "1=1,2=2.42,3=3.141528,4=4" );
	}

	SECTION("kprops/stream/kFormat")
	{
		KProps<uint64_t, KStringView, true, true> map {
			{ 1 , "one"   },
			{ 2 , "two"   },
			{ 3 , "three" },
			{ 4 , "four"  }
		};

		KString sResult;
		KOutStringStream oss(sResult);
		kJoin(oss, map);

		CHECK (sResult == "1=one,2=two,3=three,4=four" );
	}

}

TEST_CASE("kJoined")
{
	SECTION("empty/string")
	{
		std::vector<KStringView> vec;

		KString sResult = kJoined(vec);

		CHECK (sResult == "" );
	}

	SECTION("vector/string")
	{
		std::vector<KStringView> vec {
			"one",
			"two",
			"three",
			"four"
		};

		KString sResult = kJoined(vec);

		CHECK (sResult == "one,two,three,four" );
	}

	SECTION("list/string")
	{
		std::list<KStringView> list {
			"one",
			"two",
			"three",
			"four"
		};

		KString sResult = kJoined(list);

		CHECK (sResult == "one,two,three,four" );
	}

	SECTION("map/string")
	{
		std::map<KStringView, KStringView> map {
			{ "1" , "one"   },
			{ "2" , "two"   },
			{ "3" , "three" },
			{ "4" , "four"  }
		};

		KString sResult = kJoined(map);

		CHECK (sResult == "1=one,2=two,3=three,4=four" );
	}

	SECTION("kprops/string")
	{
		KProps<KStringView, KStringView, true, true> map {
			{ "1" , "one"   },
			{ "2" , "two"   },
			{ "3" , "three" },
			{ "4" , "four"  }
		};

		KString sResult = kJoined(map);

		CHECK (sResult == "1=one,2=two,3=three,4=four" );
	}

	SECTION("empty/string/kFormat")
	{
		std::vector<uint64_t> vec;

		KString sResult = kJoined(vec);

		CHECK (sResult == "" );
	}

	SECTION("vector/string/kFormat")
	{
		std::vector<uint64_t> vec {
			1,
			2,
			3,
			4
		};

		KString sResult = kJoined(vec);

		CHECK (sResult == "1,2,3,4" );
	}

	SECTION("list/string/kFormat")
	{
		std::list<uint64_t> list {
			1,
			2,
			3,
			4
		};

		KString sResult = kJoined(list);

		CHECK (sResult == "1,2,3,4" );
	}

	SECTION("map/string/kFormat")
	{
		std::map<uint64_t, KStringView> map {
			{ 1 , "one"   },
			{ 2 , "two"   },
			{ 3 , "three" },
			{ 4 , "four"  }
		};

		KString sResult = kJoined(map);

		CHECK (sResult == "1=one,2=two,3=three,4=four" );
	}

	SECTION("map/string/kFormat/2")
	{
		std::map<KStringView, double> map {
			{ "1" , 1        },
			{ "2" , 2.42     },
			{ "3" , 3.141528 },
			{ "4" , 4.0      }
		};

		KString sResult = kJoined(map);

		CHECK (sResult == "1=1,2=2.42,3=3.141528,4=4" );
	}

	SECTION("kprops/string/kFormat")
	{
		KProps<uint64_t, KStringView, true, true> map {
			{ 1 , "one"   },
			{ 2 , "two"   },
			{ 3 , "three" },
			{ 4 , "four"  }
		};

		KString sResult = kJoined(map);

		CHECK (sResult == "1=one,2=two,3=three,4=four" );
	}
}
