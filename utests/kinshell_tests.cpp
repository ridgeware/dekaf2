#include "catch.hpp"

#include <dekaf2/system/process/kinshell.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/system/os/ksystem.h>

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
            kSleep(chrono::microseconds(1));
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

#ifdef DEKAF2_IS_WINDOWS

using namespace dekaf2;

namespace {

KString ReadOutput(KInShell& Shell)
{
	KString sOutput;
	Shell.ReadRemaining(sOutput);
	sOutput.TrimRight();
	return sOutput;
}

} // end of anonymous namespace

TEST_CASE("KInShell Windows")
{
	SECTION("environment for the child")
	{
		KInShell Shell("echo [%DEKAF2_SHELL_TEST%]", "/bin/sh", {{ "DEKAF2_SHELL_TEST", "set for the child" }});
		REQUIRE ( Shell.is_open() );
		CHECK   ( ReadOutput(Shell) == "[set for the child]" );
		CHECK   ( Shell.Close() == 0 );
	}

	SECTION("environment variable removed for the child")
	{
		kSetEnv("DEKAF2_SHELL_TEST_REMOVE", "visible");
		KInShell Shell("echo [%DEKAF2_SHELL_TEST_REMOVE%]", "/bin/sh", {{ "DEKAF2_SHELL_TEST_REMOVE", "" }});
		REQUIRE ( Shell.is_open() );
		// cmd.exe leaves the reference to an undefined variable as it is
		CHECK   ( ReadOutput(Shell) == "[%DEKAF2_SHELL_TEST_REMOVE%]" );
		CHECK   ( Shell.Close() == 0 );
		kUnsetEnv("DEKAF2_SHELL_TEST_REMOVE");
	}

	SECTION("command starting with a quote")
	{
		// cmd.exe /c alone would remove the first and the last quote of this command
		KInShell Shell(R"("%COMSPEC%" /d /c echo "quoted")");
		REQUIRE ( Shell.is_open() );
		CHECK   ( ReadOutput(Shell) == R"("quoted")" );
		CHECK   ( Shell.Close() == 0 );
	}

	SECTION("direct execution without the command interpreter")
	{
		KInShell Shell("cmd.exe /d /c echo direct", "");
		REQUIRE ( Shell.is_open() );
		CHECK   ( ReadOutput(Shell) == "direct" );
		CHECK   ( Shell.Close() == 0 );
	}

	SECTION("direct execution of a missing program")
	{
		KInShell Shell;
		CHECK ( Shell.Open("dekaf2_this_program_does_not_exist.exe", "") == false );
		CHECK ( Shell.GetErrno() == DEKAF2_POPEN_COMMAND_NOT_FOUND );
	}

	SECTION("Close() terminates a child that does not end within the timeout")
	{
		// ping waits about one second between its echo requests
		KInShell Shell("ping -n 30 127.0.0.1 > nul");
		REQUIRE ( Shell.is_open() );
		CHECK   ( Shell.IsRunning() );
		CHECK   ( Shell.GetProcessID() > 0 );

		KStopTime Timer;
		CHECK ( Shell.Close(chrono::milliseconds(200)) == -1 );
		CHECK ( Timer.elapsed().milliseconds().count() < 5000 );
		CHECK ( Shell.IsRunning() == false );
		CHECK ( Shell.GetProcessID() == 0 );
	}

	SECTION("Terminate() leaves the pipe open")
	{
		KInShell Shell("echo before & ping -n 30 127.0.0.1 > nul");
		REQUIRE ( Shell.is_open() );

		KString sLine;
		CHECK ( Shell.ReadLine(sLine) );
		sLine.TrimRight();
		CHECK ( sLine == "before" );

		KStopTime Timer;
		CHECK ( Shell.Terminate() );
		// the end of the output comes once the job with all its processes is gone
		CHECK ( ReadOutput(Shell).empty() );
		CHECK ( Shell.Close() == -1 );
		CHECK ( Timer.elapsed().milliseconds().count() < 5000 );
	}
}

#endif // DEKAF2_IS_WINDOWS
