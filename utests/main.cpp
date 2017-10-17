
#define CATCH_CONFIG_RUNNER
#include "catch.hpp"
#include <dekaf2/dekaf2.h>

#include <string>
#include <iostream>

#ifndef DEKAF2_DO_NOT_WARN_ABOUT_COW_STRING
bool stdstring_supports_cow()
{
	// make sure the string is longer than the size of potential
	// implementation of small-string.
	std::string s1 = "012345678901234567890123456789"
	                 "012345678901234567890123456789"
	                 "012345678901234567890123456789"
	                 "012345678901234567890123456789"
	                 "012345678901234567890123456789";
	std::string s2 = s1;
	std::string s3 = s2;

	bool result1 = s1.data() == s2.data();
	bool result2 = s1.data() == s3.data();

	s2[0] = 'X';

	bool result3 = s1.data() != s2.data();
	bool result4 = s1.data() == s3.data();

	s3[0] = 'X';

	bool result5 = s1.data() != s3.data();

	return result1 && result2 &&
	       result3 && result4 &&
	       result5;
}
#endif

int main( int argc, char* const argv[] )
{
	dekaf2::Dekaf().SetMultiThreading();

#ifndef DEKAF2_DO_NOT_WARN_ABOUT_COW_STRING
#if _GLIBCXX_USE_CXX11_ABI
	bool old_ABI = false;
#else
	bool old_ABI = true;
#endif

	if (stdstring_supports_cow() || old_ABI)
	{
		std::cout << "Warning: The std::string implementation you use works with Copy On Write.\n";
		std::cout << "         This is a bad thing! Beside reducing performance in modern C++,\n";
		std::cout << "         it is also not legal starting with the C++11 standard.\n";
		std::cout << "         Make sure you switch to using -D_GLIBCXX_USE_CXX11_ABI=1 when\n";
		std::cout << "         invoking g++ ! And if RedHat does not let you do so, then\n";
		std::cout << "         modify the c++config.h that undefs _GLIBCXX_USE_CXX11_ABI\n";
	}
#endif

    int result = Catch::Session().run( argc, argv );

    // global clean-up...

    return result;
}
