#include <string>
#include <format>

bool test ()
{
	auto sTest = std::format("Hello {} {}!", "world", 42);
	std::string sExpected = "Hello world 42!";
	return sTest == sExpected;
}

int main()
{
	return test();
}
