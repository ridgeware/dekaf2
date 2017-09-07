#include "catch.hpp"

#include <dekaf2/kinpipe.h>
#include <dekaf2/kstring.h>

#include <iostream>
#include <ostream>
#include <stdio.h>
//#include <stdlib.h>

using namespace dekaf2;

TEST_CASE("KInPipe")
{
	SECTION("KInPipe normal Open and Close")
	{
		INFO("normal_open_close_test::Start:");

		KInPipe pipe;
		pipe.SetReaderTrim("");

		// open the pipe
		CHECK(pipe.Open("/bin/sh -c \"echo 'some random datum' > /tmp/kinpipetest.file\""));
		pipe.WaitForFinished(10);
		CHECK_FALSE(pipe.IsRunning());
		CHECK(0 == pipe.Close());
		CHECK(pipe.Open("/bin/sh -c \"ls -al /tmp/kinpipetest.file | grep kinpipetest | wc -l\""));

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
		KString sCommand("/bin/sh -c \"echo 'some random data' > /tmp/kinpipetest.file 2>&1\"");//
		CHECK(pipe.splitArgs(sCommand, argVector));

		std::ofstream outFile;
		outFile.open("/tmp/hermes/utestLog.txt");
		CHECK(argVector.size() == 4);
		std::vector<const char*> compVector = {"/bin/sh", "-c", "echo 'some random data' > /tmp/kinpipetest.file 2>&1", NULL};
		for (size_t i = 0; i < argVector.size() - 1; i++)
		{
			CHECK(strcmp(compVector[i], argVector[i]) == 0);
		}

		INFO("KInPipe split arg test::Done:");
	} // normal open close

}
