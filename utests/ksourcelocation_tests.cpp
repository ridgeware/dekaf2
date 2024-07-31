#include "catch.hpp"

#include <dekaf2/ksourcelocation.h>
#include <dekaf2/kstring.h>
#include <dekaf2/kformat.h>

using namespace dekaf2;

KString Logger(KStringView sText, KSourceLocation location = KSourceLocation::current())
{
	return kFormat("{}:{}", location.function_name(), location.line());
}

KString TestFunction1(bool Callme)
{
	return Logger("I am a TestFunction1");
}

struct NewObject
{
	KString TestFunction2(bool Callme)
	{
		return Logger("I am a TestFunction1");
	}
};

KString Logger2(KStringView sText, KStringView sFunction = KSourceLocation::current().function_name(), uint32_t iLine = KSourceLocation::current().line())
{
	return kFormat("{}:{}", sFunction, iLine);
}

KString TestFunction3(bool Callme)
{
	return Logger2("I am a TestFunction1");
}

struct NewObject2
{
	KString TestFunction4(bool Callme)
	{
		return Logger2("I am a TestFunction1");
	}
};

TEST_CASE("KSourceLocation") {

#if DEKAF2_HAS_STD_SOURCE_LOCATION
	SECTION("functions")
	{
		CHECK ( TestFunction1(true) == "dekaf2::KString TestFunction1(bool):16" );
		CHECK ( NewObject().TestFunction2(true) == "dekaf2::KString NewObject::TestFunction2(bool):23" );
		CHECK ( TestFunction3(true) == "dekaf2::KString TestFunction3(bool):34" );
		CHECK ( NewObject2().TestFunction4(true) == "dekaf2::KString NewObject2::TestFunction4(bool):41" );
	}
#else
	SECTION("functions")
	{
#if DEKAF2_HAS_BUILTIN_FUNCTION && DEKAF2_HAS_BUILTIN_LINE
		CHECK ( TestFunction1(true) == "TestFunction1:16" );
		CHECK ( NewObject().TestFunction2(true) == "TestFunction2:23" );
		CHECK ( TestFunction3(true) == "TestFunction3:34" );
		CHECK ( NewObject2().TestFunction4(true) == "TestFunction4:41" );
#else
		CHECK ( TestFunction1(true) == "" );
		CHECK ( NewObject().TestFunction2(true) == "" );
		CHECK ( TestFunction3(true) == "" );
		CHECK ( NewObject2().TestFunction4(true) == "" );
#endif
	}
#endif
}
