#include "catch.hpp"

#include <dekaf2/kstream.h>
#include <dekaf2/kfdreader.h>
#include <vector>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


using namespace dekaf2;

TEST_CASE("KReader") {

	SECTION("KInFile exception safety")
	{
		KString buf;

		SECTION("default constructor")
		{
			KInFile File;
			CHECK ( File.ReadLine(buf) == false );
			CHECK ( buf.empty()        == true  );
		}

		SECTION("nonexisting file")
		{
			KInFile File("/tmp/this_file_should_not_exist_ASKJFHsdkfgj37r6");
			CHECK ( File.ReadLine(buf) == false );
			CHECK ( buf.empty()        == true  );
		}

	}

	SECTION("KFDReader exception safety")
	{
		KString buf;

		SECTION("default constructor")
		{
			KFDReader File;
			CHECK ( File.ReadLine(buf) == false );
			CHECK ( buf.empty()        == true  );
		}

		SECTION("nonexisting file")
		{
			KFDReader File(-3);
			CHECK ( File.ReadLine(buf) == false );
			CHECK ( buf.empty()        == true  );
		}

	}

	SECTION("KFPReader exception safety")
	{
		KString buf;

		SECTION("default constructor")
		{
			KFPReader File;
			CHECK ( File.ReadLine(buf) == false );
			CHECK ( buf.empty()        == true  );
		}

		SECTION("nonexisting file")
		{
			FILE* fp = nullptr;
			KFPReader File(fp);
			CHECK ( File.ReadLine(buf) == false );
			CHECK ( buf.empty()        == true  );
		}

	}

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
		"line 9"
	};

	size_t iSize = sOut.size();

	SECTION("create test file")
	{
		KOutFile fWriter(sFile, std::ios_base::trunc);
		CHECK( fWriter.is_open() == true );

		if (fWriter.is_open())
		{
			fWriter.Write(sOut);
		}
		CHECK ( fWriter.good() );
		CHECK ( fWriter        );
		fWriter.close();
		CHECK( fWriter.is_open() == false );
	}

	SECTION("KFileReader read all 1")
	{
		KInFile File(sFile);
		KString sRead;
		CHECK( File.eof() == false);
		CHECK( File.GetContent(sRead) == true );
		CHECK( sRead == sOut );
		CHECK( File.eof() == true);
		File.close();
		CHECK( File.is_open() == false );
	}

	SECTION("KFileReader read all 2")
	{
		KInFile File(sFile);
		CHECK( File.eof() == false);
		KString sRead;
		for (;;)
		{
			auto ch = File.Read();
			if (ch == std::istream::traits_type::eof())
			{
				break;
			}
			sRead += static_cast<KString::value_type>(ch);
		}
		CHECK( sRead == sOut );
		CHECK( File.eof() == true);
		File.close();
		CHECK( File.is_open() == false );
	}

	SECTION("KFileReader read iterator 1")
	{
		KInFile File(sFile);
		File.SetReaderRightTrim("");
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
		s1 = *++it;
		CHECK( s1 == "line 6\n" );
		s1 = *++it;
		CHECK( s1 == "line 7\n" );
		s1 = *++it;
		CHECK( s1 == "line 8\n" );
		s1 = *++it;
		CHECK( s1 == "line 9" );
		File.close();
		CHECK( File.is_open() == false );
	}

	SECTION("KFileReader read iterator 2")
	{
		KInFile File(sFile, std::ios_base::in);
		File.SetReaderRightTrim("\r\n4 ");
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
		s1 = *++it;
		CHECK( s1 == "line 6" );
		s1 = *++it;
		CHECK( s1 == "line 7" );
		s1 = *++it;
		CHECK( s1 == "line 8" );
		s1 = *++it;
		CHECK( s1 == "line 9" );
		File.close();
		CHECK( File.is_open() == false );
	}

	SECTION("KFileReader read iterator 3")
	{
		KInFile File(sFile);
		CHECK( File.eof() == false);
		auto it = File.begin();
		for (int iCount = 0; iCount < 9; ++iCount)
		{
			++it;
		}
		CHECK( File.eof() == true);
		File.close();
		CHECK( File.is_open() == false );
	}

	SECTION("KFileReader read iterator 4")
	{
		KInFile File(sFile, std::ios_base::in);
		CHECK( File.eof() == false);
		CHECK( File.GetSize() == iSize );
		CHECK( File.GetRemainingSize() == iSize );
		CHECK( File.GetRemainingSize() == iSize );
		int iCount = 0;
		for (const auto& it : File)
		{
			++iCount;
			CHECK( File.eof() == (iCount == 9)     );
			CHECK( it.StartsWith("line ") == true  );
			CHECK( it.EndsWith  ("\n")    == false );
		}
		CHECK( File.eof() == true );
		CHECK( iCount == 9 );
		File.close();
		CHECK( File.is_open() == false );
	}

	SECTION("KFileReader char read iterator 1")
	{
		KInFile File(sFile, std::ios_base::in);
		auto it = File.char_begin();
		for (int iCount = 0; iCount < 9; ++iCount)
		{
			CHECK( *it++ == 'l' );
			CHECK( *it   == 'i' );
			CHECK( *++it == 'n' );
			++it;
			CHECK( *it   == 'e' );
			it++;
			CHECK( *it++ == ' ' );
			CHECK( *it++ == ('1' + iCount) );
			if (iCount < 8)
			{
				CHECK( *it++ == '\n' );
			}
		}
		CHECK( it == File.char_end() );
		File.close();
		CHECK( File.is_open() == false );
	}

	SECTION("KFDReader test 1")
	{
		int fd = ::open(sFile.c_str(), O_RDONLY);
		KFDReader File(fd);
		KString sRead;
		CHECK( File.eof() == false);
		CHECK( File.ReadRemaining(sRead) == true );
		CHECK( sRead == sOut );
		CHECK( File.eof() == true);
		File.close();
		CHECK( File.is_open() == false );
	}

	SECTION("KFDReader read iterator 1")
	{
		int fd = ::open(sFile.c_str(), O_RDONLY);
		KFDReader File(fd);
		KString sRead;
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
		File.close();
		CHECK( File.is_open() == false );
	}

	SECTION("KFDReader read iterator 2")
	{
		int fd = ::open(sFile.c_str(), O_RDONLY);
		KFDReader File(fd);
		KString sRead;
		CHECK( File.eof() == false);
		int iCount = 0;
		for (const auto& it : File)
		{
			++iCount;
			CHECK( File.eof() == (iCount == 9)     );
			CHECK( it.StartsWith("line ") == true  );
			CHECK( it.EndsWith  ("\n")    == false );
		}
		CHECK( File.eof() == true );
		CHECK( iCount == 9 );
		File.close();
		CHECK( File.is_open() == false );
	}


	SECTION("KFPReader test 1")
	{
		FILE* fp = std::fopen(sFile.c_str(), "r");
		KFPReader File(fp);
		KString sRead;
		CHECK( File.eof() == false);
		CHECK( File.ReadRemaining(sRead) == true );
		CHECK( sRead == sOut );
		CHECK( File.eof() == true);
		File.close();
		CHECK( File.is_open() == false );
	}

	SECTION("KFPReader read iterator 1")
	{
		FILE* fp = std::fopen(sFile.c_str(), "r");
		KFPReader File(fp);
		KString sRead;
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
		File.close();
		CHECK( File.is_open() == false );
	}

	SECTION("KFPReader read iterator 2")
	{
		FILE* fp = std::fopen(sFile.c_str(), "r");
		KFPReader File(fp);
		File.SetReaderRightTrim("\n");
		KString sRead;
		CHECK( File.eof() == false);
		int iCount = 0;
		for (const auto& it : File)
		{
			++iCount;
			CHECK( File.eof() == (iCount == 9)     );
			CHECK( it.StartsWith("line ") == true  );
			CHECK( it.EndsWith  ("\n")    == false );
		}
		CHECK( File.eof() == true );
		CHECK( iCount == 9 );
		File.close();
		CHECK( File.is_open() == false );
	}


}
