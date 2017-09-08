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

		KInPipe pipe;
		pipe.SetReaderTrim("");

		CHECK(pipe.Open("/bin/sh -c \"mkdir /tmp/kinpipetests\""));
		CHECK(pipe.Open("/bin/sh -c \"echo 'some random datum' > /tmp/kinpipetests/kinpipetest.file\""));
		pipe.WaitForFinished(10);
		CHECK_FALSE(pipe.IsRunning());
		CHECK(0 == pipe.Close());
		CHECK(pipe.Open("/bin/sh -c \"ls -al /tmp/kinpipetests/kinpipetest.file | grep kinpipetest | wc -l\""));

		KString sCurrentLine;

		CHECK(pipe.IsRunning());
		bool output = pipe.ReadLine(sCurrentLine);
		CHECK(output);
		CHECK("1\n" == sCurrentLine);
		CHECK(pipe.IsRunning());
		CHECK(pipe.is_open());
		CHECK(0 == pipe.Close());

		INFO("KInPipe normal_open_close_test::Done:");
	} // normal open close

	SECTION("KInPipe split arg test")
	{
		INFO("KInPipe split arg test::Start:");

		KInPipe pipe;
		pipe.SetReaderTrim("");

		char ** args;
		size_t len;
		std::vector<char*> argVector;
		KString sCommand("/bin/sh -c \"echo 'some random data' > /tmp/kinpipetests/kinpipetest.file 2>&1\"");//
		CHECK(pipe.splitArgs(sCommand, argVector));

		std::ofstream outFile;
		outFile.open("/tmp/hermes/utestLog.txt");
		CHECK(argVector.size() == 4);
		std::vector<const char*> compVector = {"/bin/sh", "-c", "echo 'some random data' > /tmp/kinpipetests/kinpipetest.file 2>&1", NULL};
		for (size_t i = 0; i < argVector.size() - 1; i++)
		{
			CHECK(strcmp(compVector[i], argVector[i]) == 0);
		}

		INFO("KInPipe split arg test::Done:");
	} // normal open close

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
		CHECK(pipe.Open("/bin/sh -c \"ls /tmp/kinpipetests/ 2>&1\""));
		CHECK(pipe.is_open());
		CHECK(pipe.IsRunning());
		output = pipe.ReadLine(sCurrentLine);
		CHECK(output);
		CHECK("ls: cannot access /tmp/kinpipetests/: No such file or directory\n" == sCurrentLine);
		CHECK(2 == pipe.Close()); // when ls can't find returns code 2
		CHECK_FALSE(pipe.IsRunning());


		INFO("KInPipe  write_pipe and confirm by reading::Done:");

	} // cleanup test
#endif

}
