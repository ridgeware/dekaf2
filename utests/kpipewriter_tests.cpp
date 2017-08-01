#include "catch.hpp"
#include "kpipewriter.h"
#include "kpipereader.h"

#include <iostream>

#define kpwPRINT 0

using namespace dekaf2;
TEST_CASE("KPipeWriter")
{

	SECTION("KPipe  write_pipe")
	{
		INFO("KPipe  write_pipe::Start:");

		KPipeWriter pipe;

		//CHECK(pipe.Open("echo rdoanm txet 1 > /tmp/tmp.file"));
		CHECK(pipe.Open("cat > /tmp/tmp.file"));
		// Write more to pipe
		KString str("echo rdoanm txet over 9000 \n line 2 \n line 3 \n line 4 \n SS level 3! \n line 6 \n line 7");
		pipe.Write(str);
		CHECK(pipe.is_open());
		CHECK(0 == pipe.Close());

		INFO("KPipe  write_pipe::Done:");

	} // write_pipe

	SECTION("KPipe  write_pipe")
	{
		INFO("KPipe  write_pipe then read to confirm");

		KPipeWriter pipe;

		//CHECK(pipe.Open("echo rdoanm txet 1 > /tmp/tmp.file"));
		CHECK(pipe.Open("cat > /tmp/tmp.file"));
		// Write more to pipe
		KString str("echo rdoanm txet over 9000 \n line 2 \n line 3 \n line 4 \n SS level 3! \n line 6 \n line 7");
		pipe.Write(str);
		CHECK(pipe.is_open());
		CHECK(0 == pipe.Close());

		KPipeReader readPipe;
		CHECK(readPipe.Open("cat /tmp/tmp.file"));
		KString output;
		for (auto iter = readPipe.begin(); iter != readPipe.end(); iter++)
		{
			output = output + *iter;
		}
#if kpwPRINT
		std::cout << output << std::endl;
#endif
		CHECK(output.compare(str) == 0);

		INFO("KPipe  write_pipe::Done:");

	} // write_pipe


}
