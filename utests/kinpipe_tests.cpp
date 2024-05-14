#include "catch.hpp"

#include <dekaf2/kinpipe.h>

#ifdef DEKAF2_HAS_PIPES

#include <dekaf2/kstring.h>

#include <iostream>
#include <ostream>
#include <stdio.h>

#define KInPipeCleanup 1

using namespace dekaf2;

namespace {
KTempDir TempDir;
}

TEST_CASE("KInPipe")
{
	SECTION("KInPipe normal Open and Close")
	{
		KInPipe pipe;
		pipe.SetReaderTrim("");

		CHECK(pipe.Open(kFormat("/bin/sh -c \"echo 'some random datum' > {}/kinpipetest.file\"", TempDir.Name())));
		pipe.Wait(1000);
		CHECK_FALSE(pipe.IsRunning());
		CHECK(0 == pipe.Close());
		CHECK(pipe.Open(kFormat("ls -al {}/kinpipetest.file | grep kinpipetest | wc -l", TempDir.Name()), "/bin/sh"));

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
		CHECK(pipe.Open(kFormat("/bin/sh -c \"ls {}/ | wc -l\"", TempDir.Name())));
		CHECK(pipe.is_open());
		CHECK(pipe.IsRunning());
		bool output = pipe.ReadLine(sCurrentLine);
		sCurrentLine.TrimLeft();
		CHECK(output);
		CHECK("1\n" == sCurrentLine);
		CHECK(0 == pipe.Close());
		CHECK_FALSE(pipe.IsRunning());

		// Remove test files
		CHECK(pipe.Open(kFormat("/bin/sh -c \"rm -rf {}/\"", TempDir.Name())));
		CHECK(pipe.is_open());
		CHECK(pipe.IsRunning());
		CHECK(0 == pipe.Close());
		CHECK_FALSE(pipe.IsRunning());

		// Double check they're gone
		CHECK(pipe.Open(kFormat("/bin/sh -c \"ls {}/ 2>/dev/null | wc -l\"", TempDir.Name())));
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
