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
		bool bEmpty    { false };
		bool bTest     { false };
		bool bEmpty2   { false };
		bool bSingle   { false };
		bool bMulti    { false };
		bool bStop     { false };
		int  iInteger1 {     0 };
		int  iInteger2 {     0 };
		KStringViewZ sDatabase;
		KStringViewZ sSingleArg;
		KString      sJoinedArgs;
	};

	Accomplished a;
	KOptions Options(false);

	// deprecated
	Options.RegisterOption("e,empty", [&]()
	{
		a.bEmpty = true;
	});

	Options.Option("t,test")
	([&]()
	{
		a.bTest = true;
	});

	// deprecated
	Options.RegisterOption("empty2", [&]()
	{
		a.bEmpty2 = true;
	});

	// deprecated
	Options.RegisterOption("s,single <arg>", "missing single argument", [&](KStringViewZ sSingle)
	{
		a.bSingle = true;
		a.sSingleArg = sSingle;
	});

	// deprecated
	Options.RegisterOption("m,multiple", 2, "missing at least two arguments", [&](KOptions::ArgList& sMultiple)
	{
		a.bMulti = true;
		CHECK( sMultiple.pop() == "first");
		CHECK( sMultiple.pop() == "second");
	});

	// deprecated
	Options.RegisterUnknownOption([&](KOptions::ArgList& Args)
	{
		a.sJoinedArgs = kJoined(Args);
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

	Options
		.Option("b")
		.Help("stop execution")
		.Set(a.bStop, "true");

	Options
		.Option("i,Integral")
		.Help("fill an integer from a string")
		.Set(a.iInteger1);

	Options
		.Option("I")
		.Help("fill an integer from a string with a default")
		.Set(a.iInteger2, "123");

	struct Something
	{
		Something() = default;
		Something(KStringView s) 
		{
			auto p = s.Split();
			if (p.size() == 2)
			{
				x = p[0].Int32();
				y = p[1].Int32();
			}
		}

		int x {};
		int y {};
	};

	Something something;

	Options
		.Option("Something")
		.Help("fill something from a string")
		.Set(something);

	Options.Command("run").Help("start running")([](){});

	SECTION("combined short args")
	{
		const char* CLI[] {
			"MyProgramName",
			"-ebts", "first",
			"-m", "first", "second",
			"-Ii", "952",
			"-clear", "database1",
			"-Something", "555,333",
			"-unknown", "arg1", "arg2"
		};

		Options.Parse(sizeof(CLI)/sizeof(char*), CLI);

		CHECK( a.bEmpty    == true );
		CHECK( a.bTest     == true );
		CHECK( a.bSingle   == true );
		CHECK( a.bMulti    == true );
		CHECK( a.bStop     == true );
		CHECK( a.iInteger1 ==  952 );
		CHECK( a.iInteger2 ==  123 );
		CHECK( a.sSingleArg == "first" );
		CHECK( a.sJoinedArgs == "arg2,arg1,unknown" );
		CHECK( something.x ==  555 );
		CHECK( something.y ==  333 );
		CHECK( a.sDatabase == "DATABASE1" );
	}

	SECTION("long args with =")
	{
		const char* CLI[] {
			"MyProgramName",
			"--clear=database1",
			"--Integral=987",
			"-unknown", "arg1", "arg2"
		};

		Options.Parse(sizeof(CLI)/sizeof(char*), CLI);

		CHECK( a.bEmpty    == false );
		CHECK( a.bTest     == false );
		CHECK( a.bSingle   == false );
		CHECK( a.bMulti    == false );
		CHECK( a.bStop     == false );
		CHECK( a.iInteger1 ==   987 );
		CHECK( a.iInteger2 ==     0 );
		CHECK( a.sJoinedArgs == "arg2,arg1,unknown" );
		CHECK( a.sDatabase == "DATABASE1" );
	}

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

		CHECK( a.bEmpty    == true  );
		CHECK( a.bTest     == false );
		CHECK( a.bSingle   == true  );
		CHECK( a.bMulti    == true  );
		CHECK( a.bStop     == false );
		CHECK( a.iInteger1 ==     0 );
		CHECK( a.iInteger2 ==     0 );
		CHECK( a.sSingleArg == "first" );
		CHECK( a.sJoinedArgs == "arg2,arg1,unknown" );
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

		CHECK( a.bEmpty    == true  );
		CHECK( a.bTest     == false );
		CHECK( a.bSingle   == true  );
		CHECK( a.bMulti    == true  );
		CHECK( a.bStop     == false );
		CHECK( a.iInteger1 ==     0 );
		CHECK( a.iInteger2 ==     0 );
		CHECK( a.sSingleArg == "first" );
	}

	SECTION("CGI")
	{
		KString sCLI {
			"-empty&"
			"-s=first"
		};
		
		kSetEnv(KCGIInStream::QUERY_STRING, sCLI);

		Options.ParseCGI("MyProgramName");

		CHECK( a.bEmpty    == true  );
		CHECK( a.bTest     == false );
		CHECK( a.bSingle   == true  );
		CHECK( a.bMulti    == false );
		CHECK( a.bStop     == false );
		CHECK( a.iInteger1 ==     0 );
		CHECK( a.iInteger2 ==     0 );
		CHECK( a.sSingleArg == "first" );
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

		CHECK( a.bEmpty    == true  );
		CHECK( a.bTest     == false );
		CHECK( a.bEmpty2   == true  );
		CHECK( a.bSingle   == true  );
		CHECK( a.bMulti    == true  );
		CHECK( a.bStop     == false );
		CHECK( a.iInteger1 ==     0 );
		CHECK( a.iInteger2 ==     0 );
		CHECK( a.sSingleArg == "first" );
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

		CHECK( a.bEmpty    == true  );
		CHECK( a.bTest     == false );
		CHECK( a.bEmpty2   == true  );
		CHECK( a.bSingle   == true  );
		CHECK( a.bMulti    == true  );
		CHECK( a.bStop     == false );
		CHECK( a.iInteger1 ==     0 );
		CHECK( a.iInteger2 ==     0 );
		CHECK( a.sSingleArg == "first" );
	}
/*
	SECTION("Help")
	{
		Options.SetAdditionalHelp("This:\n    indented\n\nhere\n    and more");
		Options.Parse("MyProg --help");
	}
 */
}
