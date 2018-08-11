#include "catch.hpp"

#include <dekaf2/kfilesystem.h>
#include <vector>
#include <fstream>
#include <dekaf2/kreader.h>
#include <dekaf2/kwriter.h>

using namespace dekaf2;

TEST_CASE("KFilesystem") {

	KString sDirectory("/tmp/filetests12r4948t5/depth/three");

	KString sFile("/tmp/filetests12r4948t5/depth/three/KFilesystem.test");

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

	SECTION("create nested dir")
	{
		kCreateDir(sDirectory);
		CHECK ( kDirExists(sDirectory) == true );
	}

 	SECTION("openmode")
	{
		{
			KOutFile fWriter(sFile, std::ios_base::trunc);
			CHECK( fWriter.is_open() == true );

			if (fWriter.is_open())
			{
				fWriter.Write(sOut);
			}
		}

		{
			KOutFile fWriter(sFile, std::ios_base::app);
			CHECK( fWriter.is_open() == true );

			if (fWriter.is_open())
			{
				fWriter.Write(sOut);
			}
		}

		CHECK( kFileExists(sFile, true) == true );
		CHECK( kGetSize(sFile) == 63 * 2 );

		kRemoveFile(sFile);
		CHECK( kFileExists(sFile) == false );

	}

	SECTION("setup test file")
	{
		time_t now = time(0);

		KOutFile fWriter(sFile, std::ios_base::trunc);
		CHECK( fWriter.is_open() == true );

		CHECK ( kGetLastMod(sFile) - now < 2 );

		if (fWriter.is_open())
		{
			fWriter.Write(sOut);
		}
	}

	SECTION("KFile stats")
	{
		CHECK( kFileExists(sFile, true) == true );
		CHECK( kGetSize(sFile) == 63 );
	}

	SECTION("name manipulations")
	{
		KString sPathname = "/this/is/a/name.txt";
		CHECK ( kExtension(sPathname) == "txt" );
		CHECK ( kBasename(sPathname) == "name.txt" );
		CHECK ( kDirname(sPathname) == "/this/is/a/" );
		CHECK ( kDirname(sPathname, false) == "/this/is/a" );
	}

	SECTION("KDirectory")
	{
		KDirectory Dir(sDirectory);
		CHECK ( Dir.size() == 1 );
		CHECK ( Dir.Find("KFilesystem.test") == true );
		CHECK ( Dir.Match( KDirectory::EntryType::REGULAR) == 1);
		CHECK ( Dir.Match( KDirectory::EntryType::REGULAR, true) == 0);
		CHECK ( Dir.empty() == true );
		CHECK ( Dir.Open(sDirectory) == 1 );
		Dir.RemoveHidden();
		CHECK ( Dir.size() == 1 );
		CHECK ( Dir.Match(".*\\.test") == 1 );
		CHECK ( Dir.Match(".*\\.test", true) == 0 );
		CHECK ( Dir.empty() == true );
		CHECK ( Dir.Open(sDirectory, KDirectory::EntryType::REGULAR) == 1 );
		CHECK ( Dir.size() == 1 );
	}

	SECTION("KDiskStat")
	{
		KDiskStat Stat(sDirectory);
		CHECK ( Stat.Total() > 100000ULL );
		CHECK ( Stat.Error() == "" );
	}

}

TEST_CASE("KFilesystem cleanup")
{
	CHECK ( kRemoveDir("/tmp/filetests12r4948t5") == true );
	CHECK ( kDirExists("/tmp/filetests12r4948t5") == false);
}
