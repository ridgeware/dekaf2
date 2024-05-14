#include "catch.hpp"
#include <dekaf2/koutpipe.h>
#include <dekaf2/kinpipe.h>

#ifdef DEKAF2_HAS_PIPES

#include <iostream>

using namespace dekaf2;

namespace {
KTempDir TempDir;
}

TEST_CASE("KOutPipe")
{

	SECTION("KOutPipe write_pipe")
	{
		KOutPipe pipe;
		KString str("rdoanm txet over 9000 \n line 2 \n line 3 \n line 4 \n SS level 3! \n line 6 \n line 7\n\n");

		CHECK(pipe.Open(kFormat("/bin/sh -c \"cat > {}/koutpipetest1.file\"", TempDir.Name())));
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

	SECTION("KOutPipe write_pipe and confirm by reading")
	{
		KOutPipe pipe;
		KInPipe readPipe;
		KString sCurrentLine;
		KString str("rdoanm text over 9000 \n line 2 \n line 3 \n line 4 \n SS level 3! \n line 6 \n line 7\n\n");
		readPipe.SetReaderTrim("");

		// Generate test file
		CHECK(pipe.Open(kFormat("/bin/sh -c \"cat > {}/koutpipetest2.file\"", TempDir.Name())));
		pipe.Write(str);
		CHECK(pipe.is_open());
		CHECK(pipe.IsRunning());
		CHECK(0 == pipe.Close());
		CHECK_FALSE(pipe.IsRunning());

		// Verify contents of test file
		CHECK(readPipe.Open(kFormat("/bin/sh -c \"cat {}/koutpipetest2.file\"", TempDir.Name())));
		CHECK(readPipe.is_open());
		CHECK(readPipe.IsRunning());
		bool output = readPipe.ReadLine(sCurrentLine);
		CHECK(output);
		CHECK("rdoanm text over 9000 \n" == sCurrentLine);
		CHECK(0 == readPipe.Close());
		CHECK_FALSE(readPipe.IsRunning());
	}
}

#endif
