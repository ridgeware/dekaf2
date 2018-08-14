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

		KOutFile fOut(sDirectory + "/test.txt");

	}

	SECTION("KFile stats")
	{
		CHECK( kFileExists(sFile, true) == true );
		CHECK( kGetSize(sFile) == 63 );
	}

	SECTION("name manipulations")
	{
		KStringView sPathname = "/this/is/a/name.txt";
		CHECK ( kExtension(sPathname) == "txt" );
		CHECK ( kExtension("/this.is/a./name") == "" );
		CHECK ( kBasename(sPathname) == "name.txt" );
		CHECK ( kDirname(sPathname) == "/this/is/a/" );
		CHECK ( kDirname(sPathname, false) == "/this/is/a" );
		CHECK ( kRemoveExtension(sPathname) == "/this/is/a/name" );
		CHECK ( kRemoveExtension("/this.is/a./name") == "/this.is/a./name" );
	}

	SECTION("KDirectory")
	{
		KDirectory Dir(sDirectory);
		CHECK ( Dir.size() == 2 );
		CHECK ( Dir.Match( KDirectory::EntryType::REGULAR) == 2);
		CHECK ( Dir.Match( KDirectory::EntryType::REGULAR, true) == 0);
		CHECK ( Dir.empty() == true );
		CHECK ( Dir.Open(sDirectory) == 2 );
		Dir.RemoveHidden();
		CHECK ( Dir.size() == 2 );
		CHECK ( Dir.Match(".*\\.test") == 1 );
		CHECK ( Dir.Match(".*\\.test", true) == 0 );
		CHECK ( Dir.empty() == true );
		CHECK ( Dir.Open(sDirectory, KDirectory::EntryType::REGULAR) == 2 );
		CHECK ( Dir.size() == 2 );
		CHECK ( Dir.Find("KFilesystem.test") == true );
		CHECK ( Dir.Find("test.txt") == true );
		CHECK ( Dir.Find("KFi*ystem.t?st") == true );
		CHECK ( Dir.WildCardMatch("KFilesystem.test") == 1 );
		CHECK ( Dir.WildCardMatch("*.t?st") == 1 );
		CHECK ( Dir.WildCardMatch("*.t?st", true) == 0 );
		CHECK ( Dir.empty() == true );
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
