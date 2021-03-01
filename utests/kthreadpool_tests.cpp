#include "catch.hpp"

#include <dekaf2/kthreadpool.h>
#include <dekaf2/kstring.h>

using namespace dekaf2;

TEST_CASE("KThreadPool")
{
	KThreadPool Queue(0);

	struct MyClass
	{
		MyClass(int i) : m_i(i) {}
		int Add(int i) const { return m_i + i; }
		void Inc(int i) { m_i += i; }
		void Dec() { --m_i; }
		std::atomic_int m_i;
	};

	int i1 = 0;
	int i2 = 0;

	auto future1 = Queue.push([&i1]()
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(200));
		i1 = 1;
	});

	MyClass C(10);

	auto future3 = Queue.push(&MyClass::Inc, &C, 3);
	auto future4 = Queue.push(&MyClass::Dec, &C);

	auto w1 = future1.wait_for(std::chrono::seconds(2));
	auto w3 = future3.wait_for(std::chrono::seconds(2));
	auto w4 = future4.wait_for(std::chrono::seconds(2));

	CHECK ( w1 == std::future_status::ready   );
	CHECK ( w3 == std::future_status::ready   );
	CHECK ( w4 == std::future_status::ready   );

	CHECK ( i1 == 1 );
	CHECK ( future1.valid() );

#ifndef DEKAF2_IS_WINDOWS
	// VS 2017 has issues compiling this - it is clearly
	// not an implementation problem of dekaf2, therefore
	// we simply drop the test for windows..
	auto future2 = Queue.push([&i2]() -> KString
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(200));
		i2 = 2;
		return "done";
	});

	auto w2 = future2.wait_for(std::chrono::seconds(2));
	auto future5 = Queue.push(&MyClass::Add, &C, 5);
	auto w5 = future5.wait_for(std::chrono::seconds(2));

	CHECK ( w2 == std::future_status::ready   );
	CHECK ( w5 == std::future_status::ready   );
	CHECK ( i2 == 2 );
	CHECK ( future2.valid() );
	CHECK ( future5.valid() );
	CHECK ( future2.get() == "done" );
	CHECK ( future5.get() == 17 );
#endif

}
