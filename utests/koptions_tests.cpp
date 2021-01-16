#include "catch.hpp"
#include <dekaf2/koptions.h>
#include <dekaf2/ksystem.h>
#include <dekaf2/kfilesystem.h>
#include <dekaf2/kcgistream.h>


using namespace dekaf2;

TEST_CASE("KOptions")
{
	struct Accomplished
	{
		bool bEmpty  { false };
		bool bEmpty2 { false };
		bool bSingle { false };
		bool bMulti  { false };
		KStringViewZ sDatabase;
	};

	Accomplished a;
	KOptions Options(false);
	KString sJoinedArgs;

	Options.RegisterOption("e,empty", [&]()
	{
		a.bEmpty = true;
	});

	Options.RegisterOption("empty2", [&]()
	{
		a.bEmpty2 = true;
	});

	Options.RegisterOption("s,single <arg>", "missing single argument", [&](KStringViewZ sSingle)
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

	Options
		.Option("c,clear", "database name")
		.Help("clear the entire database")
		.Type(KOptions::String)
		.ToUpper()
		.Range(1, 30)
		.MinArgs(1)
		.Callback([&](KStringViewZ sDatabase)
		{
			a.sDatabase = sDatabase;
		});

	Options.Command("run").Help("start running")([](){});

	SECTION("argc/argv")
	{
		const char* CLI[] {
			"MyProgramName",
			"-empty",
			"-single", "first",
			"-m", "first", "second",
			"-clear", "database1",
			"-unknown", "arg1", "arg2"
		};

		Options.Parse(sizeof(CLI)/sizeof(char*), CLI);

		CHECK( a.bEmpty  == true );
		CHECK( a.bSingle == true );
		CHECK( a.bMulti  == true );
		CHECK( sJoinedArgs == "arg2,arg1,unknown" );
		CHECK( a.sDatabase == "DATABASE1" );
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

	SECTION("IniFile")
	{
		KTempDir Temp;
		CHECK ( Temp == true );

		auto sIniFile = kFormat("{}/xyz.ini", Temp.Name());
		{
			KOutFile Out(sIniFile);
			CHECK ( Out.is_open() == true );

			Out.WriteLine("-empty2");
			Out.WriteLine("-single first");
			Out.WriteLine("-m first second");
		}

		const char* CLI[] {
			"MyProgramName",
			"-ini", sIniFile.c_str(),
			"-empty"
		};

		Options.Parse(sizeof(CLI)/sizeof(char*), CLI);

		CHECK( a.bEmpty  == true );
		CHECK( a.bEmpty2 == true );
		CHECK( a.bSingle == true );
		CHECK( a.bMulti  == true );
	}

	SECTION("IniFile reverse")
	{
		KTempDir Temp;
		CHECK ( Temp == true );

		auto sIniFile = kFormat("{}/xyz.ini", Temp.Name());
		{
			KOutFile Out(sIniFile);
			CHECK ( Out.is_open() == true );

			Out.WriteLine("-empty2");
			Out.WriteLine("-s first");
			Out.WriteLine("-m first second");
		}

		const char* CLI[] {
			"MyProgramName",
			"-empty",
			"-ini", sIniFile.c_str()
		};

		Options.Parse(sizeof(CLI)/sizeof(char*), CLI);

		CHECK( a.bEmpty  == true );
		CHECK( a.bEmpty2 == true );
		CHECK( a.bSingle == true );
		CHECK( a.bMulti  == true );
	}
/*
	SECTION("Help")
	{
		Options.Parse("MyProg --help");
	}
 */
}
