#include "catch.hpp"

#include <dekaf2/kthreads.h>
#include <dekaf2/kstring.h>
#include <dekaf2/ksystem.h>

using namespace dekaf2;

namespace {

void ThreadedIntFunction(int i)
{
}

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

TEST_CASE("KThreads")
{
	{
		KThreads Threads;

		auto id1 = Threads.Add(std::thread([]()
		{
			kMilliSleep(50);
		}));

		KString sTest = "Hello";
		Foo F;

		auto id2 = Threads.Add(std::thread(&Foo::Bar, &F, sTest));

		CHECK ( Threads.empty() == false );
		CHECK ( Threads.size() == 2 );
		CHECK ( Threads.Join(id1) );
		CHECK ( Threads.empty() == false );
		CHECK ( Threads.size() == 1 );
		CHECK ( Threads.Join(id2) );
		CHECK ( Threads.empty() == true );
		CHECK ( Threads.size() == 0 );

		Threads.Create([]()
		{
			kMilliSleep(50);
		});

		Threads.Create(&Foo::Bar, &F, sTest);

		Threads.Join();

		CHECK ( Threads.empty() == true );
		CHECK ( Threads.size() == 0 );

		KString sTest2 = "Hello again";

		Threads.Add(std::thread(&ThreadedFunction, sTest2));

		Threads.Create(&ThreadedIntFunction, 1);
		Threads.Create(&ThreadedFunction, sTest2);
		Threads.Create(&ThreadedFunction, sTest2);
		Threads.Create(&ThreadedFunction, std::move(sTest2));

		Threads.Join();
	}
}
