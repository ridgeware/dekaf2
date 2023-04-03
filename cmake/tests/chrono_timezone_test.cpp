#include <chrono>

inline std::chrono::seconds timezone_test()
{
	return std::chrono::current_zone()->get_info(std::chrono::system_clock::now()).offset;
}

int main()
{
	return timezone_test() == std::chrono::hours(1);
}

