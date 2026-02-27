#include <chrono>

std::chrono::utc_clock clock_cast_test(std::chrono::milliseconds msec)
{
	auto t = std::chrono::sys_days{std::chrono::July/1/2015} - msec;
	auto u = std::chrono::clock_cast<std::chrono::utc_clock>(t);
	return u;
}

int main()
{
	return (clock_cast_test(std::chrono::milliseconds(500)) > std::chrono::utc_clock::now();
}

