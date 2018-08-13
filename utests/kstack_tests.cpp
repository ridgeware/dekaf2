#include "catch.hpp"

#include <dekaf2/kstack.h>

#include <iostream>

using namespace dekaf2;

TEST_CASE("KStack")
{
	int iEmptyValue = int();

	SECTION("Basic KStack Test (const Stack_Value& Return Version)")
	{
		KStack<int> kStack;

		int my1Int = 5, my2Int = 7, my3Int = 9, my4Int = 11, my5Int = 13, my6Int = 15;
		int subInt = 10;
		// Push tests data onto stack
		CHECK(kStack.push(my4Int));
		CHECK(kStack.push(my5Int));
		CHECK(kStack.push(my6Int));
		CHECK(kStack.push_front(my3Int));
		CHECK(kStack.push_front(my2Int));
		CHECK(kStack.push_front(my1Int));

		// Look at the top and bottom
		const int& rtop = kStack.top();
		CHECK(rtop == 15);
		const int& rbottom = kStack.front();
		CHECK(rbottom == 5);

		// Look somewhere in the middle
		const int& rmiddle = kStack.at(3);
		CHECK(rmiddle == 11);
		// Set something in the middle
		kStack[3] = subInt;
		CHECK(6 == kStack.size());
		// Check it
		CHECK_FALSE(iEmptyValue == rmiddle);
		CHECK(kStack.at(3) == subInt);
		// We're not empty yet
		CHECK_FALSE(kStack.empty());

		// Pop off the top and bottom, shrinking stack
		int popTop = kStack.pop();
		CHECK(rtop == popTop);
		CHECK(5 == kStack.size());
		CHECK_FALSE(kStack.empty());
		const int& popBottom = kStack.pop_front();
		CHECK(popBottom == rbottom);
		CHECK(4 == kStack.size());
		CHECK_FALSE(kStack.empty());

		// Clear whole stack
		kStack.clear();
		CHECK(iEmptyValue == kStack.pop());
		CHECK(iEmptyValue == kStack.top());
		CHECK(iEmptyValue == kStack.pop_front());
		CHECK(iEmptyValue == kStack.front());
		// Best be empty now
		CHECK(kStack.empty());
		CHECK(0 == kStack.size());

	}

	SECTION("Basic KStack Iterative For Loop Test ")
	{
		KStack<int> kStack;

		int my6Ints[6] = {5, 7, 9, 11, 13, 15};
		// Push test data onto stack
		CHECK(kStack.push(my6Ints[3]));
		CHECK(kStack.push(my6Ints[4]));
		CHECK(kStack.push(my6Ints[5]));
		CHECK(kStack.push_front(my6Ints[2]));
		CHECK(kStack.push_front(my6Ints[1]));
		CHECK(kStack.push_front(my6Ints[0]));

		CHECK_FALSE(kStack.empty());
		CHECK(6 == kStack.size());

		for (size_t i = 0; i < kStack.size(); ++i)
		{
			CHECK(kStack[i] == my6Ints[i]);
		}

	}

	SECTION("Basic KStack Forward Iterator Test ")
	{
		KStack<int> kStack;

		int my6Ints[6] = {5, 7, 9, 11, 13, 15};
		int subInt = 10;
		// Push test data onto stack
		CHECK(kStack.push(my6Ints[3]));
		CHECK(kStack.push(my6Ints[4]));
		CHECK(kStack.push(my6Ints[5]));
		CHECK(kStack.push_front(my6Ints[2]));
		CHECK(kStack.push_front(my6Ints[1]));
		CHECK(kStack.push_front(my6Ints[0]));

		// We're not empty yet
		CHECK_FALSE(kStack.empty());
		CHECK(6 == kStack.size());

		int i = 0;
		for (auto iter = kStack.begin(); iter != kStack.end(); ++iter)
		{
			CHECK(*iter == my6Ints[i]);
			++i;
		}

	}

	SECTION("Basic KStack Forward Const Iterator Test ")
	{
		KStack<int> kStack;

		int my6Ints[6] = {5, 7, 9, 11, 13, 15};
		int subInt = 10;
		// Push test data onto stack
		CHECK(kStack.push(my6Ints[3]));
		CHECK(kStack.push(my6Ints[4]));
		CHECK(kStack.push(my6Ints[5]));
		CHECK(kStack.push_front(my6Ints[2]));
		CHECK(kStack.push_front(my6Ints[1]));
		CHECK(kStack.push_front(my6Ints[0]));

		// We're not empty yet
		CHECK_FALSE(kStack.empty());
		CHECK(6 == kStack.size());

		int i = 0;
		for (auto iter = kStack.cbegin(); iter != kStack.cend(); ++iter)
		{
			CHECK(*iter == my6Ints[i]);
			++i;
		}

	}

	SECTION("Basic KStack Reverse Iterator Test ")
	{
		KStack<int> kStack;

		int my6Ints[6] = {5, 7, 9, 11, 13, 15};
		int subInt = 10;
		// Push test data onto stack
		CHECK(kStack.push(my6Ints[3]));
		CHECK(kStack.push(my6Ints[4]));
		CHECK(kStack.push(my6Ints[5]));
		CHECK(kStack.push_front(my6Ints[2]));
		CHECK(kStack.push_front(my6Ints[1]));
		CHECK(kStack.push_front(my6Ints[0]));

		// We're not empty yet
		CHECK_FALSE(kStack.empty());
		CHECK(6 == kStack.size());

		int i = 5;
		for (auto iter = kStack.rbegin(); iter != kStack.rend(); ++iter)
		{
			CHECK(*iter == my6Ints[i]);
			--i;
		}

	}

	SECTION("Basic KStack Reverse Const Iterator Test ")
	{
		KStack<int> kStack;

		int my6Ints[6] = {5, 7, 9, 11, 13, 15};
		int subInt = 10;
		// Push test data onto stack
		CHECK(kStack.push(my6Ints[3]));
		CHECK(kStack.push(my6Ints[4]));
		CHECK(kStack.push(my6Ints[5]));
		CHECK(kStack.push_front(my6Ints[2]));
		CHECK(kStack.push_front(my6Ints[1]));
		CHECK(kStack.push_front(my6Ints[0]));

		// We're not empty yet
		CHECK_FALSE(kStack.empty());
		CHECK(6 == kStack.size());

		int i = 5;
		for (auto iter = kStack.crbegin(); iter != kStack.crend(); ++iter)
		{
			CHECK(*iter == my6Ints[i]);
			--i;
		}

	}

}
