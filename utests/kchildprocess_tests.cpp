#include "catch.hpp"

#include <dekaf2/system/process/kchildprocess.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/io/readwrite/kwriter.h>
#include <dekaf2/core/init/dekaf2.h>
#include <dekaf2/core/logging/klog.h>
#include <dekaf2/system/os/ksystem.h>

#ifndef DEKAF2_IS_WINDOWS

#include <signal.h>

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

#endif
