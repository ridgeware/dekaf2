#include "catch.hpp"

#include <kpipereader.h>
#include <kstring.h>

#include <iostream>
#include <unistd.h>

#define kprPRINT 0

using namespace dekaf2;

#define KPIPE_DELAY (1)
KString KPipeReaderDelayCommand(unsigned int depth, unsigned int second, const KString sMessage1 = "", const KString sMessage2 = "")
{
	return fmt::format("$dekaf/utests/kpipe_delay_test.sh {} {} '{}' '{}' 2>&1",
	            depth, second/40, sMessage1.c_str(), sMessage2.c_str());
}

void kpipereader_testKillDelayTask()
{
	KString sCmd("pkill -f 'kpipe_delay_test' > /dev/null 2>&1");
	system(sCmd.c_str());

} // kpipe_testKillDelayTask

TEST_CASE("KPipeReader")
{

	SECTION("KPipeReader Curl Test")
	{
		KPipeReader pipe;
		KString sCurlCMD = "curl -i www.google.com 2> /dev/null";
		CHECK(pipe.Open(sCurlCMD));

		KString sCurrentLine;
		bool output = pipe.ReadLine(sCurrentLine);
		CHECK(output);
		//std::cout << "output is:\n" << std::endl;
		while (output)
		{
			//std::cout << sCurrentLine;
			output = pipe.ReadLine(sCurrentLine);
			//CHECK(output);

		}
		CHECK_FALSE(output);
		CHECK(pipe.Close() == 0);
	} // curl test

	SECTION("KPipeReader normal Open and Close")
	{
		INFO("normal_open_close_test::Start:");

		KPipeReader pipe;

		// open the pipe
		CHECK(pipe.Open("ls -al $dekaf/kpipe.cpp | grep kpipe.cpp | wc -l 2>&1"));

		KString sCurrentLine;

		bool output = pipe.ReadLine(sCurrentLine);
		CHECK(output);
		//pipe.ReadLine(sCurrentLine);
#if kprPRINT
		std::cout << "output is: " << sCurrentLine << std::endl;
#endif
		CHECK("1\n" == sCurrentLine);

		CHECK(pipe.is_open());
		CHECK(0 == pipe.Close());

		INFO("normal_open_close_test::Done:");
	} // normal open close

	SECTION("KPipeReader  get_errno_should_return_zero")
	{
		INFO("get_pid_should_not_return_zero::Start:");

		KPipeReader   pipe;
		kpipereader_testKillDelayTask();

		// open the pipe
		CHECK(pipe.Open(KPipeReaderDelayCommand(10, KPIPE_DELAY).c_str()));
		CHECK(0 == pipe.GetErrno());

		kpipereader_testKillDelayTask();

		INFO("KPipeReader get_pid_should_not_return_zero::Done:");

	} // get_pid_should_not_return_zero

	SECTION("KPipeReader  check_is_running")
	{
		INFO("KPipeReader check_is_running::Start:");

		KPipeReader pipe;

		CHECK(pipe.Open("usleep 2"));

		CHECK(pipe.is_open());


		int iLoopCount = 10;
		while ((true == pipe.is_open()) && (0 < --iLoopCount))
		{
			usleep(1);
		}

		CHECK(pipe.is_open());

		CHECK(0 == pipe.Close());
		CHECK(-1 == pipe.Close());

		INFO("KPipeReader check_is_running::Done:");

	} // check_is_running

	SECTION("KPipeReader  is_open_return_false_if_pipe_was_never_open")
	{
		INFO("KPipeReader is_open_return_false_if_pipe_is_not_open::Start:");

		KPipeReader pipe;
		CHECK_FALSE(pipe.is_open());

		INFO("KPipeReader is_open_return_false_if_pipe_is_not_open::Done:");

	} // is_running_return_false_if_pipe_is_not_open

	SECTION("KPipeReader  cannot_readline_of_bad_pipe")
	{
		INFO("KPipeReader cannot_readline_of_bad_pipe::Start:");

		KPipeReader pipe;
		//CHECK_FALSE(pipe.Open("echo rdoanm txet > /tmp/tmp.file"));
		CHECK_FALSE(pipe.is_open());
		KString outBuff;
		CHECK_FALSE(pipe.ReadLine(outBuff));

		INFO("KPipe cannot_readline_of_bad_pipe::Done:");

	} // cannot_readline_of_bad_pipe

	SECTION("KPipeReader  using_file_star_operator")
	{
		INFO("KPipeReader  using_file_star_operator::Start:");

		KPipeReader pipe;

		CHECK(pipe.Open("echo rdoanm txet > /tmp/tmp.file && cat /tmp/tmp.file > /dev/null"));
		//CHECK(pipe.Open("echo rdoanm txet > /tmp/tmp.file", "w"));

		FILE* fileDesc(pipe);

		CHECK(fileDesc);

		CHECK(pipe.is_open());

		CHECK(0 == pipe.Close());

		INFO("KPipeReader  using_file_star_operator::Done:");

	} // using_file_star_operator

	SECTION("KPipeReader Iterator Test")
	{
		KPipeReader   pipe;
		//KString sCurlCMD = "echo 'random text asdfjkl;asdfjkl;\nqwerty\nuoip\nzxcvbnm,zxcvbnm,\n\n' > /tmp/tmp.file && cat /tmp/tmp.file 2> /dev/null";
		KString sCurlCMD = "echo 'random text asdfjkl;asdfjkl; qwerty uoip zxcvbnm,zxcvbnm,  ' > /tmp/tmp.file && cat /tmp/tmp.file 2> /dev/null";
		CHECK(pipe.Open(sCurlCMD));

		KString sCurrentLine;
		KString output;
		//for (auto iter : pipe.begin())
		for (auto iter = pipe.begin(); iter != pipe.end(); iter++)
		{
			output = output + *iter;
			//std::cout << *iter ;
		}
#if kprPRINT
		std::cout << output << std::endl;
#endif
		CHECK_FALSE(output.empty());
		CHECK(output.length() == 60);

		CHECK(pipe.Close() == 0);
	} // Iterator Test

	SECTION("KPipe Curl Iterator Test")
	{
		KPipeReader   pipe;
		KString sCurlCMD = "curl -i www.google.com 2> /dev/null";
		CHECK(pipe.Open(sCurlCMD));

		KString sCurrentLine;
		KString output;

		for (auto iter = pipe.begin(); iter != pipe.end(); iter++)
		{
			output = output + *iter;
			//std::cout << *iter ;
		}
#if kprPRINT
		std::cout << output << std::endl;
#endif

		CHECK_FALSE(output.empty());
		CHECK(pipe.Close() == 0);
	} // Curl Iterator Test

	SECTION("KPipeReader fail to open test")
	{
		INFO("normal_open_close_test::Start:");

		KPipeReader pipe;

		// open the pipe
		//CHECK_FALSE(pipe.Open("cd / ; find . -iname 'cpp' 2>&1 | xargs cat 2>&1"));
		//[onelink@localhost /]$ find . -iname "cpp" | xargs cat

		KString sCurrentLine;

		//bool output = pipe.ReadLine(sCurrentLine);
		//CHECK(output);
		//pipe.ReadLine(sCurrentLine);
#if kprPRINT
		std::cout << "output is: " << sCurrentLine << std::endl;
#endif
		//CHECK("1\n" == sCurrentLine);

		//CHECK_FALSE(pipe.is_open());
		//CHECK(0 == pipe.Close());

		INFO("normal_open_close_test::Done:");
	} // normal open close
}
