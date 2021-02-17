#include "catch.hpp"

#include <dekaf2/kcasestring.h>
#include <dekaf2/kprops.h>
#include <vector>

using namespace dekaf2;

TEST_CASE("KCaseString") {

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
			CHECK ( kCaseBeginsWith(it.left, it.right) == it.result );
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
			CHECK ( kCaseBeginsWithLeft(it.left, it.right) == it.result );
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
			CHECK ( kCaseEndsWith(it.left, it.right) == it.result );
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
			CHECK ( kCaseEndsWithLeft(it.left, it.right) == it.result );
		}
	}

	SECTION("const char compares")
	{
		KCaseStringView csv;
		KStringView sv;

		csv = "aBcDeFgH";
		sv  = "abcdefgh";
		CHECK ( csv == "abcdefgh" );
		CHECK ( csv == sv );
		CHECK ( "abcdefgh" == csv );
		CHECK ( sv == csv );

		csv = "aBcDeFgH";
		sv  = "abcdexgh";
		CHECK ( csv != "abcdexgh" );
		CHECK ( csv != sv );
		CHECK ( "abcdexgh" != csv);
		CHECK ( sv != csv );

		csv = "  \t\r aBcDeFgH\n ";
		sv  = "abcdefgh";
		CHECK ( csv != "abcdefgh" );
		CHECK ( csv != sv );
		CHECK ( "abcdefgh" != csv );
		CHECK ( sv != csv );
	}

}

TEST_CASE("KCaseTrimString") {

	SECTION("const char compares")
	{
		KCaseTrimStringView csv;
		KStringView sv;

		csv = "aBcDeFgH";
		sv  = "abcdefgh";
		CHECK ( csv == "abcdefgh" );
		CHECK ( csv == sv );
		CHECK ( "abcdefgh" == csv );
		CHECK ( sv == csv );

		csv = "aBcDeFgH";
		sv  = "abcdexgh";
		CHECK ( csv != "abcdexgh" );
		CHECK ( csv != sv );
		CHECK ( "abcdexgh" != csv);
		CHECK ( sv != csv );

		csv = "  \t\r aBcDeFgH\n ";
		sv  = "abcdefgh";
		CHECK ( csv == "abcdefgh" );
		CHECK ( csv == sv );
		CHECK ( "abcdefgh" == csv );
		CHECK ( sv == csv );

		csv = "  \t\r aBcDeFgH\n ";
		sv  = "abcdexgh";
		CHECK ( csv != "abcdexgh" );
		CHECK ( csv != sv );
		CHECK ( "abcdexgh" != csv);
		CHECK ( sv != csv );
	}

	KProps<KCaseTrimString, KString> props;

	props.Add("    CoOkie ", "Value1");
	props.Add("ContentType   ", "Value2");

	CHECK ( props["cookie"] == "Value1");
	CHECK ( props["CoOkie"] == "Value1");
	CHECK ( props["    CoOkie "] == "Value1");
	CHECK ( props["    cookie"] == "Value1");
	CHECK ( props["    cookie "] == "Value1");
	CHECK ( props[" \r\nCoOkie "] == "Value1");
	CHECK ( props[" cookie"] == "Value1");
	CHECK ( props[" cookie\n"] == "Value1");
	CHECK ( props["contenttype"] == "Value2");
	CHECK ( props["coOkie"] == "Value1");

}
