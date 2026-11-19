#include "catch.hpp"

#include <dekaf2/system/process/kinshell.h>
#include <dekaf2/core/strings/kstring.h>

#ifndef DEKAF2_IS_WINDOWS

#include <iostream>
#ifdef DEKAF2_IS_WINDOWS
	#include <io.h>
#else
	#include <unistd.h>
	#include <sys/resource.h>
#endif

using namespace dekaf2;

namespace {

// Google answers some addresses, like those of CI runners, with 429 Too Many Requests,
// and kurl returns 1 for any status other than 2xx. That is no failure of the pipe:
// warns, and returns true to make the caller leave out the check of the exit code
bool IsThrottled(KStringView sStatusLine)
{
	if (sStatusLine.starts_with("HTTP/") && sStatusLine.contains(" 429"))
	{
		WARN("www.google.com answers with 429 (too many requests from this address) - exit code not checked");
		return true;
	}

	return false;
}

} // end of anonymous namespace

TEST_CASE("KInShell")
{

	SECTION("KPipeReader Curl Test")
	{
		KInShell pipe;
		KString sCurlCMD = "kurl -i www.google.com 2> /dev/null";
		CHECK(pipe.Open(sCurlCMD));

		KString sCurrentLine;
		bool bOutput = pipe.ReadLine(sCurrentLine);
		CHECK(bOutput);
		// the status line of the response
		KString sStatusLine = sCurrentLine;
		while (bOutput)
		{
			bOutput = pipe.ReadLine(sCurrentLine);

		}
		CHECK_FALSE(bOutput);

		if (IsThrottled(sStatusLine))
		{
			pipe.Close();
		}
		else
		{
			CHECK(pipe.Close() == 0);
		}
	}

	SECTION("KInShell Curl Iterator Test")
	{
		KInShell   pipe;
		KString sCurlCMD = "kurl -i www.google.com 2> /dev/null";
		CHECK(pipe.Open(sCurlCMD));

		KString sCurrentLine;
		KString output;

		// the status line of the response
		KString sStatusLine;

		for (auto iter = pipe.begin(); iter != pipe.end(); iter++)
		{
			if (output.empty())
			{
				sStatusLine = *iter;
			}

			output = output + *iter;
		}

		CHECK_FALSE(output.empty());

		if (IsThrottled(sStatusLine))
		{
			pipe.Close();
		}
		else
		{
			CHECK(pipe.Close() == 0);
		}
	}

}

#endif // !DEKAF2_IS_WINDOWS
