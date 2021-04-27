#include "catch.hpp"

#include <dekaf2/kthreadsafe.h>
#include <dekaf2/kstring.h>

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

}
