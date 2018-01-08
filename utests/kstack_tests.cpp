#include "catch.hpp"

#include <dekaf2/kstack.h>

#include <iostream>

using namespace dekaf2;

TEST_CASE("KStack")
{
	int iEmptyValue = int();
	SECTION("Basic KStack Test (Bool Version)")
	{
		KStack<int> kStack;

		int my1Int = 5, my2Int = 7, my3Int = 9, my4Int = 11, my5Int = 13, my6Int = 15;
		int subInt = 10;
		// Push test data onto stack
		CHECK(kStack.Push(my4Int));
		CHECK(kStack.Push(my5Int));
		CHECK(kStack.Push(my6Int));
		CHECK(kStack.PushBottom(my3Int));
		CHECK(kStack.PushBottom(my2Int));
		CHECK(kStack.PushBottom(my1Int));

		int top, bottom, popTop, popBottom;
		int middle;

		// Look at the top and bottom
		CHECK(kStack.Peek(top));
		CHECK(top == 15);
		CHECK(kStack.PeekBottom(bottom));
		CHECK(bottom == 5);

		// Look somewhere in the middle
		CHECK(kStack.GetItem(3, middle));
		CHECK_FALSE(kStack.GetItem(100, bottom));
		CHECK(middle == 11);
		// Set something in the middle
		CHECK(kStack.SetItem(3, subInt));
		CHECK_FALSE(kStack.SetItem(100, subInt));
		CHECK(6 == kStack.size());
		// Check it
		CHECK(kStack.GetItem(3, middle));
		CHECK(middle == subInt);
		// We're not empty yet
		CHECK_FALSE(kStack.empty());

		// Pop off the top and bottom, shrinking stack
		CHECK(kStack.Pop(popTop));
		CHECK(top == popTop);
		CHECK(5 == kStack.size());
		CHECK_FALSE(kStack.empty());
		CHECK(kStack.PopBottom(popBottom));
		CHECK(popBottom == bottom);
		CHECK(4 == kStack.size());
		CHECK_FALSE(kStack.empty());

		// Clear whole stack
		kStack.clear();
		CHECK_FALSE(kStack.Pop(popTop));
		CHECK_FALSE(kStack.Peek(top));
		CHECK_FALSE(kStack.PopBottom(popBottom));
		CHECK_FALSE(kStack.PeekBottom(bottom));
		// Best be empty now
		CHECK(kStack.empty());
		CHECK(0 == kStack.size());

	}

	SECTION("Basic KStack Test (const Stack_Value& Return Version)")
	{
		KStack<int> kStack;

		int my1Int = 5, my2Int = 7, my3Int = 9, my4Int = 11, my5Int = 13, my6Int = 15;
		int subInt = 10;
		// Push tests data onto stack
		CHECK(kStack.Push(my4Int));
		CHECK(kStack.Push(my5Int));
		CHECK(kStack.Push(my6Int));
		CHECK(kStack.PushBottom(my3Int));
		CHECK(kStack.PushBottom(my2Int));
		CHECK(kStack.PushBottom(my1Int));

		// Look at the top and bottom
		const int& rtop = kStack.Peek();
		CHECK(rtop == 15);
		const int& rbottom = kStack.PeekBottom();
		CHECK(rbottom == 5);

		// Look somewhere in the middle
		const int& rmiddle = kStack.GetItem(3);
		CHECK(rmiddle == 11);
		// Set something in the middle
		CHECK(kStack.SetItem(3, subInt));
		CHECK(6 == kStack.size());
		// Check it
		CHECK_FALSE(iEmptyValue == rmiddle);
		CHECK(kStack.GetItem(3) == subInt);
		// We're not empty yet
		CHECK_FALSE(kStack.empty());

		// Pop off the top and bottom, shrinking stack
		const int& popTop = kStack.Pop();
		CHECK(rtop == popTop);
		CHECK(5 == kStack.size());
		CHECK_FALSE(kStack.empty());
		const int& popBottom = kStack.PopBottom();
		CHECK(popBottom == rbottom);
		CHECK(4 == kStack.size());
		CHECK_FALSE(kStack.empty());

		// Clear whole stack
		kStack.clear();
		CHECK(iEmptyValue == kStack.Pop());
		CHECK(iEmptyValue == kStack.Peek());
		CHECK(iEmptyValue == kStack.PopBottom());
		CHECK(iEmptyValue == kStack.PeekBottom());
		// Best be empty now
		CHECK(kStack.empty());
		CHECK(0 == kStack.size());

	}

	SECTION("Basic KStack Iterative For Loop Test ")
	{
		KStack<int> kStack;

		int my6Ints[6] = {5, 7, 9, 11, 13, 15};
		// Push test data onto stack
		CHECK(kStack.Push(my6Ints[3]));
		CHECK(kStack.Push(my6Ints[4]));
		CHECK(kStack.Push(my6Ints[5]));
		CHECK(kStack.PushBottom(my6Ints[2]));
		CHECK(kStack.PushBottom(my6Ints[1]));
		CHECK(kStack.PushBottom(my6Ints[0]));

		CHECK_FALSE(kStack.empty());
		CHECK(6 == kStack.size());

		for (int i = 0; i < kStack.size(); ++i)
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
		CHECK(kStack.Push(my6Ints[3]));
		CHECK(kStack.Push(my6Ints[4]));
		CHECK(kStack.Push(my6Ints[5]));
		CHECK(kStack.PushBottom(my6Ints[2]));
		CHECK(kStack.PushBottom(my6Ints[1]));
		CHECK(kStack.PushBottom(my6Ints[0]));

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
		CHECK(kStack.Push(my6Ints[3]));
		CHECK(kStack.Push(my6Ints[4]));
		CHECK(kStack.Push(my6Ints[5]));
		CHECK(kStack.PushBottom(my6Ints[2]));
		CHECK(kStack.PushBottom(my6Ints[1]));
		CHECK(kStack.PushBottom(my6Ints[0]));

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
		CHECK(kStack.Push(my6Ints[3]));
		CHECK(kStack.Push(my6Ints[4]));
		CHECK(kStack.Push(my6Ints[5]));
		CHECK(kStack.PushBottom(my6Ints[2]));
		CHECK(kStack.PushBottom(my6Ints[1]));
		CHECK(kStack.PushBottom(my6Ints[0]));

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
		CHECK(kStack.Push(my6Ints[3]));
		CHECK(kStack.Push(my6Ints[4]));
		CHECK(kStack.Push(my6Ints[5]));
		CHECK(kStack.PushBottom(my6Ints[2]));
		CHECK(kStack.PushBottom(my6Ints[1]));
		CHECK(kStack.PushBottom(my6Ints[0]));

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
