#include "catch.hpp"
#include <dekaf2/koutpipe.h>
#include <dekaf2/kinpipe.h>

#ifdef DEKAF2_HAS_PIPES

#include <iostream>

#define KOutPipeCleanup 1

using namespace dekaf2;
TEST_CASE("KOutPipe")
{

	SECTION("KOutPipe  write_pipe")
	{
		KOutPipe pipe("mkdir /tmp/koutpipetests", "/bin/sh");
		KString str("rdoanm txet over 9000 \n line 2 \n line 3 \n line 4 \n SS level 3! \n line 6 \n line 7\n\n");

		CHECK(pipe.Open("/bin/sh -c \"cat > /tmp/koutpipetests/koutpipetest1.file\""));
		pipe.Write(str);
		CHECK(pipe.is_open());
		CHECK(pipe.IsRunning());

		CHECK(0 == pipe.Close());
		CHECK_FALSE(pipe.IsRunning());
	}

	SECTION("KOutPipe fail_to_open")
	{
		KOutPipe pipe;
		CHECK_FALSE(pipe.Open(""));
		CHECK_FALSE(pipe.IsRunning());
		CHECK(EINVAL == pipe.Close());
	}

	SECTION("KOutPipe  write_pipe and confirm by reading")
	{
		KOutPipe pipe;
		KInPipe readPipe;
		KString sCurrentLine;
		KString str("rdoanm txet over 9000 \n line 2 \n line 3 \n line 4 \n SS level 3! \n line 6 \n line 7\n\n");
		readPipe.SetReaderTrim("");

		// Generate test file
		CHECK(pipe.Open("/bin/sh -c \"cat > /tmp/koutpipetests/koutpipetest2.file\""));
		pipe.Write(str);
		CHECK(pipe.is_open());
		CHECK(pipe.IsRunning());
		CHECK(0 == pipe.Close());
		CHECK_FALSE(pipe.IsRunning());

		// Verify contents of test file
		CHECK(readPipe.Open("/bin/sh -c \"cat /tmp/koutpipetests/koutpipetest2.file\""));
		CHECK(readPipe.is_open());
		CHECK(readPipe.IsRunning());
		bool output = readPipe.ReadLine(sCurrentLine);
		CHECK(output);
		CHECK("rdoanm txet over 9000 \n" == sCurrentLine);
		CHECK(0 == readPipe.Close());
		CHECK_FALSE(readPipe.IsRunning());
	}

#if KOutPipeCleanup
	SECTION("KOutPipe  cleanup test")
	{
		// This really doesn't test KOutPipe, it kinda tests kinpipe, but the purpose is to remove test files after testing

		KOutPipe pipe;
		KInPipe readPipe;
		readPipe.SetReaderTrim("");
		KString sCurrentLine;

		// Double check test files are there
		CHECK(readPipe.Open("/bin/sh -c \"ls /tmp/koutpipetests/ | wc -l\""));
		CHECK(readPipe.is_open());
		CHECK(readPipe.IsRunning());
		bool output = readPipe.ReadLine(sCurrentLine);
		sCurrentLine.TrimLeft();
		CHECK(output);
		CHECK("2\n" == sCurrentLine);
		CHECK(0 == readPipe.Close());
		CHECK_FALSE(readPipe.IsRunning());

		// Remove test files
		CHECK(pipe.Open("/bin/sh -c \"rm -rf /tmp/koutpipetests/\""));
		CHECK(pipe.is_open());
		CHECK(pipe.IsRunning());
		CHECK(0 == pipe.Close());
		CHECK_FALSE(pipe.IsRunning());


		// Double check they're gone
		CHECK(readPipe.Open("/bin/sh -c \"ls /tmp/koutpipetests/ 2>/dev/null | wc -l\""));
		CHECK(readPipe.is_open());
		CHECK(readPipe.IsRunning());
		output = readPipe.ReadLine(sCurrentLine);
		sCurrentLine.TrimLeft();
		CHECK(output);
		CHECK("0\n" == sCurrentLine);
		CHECK(0 == readPipe.Close());
		CHECK_FALSE(readPipe.IsRunning());
	}
#endif

}

#endif
