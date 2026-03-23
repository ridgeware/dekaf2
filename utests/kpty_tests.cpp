#include "catch.hpp"
#include <dekaf2/kpty.h>

#ifdef DEKAF2_HAS_PIPES

#include <dekaf2/kstring.h>

using namespace dekaf2;

TEST_CASE("KPTY")
{

	SECTION("NoLogin shell - echo command")
	{
		KPTY pty;
		CHECK_FALSE(pty.IsRunning());

		CHECK(pty.Open(KBasePTY::NoLogin, "/bin/sh", std::chrono::seconds(5)));
		CHECK(pty.IsRunning());
		CHECK(pty.is_open());

		// send a command
		pty << "echo hello_from_pty\n" << std::flush;

		// read lines until we find our output (skip shell prompt/echo)
		KString sLine;
		bool bFound = false;

		for (int i = 0; i < 20; ++i)
		{
			if (!pty.ReadLine(sLine))
			{
				break;
			}

			if (sLine.find("hello_from_pty") != KString::npos)
			{
				bFound = true;
				break;
			}
		}

		CHECK(bFound);

		// send exit
		pty << "exit\n" << std::flush;

		auto iExit = pty.Close(5000);
		CHECK_FALSE(pty.IsRunning());
		CHECK(iExit == 0);
	}

	SECTION("NoLogin shell - default shell")
	{
		KPTY pty(KBasePTY::NoLogin);
		CHECK(pty.IsRunning());
		CHECK(pty.is_open());

		pty << "exit\n" << std::flush;

		// give the shell time to process exit
		pty.Wait(2000);

		auto iExit = pty.Close(5000);
		CHECK_FALSE(pty.IsRunning());
		// exit code may be non-zero if default shell rc files fail in a raw PTY
		CHECK(iExit >= 0);
	}

	SECTION("SetWindowSize")
	{
		KPTY pty(KBasePTY::NoLogin, "/bin/sh", std::chrono::seconds(5));
		CHECK(pty.IsRunning());

		CHECK(pty.SetWindowSize(40, 120));

		pty << "exit\n" << std::flush;
		pty.Close(5000);
	}

	SECTION("Kill running process")
	{
		KPTY pty(KBasePTY::NoLogin, "/bin/sh", std::chrono::seconds(5));
		CHECK(pty.IsRunning());

		CHECK(pty.Kill(2000));
		CHECK_FALSE(pty.IsRunning());
	}

	SECTION("Read timeout")
	{
		KPTY pty;
		// very short timeout
		CHECK(pty.Open(KBasePTY::NoLogin, "/bin/sh", std::chrono::milliseconds(100)));
		CHECK(pty.IsRunning());

		// consume any initial prompt output
		KString sLine;
		while (pty.ReadLine(sLine)) {}

		// clear stream state after timeout
		pty.clear();

		// now there should be no more output - the next read should timeout
		CHECK_FALSE(pty.ReadLine(sLine));

		pty << "exit\n" << std::flush;
		pty.Close(5000);
	}

}

#endif
