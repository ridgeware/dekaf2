#include "catch.hpp"

#include <dekaf2/kinshell.h>
#include <dekaf2/kstring.h>
#include <dekaf2/ksystem.h>

#ifndef DEKAF2_IS_WINDOWS

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
}

TEST_CASE("KInShell")
{
    SECTION("KInShell normal Open and Close")
    {
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
	}

    SECTION("KInShell get_errno_should_return_zero")
    {
        KInShell   pipe;
        kpipereader_testKillDelayTask();

        // open the pipe
        CHECK(pipe.Open(KPipeReaderDelayCommand(10, KPIPE_DELAY).c_str()));
        CHECK(0 == pipe.GetErrno());

        kpipereader_testKillDelayTask();
    }

    SECTION("KInShell check_is_running")
    {
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
    }

    SECTION("KInShell  is_open_return_false_if_pipe_was_never_open")
    {
        KInShell pipe;
        CHECK_FALSE(pipe.is_open());
    }

    SECTION("KInShell  cannot_readline_of_bad_pipe")
    {
        KInShell pipe;
        CHECK_FALSE(pipe.is_open());
        KString outBuff;
        CHECK_FALSE(pipe.ReadLine(outBuff));
    }

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
    }


    SECTION("KInShell fail to open test")
    {
        KInShell pipe;

        CHECK_FALSE(pipe.Open(""));

        CHECK(pipe.Close() == EINVAL);
    }

    SECTION("KInShell ReadAll")
    {
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
    }

}

#endif // DEKAF2_IS_WINDOWS
