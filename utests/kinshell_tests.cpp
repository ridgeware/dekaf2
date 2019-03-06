#include "catch.hpp"

#include <dekaf2/kinshell.h>
#include <dekaf2/kstring.h>
#include <dekaf2/ksystem.h>

#define kprPRINT 1

using namespace dekaf2;

static size_t KPIPE_DELAY (1);

KString KPipeReaderDelayCommand(unsigned int depth, size_t second, const KString sMessage1 = "", const KString sMessage2 = "")
{
    return fmt::format("$dekaf/utests/kpipe_delay_test.sh {} {} '{}' '{}' 2>&1",
                depth, second/40, sMessage1, sMessage2);
}

void kpipereader_testKillDelayTask()
{
    KString sCmd("pkill -f 'kpipe_delay_test' > /dev/null 2>&1");
    system(sCmd.c_str());

} // kpipe_testKillDelayTask

TEST_CASE("KInShell")
{
    SECTION("KInShell normal Open and Close")
    {
        INFO("normal_open_close_test::Start:");

        KInShell pipe;
        pipe.SetReaderTrim("");

        // open the pipe
        CHECK(pipe.Open("echo some random data > /tmp/kinshelltest.file && ls -al /tmp/kinshelltest.file | grep kinshelltest | wc -l 2>&1"));

        KString sCurrentLine;

        bool output = pipe.ReadLine(sCurrentLine);
        CHECK(output);
        sCurrentLine.TrimLeft();
        CHECK("1\n" == sCurrentLine);
        CHECK(pipe.is_open());
        CHECK(0 == pipe.Close());

        INFO("normal_open_close_test::Done:");
    } // normal open close

    SECTION("KInShell  get_errno_should_return_zero")
    {
        INFO("get_pid_should_not_return_zero::Start:");

        KInShell   pipe;
        kpipereader_testKillDelayTask();

        // open the pipe
        CHECK(pipe.Open(KPipeReaderDelayCommand(10, KPIPE_DELAY).c_str()));
        CHECK(0 == pipe.GetErrno());

        kpipereader_testKillDelayTask();

        INFO("KInShell get_pid_should_not_return_zero::Done:");

    } // get_pid_should_not_return_zero

    SECTION("KInShell  check_is_running")
    {
        INFO("KInShell check_is_running::Start:");

        KInShell pipe;

        CHECK(pipe.Open("sleep 1"));

        CHECK(pipe.is_open());


        int iLoopCount = 10;
        while ((true == pipe.is_open()) && (0 < --iLoopCount))
        {
            kMicroSleep(1);
        }

        CHECK(pipe.is_open());

        CHECK(0 == pipe.Close());
        CHECK(0 == pipe.Close());

        INFO("KInShell check_is_running::Done:");

    } // check_is_running

    SECTION("KInShell  is_open_return_false_if_pipe_was_never_open")
    {
        INFO("KInShell is_open_return_false_if_pipe_is_not_open::Start:");

        KInShell pipe;
        CHECK_FALSE(pipe.is_open());

        INFO("KInShell is_open_return_false_if_pipe_is_not_open::Done:");

    } // is_running_return_false_if_pipe_is_not_open

    SECTION("KInShell  cannot_readline_of_bad_pipe")
    {
        INFO("KInShell cannot_readline_of_bad_pipe::Start:");

        KInShell pipe;
        CHECK_FALSE(pipe.is_open());
        KString outBuff;
        CHECK_FALSE(pipe.ReadLine(outBuff));

        INFO("KInShell cannot_readline_of_bad_pipe::Done:");

    } // cannot_readline_of_bad_pipe

    SECTION("KInShell  using_file_star_operator")
    {
        INFO("KInShell  using_file_star_operator::Start:");

        KInShell pipe;

        CHECK(pipe.Open("echo random txet > /tmp/KInShelltest.file && cat /tmp/KInShelltest.file > /dev/null"));

        FILE* fileDesc(pipe);

        CHECK(fileDesc);

        CHECK(pipe.is_open());

        CHECK(0 == pipe.Close());

        INFO("KInShell  using_file_star_operator::Done:");

    } // using_file_star_operator

    SECTION("KInShell Iterator Test")
    {
        KInShell   pipe;
        pipe.SetReaderTrim("");
        KString sCurlCMD = "echo 'random text asdfjkl;asdfjkl; qwerty uoip zxcvbnm,zxcvbnm,  ' > /tmp/KInShelltest.file && cat /tmp/KInShelltest.file 2> /dev/null";
        CHECK(pipe.Open(sCurlCMD));

        KString sCurrentLine;
        KString output;
        for (auto iter = pipe.begin(); iter != pipe.end(); iter++)
        {
            output = output + *iter;
        }

        CHECK_FALSE(output.empty());
        CHECK(output.length() == 60);

        CHECK(pipe.Close() == 0);
    } // Iterator Test


    SECTION("KInShell fail to open test")
    {
        INFO("KInShell normal_open_close_test::Start:");

        KInShell pipe;

        CHECK_FALSE(pipe.Open(""));

        CHECK(pipe.Close() == 0);

        INFO("KInShell normal_open_close_test::Done:");
    } // normal open close

    SECTION("KInShell ReadAll")
    {
        INFO("ReadAll::Start:");

        KInShell pipe;
        pipe.SetReaderTrim("");

        // open the pipe
        CHECK(pipe.Open("echo some random data > /tmp/kinshelltest.file && ls -al /tmp/kinshelltest.file | grep kinshelltest | wc -l 2>&1"));

        KString sCurrentLine;

        bool output = pipe.ReadAll(sCurrentLine);
        CHECK(output);
        sCurrentLine.TrimLeft();
        CHECK("1\n" == sCurrentLine);
        CHECK(pipe.is_open());
        CHECK(0 == pipe.Close());

        INFO("normal_open_close_test::Done:");
    } // normal open close

}
