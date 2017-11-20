#include "catch.hpp"
#include <dekaf2/kpipe.h>

#include <iostream>

#define KPipeCleanup 1

using namespace dekaf2;

TEST_CASE("KPipe")
{

	SECTION("KOutPipe  write_pipe and confirm by reading")
	{
		INFO("KOutPipe  write_pipe and confirm by reading::Start:");

		KPipe pipe("/bin/sh -c \"mkdir /tmp/kpipetests\"");
		KString sCurrentLine;
		KString str("rdoanm txet over 9000 \n line 2 \n line 3 \n line 4 \n SS level 3! \n line 6 \n line 7\n\n");
		pipe.SetReaderTrim("");

		// Generate test file
		CHECK(pipe.IsRunning());
		CHECK(0 == pipe.Close());
		CHECK_FALSE(pipe.IsRunning());
		CHECK(pipe.Open("/bin/sh -c \"cat > /tmp/kpipetests/koutpipetest1.file\""));
		pipe.Write(str);
		CHECK(pipe.IsRunning());
		CHECK(0 == pipe.Close());
		CHECK_FALSE(pipe.IsRunning());

		// Verify contents of test file
		CHECK(pipe.Open("/bin/sh -c \"cat /tmp/kpipetests/koutpipetest1.file\""));
		CHECK(pipe.IsRunning());
		bool output = pipe.ReadLine(sCurrentLine);
		CHECK(output);
		CHECK("rdoanm txet over 9000 \n" == sCurrentLine);
		CHECK(0 == pipe.Close());
		CHECK_FALSE(pipe.IsRunning());

		INFO("KOutPipe  write_pipe and confirm by reading::Done:");

	} // read and write pipe

	SECTION("KPipe fail_to_open")
	{
		INFO("KPipe fail_to_open::Start:");

		KPipe pipe;
		CHECK_FALSE(pipe.Open(""));
		CHECK_FALSE(pipe.IsRunning());
		CHECK(-1 == pipe.Close());

		INFO("KOutPipe fail_to_open::Done:");
	} // fail_to_open

#if KPipeCleanup
	SECTION("KOutPipe  cleanup test")
	{
		INFO("KOutPipe  write_pipe and confirm by reading::Start:");
		// This really doesn't test KOutPipe, it kinda tests kinpipe, but the purpose is to remove test files after testing

		KPipe pipe;
		pipe.SetReaderTrim("");
		KString sCurrentLine;

		// Double check test files are there
		CHECK(pipe.Open("/bin/sh -c \"ls /tmp/kpipetests/ | wc -l\""));
		CHECK(pipe.IsRunning());
		bool output = pipe.ReadLine(sCurrentLine);
		sCurrentLine.TrimLeft();
		CHECK(output);
		CHECK("1\n" == sCurrentLine);
		CHECK(0 == pipe.Close());
		CHECK_FALSE(pipe.IsRunning());

		// Remove test files
		CHECK(pipe.Open("/bin/sh -c \"rm -rf /tmp/kpipetests/\""));
		CHECK(pipe.IsRunning());
		CHECK(0 == pipe.Close());
		CHECK_FALSE(pipe.IsRunning());


		// Double check they're gone
		CHECK(pipe.Open("/bin/sh -c \"ls /tmp/kpipetests/ 2>/dev/null | wc -l\""));
		CHECK(pipe.IsRunning());
		output = pipe.ReadLine(sCurrentLine);
		sCurrentLine.TrimLeft();
		CHECK(output);
		CHECK("0\n" == sCurrentLine);
		CHECK(0 == pipe.Close());
		CHECK_FALSE(pipe.IsRunning());

		INFO("KOutPipe  write_pipe and confirm by reading::Done:");

	} // cleanup test
#endif

}
