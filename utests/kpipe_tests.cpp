#include "catch.hpp"

#include <klog.h>
#include <kpipe.h>

#include <signal.h>
#include <unistd.h>
#include <kstring.h>
#include <iostream>

#include <fmt/format.h>

using namespace dekaf2;
//using fmt::format;

#define KPIPE_DELAY (5)
KString KPipeDelayCommand(unsigned int depth, unsigned int second, const KString sMessage1 = "", const KString sMessage2 = "")
{
	return fmt::format("$dekaf/utests/kpipe_delay_test.sh {} {} '{}' '{}' 2>&1",
	            depth, second/40, sMessage1.c_str(), sMessage2.c_str());
}


void kpipe_testKillDelayTask()
{
	KString sCmd("pkill -f 'kpipe_delay_test' > /dev/null 2>&1");
	system(sCmd.c_str());

} // kpipe_testKillDelayTask

KString string_to_hex(KString input)
{
	static const char* const lut = "0123456789ABCDEF";
	size_t len = input.length();

	KString output;
	output.reserve(4 * len);
	for (size_t i = 0; i < len; ++i)
	{
		const unsigned char c = input[i];
		output.push_back(lut[c >> 4]);
		output.push_back(lut[c & 15]);
		output.push_back(' ');
	}
	return output;

} // string_to_hex

TEST_CASE("KPipe")
{
	SECTION("KPipe Curl Test")
	{
		KPIPE   pipe;
		KString sCurlCMD = "curl -i www.acme.com 2> /dev/null";
		CHECK(pipe.Open(sCurlCMD, "r"));

		KString sCurrentLine;
		bool output = pipe.getline(sCurrentLine, 4096);
		CHECK(output);
		//std::cout << "output is:\n" << std::endl;
		while (output)
		{
			//std::cout << sCurrentLine;
			output = pipe.getline(sCurrentLine, 4096);
			//CHECK(output);

		}
		CHECK_FALSE(output);
		CHECK(pipe.Close() == 0);
	}

	SECTION("KPipe normal Open and Close")
	{
		KLog().warning("normal_open_close_test::Start:");

		KPIPE   pipe;

		// open the pipe
		CHECK(pipe.Open("ls -al $dekaf/kpipe.cpp | grep kpipe.cpp | wc -l 2>&1", "r"));

		KString sCurrentLine;

		bool output = pipe.getline(sCurrentLine, 4096);
		//std::cout << "output is: " << output << std::endl;
		CHECK(output);
		REQUIRE("1\n" == sCurrentLine);


		REQUIRE(0 == pipe.Close());

		KLog().warning("normal_open_close_test::Done:");
	}

#if 0
	SECTION("KPipe getline_with_timeout_failure)")
	{
		KLog().warning("getline_with_timeout_failure::Start:");

		KPIPE   pipe;

		kpipe_testKillDelayTask();

		// open the pipe
		CHECK(pipe.Open(KPipeDelayCommand(3, 20).c_str(), "r"));

		KString sCurrentLine = "Some string";

		CHECK(pipe.getline(sCurrentLine, 4096, 1));
		CHECK("Current Level = 3\n" == sCurrentLine);

		CHECK(pipe.getline(sCurrentLine, 4096, 1));
		CHECK("Current Level = 2\n" == sCurrentLine);

		CHECK(pipe.getline(sCurrentLine, 4096, 1));
		CHECK("Current Level = 1\n" == sCurrentLine);

		CHECK_FALSE(pipe.getline(sCurrentLine, 4096, 0));
		CHECK("" == sCurrentLine);

		CHECK(-1 == pipe.Close());

		kpipe_testKillDelayTask();

		KLog().warning("getline_with_timeout_failure::Done:");

	} // getline_with_timeout_failure

	SECTION("KPipe::getlin with timeout success")
	{
		KLog().warning("getline_with_timeout_success_II::Start:");

		KPIPE   pipe;

		kpipe_testKillDelayTask();

		// open the pipe
		CHECK(pipe.Open(KPipeDelayCommand(3, 5).c_str(), "r"));

		KString sCurrentLine = "Some string";

		CHECK(pipe.getline(sCurrentLine, 4096, 2));
		CHECK("Current Level = 3\n" == sCurrentLine);

		CHECK(pipe.getline(sCurrentLine, 4096, 2));
		CHECK("Current Level = 2\n" == sCurrentLine);

		CHECK(pipe.getline(sCurrentLine, 4096, 2));
		CHECK("Current Level = 1\n" == sCurrentLine);

		CHECK(pipe.getline(sCurrentLine, 4096, 10));
		CHECK("Current Level = 0\n" == sCurrentLine);

		CHECK(0 == pipe.Close());

		kpipe_testKillDelayTask();

		KLog().warning("getline_with_timeout_success_II::Done:");

	} // getline_with_timeout_success_II

	SECTION("KPipe  getline_with_partial_text_and_timeout_failure")
	{
		KLog().warning("getline_with_partial_text_and_timeout_failure::Start:");
		KPIPE   pipe;

		kpipe_testKillDelayTask();
		// open the pipe

		KString pipeDelayCMD(KPipeDelayCommand(3, 20, "getline_with_partial_text_and_timeout_failure", "GoodTimesForMe"));
		CHECK(pipe.Open(pipeDelayCMD, "r"));

		KString sCurrentLine = "Some string";

		CHECK(pipe.getline(sCurrentLine, 4096, 2));
		CHECK("Current Level = 3\n" == sCurrentLine);

		CHECK(pipe.getline(sCurrentLine, 4096, 2));
		CHECK("Current Level = 2\n" == sCurrentLine);

		CHECK(pipe.getline(sCurrentLine, 4096, 2));
		CHECK("Current Level = 1\n" == sCurrentLine);

		CHECK_FALSE(pipe.getline(sCurrentLine, 4096, 0));
		//CHECK("getline_with_partial_text_and_timeout_failureGoodTimesForMe" == sCurrentLine);
		CHECK("" == sCurrentLine);

		CHECK(-1 == pipe.Close());

		kpipe_testKillDelayTask();

		KLog().warning("getline_with_partial_text_and_timeout_failure::Done:");

	} // getline_with_partial_text_and_timeout_failure
#endif

#if 0
	SECTION("KPipe  get_pid_should_not_return_zero")
	{
		KLog().warning("get_pid_should_not_return_zero::Start:");

		KPIPE   pipe;
		kpipe_testKillDelayTask();

		// open the pipe
		CHECK(pipe.Open(KPipeDelayCommand(10, KPIPE_DELAY).c_str(), "r"));
		CHECK(0 != pipe.getPid());

		kpipe_testKillDelayTask();

		KLog().warning("get_pid_should_not_return_zero::Done:");

	} // get_pid_should_not_return_zero

	SECTION("KPipe  new_process_pid_should_not_be_equal_current_pid")
	{
		KLog().warning("new_process_pid_should_not_be_equal_current_pid::Start:");

		KPIPE   pipe;
		kpipe_testKillDelayTask();

		// open the pipe
		CHECK(pipe.Open(KPipeDelayCommand(10, KPIPE_DELAY).c_str(), "r"));
		CHECK(getpid() != pipe.getPid());

		kpipe_testKillDelayTask();

		KLog().warning("new_process_pid_should_not_be_equal_current_pid::Done:");

	} // new_process_pid_should_not_be_equal_current_pid
#endif

#if 0
	SECTION("KPipe  getline_with_timeout_success")
	{
		KLog().warning("getline_with_timeout_success::Start:");

		KPIPE   pipe;

		// open the pipe
		CHECK(pipe.Open("ls -al $dekaf | grep kpipe | wc -l 2>&1", "r"));

		KString sCurrentLine = "Some string";

		CHECK(pipe.getline(sCurrentLine, 4096, 20));
		CHECK("1\n" == sCurrentLine);

		// dont expect any more data
		CHECK_FALSE(pipe.getline(sCurrentLine, 4096, 2));
		CHECK((KString::size_type)0 == sCurrentLine.length());
		if (0 != sCurrentLine.length())
		{
			//printf("unexpected output '%s'\n", string_to_hex(sCurrentLine).c_str());
			std::cout << "unexpected output \"" << string_to_hex(sCurrentLine).c_str() << "\"" << std::endl;
		}

		CHECK(0 == pipe.Close());

		KLog().warning("getline_with_timeout_success::Done:");

	} // getline_with_timeout_success

	SECTION("KPipe  getline_huge_with_timeout_success")
	{
		KLog().warning("getline_huge_with_timeout_success::Start:");

		KPIPE   pipe;

		// open the pipe
		CHECK(pipe.Open("sudo ls $dekaf 2>&1", "r"));

		KString sCurrentLine = "Some string";

		// read in a large number of lines
		int iNumLinesReceived = 0;
		pipe.getline(sCurrentLine, 4096, 20);
		while (0 != sCurrentLine.length())
		{
			iNumLinesReceived++;
			pipe.getline(sCurrentLine, 4096, 20);
		}

		CHECK(0 < iNumLinesReceived);
		CHECK(0 == pipe.Close());

		KLog().warning("getline_huge_with_timeout_success::Done:");

	} // getline_huge_with_timeout_success
#endif

#if 0
	SECTION("KPipe  check_is_running")
	{
		KLog().warning("check_is_running::Start:");

		KPIPE pipe;

		CHECK(pipe.Open("sleep 20", "r"));

		CHECK(pipe.isRunning());

		kill(pipe.getPid(), SIGKILL);

		int iLoopCount = 10;
		while ((true == pipe.isRunning()) && (0 < --iLoopCount))
		{
			sleep(1);
		}

		CHECK_FALSE(pipe.isRunning());

		CHECK(-1 == pipe.Close());

		KLog().warning("check_is_running::Done:");

	} // check_is_running

	SECTION("KPipe  is_running_return_false_after_close")
	{
		KLog().warning("is_running_return_false_after_close::Start:");

		KPIPE pipe;

		CHECK(pipe.Open("sleep 20", "r"));

		kill(pipe.getPid(), SIGKILL);

		CHECK(-1 == pipe.Close());

		CHECK_FALSE(pipe.isRunning());

		KLog().warning("is_running_return_false_after_close::Done:");

	} // is_running_return_false_after_close

	SECTION("KPipe  is_running_return_false_if_pipe_was_never_open")
	{
		KLog().warning("is_running_return_false_if_pipe_is_not_open::Start:");

		KPIPE pipe;
		CHECK_FALSE(pipe.isRunning());

		KLog().warning("is_running_return_false_if_pipe_is_not_open::Done:");

	} // is_running_return_false_if_pipe_is_not_open
#endif

}
