#include "catch.hpp"
#include <dekaf2/util/cli/koptions.h>
#include <dekaf2/system/os/ksystem.h>
#include <dekaf2/system/filesystem/kfilesystem.h>
#include <dekaf2/http/server/kcgistream.h>
#include <dekaf2/io/streams/kstringstream.h>


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
	std::vector<KString> ArgVec;
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

	Options
		.Option("f,file")
		.Help("a file name");

	Options
		.Option("r,real")
		.Help("a floating point number");

	Options
		.Option("bool")
		.Help("a boolean");

	Options
		.Option("bool2")
		.Help("a boolean");

	Options
		.Option("bool3")
		.Help("a boolean");

	Options
		.Option("neg")
		.Help("a negative number");

	Options
		.Option("all")
		.Help("all you can eat")
		.AllArgs()
		.Callback([&](KOptions::ArgList& Args)
		{
			ArgVec.clear();
			while (!Args.empty())
			{
				ArgVec.push_back(Args.pop());
			}
		});


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

	SECTION("varargs")
	{
		const char* CLI[] {
			"MyProgramName",
			"-ebts", "first",
			"-all", "first", "second",
			"-Ii", "952",
			"--test", "done"
		};

		Options.AllowAdHocArgs();
		int iResult = Options.Parse(sizeof(CLI)/sizeof(char*), CLI);
		CHECK ( iResult == 0 );
		CHECK( a.bEmpty    == true );
		CHECK( a.bTest     == true );
		CHECK( a.bSingle   == true );
		CHECK( a.sSingleArg == "first" );
		CHECK( kJoined(ArgVec) == "first,second,-Ii,952,--test,done" );
	}

	SECTION("multiple ad-hoc values without declaring .AllowAdHocArgs()")
	{
		const char* CLI[] {
			"MyProgramName",
			"-unknown", "arg1", "arg2"
		};

		KOptions Opt(false);
		Opt.Parse(sizeof(CLI)/sizeof(char*), CLI);
		KStringView sValue = Opt("unknown", "a default value");
		CHECK ( sValue == "arg1" );
	}

	SECTION("combined short args")
	{
		const char* CLI[] {
			"MyProgramName",
			"-ebts", "first",
			"-m", "first", "second",
			"-Ii", "952",
			"-neg", "-1234567",
			"-bool",
//			"-bool2=false",
//			"-bool3=true",
			"-file", "filename.txt",
			"-real", "123.456",
			"-clear", "database1",
			"-Something", "555,333",
			"-adhoc1", "hello",
			"-unknown", "arg1", "arg2"
		};

		Options.AllowAdHocArgs();
		int iResult = Options.Parse(sizeof(CLI)/sizeof(char*), CLI);
		CHECK ( iResult == 0 );

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

		CHECK( Options("file").String() == "filename.txt" );
		CHECK( Options("f").String()    == "filename.txt" );
		CHECK( Options("i").UInt64()    == 952 );
		CHECK( Options("bool" ).Bool()  == true  );
//		CHECK( Options("bool2").Bool()  == false );
//		CHECK( Options("bool3").Bool()  == true  );
		CHECK( Options("real" ).Double()== 123.456 );
		CHECK((Options("m").Vector()    == std::vector<KStringViewZ>{ "first", "second" }));

		KString sFile = Options("file");
		CHECK ( sFile == "filename.txt" );
		std::string sFile2 = Options("file");
		CHECK ( sFile2 == "filename.txt" );
#ifdef DEKAF2_HAS_STD_STRING_VIEW
		std::string_view sFile3 = Options("file");
		CHECK ( sFile3 == "filename.txt" );
#endif
		uint64_t iUInt = Options("i");
		CHECK ( iUInt == 952 );
		int64_t iInt = Options("neg");
		CHECK ( iInt == -1234567 );
	}

	SECTION("long args with =")
	{
		const char* CLI[] {
			"MyProgramName",
			"--clear=database1",
			"--Integral=987",
			"-unknown", "arg1", "arg2"
		};

		int iResult = Options.Parse(sizeof(CLI)/sizeof(char*), CLI);
		CHECK ( iResult == 0 );

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

		int iResult = Options.Parse(sizeof(CLI)/sizeof(char*), CLI);
		CHECK ( iResult == 0 );

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

		int iResult = Options.Parse(sCLI);
		CHECK ( iResult == 0 );

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

		int iResult = Options.ParseCGI("MyProgramName");
		CHECK ( iResult == 0 );

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

		int iResult = Options.Parse(sizeof(CLI)/sizeof(char*), CLI);
		CHECK ( iResult == 0 );

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

		int iResult = Options.Parse(sizeof(CLI)/sizeof(char*), CLI);
		CHECK ( iResult == 0 );

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
}

TEST_CASE("KOptions2")
{
	SECTION("flat")
	{
		const char* CLI[] {
			"MyProgramName",
			"-f", "test.txt",
			"-search", "searchme",
			"-v"
		};

		KOptions Options(true, sizeof(CLI)/sizeof(char*), CLI);

		KString sFilename = Options("f,filename <filename>: file to search in");
		KString sSearch   = Options("s,search <search string>: string to search for");
		bool    bVersion  = Options("v,version: show version information", false);

		CHECK ( sFilename == "test.txt" );
		CHECK ( sSearch   == "searchme" );
		CHECK ( bVersion  == true       );

		CHECK ( Options.Check() );
	}

	SECTION("fail check on missing required arguments")
	{
		const char* CLI[] {
			"MyProgramName", 
			"-v"
		};

		KOptions Options(true, sizeof(CLI)/sizeof(char*), CLI);

		KString sFilename = Options("f,filename <filename>: file to search in");
		KString sSearch   = Options("s,search <search string>: string to search for");
		bool    bVersion  = Options("v,version: show version information", false);

		KString sOut;
		KOutStringStream oss(sOut);

		// Check() returns false for no options or help output
		CHECK ( Options.Check(oss) == false );
	}


	SECTION("flat help")
	{
		const char* CLI[] {
			"MyProgramName"
		};

		KOptions Options(true, sizeof(CLI)/sizeof(char*), CLI);

		KString sFilename = Options("f,filename <filename>: file to search in");
		KString sSearch   = Options("s,search <search string>: string to search for");
		bool    bVersion  = Options("v,version: show version information", false);

		KString sOut;
		KOutStringStream oss(sOut);

		// Check() returns false for no options or help output
		CHECK ( Options.Check(oss) == false );
//		KOut.Write(sOut);
		CHECK ( sOut.find("-f,filename <filename>   "          ) != npos );
		CHECK ( sOut.find("-s,search <search string> "         ) != npos );
		CHECK ( sOut.find("-v,version     "         ) != npos );
		CHECK ( sOut.find("file to search in"       ) != npos );
		CHECK ( sOut.find("string to search for"    ) != npos );
		CHECK ( sOut.find("show version information") != npos );
	}

	SECTION("adhoc args call operator")
	{
		const char* CLI[] {
			"MyProgramName",
			"-ui", "952",
			"-neg", "-1234567",
			"-bool",
//			"-bool2=false",
//			"-bool3=true",
			"-file", "filename.txt",
			"-real", "123.456",
			"-adhoc1", "hello",
			"-unknown", "arg1", "arg2"
		};

		KOptions Opt(false, sizeof(CLI)/sizeof(char*), CLI);

		KString sFile = Opt("file");
		CHECK ( sFile == "filename.txt" );
		uint64_t iUInt = Opt("ui");
		CHECK ( iUInt == 952 );
		int64_t iInt = Opt("neg");
		CHECK ( iInt == -1234567 );
		int iInt2 = Opt("neg");
		CHECK ( iInt2 == -1234567 );
		KStringViewZ sAdHoc = Opt("adhoc1");
		CHECK ( sAdHoc == "hello" );
		KStringView sDefault = Opt("unknown", "a default value");
		CHECK ( sDefault == "arg1" );
		KStringView sDefault2 = Opt("reallyunknown", {"a vector", "of values"});
		CHECK ( sDefault2 == "a vector" );
		KStringView sDefault22 = Opt("reallyunknown")[1];
		CHECK ( sDefault22 == "of values" );
		int i = 0;
		for (auto sVal : Opt("reallyunknown"))
		{
			if (i == 0) CHECK (sVal == "a vector" );
			else if (i == 1) CHECK (sVal == "of values" );
			else CHECK ( false );
			++i;
		}
		CHECK ( i == 2 );
		bool bBool = Opt("bool");
		CHECK ( bBool == true );
//		bool bBool2 = Opt("bool2");
//		CHECK ( bBool2 == false );
//		bool bBool3 = Opt("bool3");
//		CHECK ( bBool3 == true );
		double real = Opt("real");
		CHECK ( real == 123.456 );
		iUInt = Opt("notfound", "234");
		CHECK ( iUInt == 234 );
		const char* p = Opt("notfound2", "hello world").c_str();
		CHECK ( KStringViewZ(p) == "hello world" );
		uint32_t ui32 = Opt("notfound3", 123);
		CHECK ( ui32 == 123 );
		bool bb = Opt("notfound4", true);
		CHECK ( bb == true );
		std::size_t iSize = Opt("notfound5", 1234567);
		CHECK ( iSize == 1234567);
//		iUInt = Opt("notfound2");
		auto UnknownArgs = Opt.GetUnknownCommands();
		CHECK ( UnknownArgs.size() == 1 );
		CHECK ( UnknownArgs.front() == "arg2" );
		CHECK ( Opt.Check() == true );
	}

	SECTION("adhoc args named type")
	{
		const char* CLI[] {
			"MyProgramName",
			"-ui", "952",
			"-neg", "-1234567",
			"-bool",
//			"-bool2=false",
//			"-bool3=true",
			"-file", "filename.txt",
			"-real", "123.456", "234.567",
			"-adhoc1", "hello",
			"-unknown", "arg1", "arg2"
		};

		KOptions Opt(false, sizeof(CLI)/sizeof(char*), CLI);

		auto sFile = Opt("f,file <filename>: a file to read", "xxx").String();
		CHECK ( sFile == "filename.txt" );
		auto iUInt = Opt("ui").UInt32();
		CHECK ( iUInt == 952 );
		auto iInt = Opt("neg").Int32();
		CHECK ( iInt == -1234567 );
		auto sAdHoc = Opt("adhoc1").String();
		CHECK ( sAdHoc == "hello" );
		auto sDefault = Opt("unknown", "a default value").String();
		CHECK ( sDefault == "arg1" );
		auto sDefault2 = Opt("reallyunknown", "a default value").String();
		CHECK ( sDefault2 == "a default value" );
		auto bBool = Opt("bool").Bool();
		CHECK ( bBool == true );
//		auto bBool2 = Opt("bool2").Bool();
//		CHECK ( bBool2 == false );
//		auto bBool3 = Opt("bool3").Bool();
//		CHECK ( bBool3 == true );
		auto real = Opt("real").Double();
		CHECK ( real == 123.456 );
		iUInt = Opt("notfound", "234").UInt32();
		CHECK ( iUInt == 234 );
		auto values = Opt("unknown");
		CHECK ( values.size() == 2 );
		CHECK ( values[0] == "arg1" );
		CHECK ( values[1] == "arg2" );
		CHECK ( *values.begin() == "arg1" );
		CHECK_THROWS ( values[2] == "arg2" );
		CHECK ( Opt("real")[1].Double() == 234.567 );
		auto UnknownArgs = Opt.GetUnknownCommands();
		CHECK ( UnknownArgs.size() == 2 );
		CHECK ( UnknownArgs[0] == "arg2" );
		CHECK ( UnknownArgs[1] == "234.567" );
		CHECK ( Opt.Check() == true );
	}

	SECTION("ad hoc with help")
	{
		const char* CLI[] {
			"MyProgramName",
			"-help"
		};

		KOptions Opt(false, sizeof(CLI)/sizeof(char*), CLI);

		auto sFile = Opt("f,file <filename>: a file to read").String();
		CHECK ( sFile == "" );
		KString sOut;
		KOutStringStream oss(sOut);
		CHECK ( Opt.Check(oss) == false );
		CHECK ( sOut.find("-f,file <filename>") != npos );
		CHECK ( sOut.find("a file to read")     != npos );
	}

	SECTION("ad hoc with larger help and no input parms")
	{
		const char* CLI[] {
			"MyProgramName"
		};

		KOptions Opt(true, sizeof(CLI)/sizeof(char*), CLI);

		KString sFile = Opt("file,f <filename>");
		uint64_t iUInt = Opt("ui");
		if (iUInt) {}
		int64_t iInt = Opt("neg");
		int iInt2 = Opt("neg");
		KStringViewZ sAdHoc = Opt("adhoc1");
		KStringView sDefault = Opt("unknown", "a default value");
		KStringView sDefault2 = Opt("reallyunknown", {"a vector", "of values"});
		bool bBool = Opt("bool");
		bool bBool2 = Opt("bool2");
		bool bBool3 = Opt("bool3");
		double real = Opt("real");
		iUInt = Opt("notfound", "234");
		const char* p = Opt("notfound2", "hello world").c_str();
		uint32_t ui32 = Opt("notfound3", 123);
		bool bb = Opt("notfound4", true);

		if (sAdHoc   .empty()) { /* gcc 13 wants this */ }
		if (sDefault .empty()) { /* gcc 13 wants this */ }
		if (sDefault2.empty()) { /* gcc 13 wants this */ }

		KString sOut;
		KOutStringStream oss(sOut);

		CHECK ( Opt.Check(oss) == false );
		CHECK ( sOut.find("-file,f <filename>") != npos );
		CHECK ( sOut.find("-notfound4")         != npos );
//		KOut.Write(sOut);
	}
}

TEST_CASE("KOptions control flow")
{
	SECTION("Command with Stop - Terminate returns true")
	{
		bool bCommandRan { false };

		const char* CLI[] {
			"MyProgramName",
			"deploy", "target1"
		};

		KOptions Opt(false);
		Opt.Command("deploy <target>")
			.Help("deploy to a target")
			.MinArgs(1)
			.MaxArgs(1)
			.Stop()
		([&](KOptions::ArgList& Args)
		{
			CHECK ( Args.pop() == "target1" );
			bCommandRan = true;
		});

		int iRetval = Opt.Parse(sizeof(CLI)/sizeof(char*), CLI);
		CHECK ( iRetval    == 0    );
		CHECK ( Opt.Terminate()    );
		CHECK ( bCommandRan        );
	}

	SECTION("Command with Stop - not matched - Terminate returns false")
	{
		bool bCommandRan { false };

		const char* CLI[] {
			"MyProgramName",
			"-verbose"
		};

		KOptions Opt(false);
		Opt.AllowAdHocArgs();
		Opt.Command("deploy <target>")
			.Help("deploy to a target")
			.MinArgs(1)
			.MaxArgs(1)
			.Stop()
		([&](KOptions::ArgList& Args)
		{
			bCommandRan = true;
		});

		int iRetval = Opt.Parse(sizeof(CLI)/sizeof(char*), CLI);
		CHECK ( iRetval         == 0  );
		CHECK ( Opt.Terminate() == false );
		CHECK ( bCommandRan     == false );

		bool bVerbose = Opt("verbose : be verbose", false);
		CHECK ( bVerbose == true );
	}

	SECTION("Command with Final - immediate termination")
	{
		bool bFinalRan  { false };
		bool bSecondRan { false };

		const char* CLI[] {
			"MyProgramName",
			"quit"
		};

		KOptions Opt(false);
		Opt.Command("quit")
			.Help("quit the program")
			.Final()
		([&](KOptions::ArgList& Args)
		{
			bFinalRan = true;
		});

		Opt.Option("other")
			.Help("other option")
		([&]()
		{
			bSecondRan = true;
		});

		int iRetval = Opt.Parse(sizeof(CLI)/sizeof(char*), CLI);
		CHECK ( iRetval         == 0    );
		CHECK ( Opt.Terminate() == true );
		CHECK ( bFinalRan       == true );
		CHECK ( bSecondRan      == false );
	}

	SECTION("ad-hoc + help: Terminate returns true after ad-hoc registration")
	{
		const char* CLI[] {
			"MyProgramName",
			"-help"
		};

		KOptions Opt(false);
		Opt.AllowAdHocArgs();
		int iRetval = Opt.Parse(sizeof(CLI)/sizeof(char*), CLI);

		// register ad-hoc options (with defaults, since -help was given)
		KStringView sFile = Opt("f,file <filename> : a file to read", "");
		bool        bVerbose = Opt("v,verbose : be verbose", false);

		CHECK ( iRetval == 0 );
		// Terminate must be true because deferred help is pending
		CHECK ( Opt.Terminate() == true );
		// values should be defaults
		CHECK ( sFile.empty()       );
		CHECK ( bVerbose == false   );

		KString sOut;
		KOutStringStream oss(sOut);
		// Check prints the deferred help
		CHECK ( Opt.Check(oss)  == false );
		CHECK ( sOut.find("-f,file <filename>") != npos );
		CHECK ( sOut.find("a file to read")     != npos );
		CHECK ( sOut.find("-v,verbose")         != npos );
		CHECK ( sOut.find("be verbose")         != npos );
	}

	SECTION("ad-hoc without help: Terminate returns false")
	{
		const char* CLI[] {
			"MyProgramName",
			"-file", "test.txt",
			"-verbose"
		};

		KOptions Opt(false);
		Opt.AllowAdHocArgs();
		int iRetval = Opt.Parse(sizeof(CLI)/sizeof(char*), CLI);

		KStringView sFile = Opt("f,file <filename> : a file to read", "");
		bool        bVerbose = Opt("v,verbose : be verbose", false);

		CHECK ( iRetval == 0 );
		CHECK ( Opt.Terminate() == false );
		CHECK ( sFile    == "test.txt" );
		CHECK ( bVerbose == true       );
		CHECK ( Opt.Check() == true    );
	}

	SECTION("mixed Command+Stop with ad-hoc: command path")
	{
		bool bDiffRan { false };
		KStringViewZ sDiffLeft;
		KStringViewZ sDiffRight;

		const char* CLI[] {
			"MyProgramName",
			"diff", "left.dbc", "right.dbc"
		};

		KOptions Opt(false);
		Opt.AllowAdHocArgs();
		Opt.Command("diff <dbc1> <dbc2>")
			.Help("diff two databases")
			.MinArgs(2)
			.MaxArgs(2)
			.Stop()
		([&](KOptions::ArgList& Args)
		{
			sDiffLeft  = Args.pop();
			sDiffRight = Args.pop();
			bDiffRan = true;
		});

		int iRetval = Opt.Parse(sizeof(CLI)/sizeof(char*), CLI);

		// register ad-hoc options (even though diff was matched)
		KStringView sDBC = Opt("dbc : dbc file name", "");
		bool bQuiet      = Opt("q,quiet : quiet mode", false);

		CHECK ( iRetval         == 0         );
		CHECK ( Opt.Terminate() == true      );
		CHECK ( bDiffRan        == true      );
		CHECK ( sDiffLeft       == "left.dbc"  );
		CHECK ( sDiffRight      == "right.dbc" );
		// ad-hoc options have defaults
		CHECK ( sDBC.empty()              );
		CHECK ( bQuiet          == false  );
	}

	SECTION("mixed Command+Stop with ad-hoc: non-command path")
	{
		bool bDiffRan { false };

		const char* CLI[] {
			"MyProgramName",
			"-dbc", "mydb.dbc",
			"-quiet"
		};

		KOptions Opt(false);
		Opt.AllowAdHocArgs();
		Opt.Command("diff <dbc1> <dbc2>")
			.Help("diff two databases")
			.MinArgs(2)
			.MaxArgs(2)
			.Stop()
		([&](KOptions::ArgList& Args)
		{
			bDiffRan = true;
		});

		int iRetval = Opt.Parse(sizeof(CLI)/sizeof(char*), CLI);

		KStringView sDBC = Opt("dbc : dbc file name", "");
		bool bQuiet      = Opt("q,quiet : quiet mode", false);

		CHECK ( iRetval         == 0          );
		CHECK ( Opt.Terminate() == false      );
		CHECK ( bDiffRan        == false      );
		CHECK ( sDBC            == "mydb.dbc" );
		CHECK ( bQuiet          == true       );
		CHECK ( Opt.Check()     == true       );
	}

	SECTION("mixed Command+Stop with ad-hoc: help path")
	{
		const char* CLI[] {
			"MyProgramName",
			"-help"
		};

		KOptions Opt(false);
		Opt.AllowAdHocArgs();
		Opt.Command("diff <dbc1> <dbc2>")
			.Help("diff two databases")
			.MinArgs(2)
			.MaxArgs(2)
			.Stop()
		([&](KOptions::ArgList& Args)
		{
		});

		int iRetval = Opt.Parse(sizeof(CLI)/sizeof(char*), CLI);

		KStringView sDBC = Opt("dbc : dbc file name", "");    (void)sDBC;
		bool bQuiet      = Opt("q,quiet : quiet mode", false); (void)bQuiet;

		CHECK ( iRetval         == 0     );
		CHECK ( Opt.Terminate() == true  );

		KString sOut;
		KOutStringStream oss(sOut);
		CHECK ( Opt.Check(oss) == false );
		// help output should contain both the command and the ad-hoc options
		CHECK ( sOut.find("diff")           != npos );
		CHECK ( sOut.find("-dbc")           != npos );
		CHECK ( sOut.find("dbc file name")  != npos );
		CHECK ( sOut.find("-q,quiet")       != npos );
		CHECK ( sOut.find("quiet mode")     != npos );
	}

	SECTION("Command with missing required args")
	{
		const char* CLI[] {
			"MyProgramName",
			"deploy"
		};

		KString sOut;
		KOutStringStream oss(sOut);

		KOptions Opt(false);
		Opt.Command("deploy <target>")
			.Help("deploy to a target")
			.MinArgs(1)
			.MaxArgs(1)
			.Stop()
		([&](KOptions::ArgList& Args)
		{
		});

		int iRetval = Opt.Parse(sizeof(CLI)/sizeof(char*), CLI, oss);
		CHECK ( iRetval != 0 );
	}

	SECTION("required option not provided")
	{
		const char* CLI[] {
			"MyProgramName",
			"-verbose"
		};

		KOptions Opt(false);
		Opt.AllowAdHocArgs();
		Opt.Option("output <file>")
			.Help("output file")
			.MinArgs(1)
			.Required()
		([&](KStringViewZ sFile)
		{
		});

		KString sOut;
		KOutStringStream oss(sOut);
		int iRetval = Opt.Parse(sizeof(CLI)/sizeof(char*), CLI, oss);
		CHECK ( iRetval != 0 );
	}

	SECTION("empty args with EmptyParmsIsError=true")
	{
		const char* CLI[] {
			"MyProgramName"
		};

		KString sOut;
		KOutStringStream oss(sOut);

		KOptions Opt(true);
		Opt.Option("v,verbose")
			.Help("be verbose")
		([&]()
		{
		});

		int iRetval = Opt.Parse(sizeof(CLI)/sizeof(char*), CLI, oss);
		// should trigger help output and return non-zero
		CHECK ( iRetval != 0 );
		// help output should contain the registered option
		CHECK ( sOut.find("-v,verbose") != npos );
	}

	SECTION("empty args with EmptyParmsIsError=false")
	{
		const char* CLI[] {
			"MyProgramName"
		};

		KOptions Opt(false);
		Opt.Option("v,verbose")
			.Help("be verbose")
		([&]()
		{
		});

		int iRetval = Opt.Parse(sizeof(CLI)/sizeof(char*), CLI);
		CHECK ( iRetval         == 0     );
		CHECK ( Opt.Terminate() == false );
	}

	SECTION("excess arguments are reported")
	{
		const char* CLI[] {
			"MyProgramName",
			"unexpected_arg"
		};

		KString sOut;
		KOutStringStream oss(sOut);

		KOptions Opt(false);
		int iRetval = Opt.Parse(sizeof(CLI)/sizeof(char*), CLI, oss);
		CHECK ( iRetval != 0 );
		CHECK ( sOut.find("excess") != npos );
	}

	SECTION("multiple commands - only first matched")
	{
		bool bDeployRan { false };
		bool bStatusRan { false };

		const char* CLI[] {
			"MyProgramName",
			"status"
		};

		KOptions Opt(false);
		Opt.Command("deploy <target>")
			.Help("deploy to a target")
			.MinArgs(1)
			.Stop()
		([&](KOptions::ArgList& Args)
		{
			bDeployRan = true;
		});

		Opt.Command("status")
			.Help("show status")
			.Stop()
		([&](KOptions::ArgList& Args)
		{
			bStatusRan = true;
		});

		int iRetval = Opt.Parse(sizeof(CLI)/sizeof(char*), CLI);
		CHECK ( iRetval         == 0     );
		CHECK ( Opt.Terminate() == true  );
		CHECK ( bDeployRan      == false );
		CHECK ( bStatusRan      == true  );
	}

	SECTION("Command with Stop does not prevent subsequent options")
	{
		bool bCommandRan { false };
		bool bOptionRan  { false };

		const char* CLI[] {
			"MyProgramName",
			"-verbose",
			"deploy", "target1"
		};

		KOptions Opt(false);
		Opt.Option("verbose")
			.Help("be verbose")
		([&]()
		{
			bOptionRan = true;
		});

		Opt.Command("deploy <target>")
			.Help("deploy to a target")
			.MinArgs(1)
			.MaxArgs(1)
			.Stop()
		([&](KOptions::ArgList& Args)
		{
			CHECK ( Args.pop() == "target1" );
			bCommandRan = true;
		});

		int iRetval = Opt.Parse(sizeof(CLI)/sizeof(char*), CLI);
		CHECK ( iRetval         == 0    );
		CHECK ( Opt.Terminate() == true );
		CHECK ( bOptionRan      == true );
		CHECK ( bCommandRan     == true );
	}
}
