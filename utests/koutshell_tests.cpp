#include "catch.hpp"
#include <dekaf2/koutshell.h>
#include <dekaf2/kinshell.h>

#include <iostream>

using namespace dekaf2;
TEST_CASE("KPipeWriter")
{

	SECTION("KPipe  write_pipe")
	{
		INFO("KPipe  write_pipe::Start:");

		KOutShell pipe;

		CHECK(pipe.Open("cat > /tmp/KOutShelltest.file"));

		KString str("echo rdoanm txet over 9000 \n line 2 \n line 3 \n line 4 \n SS level 3! \n line 6 \n line 7");
		pipe.Write(str);
		CHECK(pipe.is_open());
		CHECK(0 == pipe.Close());

		INFO("KPipe  write_pipe::Done:");

	} // write_pipe

	SECTION("KPipe  write_pipe")
	{
		INFO("KPipe  write_pipe then read to confirm");

		KOutShell pipe;

		CHECK(pipe.Open("cat > /tmp/KOutShelltest.file"));
		// Write more to pipe
		KString str("echo rdoanm txet over 9000 \n line 2 \n line 3 \n line 4 \n SS level 3! \n line 6 \n line 7");
		pipe.Write(str);
		CHECK(pipe.is_open());
		CHECK(0 == pipe.Close());

		KInShell readPipe;
		CHECK(readPipe.Open("cat /tmp/KOutShelltest.file"));
		KString output;
		for (auto iter = readPipe.begin(); iter != readPipe.end(); iter++)
		{
			output = output + *iter;
		}
		CHECK(output.compare(str) == 0);

		INFO("KPipe  write_pipe::Done:");

	} // write_pipe

	SECTION("KPipeReader fail to open test")
	{
		INFO("KPipeReader fail to open test::Start:");

		KOutShell pipe;

		// fail open the pipe with empty string
		CHECK_FALSE(pipe.Open(""));

		CHECK(pipe.Close() == -1);

		INFO("KPipeReader fail to open test::Done:");
	} // normal open close
}
