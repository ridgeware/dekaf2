#include "catch.hpp"

#include <dekaf2/ksharedmemory.h>

#ifndef DEKAF2_IS_WINDOWS

#include <dekaf2/kthreadpool.h>
#include <dekaf2/kstring.h>

using namespace dekaf2;

namespace {

int string()
{
	KIPCMessages IPC(348276);

	auto sMessage = IPC.Receive();

	if (sMessage != "hello world")
	{
		return 1;
	}

	if (!IPC.Send("hello hello"))
	{
		return 1;
	}

	return 7;
}

struct MyTrivialObject
{
	std::size_t iSize;
	int uid;
	char message[100];
};

using MyTrivialIPCObject = KIPCMessagePayload<MyTrivialObject>;

int object()
{
	KIPCMessages IPC(348275);

	MyTrivialIPCObject Object;

	bool bRet = IPC.Receive(Object);

	if (Object->uid != 123)
	{
		return 2;
	}

	Object->uid = 456;

	if (!IPC.Send(Object))
	{
		return 1;
	}
	return 9;
}

} // end of anonymous namespace

TEST_CASE("KSharedMemory")
{
	SECTION("strings")
	{
		// using threads here because processes do not bode well with the catch framework
		KThreadPool Threads(1);
		auto future = Threads.push(&string);

		KIPCMessages IPC(348276);
		bool bOK = IPC.Send("hello world");
		CHECK ( bOK );

		auto w = future.wait_for(std::chrono::seconds(10));
		CHECK ( w == std::future_status::ready   );
		if (w == std::future_status::ready)
		{
			CHECK ( future.valid() );
			CHECK ( future.get() == 7 );
		}

		auto sMessage = IPC.Receive();
		CHECK ( sMessage == "hello hello" );
	}

	SECTION("objects")
	{
		KThreadPool Threads(1);
		auto future = Threads.push(&object);

		KIPCMessages IPC(348275, sizeof(MyTrivialIPCObject));
		MyTrivialIPCObject Object;
		Object->uid = 123;
		bool bOK = IPC.Send(Object);
		CHECK ( bOK );

		auto w = future.wait_for(std::chrono::seconds(10));
		CHECK ( w == std::future_status::ready   );
		if (w == std::future_status::ready)
		{
			CHECK ( future.valid() );
			CHECK ( future.get() == 9 );
		}

		bOK = IPC.Receive(Object);
		CHECK ( Object->uid == 456 );
	}
}

#endif
