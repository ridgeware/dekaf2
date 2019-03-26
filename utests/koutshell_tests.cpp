#include "catch.hpp"
#include <dekaf2/koutshell.h>
#include <dekaf2/kinshell.h>

#ifndef DEKAF2_IS_WINDOWS

#include <iostream>

using namespace dekaf2;
TEST_CASE("KOutShell")
{

	SECTION("KOutShell  write_pipe")
	{
		INFO("KOutShell  write_pipe::Start:");

		KOutShell pipe;

		CHECK(pipe.Open("cat > /tmp/KOutShelltest.file"));

		KString str("echo rdoanm txet over 9000 \n line 2 \n line 3 \n line 4 \n SS level 3! \n line 6 \n line 7");
		pipe.Write(str);
		CHECK(pipe.is_open());
		CHECK(0 == pipe.Close());

		INFO("KOutShell  write_pipe::Done:");

	} // write_pipe

	SECTION("KOutShell  write_pipe")
	{
		INFO("KOutShell  write_pipe then read to confirm");

		KOutShell pipe;

		CHECK(pipe.Open("cat > /tmp/KOutShelltest.file"));
		// Write more to pipe
		KString str("echo rdoanm txet over 9000 \n line 2 \n line 3 \n line 4 \n SS level 3! \n line 6 \n line 7");
		pipe.Write(str);
		CHECK(pipe.is_open());
		CHECK(0 == pipe.Close());

		KInShell readPipe;
		readPipe.SetReaderTrim("");
		CHECK(readPipe.Open("cat /tmp/KOutShelltest.file"));
		KString output;
		for (auto iter = readPipe.begin(); iter != readPipe.end(); ++iter)
		{
			output = output + *iter;
		}
		CHECK(output.compare(str) == 0);

		INFO("KOutShell  write_pipe::Done:");

	} // write_pipe

	SECTION("KOutShell fail to open test")
	{
		INFO("KOutShell fail to open test::Start:");

		KOutShell pipe;

		// fail open the pipe with empty string
		CHECK_FALSE(pipe.Open(""));

		CHECK(pipe.Close() == EINVAL);

		INFO("KOutShell fail to open test::Done:");
	} // normal open close

}

#endif // DEKAF2_IS_WINDOWS
