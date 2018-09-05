#include "catch.hpp"

#include <dekaf2/kinpipe.h>
#include <dekaf2/kstring.h>

#include <iostream>
#include <ostream>
#include <stdio.h>
//#include <stdlib.h>

#define KInPipeCleanup 1

using namespace dekaf2;

TEST_CASE("KInPipe")
{
	SECTION("KInPipe normal Open and Close")
	{
		INFO("normal_open_close_test::Start:");

		KInPipe pipe("/bin/sh -c \"mkdir /tmp/kinpipetests\"");
		pipe.SetReaderTrim("");

		CHECK(pipe.Open("/bin/sh -c \"echo 'some random datum' > /tmp/kinpipetests/kinpipetest.file\""));
		pipe.WaitForFinished(10);
		CHECK_FALSE(pipe.IsRunning());
		CHECK(0 == pipe.Close());
		CHECK(pipe.Open("/bin/sh -c \"ls -al /tmp/kinpipetests/kinpipetest.file | grep kinpipetest | wc -l\""));

		KString sCurrentLine;

		CHECK(pipe.IsRunning());
		bool output = pipe.ReadLine(sCurrentLine);
		sCurrentLine.TrimLeft();
		CHECK(output);
		CHECK("1\n" == sCurrentLine);
		CHECK(pipe.IsRunning());
		CHECK(pipe.is_open());
		CHECK(0 == pipe.Close());

		INFO("KInPipe normal_open_close_test::Done:");
	} // normal open close

	SECTION("KInPipe fail_to_open")
	{
		INFO("KInPipe fail_to_open::Start:");

		KInPipe pipe;
		CHECK_FALSE(pipe.Open(""));
		CHECK_FALSE(pipe.IsRunning());
		CHECK(-1 == pipe.Close());

		INFO("KInPipe fail_to_open::Done:");
	} // fail_to_open

#if KInPipeCleanup
	SECTION("KInPipe  cleanup test")
	{
		INFO("KInPipe  write_pipe and confirm by reading::Start:");

		KInPipe pipe;
		pipe.SetReaderTrim("");
		KString sCurrentLine;

		// Double check test files are there
		//CHECK(pipe.Open("/bin/sh -c \"ls /tmp/kinpipetests/ \""));
		CHECK(pipe.Open("/bin/sh -c \"ls /tmp/kinpipetests/ | wc -l\""));
		CHECK(pipe.is_open());
		CHECK(pipe.IsRunning());
		bool output = pipe.ReadLine(sCurrentLine);
		sCurrentLine.TrimLeft();
		CHECK(output);
// If having problems on cleanup, just try printing ls output to see what files weren't generated
//		std::cout << "current line: " << sCurrentLine << std::endl;
//		while(output)
//		{
//			output = pipe.ReadLine(sCurrentLine);
//			if (output) std::cout << "current line: " << sCurrentLine << std::endl;
//		}

		CHECK("1\n" == sCurrentLine);
		CHECK(0 == pipe.Close());
		CHECK_FALSE(pipe.IsRunning());

		// Remove test files
		CHECK(pipe.Open("/bin/sh -c \"rm -rf /tmp/kinpipetests/\""));
		CHECK(pipe.is_open());
		CHECK(pipe.IsRunning());
		CHECK(0 == pipe.Close());
		CHECK_FALSE(pipe.IsRunning());


		// Double check they're gone
		CHECK(pipe.Open("/bin/sh -c \"ls /tmp/kinpipetests/ 2>/dev/null | wc -l\""));
		CHECK(pipe.is_open());
		CHECK(pipe.IsRunning());
		output = pipe.ReadLine(sCurrentLine);
		sCurrentLine.TrimLeft();
		CHECK(output);
		CHECK("0\n" == sCurrentLine);
		CHECK(0 == pipe.Close());
		CHECK_FALSE(pipe.IsRunning());


		INFO("KInPipe  write_pipe and confirm by reading::Done:");

	} // cleanup test
#endif

}

