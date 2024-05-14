#include "catch.hpp"

#include <dekaf2/kinshell.h>
#include <dekaf2/kstring.h>
#include <dekaf2/ksystem.h>

#ifndef DEKAF2_IS_WINDOWS

#define kprPRINT 1

using namespace dekaf2;

namespace {
KTempDir TempDir;
static size_t KPIPE_DELAY (1);
}

KString KPipeReaderDelayCommand(unsigned int depth, size_t second, const KString sMessage1 = "", const KString sMessage2 = "")
{
    return kFormat("$dekaf/utests/kpipe_delay_test.sh {} {} '{}' '{}' 2>&1",
                depth, second/40, sMessage1, sMessage2);
}

int kpipereader_testKillDelayTask()
{
    KString sCmd("pkill -f 'kpipe_delay_test' > /dev/null 2>&1");
    return std::system(sCmd.c_str());
}

TEST_CASE("KInShell")
{
    SECTION("KInShell normal Open and Close")
    {
        KInShell pipe;
        pipe.SetReaderTrim("");

        // open the pipe
        CHECK(pipe.Open(kFormat("echo some random data > {}/kinshelltest.file && ls -al {}/kinshelltest.file | grep kinshelltest | wc -l 2>&1", TempDir.Name(), TempDir.Name())));

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

        CHECK(pipe.Open("sleep 1", "/bin/sh"));

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
        KString sCurlCMD = kFormat("echo 'random text asdfjkl;asdfjkl; qwerty uoip zxcvbnm,zxcvbnm,  ' > {}/KInShelltest2.file && cat {}/KInShelltest2.file 2> /dev/null", TempDir.Name(), TempDir.Name());
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
        CHECK(pipe.Open(kFormat("echo some random data > {}/kinshelltest3.file && ls -al {}/kinshelltest3.file | grep kinshelltest3 | wc -l 2>&1", TempDir.Name(), TempDir.Name())));

        KString sCurrentLine;

        bool output = pipe.ReadRemaining(sCurrentLine);
        CHECK(output);
        sCurrentLine.TrimLeft();
        CHECK("1\n" == sCurrentLine);
        CHECK(pipe.is_open());
        CHECK(0 == pipe.Close());
    }

}

#endif // DEKAF2_IS_WINDOWS
