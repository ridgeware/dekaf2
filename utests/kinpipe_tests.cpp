#include "catch.hpp"

#include <dekaf2/kinpipe.h>

#ifdef DEKAF2_HAS_PIPES

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
		KInPipe pipe("/bin/sh -c \"mkdir /tmp/kinpipetests\"");
		pipe.SetReaderTrim("");

		CHECK(pipe.Open("/bin/sh -c \"echo 'some random datum' > /tmp/kinpipetests/kinpipetest.file\""));
		pipe.Wait(1000);
		CHECK_FALSE(pipe.IsRunning());
		CHECK(0 == pipe.Close());
		CHECK(pipe.Open("/bin/sh -c \"ls -al /tmp/kinpipetests/kinpipetest.file | grep kinpipetest | wc -l\""));

		KString sCurrentLine;

		CHECK(pipe.IsRunning());
		bool output = pipe.ReadLine(sCurrentLine);
		sCurrentLine.TrimLeft();
		CHECK(output);
		CHECK("1\n" == sCurrentLine);
		CHECK(pipe.is_open());
		CHECK(0 == pipe.Close());
	}

	SECTION("KInPipe fail_to_open")
	{
		KInPipe pipe;
		CHECK_FALSE(pipe.Open(""));
		CHECK_FALSE(pipe.IsRunning());
		CHECK(EINVAL == pipe.Close());
	}

#if KInPipeCleanup
	SECTION("KInPipe cleanup test")
	{
		KInPipe pipe;
		pipe.SetReaderTrim("");
		KString sCurrentLine;

		// Double check test files are there
		CHECK(pipe.Open("/bin/sh -c \"ls /tmp/kinpipetests/ | wc -l\""));
		CHECK(pipe.is_open());
		CHECK(pipe.IsRunning());
		bool output = pipe.ReadLine(sCurrentLine);
		sCurrentLine.TrimLeft();
		CHECK(output);
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
	}
#endif

}

#endif
