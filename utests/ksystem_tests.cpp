#include "catch.hpp"

#include <dekaf2/kstring.h>
#include <dekaf2/ksystem.h>

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

		CHECK ( iRet == DEKAF2_POPEN_COMMAND_NOT_FOUND );
		CHECK ( sOutput.Contains("abasdkhjfgbsarkjghvasgskufhse") );
#ifdef DEKAF2_IS_WINDOWS
		CHECK ( sOutput.Contains("not recognized") );
#else
		CHECK ( sOutput.Contains("not found") );
#endif

		iRet = kSystem("echo hello world");
		CHECK ( iRet == 0 );

		iRet = kSystem("exit 123");
		CHECK ( iRet == 123 );

		iRet = kSystem("abasdkhjfgbsarkjghvasgskufhse");
		CHECK ( iRet == DEKAF2_POPEN_COMMAND_NOT_FOUND);
	}

}

