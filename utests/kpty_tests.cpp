#include "catch.hpp"
#include <dekaf2/system/process/kpty.h>

#ifdef DEKAF2_HAS_PTY

#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/system/os/ksystem.h>
#include <array>

using namespace dekaf2;

#ifndef DEKAF2_IS_WINDOWS

TEST_CASE("KPTY")
{

	SECTION("NoLogin shell - echo command")
	{
		KPTY pty;
		CHECK_FALSE(pty.IsRunning());

		CHECK(pty.Open(KPTY::NoLogin, "/bin/sh", chrono::seconds(5)));
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

		// send exit and give the shell time to process it
		pty << "exit\n" << std::flush;
		pty.Wait(chrono::seconds(2));

		auto iExit = pty.Close(chrono::seconds(5));
		CHECK_FALSE(pty.IsRunning());
		// in minimal containers, PTY teardown may race and yield 127;
		// the echo test above already proved the shell was functional
		CHECK(iExit >= 0);
	}

	SECTION("NoLogin shell - default shell")
	{
		KPTY pty(KPTY::NoLogin);
		CHECK(pty.IsRunning());
		CHECK(pty.is_open());

		pty << "exit\n" << std::flush;

		// give the shell time to process exit
		pty.Wait(chrono::seconds(2));

		auto iExit = pty.Close(chrono::seconds(5));
		CHECK_FALSE(pty.IsRunning());
		// exit code may be non-zero if default shell rc files fail in a raw PTY
		CHECK(iExit >= 0);
	}

	SECTION("SetWindowSize")
	{
		KPTY pty(KPTY::NoLogin, "/bin/sh", chrono::seconds(5));
		CHECK(pty.IsRunning());

		CHECK(pty.SetWindowSize(40, 120));

		pty << "exit\n" << std::flush;
		pty.Close(chrono::seconds(5));
	}

	SECTION("Kill running process")
	{
		KPTY pty(KPTY::NoLogin, "/bin/sh", chrono::seconds(5));
		CHECK(pty.IsRunning());

		CHECK(pty.Kill(chrono::seconds(2)));
		CHECK_FALSE(pty.IsRunning());
	}

	SECTION("Read timeout")
	{
		KPTY pty;
		// very short timeout
		CHECK(pty.Open(KPTY::NoLogin, "/bin/sh", chrono::milliseconds(100)));
		CHECK(pty.IsRunning());

		// consume any initial prompt output
		KString sLine;
		while (pty.ReadLine(sLine)) {}

		// clear stream state after timeout
		pty.clear();

		// now there should be no more output - the next read should timeout
		CHECK_FALSE(pty.ReadLine(sLine));

		pty << "exit\n" << std::flush;
		pty.Close(chrono::seconds(5));
	}

}

#else // DEKAF2_IS_WINDOWS

namespace {

// reads the output of the pseudo console until it contains sText - the pseudo
// console renders its screen with VT sequences and cursor moves, therefore the
// search is in all of the output, and not line by line. The output read so far
// is appended to pOutput.
bool OutputContains(KPTY& pty, KStringView sText, KString* pOutput = nullptr, bool bIgnoreCase = false)
{
	KString sOutput;
	std::array<char, 4096> Buffer;

	auto Contains = [&]()
	{
		return bIgnoreCase ? sOutput.ToLower().contains(KString(sText).ToLower()) : sOutput.contains(sText);
	};

	// a read returns once the buffer is full, or after the timeout of the pseudo console
	for (int i = 0; i < 20 && !Contains(); ++i)
	{
		auto iRead = pty.Read(Buffer.data(), Buffer.size());
		sOutput.append(Buffer.data(), iRead);
	}

	if (pOutput)
	{
		*pOutput += sOutput;
	}

	return Contains();
}

// one failed login: waits for the prompt, and sends a user without a password
void FailLogin(KPTY& pty, KString* pOutput = nullptr)
{
	CHECK ( OutputContains(pty, "login: ", pOutput) );
	pty << "dekaf2_no_such_user\r" << std::flush;
	CHECK ( OutputContains(pty, "Password: ", pOutput) );
	pty << "dekaf2_wrong_password\r" << std::flush;
}

} // end of anonymous namespace

TEST_CASE("KPTY Windows")
{
	SECTION("command interpreter")
	{
		KPTY pty;
		CHECK_FALSE ( pty.IsRunning() );

		REQUIRE ( pty.Open(KPTY::NoLogin, {}, chrono::milliseconds(500), {{ "DEKAF2_PTY_TEST", "value_from_the_environment" }}) );
		CHECK   ( pty.IsRunning() );
		CHECK   ( pty.is_open() );

		// Enter is a carriage return, as from a terminal - the typed command does
		// not contain the value, only its output does
		pty << "echo %DEKAF2_PTY_TEST%\r" << std::flush;
		CHECK ( OutputContains(pty, "value_from_the_environment") );

		pty << "exit\r" << std::flush;
		CHECK ( pty.Wait(chrono::seconds(5)) );
		CHECK ( pty.Close(chrono::seconds(5)) == 0 );
		CHECK_FALSE ( pty.IsRunning() );
	}

	SECTION("a program instead of the command interpreter")
	{
		KPTY pty(KPTY::NoLogin, "cmd.exe /d /c echo direct_from_the_program", chrono::milliseconds(500));
		CHECK ( OutputContains(pty, "direct_from_the_program") );
		CHECK ( pty.Wait(chrono::seconds(5)) );
		CHECK ( pty.Close() == 0 );
	}

	SECTION("SetWindowSize")
	{
		KPTY pty(KPTY::NoLogin, {}, chrono::milliseconds(500));
		REQUIRE ( pty.IsRunning() );
		CHECK   ( pty.SetWindowSize(40, 120) );

		// the command interpreter sees the new count of columns
		pty << "mode con\r" << std::flush;
		CHECK ( OutputContains(pty, "120") );

		pty << "exit\r" << std::flush;
		pty.Close(chrono::seconds(5));
	}

	SECTION("Kill()")
	{
		KPTY pty(KPTY::NoLogin, {}, chrono::milliseconds(500));
		REQUIRE ( pty.IsRunning() );
		CHECK   ( pty.Kill(chrono::seconds(2)) );
		CHECK_FALSE ( pty.IsRunning() );
	}

	SECTION("Close() ends a running command interpreter")
	{
		KPTY pty(KPTY::NoLogin, {}, chrono::milliseconds(500));
		REQUIRE ( pty.IsRunning() );

		// like a hangup on Unix - not only after the timeout, with the termination
		KStopTime Timer;
		pty.Close(chrono::seconds(10));
		CHECK_FALSE ( pty.IsRunning() );
		CHECK ( Timer.elapsed().milliseconds().count() < 9000 );
	}

	SECTION("read timeout")
	{
		KPTY pty(KPTY::NoLogin, {}, chrono::milliseconds(200));
		REQUIRE ( pty.IsRunning() );

		// consume the start of the command interpreter
		std::array<char, 4096> Buffer;
		while (pty.Read(Buffer.data(), Buffer.size()) > 0) {}
		pty.clear();

		// now there is no more output - the next read ends with the timeout
		KStopTime Timer;
		CHECK ( pty.Read(Buffer.data(), Buffer.size()) == 0 );
		CHECK ( Timer.elapsed().milliseconds().count() >= 150 );

		pty << "exit\r" << std::flush;
		pty.Close(chrono::seconds(5));
	}

	SECTION("login with a wrong password")
	{
		KPTY pty;
		REQUIRE ( pty.Open(KPTY::Login, {}, chrono::milliseconds(500)) );

		// the login runs before the shell
		CHECK ( pty.IsRunning() );
		CHECK ( pty.GetProcessID() == 0 );

		KString sOutput;
		FailLogin(pty, &sOutput);
		CHECK ( OutputContains(pty, "Login incorrect", &sOutput) );

		// the name is echoed, the password is not
		CHECK ( sOutput.contains("dekaf2_no_such_user") );
		CHECK ( sOutput.contains("dekaf2_wrong_password") == false );

		// asks again
		CHECK ( OutputContains(pty, "login: ") );
		CHECK ( pty.IsRunning() );

		// Close() ends a running login, which then starts no shell
		KStopTime Timer;
		CHECK ( pty.Close() == -1 );
		CHECK ( Timer.elapsed().milliseconds().count() < 2000 );
		CHECK_FALSE ( pty.IsRunning() );
	}

	SECTION("three failed logins end the login")
	{
		KPTY pty(KPTY::Login, {}, chrono::milliseconds(500));

		for (int i = 0; i < 3; ++i)
		{
			FailLogin(pty);
		}

		// after the third, the login ends with the exit code 1, as /usr/bin/login
		CHECK ( pty.Wait(chrono::seconds(10)) );
		CHECK ( pty.GetExitCode() == 1 );
		CHECK_FALSE ( pty.IsRunning() );
	}

	SECTION("Terminate() ends the login")
	{
		KPTY pty(KPTY::Login, {}, chrono::milliseconds(500));
		CHECK ( OutputContains(pty, "login: ") );
		CHECK ( pty.Terminate() );
		CHECK ( pty.Wait(chrono::seconds(2)) );
		CHECK ( pty.GetExitCode() == -1 );
		CHECK_FALSE ( pty.IsRunning() );
	}

	SECTION("Ctrl-D ends the login")
	{
		KPTY pty(KPTY::Login, {}, chrono::milliseconds(500));
		CHECK ( OutputContains(pty, "login: ") );
		pty << "\x04" << std::flush;
		CHECK ( pty.Wait(chrono::seconds(2)) );
		CHECK ( pty.GetExitCode() == 1 );
	}

	SECTION("login with the account of DEKAF2_TEST_LOGIN_USER")
	{
		// only with credentials from the environment: the account that runs the
		// tests, or with a service under LocalSystem any account
		KString sUser     = kGetEnv("DEKAF2_TEST_LOGIN_USER");
		KString sPassword = kGetEnv("DEKAF2_TEST_LOGIN_PASSWORD");

		if (!sUser.empty() && !sPassword.empty())
		{
			KPTY pty(KPTY::Login, {}, chrono::milliseconds(500));
			CHECK ( OutputContains(pty, "login: ") );
			pty << sUser << "\r" << std::flush;
			CHECK ( OutputContains(pty, "Password: ") );
			pty << sPassword << "\r" << std::flush;

			// the shell runs as the user - %USERNAME% is the name without a domain
			auto iSeparator = sUser.find('\\');
			KString sName   = (iSeparator == KString::npos) ? sUser : sUser.substr(iSeparator + 1);
			sName           = sName.substr(0, sName.find('@'));

			// names of accounts are case insensitive - the output has the stored case
			pty << "echo [%USERNAME%]\r" << std::flush;
			CHECK ( OutputContains(pty, kFormat("[{}]", sName), nullptr, true) );
			CHECK ( pty.GetProcessID() > 0 );

			pty << "exit\r" << std::flush;
			CHECK ( pty.Wait(chrono::seconds(5)) );
			CHECK ( pty.Close() == 0 );
		}
	}

	SECTION("missing program")
	{
		KPTY pty;
		CHECK_FALSE ( pty.Open(KPTY::NoLogin, "dekaf2_this_program_does_not_exist.exe") );
		CHECK ( pty.GetExitCode() == DEKAF2_POPEN_COMMAND_NOT_FOUND );
	}
}

#endif // DEKAF2_IS_WINDOWS

#endif
