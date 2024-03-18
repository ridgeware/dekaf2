#include <chrono>

inline bool nanoseconds_test()
{
	std::chrono::system_clock::time_point tp;
	tp += std::chrono::nanoseconds(1);
	return tp.time_since_epoch() == std::chrono::nanoseconds(1);
}

int main()
{
	return nanoseconds_test();
}

