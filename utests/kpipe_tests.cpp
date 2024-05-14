#include "catch.hpp"
#include <dekaf2/kpipe.h>

#ifdef DEKAF2_HAS_PIPES

#include <iostream>

#define KPipeCleanup 1

using namespace dekaf2;

namespace {
KTempDir TempDir;
}

TEST_CASE("KPipe")
{

	SECTION("KOutPipe  write_pipe and confirm by reading")
	{
		KPipe pipe;
		KString sCurrentLine;
		KString str("rdoanm text over 9000 \n line 2 \n line 3 \n line 4 \n SS level 3! \n line 6 \n line 7\n\n");
		pipe.SetReaderTrim("");

		// Generate test file
		CHECK_FALSE(pipe.IsRunning());
		CHECK(pipe.Open(kFormat("/bin/sh -c \"cat > {}/koutpipetest1.file\"", TempDir.Name())));
		pipe.Write(str);
		CHECK(pipe.IsRunning());
		CHECK(0 == pipe.Close());
		CHECK_FALSE(pipe.IsRunning());

		// Verify contents of test file
		CHECK(pipe.Open(kFormat("/bin/sh -c \"cat {}/koutpipetest1.file\"", TempDir.Name())));
#ifndef DEKAF2_HAS_MUSL
		CHECK(pipe.IsRunning());
#endif
		bool output = pipe.ReadLine(sCurrentLine);
		CHECK(output);
		CHECK("rdoanm text over 9000 \n" == sCurrentLine);
		CHECK(0 == pipe.Close());
		CHECK_FALSE(pipe.IsRunning());
	}

	SECTION("KPipe fail_to_open")
	{
		KPipe pipe;
		CHECK_FALSE(pipe.Open(""));
		CHECK_FALSE(pipe.IsRunning());
		CHECK(EINVAL == pipe.Close());
	}

#if KPipeCleanup
	SECTION("KOutPipe  cleanup test")
	{
		// This really doesn't test KOutPipe, it kinda tests kinpipe, but the purpose is to remove test files after testing

		KPipe pipe;
		pipe.SetReaderTrim("");
		KString sCurrentLine;

		// Double check test files are there
		CHECK(pipe.Open(kFormat("/bin/sh -c \"ls {}/ | wc -l\"", TempDir.Name())));
		CHECK(pipe.IsRunning());
		bool output = pipe.ReadLine(sCurrentLine);
		sCurrentLine.TrimLeft();
		CHECK(output);
		CHECK("1\n" == sCurrentLine);
		CHECK(0 == pipe.Close());
		CHECK_FALSE(pipe.IsRunning());

		// Remove test files
		CHECK(pipe.Open(kFormat("/bin/sh -c \"rm -rf {}/\"", TempDir.Name())));
		CHECK(pipe.IsRunning());
		CHECK(0 == pipe.Close());
		CHECK_FALSE(pipe.IsRunning());

		// Double check they're gone
		CHECK(pipe.Open(kFormat("/bin/sh -c \"ls {}/ 2>/dev/null | wc -l\"", TempDir.Name())));
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
