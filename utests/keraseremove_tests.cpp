#include "catch.hpp"

#include <dekaf2/keraseremove.h>
#include <dekaf2/kjoin.h>
#include <dekaf2/kstack.h>
#include <vector>
#include <list>
#include <deque>


using namespace dekaf2;

TEST_CASE("KEraseRemove")
{
	SECTION("EraseRemoveVec")
	{
		std::vector<KString> vec { "this", "is", "a", "test" };
		kEraseRemove(vec, "a");
		CHECK ( kJoined(vec, ",") == "this,is,test" );
	}

	SECTION("EraseRemoveIfVec")
	{
		std::vector<KString> vec { "this", "is", "a", "test" };
		kEraseRemoveIf(vec, [](const KString& sWord)
		{
			return sWord == "is";
		});
		CHECK ( kJoined(vec, ",") == "this,a,test" );
	}

	SECTION("EraseRemoveList")
	{
		std::list<KString> vec { "this", "is", "a", "test" };
		kEraseRemove(vec, "a");
		CHECK ( kJoined(vec, ",") == "this,is,test" );
	}

	SECTION("EraseRemoveIfList")
	{
		std::list<KString> vec { "this", "is", "a", "test" };
		kEraseRemoveIf(vec, [](const KString& sWord)
		{
			return sWord == "is";
		});
		CHECK ( kJoined(vec, ",") == "this,a,test" );
	}

	SECTION("EraseRemoveDeque")
	{
		std::deque<KString> vec { "this", "is", "a", "test" };
		kEraseRemove(vec, "a");
		CHECK ( kJoined(vec, ",") == "this,is,test" );
	}

	SECTION("EraseRemoveIfDeque")
	{
		std::deque<KString> vec { "this", "is", "a", "test" };
		kEraseRemoveIf(vec, [](const KString& sWord)
		{
			return sWord == "is";
		});
		CHECK ( kJoined(vec, ",") == "this,a,test" );
	}
}

