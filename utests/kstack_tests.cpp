#include "catch.hpp"

#include <kstack.h>

#include <iostream>

using namespace dekaf2;

TEST_CASE("KStack")
{
	SECTION("Basic KStack Test (Bool Version)")
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

		int top, bottom, middle, popTop, popBottom;

		// Look at the top and bottom
		CHECK(kStack.peek(top));
		CHECK(top == 15);
		CHECK(kStack.peek_bottom(bottom));
		CHECK(bottom == 5);

		// Look somewhere in the middle
		CHECK(kStack.getItem(3, middle));
		CHECK(middle == 9);
		// Set something in the middle
		CHECK(kStack.setItem(3, subInt));
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
		// Best be empty now
		CHECK(kStack.empty());
		CHECK(0 == kStack.size());

	}

	SECTION("Basic KStack Test (Stack_Value Return Version)")
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

		int top, bottom, middle, popTop, popBottom;

		// Look at the top and bottom
		top = kStack.peek();
		CHECK(top == 15);
		bottom = kStack.peek_bottom();
		CHECK(bottom == 5);

		// Look somewhere in the middle
		middle = kStack.getItem(3);
		CHECK(middle == 9);
		// Set something in the middle
		CHECK(kStack.setItem(3, subInt));
		CHECK(6 == kStack.size());
		// Check it
		middle = kStack.getItem(3);
		CHECK(middle == subInt);
		// We're not empty yet
		CHECK_FALSE(kStack.empty());

		// Pop off the top and bottom, shrinking stack
		popTop = kStack.pop();
		CHECK(top == popTop);
		CHECK(5 == kStack.size());
		CHECK_FALSE(kStack.empty());
		popBottom = kStack.pop_bottom();
		CHECK(popBottom == bottom);
		CHECK(4 == kStack.size());
		CHECK_FALSE(kStack.empty());

		// Clear whole stack
		kStack.clear();
		// Best be empty now
		CHECK(kStack.empty());
		CHECK(0 == kStack.size());

	}

}
