#include "catch.hpp"

#include <dekaf2/kostringstream.h>

#include <dekaf2/kformat.h>

using namespace dekaf2;

TEST_CASE("KOStringStream")
{

	SECTION("KOStringStream test 1: construction")
	{
		KString str("test.");
		OKStringStream kstringWriter(str);

		KString& retVal = kstringWriter.str();

		CHECK(str.compare(retVal) == 0);

	}

	SECTION("KOStringStream test 2: change string")
	{
		KString str("test. ");
		OKStringStream kstringWriter(str);

		KString addStr("This is my added string.");
		kstringWriter.str(addStr);

		KString& retVal = kstringWriter.str();

		KString testVal("This is my added string.");

		CHECK(testVal == retVal);
		CHECK(testVal.compare(retVal) == 0);
	}
}
