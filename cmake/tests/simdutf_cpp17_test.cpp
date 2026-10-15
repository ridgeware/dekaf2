// simdutf requires C++17: it tests the language version as below, and it uses
// if constexpr, which gcc < 7 does not know

#if defined(_MSVC_LANG) && !defined(__clang__)
	#define CPLUSPLUS_VERSION (_MSC_VER == 1900 ? 201103L : _MSVC_LANG)
#else
	#define CPLUSPLUS_VERSION __cplusplus
#endif

#if CPLUSPLUS_VERSION < 201703L
	#error simdutf requires C++17
#endif

template<typename T>
int constexpr_if_test()
{
	if constexpr (sizeof(T) > 1)
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

int main()
{
	return constexpr_if_test<char>();
}
