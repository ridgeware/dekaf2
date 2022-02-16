#include "catch.hpp"

#include <dekaf2/kchildprocess.h>
#include <dekaf2/kstring.h>
#include <dekaf2/kwriter.h>
#include <dekaf2/dekaf2.h>
#include <dekaf2/klog.h>
#include <dekaf2/ksystem.h>

#ifndef DEKAF2_IS_WINDOWS

using namespace dekaf2;

int chtest1(int argc, char** argv)
{
	kDebug(1, "hello parent");
	return 7;
}

TEST_CASE("KChildProcess")
{
	kDebug(1, "I am parent");
	KChildProcess Child;
	CHECK ( Child.Fork(chtest1) );
	CHECK ( Child.Join()        );
	kDebug(1, "child is back home");
	CHECK ( Child.GetExitStatus() == 7 );
}

#endif
