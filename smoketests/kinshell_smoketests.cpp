#include "catch.hpp"

#include <dekaf2/kinshell.h>
#include <dekaf2/kstring.h>

#ifndef DEKAF2_IS_WINDOWS

#include <iostream>
#ifdef DEKAF2_IS_WINDOWS
	#include <io.h>
#else
	#include <unistd.h>
	#include <sys/resource.h>
#endif

using namespace dekaf2;

TEST_CASE("KInShell")
{

	SECTION("KPipeReader Curl Test")
	{
		KInShell pipe;
		KString sCurlCMD = "curl -i www.google.com 2> /dev/null";
		CHECK(pipe.Open(sCurlCMD));

		KString sCurrentLine;
		bool bOutput = pipe.ReadLine(sCurrentLine);
		CHECK(bOutput);
		while (bOutput)
		{
			bOutput = pipe.ReadLine(sCurrentLine);

		}
		CHECK_FALSE(bOutput);
		CHECK(pipe.Close() == 0);
	}

	SECTION("KInShell Curl Iterator Test")
	{
		KInShell   pipe;
		KString sCurlCMD = "curl -i www.google.com 2> /dev/null";
		CHECK(pipe.Open(sCurlCMD));

		KString sCurrentLine;
		KString output;

		for (auto iter = pipe.begin(); iter != pipe.end(); iter++)
		{
			output = output + *iter;
		}

		CHECK_FALSE(output.empty());
		CHECK(pipe.Close() == 0);
	}

}

#endif // !DEKAF2_IS_WINDOWS