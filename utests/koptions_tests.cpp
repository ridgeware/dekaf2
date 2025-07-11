#include "catch.hpp"
#include <dekaf2/koptions.h>
#include <dekaf2/ksystem.h>
#include <dekaf2/kfilesystem.h>
#include <dekaf2/kcgistream.h>
#include <dekaf2/kstringstream.h>


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
