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

		//CHECK(pipe.Open("/bin/sh -c \"echo 'asdf asdf' > /tmp/dekaf2/KOutPipetest3.file\""));
		//CHECK(pipe.Open("/bin/sh -c \"echo 'some random datum' > /tmp/koutpipetest.file\""));
		CHECK(pipe.Open("/bin/sh -c \"cat > /tmp/dekaf2/koutpipetest1.file\""));
		CHECK(pipe.m_writePipe != NULL);
		KString str("rdoanm txet over 9000 \n line 2 \n line 3 \n line 4 \n SS level 3! \n line 6 \n line 7\n\n");
		//str = str + EOF;
		//KString str("echo rdoanm txet over 9000\n");//n line 2 \n line 3 \n line 4 \n SS level 3! \n line 6 \n line 7\"");
		char endFile = 0x04;
		//char endFile = '^d';
		pipe.Write(str);
		CHECK(pipe.m_writePipe != NULL);
		CHECK(pipe.is_open());
		//pipe.Write(endFile);
		//pipe.WaitForFinished(10000);
		CHECK(pipe.IsRunning());
		pipe.SendEOF();
		//CHECK_FALSE(pipe.IsRunning());
		CHECK(pipe.m_writePipe != NULL);
		CHECK(0 == pipe.Close());

		INFO("KOutPipe  write_pipe::Done:");

	} // write_pipe
}
