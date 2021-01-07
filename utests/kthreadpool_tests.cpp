#include "catch.hpp"

#include <dekaf2/kthreadpool.h>
#include <dekaf2/kstring.h>

using namespace dekaf2;

TEST_CASE("KThreadPool")
{
	KThreadPool Queue(0);

	int i1 = 0;
	int i2 = 0;

	auto future1 = Queue.push([&i1]()
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(200));
		i1 = 1;
	});

	auto future2 = Queue.push([&i2]() -> KString
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(200));
		i2 = 2;
		return "done";
	});

	auto w1 = future1.wait_for(std::chrono::seconds(2));
	auto w2 = future2.wait_for(std::chrono::seconds(2));

	CHECK ( w1 == std::future_status::ready   );
	CHECK ( w2 == std::future_status::ready   );

	CHECK ( i1 == 1 );
	CHECK ( i2 == 2 );
	CHECK ( future1.valid() );
	CHECK ( future2.valid() );
	CHECK ( future2.get() == "done" );

}
