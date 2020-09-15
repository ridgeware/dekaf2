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
		KPool<MyType, false, MyControl > Pool(Control);

		CHECK ( Pool.empty()     );
		CHECK ( Pool.size() == 0 );

		{
			auto p = Pool.pop_back();
			p->sString = "hello world";
			CHECK ( p->iPopped == 1 );
			CHECK ( p->iPushed == 0 );
		}

		CHECK ( Pool.empty() == false );
		CHECK ( Pool.size()  == 1     );

		{
			auto p = Pool.pop_back();
			CHECK ( p->sString == "hello world" );
			CHECK ( p->iPopped == 2 );
			CHECK ( p->iPushed == 1 );
		}
	}

	SECTION("Shared")
	{
		KPool<MyType, true> Pool;

		CHECK ( Pool.empty()     );
		CHECK ( Pool.size() == 0 );

		{
			auto p = Pool.pop_back();
			p->sString = "hello world";
		}

		CHECK ( Pool.empty() == false );
		CHECK ( Pool.size()  == 1     );

		{
			auto p = Pool.pop_back();
			CHECK ( p->sString == "hello world" );
		}
	}

	SECTION("Shared and Shared access")
	{
		MyControl Control;
		KSharedPool<MyType, true, MyControl > Pool(Control);

		CHECK ( Pool.empty()     );
		CHECK ( Pool.size() == 0 );

		{
			auto p = Pool.pop_back();
			p->sString = "hello world";
			CHECK ( p->iPopped == 1 );
			CHECK ( p->iPushed == 0 );
		}

		CHECK ( Pool.empty() == false );
		CHECK ( Pool.size()  == 1     );

		{
			auto p = Pool.pop_back();
			CHECK ( p->sString == "hello world" );
			CHECK ( p->iPopped == 2 );
			CHECK ( p->iPushed == 1 );
		}
	}

}
