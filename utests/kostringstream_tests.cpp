#include "catch.hpp"

#include <dekaf2/kostringstream.h>

using namespace dekaf2;

TEST_CASE("KOStringStream")
{

	SECTION("KOStringStream test 1")
	{
		KString str("test.");
		OKStringStream kstringWriter(str);

		KString& retVal = kstringWriter.GetConstructedKString();

		CHECK(str.compare(retVal) == 0);

	}

	SECTION("KOStringStream test 2")
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

#if 0
	SECTION("KOStringStream test 3")
	{
		KString str("test.");
		OKStringStream kstringWriter(str);

		//KString sRead;
		KString sMy("my");
		KString sFormat("format_string");

		kstringWriter.addFormatted("This is {} {}", sMy, sFormat);

		KString& retVal = kstringWriter.GetConstructedKString();

		KString testVal("test.This is my format_string");

		CHECK(testVal.compare(retVal) == 0);

		/*
		CHECK( File.eof() == false);
		CHECK( File.ReadRemaining(sRead) == true );
		CHECK( sRead == sOut );
		CHECK( File.eof() == true);
		File.close();
		CHECK( File.is_open() == false );
		*/
	}
#endif
}
