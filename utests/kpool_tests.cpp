#include "catch.hpp"

#include <dekaf2/kpool.h>
#include <dekaf2/kstring.h>

using namespace dekaf2;

struct MyType
{
	KString sString;
	uint32_t iPopped { 0 };
	uint32_t iPushed { 0 };
};

class MyControl : public KPoolControl<MyType>
{
public:

	void Popped(MyType* value, bool bIsNew)
	{
		++value->iPopped;
	}

	void Pushed(MyType* value)
	{
		++value->iPushed;
	}
};

TEST_CASE("KPool")
{
	SECTION("Unique")
	{
		MyControl Control;
		KPool<MyType> Pool(Control);

		CHECK ( Pool.empty()     );
		CHECK ( Pool.size() == 0 );
		CHECK ( Pool.used() == 0 );

		{
			auto p = Pool.get();
			CHECK ( Pool.size() == 1 );
			CHECK ( Pool.used() == 1 );
			p->sString = "hello world";
			// KPool internally uses lambda deleters, because they reduce to one single
			// pointer added to the type. Using std::function would add 7 words to the
			// size of a KPool<>::unique_ptr. We want to check that this really worked.
			CHECK ( sizeof(p) == sizeof(void*) * 2 );
			// It is important (and difficult to code) that KPool<type>::unique_ptr
			// returns the same type as KPool<type>.get(). Verify it worked..
			CHECK ( (std::is_same<decltype(p), KPool<MyType>::unique_ptr>::value) == true );
			CHECK ( p->iPopped == 1 );
			CHECK ( p->iPushed == 0 );
		}

		CHECK ( Pool.empty() == false );
		CHECK ( Pool.size()  == 1     );
		CHECK ( Pool.used()  == 0     );

		{
			auto p = Pool.get();
			CHECK ( Pool.size()  == 1     );
			CHECK ( Pool.used()  == 1     );
			CHECK ( p->sString == "hello world" );
			CHECK ( p->iPopped == 2 );
			CHECK ( p->iPushed == 1 );
		}
	}

	SECTION("Shared")
	{
		KPool<MyType> Pool;

		CHECK ( Pool.empty()     );
		CHECK ( Pool.size() == 0 );

		{
			KPool<MyType>::shared_ptr p = Pool.get();
			CHECK ( sizeof(p) == sizeof(void*) * 2 );
			CHECK ( (std::is_same<decltype(p), KPool<MyType>::shared_ptr>::value) == true );
			CHECK ( (std::is_same<decltype(p), std::shared_ptr<MyType>>::value) == true );
			p->sString = "hello world";
		}

		CHECK ( Pool.empty() == false );
		CHECK ( Pool.size()  == 1     );

		{
			auto p = Pool.get();
			CHECK ( p->sString == "hello world" );
		}
	}

	SECTION("Shared and Shared access")
	{
		MyControl Control;
		KSharedPool<MyType> Pool(Control);

		CHECK ( Pool.empty()     );
		CHECK ( Pool.size() == 0 );

		{
			KPool<MyType>::shared_ptr p = Pool.get();
			p->sString = "hello world";
			CHECK ( p->iPopped == 1 );
			CHECK ( p->iPushed == 0 );
		}

		CHECK ( Pool.empty() == false );
		CHECK ( Pool.size()  == 1     );

		{
			KPool<MyType>::shared_ptr p = Pool.get();
			CHECK ( p->sString == "hello world" );
			CHECK ( p->iPopped == 2 );
			CHECK ( p->iPushed == 1 );
		}
	}

}
