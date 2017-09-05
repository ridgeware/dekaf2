#include "catch.hpp"
#include <dekaf2/koutpipe.h>
#include <dekaf2/kinpipe.h>

#include <iostream>

using namespace dekaf2;
TEST_CASE("KOutPipe")
{

	SECTION("KOutPipe  write_pipe")
	{
		INFO("KOutPipe  write_pipe::Start:");

		KOutPipe pipe;

		//CHECK(pipe.Open("/bin/sh -c \"echo > /tmp/KOutPipetest.file\""));
		//CHECK(pipe.Open("/bin/sh -c \"echo 'some random datum' > /tmp/koutpipetest.file\""));
		CHECK(pipe.Open("/bin/sh -c \"cat > /tmp/dekaf2/koutpipetest1.file\""));

		KString str("rdoanm txet over 9000 \n line 2 \n line 3 \n line 4 \n SS level 3! \n line 6 \n line 7\n\n");
		//KString str("echo rdoanm txet over 9000\n");//n line 2 \n line 3 \n line 4 \n SS level 3! \n line 6 \n line 7\"");
		pipe.Write(str);
		CHECK(pipe.is_open());
		pipe.WaitForFinished(1000);
		CHECK(0 == pipe.Close());

		INFO("KOutPipe  write_pipe::Done:");

	} // write_pipe
}
