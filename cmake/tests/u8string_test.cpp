#include <string>

bool test ()
{
	std::u8string sTest;
	return sTest.empty();
}

int main()
{
	return test();
}

