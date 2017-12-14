#include "catch.hpp"

#include <dekaf2/kstream.h>
#include <unistd.h>
#include <iostream>


using namespace dekaf2;

TEST_CASE("KWriter") {

	SECTION("Printf")
	{
		KString sFile("/tmp/kwriter_test_sdfjhkshg23.txt");
		{
			KOutFile out(sFile.c_str());
			int i = 123;
			out.Printf("test: %d\n", i);
		}
		{
			KInFile inf(sFile.c_str());
			KString sRead;
			inf.ReadLine(sRead);
			CHECK (sRead == "test: 123");
		}
		unlink(sFile.c_str());
	}

}
