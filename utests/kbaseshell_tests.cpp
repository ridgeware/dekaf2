//#include "catch.hpp"

//#include <klog.h>
//#include <kpipe.h>

//#include <signal.h>
//#include <unistd.h>
//#include <kstring.h>
//#include <iostream>

//#include <fmt/format.h>

//using namespace dekaf2;
////using fmt::format;

//#define KPIPE_DELAY (1)
//KString KPipeDelayCommand(unsigned int depth, unsigned int second, const KString sMessage1 = "", const KString sMessage2 = "")
//{
//	return fmt::format("$dekaf/utests/kpipe_delay_test.sh {} {} '{}' '{}' 2>&1",
//	            depth, second/40, sMessage1.c_str(), sMessage2.c_str());
//}


//void kpipe_testKillDelayTask()
//{
//	KString sCmd("pkill -f 'kpipe_delay_test' > /dev/null 2>&1");
//	system(sCmd.c_str());

//} // kpipe_testKillDelayTask

//TEST_CASE("KPipe")
//{

//	SECTION("KPipe Curl Test")
//	{
//		KPIPE   pipe;
//		KString sCurlCMD = "curl -i www.google.com 2> /dev/null";
//		CHECK(pipe.Open(sCurlCMD, "r"));

//		KString sCurrentLine;
//		bool output = pipe.getline(sCurrentLine, 4096);
//		CHECK(output);
//		//std::cout << "output is:\n" << std::endl;
//		while (output)
//		{
//			//std::cout << sCurrentLine;
//			output = pipe.getline(sCurrentLine, 4096);
//			//CHECK(output);

//		}
//		CHECK_FALSE(output);
//		CHECK(pipe.Close() == 0);
//	}

//	SECTION("KPipe normal Open and Close")
//	{
//		INFO("normal_open_close_test::Start:");

//		KPIPE   pipe;

//		// open the pipe
//		CHECK(pipe.Open("ls -al $dekaf/kpipe.cpp | grep kpipe.cpp | wc -l 2>&1", "r"));

//		KString sCurrentLine;

//		bool output = pipe.getline(sCurrentLine, 4096);
//		//std::cout << "output is: " << output << std::endl;
//		CHECK(output);
//		REQUIRE("1\n" == sCurrentLine);


//		REQUIRE(0 == pipe.Close());

//		INFO("normal_open_close_test::Done:");
//	}

//	SECTION("KPipe  get_errno_should_return_zero")
//	{
//		INFO("get_pid_should_not_return_zero::Start:");

//		KPIPE   pipe;
//		kpipe_testKillDelayTask();

//		// open the pipe
//		CHECK(pipe.Open(KPipeDelayCommand(10, KPIPE_DELAY).c_str(), "r"));
//		CHECK(0 == pipe.getErrno());

//		kpipe_testKillDelayTask();

//		INFO("get_pid_should_not_return_zero::Done:");

//	} // get_pid_should_not_return_zero

//	SECTION("KPipe  check_is_running")
//	{
//		INFO("check_is_running::Start:");

//		KPIPE pipe;

//		CHECK(pipe.Open("usleep 2", "r"));

//		CHECK(pipe.isOpen());


//		int iLoopCount = 10;
//		while ((true == pipe.isOpen()) && (0 < --iLoopCount))
//		{
//			usleep(1);
//		}

//		CHECK(pipe.isOpen());

//		CHECK(0 == pipe.Close());
//		CHECK(-1 == pipe.Close());

//		INFO("check_is_running::Done:");

//	} // check_is_running

//	SECTION("KPipe  is_running_return_false_if_pipe_was_never_open")
//	{
//		INFO("KPipe is_running_return_false_if_pipe_is_not_open::Start:");

//		KPIPE pipe;
//		CHECK_FALSE(pipe.isOpen());

//		INFO("KPipe is_running_return_false_if_pipe_is_not_open::Done:");

//	} // is_running_return_false_if_pipe_is_not_open

//	SECTION("KPipe  cannot_readline_of_bad_pipe")
//	{
//		INFO("KPipe cannot_readline_of_bad_pipe::Start:");

//		KPIPE pipe;
//		CHECK_FALSE(pipe.Open("echo rdoanm txet > /tmp/tmp.file", "t"));
//		CHECK_FALSE(pipe.isOpen());
//		KString outBuff;
//		CHECK_FALSE(pipe.getline(outBuff));

//		INFO("KPipe cannot_readline_of_bad_pipe::Done:");

//	} // cannot_readline_of_bad_pipe

//	SECTION("KPipe opening_pipe_in_suspicious_mode")
//	{
//		INFO("KPipe opening_pipe_in_suspicious_mode::Start:");

//		KPIPE pipe;
//		CHECK_FALSE(pipe.isOpen());
//		KString outBuff;
//		CHECK_FALSE(pipe.getline(outBuff));

//		INFO("KPipe opening_pipe_in_suspicious_mode::Done:");

//	} // cannot_readline_of_bad_pipe

//	SECTION("KPipe  write_pipe")
//	{
//		INFO("KPipe  write_pipe::Start:");

//		KPIPE pipe;

//		//CHECK(pipe.Open("echo rdoanm txet > /tmp/tmp.file && cat /tmp/tmp.file > /dev/null", "w"));
//		CHECK(pipe.Open("echo rdoanm txet > /tmp/tmp.file", "w"));

//		CHECK(pipe.isOpen());
//		CHECK(0 == pipe.Close());

//		INFO("KPipe  write_pipe::Done:");

//	} // write_pipe

//	SECTION("KPipe  cannot_read_write_pipe")
//	{
//		INFO("KPipe  cannot_read_write_pipe::Start:");

//		KPIPE pipe;

//		CHECK_FALSE(pipe.Open("echo rdoanm txet > /tmp/tmp.file && cat /tmp/tmp.file > /dev/null", "rw"));
//		//CHECK(pipe.Open("echo rdoanm txet > /tmp/tmp.file", "w"));

//		CHECK_FALSE(pipe.isOpen());

//		INFO("KPipe  cannot_read_write_pipe::Done:");

//	} // cannot_read_write_pipe

//	SECTION("KPipe  using_file_star_operator")
//	{
//		INFO("KPipe  using_file_star_operator::Start:");

//		KPIPE pipe;

//		CHECK(pipe.Open("echo rdoanm txet > /tmp/tmp.file && cat /tmp/tmp.file > /dev/null", "w"));
//		//CHECK(pipe.Open("echo rdoanm txet > /tmp/tmp.file", "w"));

//		FILE* fileDesc(pipe);

//		CHECK(fileDesc);

//		CHECK(pipe.isOpen());

//		CHECK(0 == pipe.Close());

//		INFO("KPipe  using_file_star_operator::Done:");

//	} // using_file_star_operator

//	SECTION("KPipe Iterator Test")
//	{
//		KPIPE   pipe;
//		//KString sCurlCMD = "echo 'random text asdfjkl;asdfjkl;\nqwerty\nuoip\nzxcvbnm,zxcvbnm,\n\n' > /tmp/tmp.file && cat /tmp/tmp.file 2> /dev/null";
//		KString sCurlCMD = "echo 'random text asdfjkl;asdfjkl; qwerty uoip zxcvbnm,zxcvbnm,  ' > /tmp/tmp.file && cat /tmp/tmp.file 2> /dev/null";
//		CHECK(pipe.Open(sCurlCMD, "r"));

//		KString sCurrentLine;
//		KString output;
//		//for (auto iter : pipe.begin())
//		for (KStreamRW& iter = pipe.begin(); iter != pipe.end(); iter++)
//		{
//			output = output + *iter;
//			//std::cout << *iter ;
//		}
//		//std::cout << output << std::endl;
//		CHECK_FALSE(output.empty());
//		//CHECK(output.length() == 343);
//		CHECK(output.length() == 120);

//		CHECK(pipe.Close() == 0);
//	} // Iterator Test

//	SECTION("KPipe Curl Iterator Test")
//	{
//		KPIPE   pipe;
//		KString sCurlCMD = "curl -i www.google.com 2> /dev/null";
//		CHECK(pipe.Open(sCurlCMD, "r"));

//		KString sCurrentLine;
//		KString output;
//		//for (auto iter : pipe.begin())
//		for (KStreamRW& iter = pipe.begin(); iter != pipe.end(); iter++)
//		{
//			output = output + *iter;
//			//std::cout << *iter ;
//		}
//		//std::cout << output << std::endl;
//		CHECK_FALSE(output.empty());

//		CHECK(pipe.Close() == 0);
//	} // Curl Iterator Test

//}
