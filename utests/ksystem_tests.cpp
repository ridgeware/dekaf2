#include "catch.hpp"

#include <dekaf2/kstring.h>
#include <dekaf2/ksystem.h>
#include <dekaf2/klog.h>

using namespace dekaf2;

TEST_CASE("KSystem")
{
	SECTION("EnvVars")
	{
		CHECK ( kUnsetEnv("DEKAF2_UTEST_VAR1") == true );
		CHECK ( kSetEnv("DEKAF2_UTEST_VAR1", "ThisIsATest") == true );
		CHECK ( kGetEnv("DEKAF2_UTEST_VAR1") == "ThisIsATest" );
		CHECK ( kUnsetEnv("DEKAF2_UTEST_VAR1") == true );
		CHECK ( kGetEnv("DEKAF2_UTEST_VAR1") == "" );
	}

	SECTION("CWD")
	{
		auto sTemp = kGetTemp();
		CHECK ( sTemp != "" );
		auto sDir = kGetCWD();
		CHECK ( sDir != "" );
		CHECK ( sTemp != sDir );
		CHECK ( kSetCWD(sTemp) );
		auto sCD = kGetCWD();
#ifdef DEKAF2_IS_OSX
		sCD.remove_prefix("/private");
#endif
		CHECK ( sTemp == sCD );
		CHECK ( kSetCWD(sDir) );
		CHECK ( kGetCWD() == sDir );
	}

	SECTION("Home")
	{
		auto sHome = kGetHome();
		CHECK ( sHome != "" );
	}

	SECTION("WhoAmI")
	{
		auto sWho = kGetWhoAmI();
		CHECK ( sWho != "" );
	}

	SECTION("Hostname")
	{
		auto sHostname = kGetHostname();
		CHECK ( sHostname != "" );
	}

	SECTION("TID")
	{
		auto i = kGetTid();
		CHECK ( i != 0 );
	}

	SECTION("Random")
	{
		auto i = kRandom(17, 43);
		CHECK ( (i >= 17 && i <= 43) == true );
	}

	SECTION("kSystem")
	{
		KString sOutput;
		auto iRet = kSystem("echo this is some text", sOutput);
		CHECK ( iRet == 0 );
		CHECK ( sOutput == "this is some text\n" );

		iRet = kSystem("echo this is some text && exit 123", sOutput);
		CHECK ( iRet == 123 );
#ifdef DEKAF2_IS_WINDOWS
		// Windows 'echo' includes any trailing space
		CHECK(sOutput == "this is some text \n");
#else
		CHECK ( sOutput == "this is some text\n" );
#endif

		iRet = kSystem("echo this is some && echo multiline text && echo output", sOutput);
		CHECK ( iRet == 0 );
#ifdef DEKAF2_IS_WINDOWS
		CHECK ( sOutput.starts_with("this is some") );
		CHECK ( sOutput.ends_with("output\n") );
#else
		CHECK ( sOutput == "this is some\nmultiline text\noutput\n" );
#endif

#ifdef DEKAF2_IS_WINDOWS
		iRet = kSystem("echo 123 && timeout /t 1 && echo this is some text", sOutput);
		CHECK ( iRet == 0 );
		CHECK ( sOutput.starts_with("123") );
		CHECK ( sOutput.ends_with("this is some text\n") );
#else
		iRet = kSystem("echo 123 && sleep 1 && echo this is some text", sOutput);
		CHECK ( iRet == 0 );
		CHECK ( sOutput == "123\nthis is some text\n" );
#endif

#ifdef DEKAF2_IS_WINDOWS
		iRet = kSystem("dir", sOutput);
#else
		iRet = kSystem("ls -al ./", sOutput);
#endif
		CHECK ( iRet == 0 );
		CHECK ( sOutput != "" );

		iRet = kSystem("", sOutput);
		CHECK ( iRet == EINVAL );
		CHECK ( sOutput == "" );

		iRet = kSystem("abasdkhjfgbsarkjghvasgskufhse", sOutput);

		// on debian, we occasionally get 141, which is the shell's return value after a SIGPIPE
		if (iRet != 141)
		{

			CHECK ( iRet == DEKAF2_POPEN_COMMAND_NOT_FOUND );
			INFO  ( sOutput );
			CHECK ( sOutput.contains("abasdkhjfgbsarkjghvasgskufhse") );
#ifdef DEKAF2_IS_WINDOWS
			CHECK ( sOutput.contains("not recognized") );
#else
			CHECK ( sOutput.contains("not found") );
#endif
		}

		iRet = kSystem("echo hello world");
		CHECK ( iRet == 0 );

		iRet = kSystem("exit 123");
		CHECK ( iRet == 123 );

		iRet = kSystem("abasdkhjfgbsarkjghvasgskufhse");
		CHECK ( iRet == DEKAF2_POPEN_COMMAND_NOT_FOUND);
	}

	SECTION("rusage")
	{
		auto iStart1 = kGetMicroTicksPerProcess();
		auto iStart2 = kGetMicroTicksPerThread();
		auto iStart3 = kGetMicroTicksPerChildProcesses();
		KString s;
		for (auto i = 0; i < 100000; ++i)
		{
			s += 'a';
		}
		if (s[10000] != 'a')
		{
			kDebug(1, "false");
		}
		auto iTicks1 = kGetMicroTicksPerProcess()        - iStart1;
		auto iTicks2 = kGetMicroTicksPerThread()         - iStart2;
		auto iTicks3 = kGetMicroTicksPerChildProcesses() - iStart3;
		CHECK ( iTicks1 > 0 );
		CHECK ( iTicks2 > 0 );
	}
}
