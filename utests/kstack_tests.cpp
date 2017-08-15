#include "catch.hpp"

#include <kstack.h>

#include <iostream>

using namespace dekaf2;

TEST_CASE("KStack")
{
	SECTION("Basic KStack Test (Bool Version)")
	{
		KStack<int> kStack;
		CHECK(kStack.isEmptyValue(kStack.getEmptyValue()));

		int my1Int = 5, my2Int = 7, my3Int = 9, my4Int = 11, my5Int = 13, my6Int = 15;
		int subInt = 10;
		// Push test data onto stack
		CHECK(kStack.push(my4Int));
		CHECK(kStack.push(my5Int));
		CHECK(kStack.push(my6Int));
		CHECK(kStack.push_bottom(my3Int));
		CHECK(kStack.push_bottom(my2Int));
		CHECK(kStack.push_bottom(my1Int));

		int top, bottom, popTop, popBottom;
		int middle;

		// Look at the top and bottom
		CHECK(kStack.peek(top));
		CHECK(top == 15);
		CHECK(kStack.peek_bottom(bottom));
		CHECK(bottom == 5);

		// Look somewhere in the middle
		CHECK(kStack.getItem(3, middle));
		CHECK_FALSE(kStack.getItem(100, bottom));
		CHECK(middle == 9);
		// Set something in the middle
		CHECK(kStack.setItem(3, subInt));
		CHECK_FALSE(kStack.setItem(100, subInt));
		CHECK(6 == kStack.size());
		// Check it
		CHECK(kStack.getItem(3, middle));
		CHECK(middle == subInt);
		// We're not empty yet
		CHECK_FALSE(kStack.empty());

		// Pop off the top and bottom, shrinking stack
		CHECK(kStack.pop(popTop));
		CHECK(top == popTop);
		CHECK(5 == kStack.size());
		CHECK_FALSE(kStack.empty());
		CHECK(kStack.pop_bottom(popBottom));
		CHECK(popBottom == bottom);
		CHECK(4 == kStack.size());
		CHECK_FALSE(kStack.empty());

		// Clear whole stack
		kStack.clear();
		CHECK_FALSE(kStack.pop(popTop));
		CHECK_FALSE(kStack.peek(top));
		CHECK_FALSE(kStack.pop_bottom(popBottom));
		CHECK_FALSE(kStack.peek_bottom(bottom));
		// Best be empty now
		CHECK(kStack.empty());
		CHECK(0 == kStack.size());

	}

	SECTION("Basic KStack Test (Stack_Value& Return Version)")
	{
		KStack<int> kStack;

		int my1Int = 5, my2Int = 7, my3Int = 9, my4Int = 11, my5Int = 13, my6Int = 15;
		int subInt = 10;
		// Push test data onto stack
		CHECK(kStack.push(my4Int));
		CHECK(kStack.push(my5Int));
		CHECK(kStack.push(my6Int));
		CHECK(kStack.push_bottom(my3Int));
		CHECK(kStack.push_bottom(my2Int));
		CHECK(kStack.push_bottom(my1Int));

		// Look at the top and bottom
		int& rtop = kStack.peek();
		CHECK(rtop == 15);
		rtop = 16;
		CHECK(kStack.peek() == 16);
		int& rbottom = kStack.peek_bottom();
		CHECK(rbottom == 5);

		// Look somewhere in the middle
		int& rmiddle = kStack.getItem(3);
		CHECK(kStack.isEmptyValue(kStack.getItem(100)));
		CHECK(rmiddle == 9);
		// Set something in the middle
		CHECK(kStack.setItem(3, subInt));
		CHECK(6 == kStack.size());
		// Check it
		CHECK_FALSE(kStack.isEmptyValue(rmiddle));
		CHECK(kStack.getItem(3) == subInt);
		// Assign it
		rmiddle = 8;
		// Check it
		//int& middle2 = my1Int; //kStack.getItem(3);
		CHECK(kStack.getItem(3, rmiddle));
		CHECK(kStack.getItem(3) == 8);
		// We're not empty yet
		CHECK_FALSE(kStack.empty());

		// Pop off the top and bottom, shrinking stack
		int& popTop = kStack.pop();
		CHECK(rtop == popTop);
		CHECK(5 == kStack.size());
		CHECK_FALSE(kStack.empty());
		int& popBottom = kStack.pop_bottom();
		CHECK(popBottom == rbottom);
		CHECK(4 == kStack.size());
		CHECK_FALSE(kStack.empty());

		// Clear whole stack
		kStack.clear();
		CHECK(kStack.isEmptyValue(kStack.pop()));
		CHECK(kStack.isEmptyValue(kStack.peek()));
		CHECK(kStack.isEmptyValue(kStack.pop_bottom()));
		CHECK(kStack.isEmptyValue(kStack.peek_bottom()));
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
		//CHECK(kStack.push(my1Int));
		//CHECK(kStack.push(my2Int));
		//CHECK(kStack.push(my3Int));
		CHECK(kStack.push(my4Int));
		CHECK(kStack.push(my5Int));
		CHECK(kStack.push(my6Int));
		CHECK(kStack.push_bottom(my3Int));
		CHECK(kStack.push_bottom(my2Int));
		CHECK(kStack.push_bottom(my1Int));

		// Look at the top and bottom
		const int& rtop = kStack.peek();
		CHECK(rtop == 15);
		const int& rbottom = kStack.peek_bottom();
		CHECK(rbottom == 5);

		// Look somewhere in the middle
		const int& rmiddle = kStack.getItem(3);
		CHECK(rmiddle == 9);
		// Set something in the middle
		CHECK(kStack.setItem(3, subInt));
		CHECK(6 == kStack.size());
		// Check it
		CHECK_FALSE(kStack.isEmptyValue(rmiddle));
		CHECK(kStack.getItem(3) == subInt);
		// We're not empty yet
		CHECK_FALSE(kStack.empty());

		// Pop off the top and bottom, shrinking stack
		const int& popTop = kStack.pop();
		CHECK(rtop == popTop);
		CHECK(5 == kStack.size());
		CHECK_FALSE(kStack.empty());
		const int& popBottom = kStack.pop_bottom();
		CHECK(popBottom == rbottom);
		CHECK(4 == kStack.size());
		CHECK_FALSE(kStack.empty());

		// Clear whole stack
		kStack.clear();
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
		CHECK(kStack.push_bottom(my6Ints[2]));
		CHECK(kStack.push_bottom(my6Ints[1]));
		CHECK(kStack.push_bottom(my6Ints[0]));

		CHECK_FALSE(kStack.empty());
		CHECK(6 == kStack.size());

		for (int i = 0; i < kStack.size(); ++i)
		{
			CHECK(kStack[i] == my6Ints[5-i]);
		}

	}

	SECTION("Basic KStack Forward Iterator Test ")
	{
		KStack<int> kStack;
		CHECK(kStack.isEmptyValue(kStack.getEmptyValue()));

		int my6Ints[6] = {5, 7, 9, 11, 13, 15};
		int subInt = 10;
		// Push test data onto stack
		//CHECK(kStack.push(my1Int));
		//CHECK(kStack.push(my2Int));
		//CHECK(kStack.push(my3Int));
		CHECK(kStack.push(my6Ints[3]));
		CHECK(kStack.push(my6Ints[4]));
		CHECK(kStack.push(my6Ints[5]));
		CHECK(kStack.push_bottom(my6Ints[2]));
		CHECK(kStack.push_bottom(my6Ints[1]));
		CHECK(kStack.push_bottom(my6Ints[0]));

		// We're not empty yet
		CHECK_FALSE(kStack.empty());
		CHECK(6 == kStack.size());

		int i = 5;
		for (auto iter = kStack.begin(); iter != kStack.end(); ++iter)
		{
			CHECK(*iter == my6Ints[i]);
			--i;
		}

	}

	SECTION("Basic KStack Forward Const Iterator Test ")
	{
		KStack<int> kStack;
		CHECK(kStack.isEmptyValue(kStack.getEmptyValue()));

		int my6Ints[6] = {5, 7, 9, 11, 13, 15};
		int subInt = 10;
		// Push test data onto stack
		CHECK(kStack.push(my6Ints[3]));
		CHECK(kStack.push(my6Ints[4]));
		CHECK(kStack.push(my6Ints[5]));
		CHECK(kStack.push_bottom(my6Ints[2]));
		CHECK(kStack.push_bottom(my6Ints[1]));
		CHECK(kStack.push_bottom(my6Ints[0]));

		// We're not empty yet
		CHECK_FALSE(kStack.empty());
		CHECK(6 == kStack.size());

		int i = 5;
		for (auto iter = kStack.cbegin(); iter != kStack.cend(); ++iter)
		{
			CHECK(*iter == my6Ints[i]);
			--i;
		}

	}

	SECTION("Basic KStack Reverse Iterator Test ")
	{
		KStack<int> kStack;
		CHECK(kStack.isEmptyValue(kStack.getEmptyValue()));

		int my6Ints[6] = {5, 7, 9, 11, 13, 15};
		int subInt = 10;
		// Push test data onto stack
		CHECK(kStack.push(my6Ints[3]));
		CHECK(kStack.push(my6Ints[4]));
		CHECK(kStack.push(my6Ints[5]));
		CHECK(kStack.push_bottom(my6Ints[2]));
		CHECK(kStack.push_bottom(my6Ints[1]));
		CHECK(kStack.push_bottom(my6Ints[0]));

		// We're not empty yet
		CHECK_FALSE(kStack.empty());
		CHECK(6 == kStack.size());

		int i = 0;
		for (auto iter = kStack.rbegin(); iter != kStack.rend(); ++iter)
		{
			CHECK(*iter == my6Ints[i]);
			++i;
		}

	}

	SECTION("Basic KStack Reverse Const Iterator Test ")
	{
		KStack<int> kStack;
		CHECK(kStack.isEmptyValue(kStack.getEmptyValue()));

		int my6Ints[6] = {5, 7, 9, 11, 13, 15};
		int subInt = 10;
		// Push test data onto stack
		CHECK(kStack.push(my6Ints[3]));
		CHECK(kStack.push(my6Ints[4]));
		CHECK(kStack.push(my6Ints[5]));
		CHECK(kStack.push_bottom(my6Ints[2]));
		CHECK(kStack.push_bottom(my6Ints[1]));
		CHECK(kStack.push_bottom(my6Ints[0]));

		// We're not empty yet
		CHECK_FALSE(kStack.empty());
		CHECK(6 == kStack.size());

		int i = 0;
		for (auto iter = kStack.crbegin(); iter != kStack.crend(); ++iter)
		{
			CHECK(*iter == my6Ints[i]);
			++i;
		}

	}

}
