#include <atomic>
#include <cstdint>
#include <ctime>

std::atomic<int64_t> iCurrentTime(std::time(nullptr));

bool test ()
{
	return iCurrentTime > 0;
}

int main()
{
	return test();
}

