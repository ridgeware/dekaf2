#include "catch.hpp"

#include <cstring>
#include <dekaf2/ksharedref.h>
#include <dekaf2/kstring.h>
#include <dekaf2/kparallel.h>

using namespace dekaf2;

TEST_CASE("KSharedRef")
{
	SECTION("raw")
	{
		KSharedRef<KString, false> ptr1;
		CHECK ( ptr1.use_count() == 1 );
		ptr1 = KString("this is not a string");
		CHECK ( ptr1.use_count() == 1 );
		ptr1 = KString("this is a string");
		CHECK ( ptr1.use_count() == 1 );
		CHECK ( *ptr1 == "this is a string" );
		{
			auto ptr2 = KSharedRef<KString, false>("this is another string");
			ptr1 = ptr2;
		}
		CHECK ( ptr1.use_count() == 1 );
		CHECK ( *ptr1 == "this is another string" );
	}

	SECTION("Basics")
	{
		auto ptr1 = KSharedRef<KString, false>("this is not a string");
		CHECK ( ptr1.use_count() == 1 );
		ptr1 = KSharedRef<KString, false>("this is a string");
		CHECK ( ptr1.use_count() == 1 );
		auto ptr2 = ptr1;
		CHECK ( ptr1.use_count() == 2 );
		CHECK ( ptr2.use_count() == 2 );
		CHECK ( ptr1->data() == ptr2->data() );
		CHECK ( *ptr2 == "this is a string" );
		KString s2 = ptr2.get();
		CHECK ( s2 == *ptr2 );
	}

	SECTION("Multithreading")
	{
		auto ptr1 = KSharedRef<KString, true>("this is not a string");
		CHECK ( ptr1.use_count() == 1 );
		ptr1 = KSharedRef<KString, true>("this is a string");
		CHECK ( ptr1.use_count() == 1 );
		auto ptr2 = ptr1;
		CHECK ( ptr1.use_count() == 2 );
		CHECK ( ptr2.use_count() == 2 );
		CHECK ( ptr1->data() == ptr2->data() );
		CHECK ( *ptr2 == "this is a string" );
		KString s2 = ptr2.get();
		CHECK ( s2 == *ptr2 );
	}

	SECTION("MT")
	{
		auto ptr1 = KSharedRef<KString, true>("you say yes, I say no, you say stop, I say go go go");

		std::atomic_uint32_t iErrors { 0 };

		KRunThreads().Create([&ptr1,&iErrors]()
		{
			for (int i = 0; i < 1000; ++i)
			{
				auto ptr2 = ptr1;
				if (ptr2.get() != "you say yes, I say no, you say stop, I say go go go")
				{
					++iErrors;
				}
			}
		});

		CHECK ( iErrors == 0 );
	}
}
