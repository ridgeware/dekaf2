#include "catch.hpp"

#include <dekaf2/kparallel.h>
#include <dekaf2/kstring.h>
#include <dekaf2/ksystem.h>

using namespace dekaf2;

namespace {

void ThreadedFunction(KString sTest)
{
}

struct Foo
{
	void Bar(KString sTest)
	{
	}
};

}

TEST_CASE("KParallel")
{
	KRunThreads Threads;

	auto id = Threads.Create([]()
	{
		kMilliSleep(50);
	});

	KString sTest = "Hello";
	Foo F;

	id = Threads.Create(&Foo::Bar, &F, sTest);

	CHECK ( Threads.empty() == false );
	CHECK ( Threads.size() == std::thread::hardware_concurrency() );

	Threads.Join();

	CHECK ( Threads.empty() == true );
	CHECK ( Threads.size() == 0 );

	Threads.Create(&ThreadedFunction, sTest);

	Threads.Join();
}

