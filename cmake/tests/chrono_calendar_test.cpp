#include <chrono>

inline std::chrono::year_month_day calendar_test()
{
	return std::chrono::year_month_day{ std::chrono::floor<std::chrono::days>(std::chrono::system_clock::now()) };
}

int main()
{
	return calendar_test().day() == std::chrono::day(1);
}

