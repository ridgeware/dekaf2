#include "catch.hpp"

#include <kreader.h>
#include <kwriter.h>
#include <vector>

using namespace dekaf2;

TEST_CASE("KReader") {

	KString sFile("/tmp/KReader.test");

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
		KFileWriter fWriter(sFile, std::ios_base::trunc);
		CHECK( fWriter.is_open() == true );

		if (fWriter.is_open())
		{
			fWriter.Write(sOut);
		}
	}

	SECTION("KFileReader read all")
	{
		KFileReader File(sFile);
		KString sRead;
		CHECK( File.GetContent(sRead) == true );
		CHECK( sRead == sOut );
	}

	SECTION("KFileReader read iterator 1")
	{
		KFileReader File(sFile);
		auto it = File.begin();
		KString s1;
		s1 = *it;
		CHECK( s1 == "line 1\n" );
		s1 = *it;
		CHECK( s1 == "line 1\n" );
		++it;
		s1 = std::move(*it);
		CHECK( s1 == "line 2\n" );
		s1 = *it;
		CHECK( s1 != "line 2\n" );
		s1 = *++it;
		CHECK( s1 == "line 3\n" );
		s1 = *it++;
		CHECK( s1 == "line 3\n" );
		s1 = *it++;
		CHECK( s1 == "line 4\n" );
		s1 = *it;
		CHECK( s1 == "line 5\n" );

	}

	SECTION("KFileReader read iterator 2")
	{
		KFileReader File(sFile, std::ios_base::out, "\r\n4 ", '\n');
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
		CHECK( s1 == "line" );
		s1 = *it;
		CHECK( s1 == "line 5" );

	}

	SECTION("KFileReader read iterator 3")
	{
		KFileReader File(sFile, std::ios_base::out, "\n", '\n');
		for (const auto& it : File)
		{
			CHECK( it.StartsWith("line ") == true );
			CHECK( it.EndsWith  ("\n")    == false );
		}
	}

}
