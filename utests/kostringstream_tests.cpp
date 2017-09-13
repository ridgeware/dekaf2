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

		KString& retVal = kstringWriter.GetConstructedKString();

		CHECK(str.compare(retVal) == 0);

	}

	SECTION("KOStringStream test 2: add string")
	{
		KString str("test. ");
		OKStringStream kstringWriter(str);

		KString addStr("This is my added string.");
		kstringWriter.addMore(addStr);

		KString& retVal = kstringWriter.GetConstructedKString();

		KString testVal("test. This is my added string.");

		CHECK(testVal == retVal);
		CHECK(testVal.compare(retVal) == 0);
	}

	SECTION("KOStringStream test 3: add format string")
	{
		KString str("test. ");
		OKStringStream kstringWriter(str);

		KString sMy("my");
		KString sFormat("format_string");

		KString formStr("This is {} {}");
		kstringWriter.addFormatted(formStr, sMy, sFormat);
		kFormat(formStr, sMy, sFormat);

		KString& retVal = kstringWriter.GetConstructedKString();

		KString testVal("test. This is my format_string");

		CHECK(testVal.compare(retVal) == 0);
		CHECK(testVal == retVal);
	}

	SECTION("KOStringStream test 4: add out of order format string")
	{
		KString str("test. ");
		OKStringStream kstringWriter(str);

		KString sMy("my");
		KString sFormat("format_string");

		KString formStr("This is {1} {0}");
		kstringWriter.addFormatted(formStr, sFormat, sMy);
		kFormat(formStr, sMy, sFormat);

		KString& retVal = kstringWriter.GetConstructedKString();

		KString testVal("test. This is my format_string");

		CHECK(testVal.compare(retVal) == 0);
		CHECK(testVal == retVal);
	}
}
