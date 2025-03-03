#define NGHTTP2_NO_SSIZE_T 1
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
	nghttp2_data_provider2 Data;
	Data.read_callback = nullptr;

	return true;
}
#endif

int main()
{
	return nghttp2_test();
}

