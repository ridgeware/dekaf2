#include <chrono>

inline bool local_t_test()
{
	using lp = std::chrono::local_time<std::chrono::system_clock::duration>;
	lp tp;
	return tp.time_since_epoch() == lp::duration::zero();
}

int main()
{
	return local_t_test();
}

