#include "catch.hpp"

#include <kfile.h>
#include <vector>
#include <fstream>

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

	{
		KFile fWriter(sFile, KFile::WRITE);
		CHECK( fWriter.IsOpen() == true );

		if (fWriter.IsOpen())
		{
			fWriter.Write(sOut);
		}
	}

	SECTION("KFile stats")
	{
		CHECK( KFile::Exists(sFile, KFile::TEST0) == true );
		CHECK( KFile::GetSize(sFile) == 63 );
	}

	SECTION("KFile read all")
	{
		KFile File(sFile, KFile::READ | KFile::TEXT);
		KString sRead;
		CHECK( File.GetContent(sRead) == true );
		CHECK( sRead == sOut );
	}

	SECTION("KFile read iterator 1")
	{
		KFile File(sFile, KFile::READ | KFile::TEXT);
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
		KFile File(sFile, KFile::READ | KFile::TEXT);
		for (const auto& it : File)
		{
			CHECK( it.StartsWith("line ") == true );
		}
	}

}
