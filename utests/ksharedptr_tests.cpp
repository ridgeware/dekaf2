#include "catch.hpp"

#include <cstring>
#include <dekaf2/ksharedptr.h>
#include <dekaf2/kstring.h>
#include <dekaf2/kthreadpool.h>

using namespace dekaf2;

TEST_CASE("KSharedPtr")
{
	SECTION("raw")
	{
		KSharedPtr<KString, false, false> ptr1;
		CHECK ( ptr1.use_count() == 0 );
		ptr1.reset(new KString("this is a string"));
		CHECK ( ptr1.use_count() == 1 );
		CHECK ( *ptr1 == "this is a string" );
	}

	SECTION("Basics")
	{
		auto ptr1 = kMakeShared<KString, false, false>("this is not a string");
		CHECK ( ptr1.use_count() == 1 );
		ptr1 = kMakeShared<KString, false, false>("this is a string");
		CHECK ( ptr1.use_count() == 1 );
		auto ptr2 = ptr1;
		CHECK ( ptr1.use_count() == 2 );
		CHECK ( ptr2.use_count() == 2 );
		CHECK ( ptr1->data() == ptr2->data() );
		CHECK ( ptr2.unique() == false);
		ptr1.reset();
		CHECK ( ptr1.use_count() == 0 );
		CHECK ( ptr2.use_count() == 1 );
		CHECK ( ptr2.unique() == true );
		CHECK ( (ptr1 == nullptr) );
		CHECK ( !ptr1 );
		CHECK ( (ptr2 == true) );
		CHECK ( *ptr2 == "this is a string" );
		KString* s2 = ptr2.get();
		CHECK ( *s2 == *ptr2 );

		auto bptr = kMakeShared<bool, false>(true);
		CHECK ( *bptr == true );
	}

	SECTION("Multithreading")
	{
		auto ptr1 = kMakeShared<KString, true, true>("this is not a string");
		CHECK ( ptr1.use_count() == 1 );
		ptr1 = kMakeShared<KString>("this is a string");
		CHECK ( ptr1.use_count() == 1 );
		auto ptr2 = ptr1;
		CHECK ( ptr1.use_count() == 2 );
		CHECK ( ptr2.use_count() == 2 );
		CHECK ( ptr1->data() == ptr2->data() );
		CHECK ( ptr2.unique() == false);
		ptr1.reset();
		CHECK ( ptr1.use_count() == 0 );
		CHECK ( ptr2.use_count() == 1 );
		CHECK ( ptr2.unique() == true );
		CHECK ( (ptr1 == nullptr) );
		CHECK ( !ptr1 );
		CHECK ( (ptr2 == true) );
		CHECK ( *ptr2 == "this is a string" );
		KString* s2 = ptr2.get();
		CHECK ( *s2 == *ptr2 );

		auto bptr = kMakeShared<bool>(true);
		CHECK ( *bptr == true );
	}
}
