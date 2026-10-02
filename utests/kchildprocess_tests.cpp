#include "catch.hpp"

#include <dekaf2/system/process/kchildprocess.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/io/readwrite/kwriter.h>
#include <dekaf2/core/init/dekaf2.h>
#include <dekaf2/core/logging/klog.h>
#include <dekaf2/system/os/ksystem.h>
#include <dekaf2/system/os/ksignals.h>
#include <dekaf2/system/process/kinshell.h>

#ifndef DEKAF2_IS_WINDOWS

#include <signal.h>
#include <unistd.h>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#ifdef DEKAF2_IS_LINUX
	#include <dirent.h>
#endif

using namespace dekaf2;

int chtest1(int argc, char** argv)
{
	kDebug(1, "hello parent");
	return 7;
}

TEST_CASE("KChildProcess")
{
	kDebug(1, "I am parent");
	KChildProcess Child;
	CHECK ( Child.Fork(chtest1) );
	CHECK ( Child.Join()        );
	kDebug(1, "child is back home");
	CHECK ( Child.GetExitStatus() == 7 );
}

int chtest2(int argc, char** argv)
{
	// ends long before the timeout of the parent
	kSleep(chrono::milliseconds(50));
	return 3;
}

int chtest3(int argc, char** argv)
{
	// runs longer than the timeouts of the parent
	kSleep(chrono::seconds(1));
	return 4;
}

TEST_CASE("KChildProcess Join with timeout")
{
	SECTION("child ends before the timeout")
	{
		KChildProcess Child;
		REQUIRE ( Child.Fork(chtest2) );
		// waits for the child, and does not return after the first poll
		CHECK   ( Child.Join(chrono::seconds(5)) );
		CHECK   ( Child.GetExitStatus() == 3 );
		CHECK   ( Child.IsStarted() == false );
	}

	SECTION("child runs longer than the timeout")
	{
		KChildProcess Child;
		REQUIRE ( Child.Fork(chtest3) );

		KStopTime Timer;

		// a timeout shorter than the first poll returns after that poll,
		// and does not wait for the end of the child
		CHECK ( Child.Join(chrono::microseconds(1)) == false );
		CHECK ( Timer.elapsedAndClear().milliseconds().count() < 500 );

		// waits for the timeout, and does not return after the first poll
		CHECK ( Child.Join(chrono::milliseconds(100)) == false );
		auto iMilliseconds = Timer.elapsed().milliseconds().count();
		CHECK ( iMilliseconds >= 100 );
		CHECK ( iMilliseconds <  800 );
		CHECK ( Child.IsStarted() );

		// now wait for the end of the child
		CHECK ( Child.Join() );
		CHECK ( Child.GetExitStatus() == 4 );
	}

	SECTION("no child")
	{
		KChildProcess Child;
		// fails before waitpid(), which would wait for any child of the process group
		CHECK ( Child.Join(chrono::milliseconds(10)) == false );
		CHECK ( Child.GetLastError() == "no child started" );
	}
}

TEST_CASE("KChildProcess Stop and Kill")
{
	SECTION("Stop() reaches a started program")
	{
		// dekaf2 blocks all signals in its threads - the started program must not
		// inherit that signal mask
		KChildProcess Child;
		REQUIRE ( Child.Start("sleep 3") );
		CHECK   ( Child.Stop(chrono::seconds(2)) );
		CHECK   ( Child.GetExitSignal() == SIGTERM );
	}

	SECTION("Kill() ends a forked child")
	{
		KChildProcess Child;
		REQUIRE ( Child.Fork(chtest3) );
		CHECK   ( Child.Kill() );
		CHECK   ( Child.IsStarted() == false );
	}

	SECTION("Kill() ends a child that ignores SIGTERM")
	{
		// an ignored signal stays ignored through fork() and exec(), so the
		// started program ignores SIGTERM right from its start
		struct sigaction Ignore {};
		struct sigaction Previous {};
		Ignore.sa_handler = SIG_IGN;
		sigemptyset(&Ignore.sa_mask);
		::sigaction(SIGTERM, &Ignore, &Previous);

		KChildProcess Child;
		auto bStarted = Child.Start("sleep 3");

		::sigaction(SIGTERM, &Previous, nullptr);

		REQUIRE ( bStarted );

		KStopTime Timer;
		CHECK ( Child.Kill(chrono::milliseconds(100)) );
		auto iMilliseconds = Timer.elapsed().milliseconds().count();
		CHECK ( Child.GetExitSignal() == SIGKILL );
		CHECK ( iMilliseconds >= 100 );
		CHECK ( iMilliseconds <  2000 );
	}
}

namespace {

// returns true while a process of the group runs. A zombie does not count: it has
// ended, and waits for its parent to reap it. A grandchild whose parent ended gets
// the PID 1 of its namespace as parent - and in a container that runs the tests
// as PID 1, without an init, nobody reaps it, and it stays a member of the group.
bool ProcessGroupRuns(pid_t pgid)
{
	if (::kill(-pgid, 0) != 0 && errno == ESRCH)
	{
		return false;
	}

#ifdef DEKAF2_IS_LINUX
	if (auto* pDir = ::opendir("/proc"))
	{
		bool bRuns { false };

		while (auto* pEntry = ::readdir(pDir))
		{
			if (pEntry->d_name[0] < '1' || pEntry->d_name[0] > '9')
			{
				continue;
			}

			std::ifstream File(std::string("/proc/") + pEntry->d_name + "/stat");
			std::string sStat;
			std::getline(File, sStat);

			// "pid (comm) state ppid pgrp ..." - comm may contain blanks and parentheses
			auto iPos = sStat.rfind(')');

			if (iPos == std::string::npos || iPos + 2 > sStat.size())
			{
				continue;
			}

			std::istringstream Fields(sStat.substr(iPos + 2));
			std::string sState, sParent, sGroup;
			Fields >> sState >> sParent >> sGroup;

			if (std::atoi(sGroup.c_str()) == pgid && sState != "Z" && sState != "X")
			{
				bRuns = true;
				break;
			}
		}

		::closedir(pDir);

		return bRuns;
	}
#endif

	return true;
}

// waits for the end of all processes of a process group
bool ProcessGroupEnds(pid_t pgid, KDuration Timeout)
{
	KStopTime Timer;

	for (;;)
	{
		if (!ProcessGroupRuns(pgid))
		{
			return true;
		}

		if (Timer.elapsed() > Timeout)
		{
			// clean up after a failed test
			::kill(-pgid, SIGKILL);
			return false;
		}

		kSleep(chrono::milliseconds(10));
	}
}

} // end of anonymous namespace

TEST_CASE("KChildProcess process groups")
{
	SECTION("choice of the group")
	{
		// an own group only without a controlling terminal, like for a service
		CHECK ( KProcessGroup::UseOwn(KProcessGroup::Auto)   == !kHasControllingTerminal() );
		CHECK ( KProcessGroup::UseOwn(KProcessGroup::Own)    == true  );
		CHECK ( KProcessGroup::UseOwn(KProcessGroup::Shared) == false );
		// the utests run with KInit(true), which forwards the signals of the terminal
		CHECK ( KSignals::HasHandlerThread() );
	}

	SECTION("Stop() reaches the children of a shell")
	{
		KChildProcess Child;
		Child.SetProcessGroup(KProcessGroup::Own);
		// the shell runs sleep as its child, and waits for it
		REQUIRE ( Child.Start("sh -c 'sleep 30; true'") );

		auto pgid = Child.GetChildPID();
		CHECK ( ::getpgid(pgid) == pgid );

		// give the shell the time to start sleep
		kSleep(chrono::milliseconds(200));

		CHECK ( Child.Stop(chrono::seconds(2)) );
		CHECK ( ProcessGroupEnds(pgid, chrono::seconds(2)) );
	}

	SECTION("KInShell with an own process group")
	{
		KInShell Shell;
		Shell.SetProcessGroup(KProcessGroup::Own);
		REQUIRE ( Shell.Open("sleep 30; true") );

		auto pgid = Shell.GetProcessID();
		CHECK ( ::getpgid(pgid) == pgid );

		kSleep(chrono::milliseconds(200));

		CHECK ( Shell.Kill(chrono::milliseconds(500)) );
		CHECK ( ProcessGroupEnds(pgid, chrono::seconds(2)) );
	}

	SECTION("forwarded signals of the terminal")
	{
		KChildProcess Child;
		Child.SetProcessGroup(KProcessGroup::Own);
		REQUIRE ( Child.Start("sleep 30") );

		auto pgid = Child.GetChildPID();

		// with a controlling terminal Start() has registered the group already
		KSignals::AddForwardedProcessGroup(pgid);

		// SIGTERM is no signal of the terminal, it is not forwarded
		KSignals::ForwardToProcessGroups(SIGTERM);
		CHECK ( Child.Join(chrono::milliseconds(300)) == false );

		KSignals::ForwardToProcessGroups(SIGINT);
		CHECK ( Child.Join(chrono::seconds(2)) );
		CHECK ( Child.GetExitSignal() == SIGINT );
	}
}

#endif

#ifdef DEKAF2_IS_WINDOWS

#include <dekaf2/system/filesystem/kfilesystem.h>
#include <dekaf2/core/strings/kutf.h>
#include <windows.h>
#include <shellapi.h>
#ifdef _MSC_VER
	#pragma comment(lib, "shell32.lib")
#endif

using namespace dekaf2;

namespace {

// splits a command line into arguments, as the C runtime of a program does
std::vector<KString> SplitCommandLine(KStringView sCommandLine)
{
	std::vector<KString> Args;

	int  iCount { 0 };
	auto wsCommandLine = kutf::Convert<std::wstring>(sCommandLine);

	if (auto** pArgs = ::CommandLineToArgvW(wsCommandLine.c_str(), &iCount))
	{
		for (int i = 0; i < iCount; ++i)
		{
			Args.push_back(kutf::Convert<KString>(std::wstring(pArgs[i])));
		}

		::LocalFree(pArgs);
	}

	return Args;
}

// waits up to five seconds for a file
bool FileAppears(KStringViewZ sFile)
{
	for (int i = 0; i < 500; ++i)
	{
		if (kFileExists(sFile))
		{
			return true;
		}

		kSleep(chrono::milliseconds(10));
	}

	return false;
}

} // end of anonymous namespace

TEST_CASE("KChildProcess Windows")
{
	SECTION("command line for an argument vector")
	{
		std::vector<std::vector<KString>> Tests
		{
			{ "prog.exe", "simple" },
			{ "C:\\Program Files\\prog.exe", "with blank", "" },
			{ "prog.exe", "quote\"inside", "\"", "\\\"" },
			{ "prog.exe", "C:\\dir\\", "C:\\dir with blank\\", "back\\\\slashes" },
			{ "prog.exe", "tab\there", "new\nline" }
		};

		for (const auto& Args : Tests)
		{
			CHECK ( SplitCommandLine(KWindowsProcess::CommandLine(Args)) == Args );
		}
	}

	SECTION("exit status")
	{
		KChildProcess Child;
		REQUIRE ( Child.Start("cmd.exe /d /c exit 7") );
		CHECK   ( Child.IsStarted() );
		CHECK   ( Child.GetChildPID() > 0 );
		CHECK   ( Child.Join() );
		CHECK   ( Child.GetExitStatus() == 7 );
		CHECK   ( Child.GetExitSignal() == 0 );
		CHECK   ( Child.IsStarted() == false );
	}

	SECTION("Join with timeout, and Stop()")
	{
		KChildProcess Child;
		// ping waits about one second between its echo requests
		REQUIRE ( Child.Start("cmd.exe /d /c ping -n 30 127.0.0.1 > nul") );

		KStopTime Timer;
		CHECK ( Child.Join(chrono::milliseconds(100)) == false );
		CHECK ( Timer.elapsed().milliseconds().count() >= 90 );
		CHECK ( Child.IsStarted() );

		// terminates cmd.exe together with ping
		CHECK ( Child.Stop(chrono::seconds(5)) );
		CHECK ( Child.GetExitStatus() == -1 );
		CHECK ( Child.IsStarted() == false );
		CHECK ( Timer.elapsed().milliseconds().count() < 5000 );
	}

	SECTION("Kill()")
	{
		KChildProcess Child;
		REQUIRE ( Child.Start("cmd.exe /d /c ping -n 30 127.0.0.1 > nul") );
		CHECK   ( Child.Kill() );
		CHECK   ( Child.IsStarted() == false );
		CHECK   ( Child.GetExitStatus() == -1 );
	}

	SECTION("working directory")
	{
		KTempDir TempDir;
		KChildProcess Child;
		REQUIRE ( Child.Start("cmd.exe /d /c echo x> kchildprocess.txt", TempDir.Name()) );
		CHECK   ( Child.Join() );
		CHECK   ( Child.GetExitStatus() == 0 );
		CHECK   ( kFileExists(kFormat("{}\\kchildprocess.txt", TempDir.Name())) );
	}

	SECTION("missing program")
	{
		KChildProcess Child;
		CHECK ( Child.Start("dekaf2_this_program_does_not_exist.exe") == false );
		CHECK ( Child.HasError() );
		CHECK ( Child.IsStarted() == false );
	}

	SECTION("a daemonized child is no child of this process")
	{
		KTempDir TempDir;
		KChildProcess Child;
		REQUIRE ( Child.Start("cmd.exe /d /c echo x> daemon.txt", TempDir.Name(), true) );
		CHECK   ( Child.IsStarted() == false );
		CHECK   ( Child.Join() == false );
		// the daemon runs on its own
		CHECK   ( FileAppears(kFormat("{}\\daemon.txt", TempDir.Name())) );
		// give it the time to end before its directory is removed
		kSleep(chrono::milliseconds(200));
	}

	SECTION("Detach()")
	{
		KChildProcess Child;
		REQUIRE ( Child.Start("cmd.exe /d /c ping -n 3 127.0.0.1 > nul") );
		auto pid = Child.GetChildPID();
		CHECK ( Child.Detach() );
		CHECK ( Child.IsStarted() == false );

		// the child runs on, and ends on its own
		auto hProcess = ::OpenProcess(SYNCHRONIZE, FALSE, static_cast<DWORD>(pid));
		REQUIRE ( hProcess != nullptr );
		CHECK ( ::WaitForSingleObject(hProcess, 0) == WAIT_TIMEOUT );
		CHECK ( ::WaitForSingleObject(hProcess, 10000) == WAIT_OBJECT_0 );
		::CloseHandle(hProcess);
	}
}

#endif // DEKAF2_IS_WINDOWS
