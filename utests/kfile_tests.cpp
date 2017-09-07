#include "catch.hpp"

#include <dekaf2/kfile.h>
#include <vector>
#include <fstream>
#include <dekaf2/kreader.h>
#include <dekaf2/kwriter.h>

using namespace dekaf2;

TEST_CASE("KFile") {

	KString sFile("/tmp/KFile.test");

	KString sOut {
		"line 1\n"
		"line 2\n"
		"line 3\n"
		"line 4\n"
		"line 5\n"
		"line 6\n"
		"line 7\n"
		"line 8\n"
		"line 9\n"
	};

	SECTION("setup test file")
	{
		KOutFile fWriter(sFile.c_str(), std::ios_base::trunc);
		CHECK( fWriter.is_open() == true );

		if (fWriter.is_open())
		{
			fWriter.Write(sOut);
		}
	}

	SECTION("KFile stats")
	{
		CHECK( kExists(sFile, true) == true );
		CHECK( kGetSize(sFile) == 63 );
	}

	SECTION("KFile read all")
	{
		KInFile File(sFile.c_str());
		KString sRead;
		CHECK( File.GetContent(sRead) == true );
		CHECK( sRead == sOut );
	}

	SECTION("KFile read iterator 1")
	{
		KInFile File(sFile.c_str());
		auto it = File.begin();
		KString s1;
		s1 = *it;
		CHECK( s1 == "line 1" );
		s1 = *it;
		CHECK( s1 == "line 1" );
		++it;
		s1 = std::move(*it);
		CHECK( s1 == "line 2" );
		s1 = *it;
		CHECK( s1 != "line 2" );
		s1 = *++it;
		CHECK( s1 == "line 3" );
		s1 = *it++;
		CHECK( s1 == "line 3" );
		s1 = *it++;
		CHECK( s1 == "line 4" );
		s1 = *it;
		CHECK( s1 == "line 5" );

	}

	SECTION("KFile read iterator 2")
	{
		KInFile File(sFile.c_str());
		for (const auto& it : File)
		{
			CHECK( it.StartsWith("line ") == true );
		}
	}

}
