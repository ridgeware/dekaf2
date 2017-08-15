#include "catch.hpp"

#include <kinshell.h>
#include <kstring.h>

#include <iostream>
#include <unistd.h>
#include <sys/resource.h>

#define kprPRINT 0

using namespace dekaf2;

#define KPIPE_DELAY (1)
KString KPipeReaderDelayCommand(unsigned int depth, unsigned int second, const KString sMessage1 = "", const KString sMessage2 = "")
{
	    return fmt::format("$dekaf/utests/kpipe_delay_test.sh {} {} '{}' '{}' 2>&1",
		            depth, second/40, sMessage1.c_str(), sMessage2.c_str());
}

void kpipereader_testKillDelayTask()
{
	    KString sCmd("pkill -f 'kpipe_delay_test' > /dev/null 2>&1");
		system(sCmd.c_str());

} // kpipe_testKillDelayTask

TEST_CASE("KInShell")
{

	SECTION("KPipeReader Curl Test")
	{
		KInShell pipe;
		KString sCurlCMD = "curl -i www.google.com 2> /dev/null";
		CHECK(pipe.Open(sCurlCMD));

		KString sCurrentLine;
		bool output = pipe.ReadLine(sCurrentLine);
		CHECK(output);
		//std::cout << "output is:\n" << std::endl;
		while (output)
		{
			//std::cout << sCurrentLine;
			output = pipe.ReadLine(sCurrentLine);
			//CHECK(output);

		}
		CHECK_FALSE(output);
		CHECK(pipe.Close() == 0);
	} // curl test

} // end test case KPipeReader
