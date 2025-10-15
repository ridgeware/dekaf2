#include "catch.hpp"

#include <dekaf2/kthreadpool.h>
#include <dekaf2/kstring.h>

using namespace dekaf2;

TEST_CASE("KThreadPool")
{
	SECTION("basics")
	{
		KThreadPool Queue(10);

		struct MyClass
		{
			MyClass(int i) : m_i(i) {}
			int Add(int i) const { return m_i + i; }
			void Inc(int i) { m_i += i; }
			void Dec() { --m_i; }
			std::atomic<int> m_i;
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

		CHECK ( Queue.resize(5) );

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

	SECTION("forced stop")
	{
		KThreadPool Queue(10);

		std::atomic<int> iCounter { 0 };

		for (int i = 0; i < 1000; ++i)
		{
			Queue.push([&iCounter]()
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
				++iCounter;
			});
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(50));
		Queue.resize(2);

		std::this_thread::sleep_for(std::chrono::milliseconds(50));
		Queue.stop(true);

		CHECK ( Queue.n_queued() > 0 );
		CHECK ( iCounter < 1000 );

		Queue.clear();
		CHECK ( Queue.n_queued() == 0 );
	}

	SECTION("stepwise growth and shrink")
	{
		std::atomic<int> iCounter { 0 };

		{
			KThreadPool Queue(50, KThreadPool::PrestartSome, KThreadPool::ShrinkSome);

			for (int i = 0; i < 1000; ++i)
			{
				Queue.push([&iCounter]()
				{
					std::this_thread::sleep_for(std::chrono::milliseconds(100));
					++iCounter;
				});
			}

			std::this_thread::sleep_for(std::chrono::milliseconds(150));

			Queue.push([&iCounter](){ ++iCounter; });

			CHECK ( Queue.size() == 50 );
		}

		CHECK ( iCounter == 1001 );
	}
}
