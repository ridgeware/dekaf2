#include "catch.hpp"

#include <dekaf2/core/errors/ksourcelocation.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/format/kformat.h>

using namespace dekaf2;

// MSVC's std::source_location::function_name() carries decorations that GCC/Clang omit:
// a leading "class "/"struct " on the return type and the calling convention ("__cdecl ").
// Strip them so the comparison is the same across compilers (no-op on GCC/Clang).
static KString CleanFunctionName(KString sFunction)
{
	sFunction.Replace("class ",        "");
	sFunction.Replace("struct ",       "");
	sFunction.Replace("enum ",         "");
	sFunction.Replace("__cdecl ",      "");
	sFunction.Replace("__stdcall ",    "");
	sFunction.Replace("__fastcall ",   "");
	sFunction.Replace("__thiscall ",   "");
	sFunction.Replace("__vectorcall ", "");
	return sFunction;
}

KString Logger(KStringView sText, KSourceLocation location = KSourceLocation::current())
{
	return kFormat("{}:{}", CleanFunctionName(location.function_name()), location.line());
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
	return kFormat("{}:{}", CleanFunctionName(sFunction), iLine);
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
		if (TestFunction1(true).starts_with("dekaf2::"))
		{
			CHECK ( TestFunction1(true) == "dekaf2::KString TestFunction1(bool):32" );
			CHECK ( NewObject().TestFunction2(true) == "dekaf2::KString NewObject::TestFunction2(bool):39" );
			CHECK ( TestFunction3(true) == "dekaf2::KString TestFunction3(bool):50" );
			CHECK ( NewObject2().TestFunction4(true) == "dekaf2::KString NewObject2::TestFunction4(bool):57" );
		}
		else
		{
			// apple clang now removes the namespace
			CHECK ( TestFunction1(true) == "KString TestFunction1(bool):32" );
			CHECK ( NewObject().TestFunction2(true) == "KString NewObject::TestFunction2(bool):39" );
			CHECK ( TestFunction3(true) == "KString TestFunction3(bool):50" );
			CHECK ( NewObject2().TestFunction4(true) == "KString NewObject2::TestFunction4(bool):57" );
		}
	}
#else
	SECTION("functions")
	{
#if DEKAF2_HAS_BUILTIN_FUNCTION && DEKAF2_HAS_BUILTIN_LINE
		CHECK ( TestFunction1(true) == "TestFunction1:32" );
		CHECK ( NewObject().TestFunction2(true) == "TestFunction2:39" );
		CHECK ( TestFunction3(true) == "TestFunction3:50" );
		CHECK ( NewObject2().TestFunction4(true) == "TestFunction4:57" );
#else
		CHECK ( TestFunction1(true) == ":0" );
		CHECK ( NewObject().TestFunction2(true) == ":0" );
		CHECK ( TestFunction3(true) == ":0" );
		CHECK ( NewObject2().TestFunction4(true) == ":0" );
#endif
	}
#endif
}
