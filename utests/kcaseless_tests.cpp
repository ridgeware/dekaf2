#include "catch.hpp"

#include <dekaf2/kcaseless.h>
#include <dekaf2/kprops.h>
#include <vector>

using namespace dekaf2;

TEST_CASE("KCaseless") {

	SECTION("kCaseBeginsWith")
	{
		struct parms_t
		{
			KStringView left;
			KStringView right;
			bool        result;
		};

		std::vector<parms_t> pvector = {
			{ "aBcDeFgHiJ",         "AbC" , true  },
			{ "aBcDeFgHiJ",         "aBc" , true  },
			{ "aBcDeFgHiJ",         "CBA" , false },
			{ "aBcDeFgHiJ", "aBcDeFgHiJk" , false },
			{ "aBcDeFgHiJ",  "AbCdEfGhIj" , true  },
			{           "",            "" , true  },
			{           "",         "ABC" , false },
			{        "ABC",            "" , true  },
		};

		for (const auto& it : pvector)
		{
			INFO  ( kFormat("'{}' '{}'", it.left, it.right) );
			CHECK ( kCaselessBeginsWith(it.left, it.right) == it.result );
		}
	}

	SECTION("kCaseBeginsWithLeft")
	{
		struct parms_t
		{
			KStringView left;
			KStringView right;
			bool        result;
		};

		std::vector<parms_t> pvector = {
			{ "aBcDeFgHiJ",         "AbC" , false },
			{ "aBcDeFgHiJ",         "abc" , true  },
			{ "aBcDeFgHiJ",         "aBc" , false },
			{ "aBcDeFgHiJ",         "CBA" , false },
			{ "aBcDeFgHiJ", "abcdefghijk" , false },
			{ "aBcDeFgHiJ",  "abcdefghij" , true  },
			{           "",            "" , true  },
			{           "",         "ABC" , false },
			{        "ABC",            "" , true  },
		};

		for (const auto& it : pvector)
		{
			INFO  ( kFormat("'{}' '{}'", it.left, it.right) );
			CHECK ( kCaselessBeginsWithLeft(it.left, it.right) == it.result );
		}
	}

	SECTION("kCaseEndsWith")
	{
		struct parms_t
		{
			KStringView left;
			KStringView right;
			bool        result;
		};

		std::vector<parms_t> pvector = {
			{ "aBcDeFgHiJ",         "hIj" , true  },
			{ "aBcDeFgHiJ",         "HiJ" , true  },
			{ "aBcDeFgHiJ",         "XXX" , false },
			{ "aBcDeFgHiJ", "kaBcDeFgHiJ" , false },
			{ "aBcDeFgHiJ",  "AbCdEfGhIj" , true  },
			{           "",            "" , true  },
			{           "",         "ABC" , false },
			{        "ABC",            "" , true  },
		};

		for (const auto& it : pvector)
		{
			INFO  ( kFormat("'{}' '{}'", it.left, it.right) );
			CHECK ( kCaselessEndsWith(it.left, it.right) == it.result );
		}
	}

	SECTION("kCaseEndsWithLeft")
	{
		struct parms_t
		{
			KStringView left;
			KStringView right;
			bool        result;
		};

		std::vector<parms_t> pvector = {
			{ "aBcDeFgHiJ",         "hij" , true  },
			{ "aBcDeFgHiJ",         "HiJ" , false },
			{ "aBcDeFgHiJ",         "XXX" , false },
			{ "aBcDeFgHiJ", "kabcdefghij" , false },
			{ "aBcDeFgHiJ",  "abcdefghij" , true  },
			{           "",            "" , true  },
			{           "",         "ABC" , false },
			{        "ABC",            "" , true  },
		};

		for (const auto& it : pvector)
		{
			INFO  ( kFormat("'{}' '{}'", it.left, it.right) );
			CHECK ( kCaselessEndsWithLeft(it.left, it.right) == it.result );
		}
	}

	SECTION("kCaseFind")
	{
		CHECK ( kCaselessFind("this is My HaYsTACK", "HAYStack"      ) == 11);
		CHECK ( kCaselessFind("this is My HaYsTACK", "HAYStack",   11) == 11);
		CHECK ( kCaselessFind("this is My HaYsTACK", "HAYStack", npos) == npos);
		CHECK ( kCaselessFind("this is My HaYsTACK", "NonSense"      ) == npos);
	}

	SECTION("kCaseFindLeft")
	{
		CHECK ( kCaselessFindLeft("this is My HaYsTACK", "haystack"      ) == 11);
		CHECK ( kCaselessFindLeft("this is My HaYsTACK", "haystack",   11) == 11);
		CHECK ( kCaselessFindLeft("this is My HaYsTACK", "HaYsTACK"      ) == npos);
		CHECK ( kCaselessFindLeft("this is My HaYsTACK", "haystack", npos) == npos);
		CHECK ( kCaselessFindLeft("this is My HaYsTACK", "nonsense"      ) == npos);
	}

}
