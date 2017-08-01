#include "catch.hpp"

#include <kpipereader.h>
#include <kstring.h>

#include <iostream>

#define kprPRINT 0

using namespace dekaf2;
TEST_CASE("KPipeReader")
{

	SECTION("KPipe normal Open and Close")
	{
		INFO("normal_open_close_test::Start:");

		KPipeReader pipe;

		// open the pipe
		CHECK(pipe.Open("ls -al $dekaf/kpipe.cpp | grep kpipe.cpp | wc -l 2>&1"));

		KString sCurrentLine;

		bool output = pipe.ReadLine(sCurrentLine);
		CHECK(output);
		//pipe.ReadLine(sCurrentLine);
#if kprPRINT
		std::cout << "output is: " << sCurrentLine << std::endl;
#endif
		CHECK("1\n" == sCurrentLine);


		CHECK(0 == pipe.Close());

		INFO("normal_open_close_test::Done:");
	}


	SECTION("KPipe Curl Iterator Test")
	{
		KPipeReader   pipe;
		KString sCurlCMD = "curl -i www.google.com 2> /dev/null";
		CHECK(pipe.Open(sCurlCMD));

		KString sCurrentLine;
		KString output;

		for (auto iter = pipe.begin(); iter != pipe.end(); iter++)
		{
			output = output + *iter;
			//std::cout << *iter ;
		}
#if kprPRINT
		std::cout << output << std::endl;
#endif

		CHECK_FALSE(output.empty());
		CHECK(pipe.Close() == 0);
	} // Curl Iterator Test
}
