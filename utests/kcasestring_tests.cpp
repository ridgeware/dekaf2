#include "catch.hpp"

#include <dekaf2/kcasestring.h>
#include <dekaf2/kprops.h>
#include <vector>

using namespace dekaf2;

TEST_CASE("KCaseString") {

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
	CHECK ( props["contenttype"] == "Value2");
	CHECK ( props["coOkie"] == "Value1");

}
