
#define CATCH_CONFIG_RUNNER
#include "catch.hpp"
#include <dekaf2/dekaf2.h>
#include <dekaf2/klog.h>
#include <dekaf2/kstringutils.h>
#include <dekaf2/kwriter.h>

#include <string>
#include <iostream>

using namespace dekaf2;

KStringView g_Synopsis[] = {
	"",
	"dekaf2-utests - dekaf2 unit tests",
	"",
	"usage: dekaf2-utests [-k[k[k]]] ...see standard args below...",
	"",
	"where:",
	"  -k[k[k]]        : debug levels (to stdout)",
};

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
	KInit(true).SetDebugFlag("/tmp/unittest.dbg")
	           .SetMultiThreading(true)
	           .SetOnlyShowCallerOnJsonError(true);

	KLog::getInstance().KeepCLIMode(true);

	// make sure we use a utf8 locale with english number rules for the utests
	if (!kSetGlobalLocale("en_US.UTF-8"))
	{
		KErr.FormatLine("cannot set en_US.UTF-8 locale, locale is {}", kGetGlobalLocale().name());
		return -1;
	}
	// glibc needs the environment set for error messages
	kSetEnv("LANGUAGE", "en_US.UTF-8");

	bool bSynopsis{false};
	int  iLast{0};

	for (int ii=1; ii < argc; ++ii)
	{
		if (kStrIn (argv[ii], "-k,-kk,-kkk"))
		{
			iLast = ii;
			KLog::getInstance()
				.SetLevel(static_cast<int>(strlen(argv[ii]) - 1))
				.SetDebugLog(KLog::STDOUT);
			kDebugLog (0, "{}: debug now set to {}", argv[ii], KLog::getInstance().GetLevel());
		}
		else if (kStrIn (argv[ii], "-k0"))
		{
			iLast = ii;
			KLog::getInstance()
				.SetLevel( 0 )
				.SetDebugLog(KLog::STDOUT);
			kDebugLog (0, "{}: debug now set to {}", argv[ii], KLog::getInstance().GetLevel());
		}
		if (kStrIn (argv[ii], "-uk,-ukk,-ukkk"))
		{
			iLast = ii;
			KLog::getInstance()
				.SetLevel(static_cast<int>(strlen(argv[ii]) - 2))
				.SetUSecMode(true)
				.SetDebugLog(KLog::STDOUT);
			kDebugLog (0, "{}: debug now set to {}", argv[ii], KLog::getInstance().GetLevel());
		}
		else if (kStrIn (argv[ii], "-uk0"))
		{
			iLast = ii;
			KLog::getInstance()
				.SetLevel( 0 )
				.SetUSecMode(true)
				.SetDebugLog(KLog::STDOUT);
			kDebugLog (0, "{}: debug now set to {}", argv[ii], KLog::getInstance().GetLevel());
		}
		else if (kStrIn (argv[ii], "-dgrep,-dgrepv"))
		{
			if (ii < argc-1)
			{
				++iLast;
				++ii;
				iLast = ii;
				// if no -k option has been applied yet switch to -kkk
				if (KLog::getInstance().GetLevel() <= 0)
				{
					KLog::getInstance().SetLevel (3);
				}
				KLog::getInstance()
					.LogWithGrepExpression(true, KStringView(argv[ii-1]) == "-dgrepv"_ksv, argv[ii])
					.SetDebugLog (KLog::STDOUT);
			}
		}

		// part of the generic CATCH framework:
		//   -?, -h, --help                display usage information
		//   -l, --list-tests              list all/matching test cases
		//   -t, --list-tags               list all/matching tags
		else if (kStrIn (argv[ii], "-?,-h,--help"))
		{
			bSynopsis = true;
		}
	}

	// switch microseconds logging on
	KLog::getInstance()
		.SetUSecMode(true);

#ifndef DEKAF2_DO_NOT_WARN_ABOUT_COW_STRING
	if (stdstring_supports_cow())
	{
		std::cout << "Warning: The std::string implementation you use works with Copy On Write.\n";
		std::cout << "         This is an issue! Beside reducing performance in modern C++,\n";
		std::cout << "         it is also not legal starting with the C++11 standard.\n";
		std::cout << "         Make sure you switch to using -D_GLIBCXX_USE_CXX11_ABI=1 when\n";
		std::cout << "         invoking g++ ! And if RedHat does not let you do so, then\n";
		std::cout << "         modify the c++config.h that undefs _GLIBCXX_USE_CXX11_ABI\n";
	}
#endif

	if (bSynopsis)
	{
		for (unsigned long jj=0; jj < std::extent<decltype(g_Synopsis)>::value; ++jj)
		{
			KOut.WriteLine (g_Synopsis[jj]);
		}
	}

	auto iResult = Catch::Session().run( argc - iLast, &argv[iLast] );

    // global clean-up...

    return iResult;
}
