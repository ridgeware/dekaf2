#include "catch.hpp"

#include <dekaf2/io/pipes/kinpipe.h>

#ifdef DEKAF2_HAS_PIPES

#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/system/os/ksystem.h>

#include <iostream>
#include <ostream>
#include <stdio.h>
#ifdef DEKAF2_IS_UNIX
	#include <fcntl.h>
#endif

#define KInPipeCleanup 1

using namespace dekaf2;

#ifdef DEKAF2_IS_UNIX

namespace {

KTempDir TempDir;

// counts the open file descriptors of this process
std::size_t CountOpenFileDescriptors()
{
	std::size_t iCount { 0 };

	for (int fd = 0; fd < 1024; ++fd)
	{
		if (::fcntl(fd, F_GETFD) != -1)
		{
			++iCount;
		}
	}

	return iCount;
}

} // end of anonymous namespace

TEST_CASE("KInPipe")
{
	SECTION("KInPipe environment for the child")
	{
		KInPipe pipe("echo $DEKAF2_PIPE_TEST", "/bin/sh", {{ "DEKAF2_PIPE_TEST", "set for the child" }});
		CHECK ( pipe.is_open() );
		KString sLine;
		CHECK ( pipe.ReadLine(sLine) );
		CHECK ( sLine == "set for the child" );
		CHECK ( 0 == pipe.Close() );
	}

	SECTION("KInPipe closes its pipe also after the child has ended")
	{
		auto iOpenFileDescriptors = CountOpenFileDescriptors();

		for (int i = 0; i < 20; ++i)
		{
			KInPipe pipe("true");
			REQUIRE ( pipe.is_open() );

			// IsRunning() reaps the child once it has ended
			while (pipe.IsRunning())
			{
				kSleep(chrono::milliseconds(1));
			}

			CHECK ( pipe.Close() == 0 );
		}

		CHECK ( CountOpenFileDescriptors() <= iOpenFileDescriptors );
	}

	SECTION("KInPipe normal Open and Close")
	{
		KInPipe pipe;
		pipe.SetReaderTrim("");

		CHECK(pipe.Open(kFormat("/bin/sh -c \"echo 'some random datum' > {}/kinpipetest.file\"", TempDir.Name())));
		pipe.Wait(chrono::seconds(1));
		CHECK_FALSE(pipe.IsRunning());
		CHECK(0 == pipe.Close());
		CHECK(pipe.Open(kFormat("ls -al {}/kinpipetest.file | grep kinpipetest | wc -l", TempDir.Name()), "/bin/sh"));

		KString sCurrentLine;

		CHECK(pipe.IsRunning());
		bool output = pipe.ReadLine(sCurrentLine);
		sCurrentLine.TrimLeft();
		CHECK(output);
		CHECK("1\n" == sCurrentLine);
		CHECK(pipe.is_open());
		CHECK(0 == pipe.Close());
	}

	SECTION("KInPipe argument vector")
	{
		// the vector goes to the child as is - whitespace and quotes are no delimiters
		KInPipe pipe({ "echo", "a b", "c'd\"e", "-n" });
		CHECK ( pipe.is_open() );
		KString sLine;
		CHECK ( pipe.ReadLine(sLine) );
		CHECK ( sLine == "a b c'd\"e -n" );
		CHECK ( 0 == pipe.Close() );

		// an empty vector cannot be executed
		KInPipe pipe2;
		CHECK ( pipe2.Open(std::vector<KString>{}) == false );
	}

	SECTION("KInPipe fail_to_open")
	{
		KInPipe pipe;
		CHECK_FALSE(pipe.Open(""));
		CHECK_FALSE(pipe.IsRunning());
		CHECK(EINVAL == pipe.Close());
	}

#if KInPipeCleanup
	SECTION("KInPipe cleanup test")
	{
		KInPipe pipe;
		pipe.SetReaderTrim("");
		KString sCurrentLine;

		// Double check test files are there
		CHECK(pipe.Open(kFormat("/bin/sh -c \"ls {}/ | wc -l\"", TempDir.Name())));
		CHECK(pipe.is_open());
		CHECK(pipe.IsRunning());
		bool output = pipe.ReadLine(sCurrentLine);
		sCurrentLine.TrimLeft();
		CHECK(output);
		CHECK("1\n" == sCurrentLine);
		CHECK(0 == pipe.Close());
		CHECK_FALSE(pipe.IsRunning());

		// Remove test files
		CHECK(pipe.Open(kFormat("/bin/sh -c \"rm -rf {}/\"", TempDir.Name())));
		CHECK(pipe.is_open());
		CHECK(pipe.IsRunning());
		CHECK(0 == pipe.Close());
		CHECK_FALSE(pipe.IsRunning());

		// Double check they're gone
		CHECK(pipe.Open(kFormat("/bin/sh -c \"ls {}/ 2>/dev/null | wc -l\"", TempDir.Name())));
		CHECK(pipe.is_open());
		CHECK(pipe.IsRunning());
		output = pipe.ReadLine(sCurrentLine);
		sCurrentLine.TrimLeft();
		CHECK(output);
		CHECK("0\n" == sCurrentLine);
		CHECK(0 == pipe.Close());
		CHECK_FALSE(pipe.IsRunning());
	}
#endif

}

#endif // DEKAF2_IS_UNIX

#ifdef DEKAF2_IS_WINDOWS

TEST_CASE("KInPipe Windows")
{
	SECTION("output of a program")
	{
		KInPipe pipe("cmd.exe /d /c echo some text");
		REQUIRE ( pipe.is_open() );
		// binary, unlike KInShell: the line end of cmd.exe stays CRLF
		CHECK ( pipe.ReadRemaining() == "some text\r\n" );
		CHECK ( pipe.Close() == 0 );
	}

	SECTION("exit code")
	{
		KInPipe pipe("cmd.exe /d /c exit 3");
		REQUIRE ( pipe.is_open() );
		CHECK ( pipe.Close() == 3 );
	}

	SECTION("argument vector")
	{
		// the argument with the blank reaches cmd.exe in quotes, and echo prints them
		KInPipe pipe({ "cmd.exe", "/d", "/c", "echo", "a b" });
		REQUIRE ( pipe.is_open() );
		CHECK ( pipe.ReadRemaining() == "\"a b\"\r\n" );
		CHECK ( pipe.Close() == 0 );
	}

	SECTION("with the command interpreter")
	{
		KInPipe pipe("echo one& echo two", "/bin/sh");
		REQUIRE ( pipe.is_open() );
		CHECK ( pipe.ReadRemaining() == "one\r\ntwo\r\n" );
		CHECK ( pipe.Close() == 0 );
	}

	SECTION("environment for the child")
	{
		KInPipe pipe("echo [%DEKAF2_PIPE_TEST%]", "/bin/sh", {{ "DEKAF2_PIPE_TEST", "set for the child" }});
		REQUIRE ( pipe.is_open() );
		CHECK ( pipe.ReadRemaining() == "[set for the child]\r\n" );
		CHECK ( pipe.Close() == 0 );
	}

	SECTION("missing program")
	{
		KInPipe pipe;
		CHECK ( pipe.Open("dekaf2_this_program_does_not_exist.exe") == false );
		CHECK ( pipe.GetErrno() == DEKAF2_POPEN_COMMAND_NOT_FOUND );
		CHECK ( pipe.IsRunning() == false );
	}

	SECTION("empty command")
	{
		KInPipe pipe;
		CHECK ( pipe.Open("") == false );
		CHECK ( pipe.Close() == EINVAL );
	}

	SECTION("Wait() and IsRunning()")
	{
		KInPipe pipe("cmd.exe /d /c exit 0");
		REQUIRE ( pipe.is_open() );
		CHECK ( pipe.Wait(chrono::seconds(5)) );
		CHECK ( pipe.IsRunning() == false );
		CHECK ( pipe.GetProcessID() == 0 );
		CHECK ( pipe.Close() == 0 );
	}

	SECTION("Kill() terminates the child together with the processes it started")
	{
		// ping waits about one second between its echo requests
		KInPipe pipe("ping -n 30 127.0.0.1 > nul", "/bin/sh");
		REQUIRE ( pipe.is_open() );
		CHECK   ( pipe.IsRunning() );

		KStopTime Timer;
		CHECK ( pipe.Kill(chrono::seconds(5)) );
		CHECK ( Timer.elapsed().milliseconds().count() < 5000 );
		CHECK ( pipe.GetExitCode() == -1 );
		CHECK ( pipe.IsRunning() == false );
	}
}

#endif // DEKAF2_IS_WINDOWS

#endif
