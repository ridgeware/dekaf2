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
	KString sJoinedArgs;

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

	Options.RegisterUnknownOption([&](KOptions::ArgList& Args)
	{
		sJoinedArgs	= kJoined(Args);
		Args.clear();
	});

	SECTION("argc/argv")
	{
		const char* CLI[] {
			"MyProgramName",
			"-empty",
			"-s", "first",
			"-m", "first", "second",
			"-unknown", "arg1", "arg2"
		};

		Options.Parse(sizeof(CLI)/sizeof(char*), CLI);

		CHECK( a.bEmpty  == true );
		CHECK( a.bSingle == true );
		CHECK( a.bMulti  == true );
		CHECK( sJoinedArgs == "arg2,arg1,unknown" );
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
