#include "catch.hpp"
#include <dekaf2/koptions.h>
#include <dekaf2/ksystem.h>
#include <dekaf2/kcgistream.h>


using namespace dekaf2;

TEST_CASE("KOptions")
{
	struct Accomplished
	{
		bool bEmpty  { false };
		bool bSingle { false };
		bool bMulti  { false };
	};

	Accomplished a;
	KOptions Options(false);

	Options.RegisterOption("e,empty", [&]()
	{
		a.bEmpty = true;
	});

	Options.RegisterOption("s,single", "missing single argument", [&](KStringViewZ sSingle)
	{
		a.bSingle = true;
	});

	Options.RegisterOption("m,multiple", 2, "missing at least two arguments", [&](KOptions::ArgList& sMultiple)
	{
		a.bMulti = true;
		CHECK( sMultiple.pop() == "first");
		CHECK( sMultiple.pop() == "second");
	});

	SECTION("argc/argv")
	{
		const char* CLI[] {
			"MyProgramName",
			"-empty",
			"-s", "first",
			"-m", "first", "second"
		};

		Options.Parse(7, CLI);

		CHECK( a.bEmpty  == true );
		CHECK( a.bSingle == true );
		CHECK( a.bMulti  == true );
	}

	SECTION("KString")
	{
		KString sCLI {
			"MyProgramName "
			"-empty "
			"-s first "
			"-m first second"
		};

		Options.Parse(sCLI);

		CHECK( a.bEmpty  == true );
		CHECK( a.bSingle == true );
		CHECK( a.bMulti  == true );
	}

	SECTION("CGI")
	{
		KString sCLI {
			"-empty&"
			"-s=first"
		};
		
		kSetEnv(KCGIInStream::QUERY_STRING, sCLI);

		Options.ParseCGI("MyProgramName");

		CHECK( a.bEmpty  == true  );
		CHECK( a.bSingle == true  );
		CHECK( a.bMulti  == false );
	}
}
