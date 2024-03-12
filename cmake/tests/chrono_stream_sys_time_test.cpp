#include <iostream>
#include <chrono>

using sys_time = std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds>;

bool stream_op_test()
{
	sys_time tp;
	std::cout << tp;
	return true;
}

int main()
{
	return !stream_op_test();
}

