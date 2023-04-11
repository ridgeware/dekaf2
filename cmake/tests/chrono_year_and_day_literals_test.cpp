#include <chrono>

bool test ()
{
	using namespace std::chrono::literals;
	return 12y + 10m;
}

int main()
{
	return test();
}

