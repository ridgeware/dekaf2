#include <nghttp2/nghttp2.h>
#include <cstdint>
#include <type_traits>

#if (NGHTTP2_VERSION_NUM < 0x013d00)
inline bool nghttp2_test()
{
	nghttp2_data_provider Data;
	Data.read_callback = nullptr;

	return true;
}
#else
inline bool nghttp2_test()
{
	static_assert(std::is_same<nghttp2_ssize, ssize_t>::value, "nghttp2_ssize != ssize_t");

	nghttp2_data_provider2 Data;
	Data.read_callback = nullptr;

	return true;
}
#endif

int main()
{
	return nghttp2_test();
}
