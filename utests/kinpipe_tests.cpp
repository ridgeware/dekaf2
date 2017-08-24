#include "catch.hpp"

#include <dekaf2/kinpipe.h>
#include <dekaf2/kstring.h>

using namespace dekaf2;

TEST_CASE("KInPipe")
{
	SECTION("KInPipe normal Open and Close")
	{
		INFO("normal_open_close_test::Start:");

		KInPipe pipe;
		pipe.SetReaderTrim("");

		// open the pipe
		CHECK(pipe.Open("cat some random data > /tmp/kinshelltest.file 2>&1"));
		CHECK(pipe.Open("ls -al /tmp/kinshelltest.file | grep kinshelltest | wc -l 2>&1"));

		KString sCurrentLine;

		CHECK(pipe.IsRunning());
		bool output = pipe.ReadLine(sCurrentLine);
		CHECK(output);
		CHECK("1\n" == sCurrentLine);
		CHECK_FALSE(pipe.IsRunning());
		CHECK(pipe.is_open());
		CHECK(0 == pipe.Close());

		INFO("KInPipe normal_open_close_test::Done:");
	} // normal open close

}
