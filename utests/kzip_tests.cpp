#include "catch.hpp"

#include <dekaf2/dekaf2.h>
#ifdef DEKAF2_HAS_LIBZIP

#include <dekaf2/kzip.h>
#include <dekaf2/kfilesystem.h>
#include <dekaf2/ksystem.h>
#include <dekaf2/kwriter.h>
#include <dekaf2/kreader.h>
#include <dekaf2/kinshell.h>
#include <dekaf2/klog.h>

using namespace dekaf2;

TEST_CASE("KZip") {

	SECTION("Read")
	{
		KStringView sAlpha = "abcdefghijklmnopqrstuvwxyz";
		KStringView sDigit = "01234567890";

		KTempDir InputDirectory;
		CHECK ( InputDirectory.Name() != "" );
		CHECK ( kMakeDir(kFormat("{}/{}", InputDirectory.Name(), "subdir")) );

		KString sFile1 = kFormat("{}/{}"        , InputDirectory.Name(), "file1");
		KString sFile2 = kFormat("{}/subdir/{}" , InputDirectory.Name(), "file2");
		KString sFile3 = kFormat("{}/{}"        , InputDirectory.Name(), "file3");

		{
			KOutFile(sFile1).WriteLine(sAlpha).WriteLine(sAlpha);
			KOutFile(sFile2).WriteLine(sDigit).WriteLine(sDigit);
			KOutFile(sFile3).WriteLine(sAlpha).WriteLine(sDigit);

			CHECK ( kFileExists(sFile1, true) );
			CHECK ( kFileExists(sFile2, true) );
			CHECK ( kFileExists(sFile3, true) );
		}

		KTempDir ZipDir;

		KString sZipFile = kFormat("{}/{}", ZipDir.Name(), "input.zip");

		kSystem(kFormat("zip {} -r {} >/dev/null", sZipFile, InputDirectory.Name()));

		KTempDir OutputDirectory;

		KZip Zip(sZipFile);

		CHECK ( Zip.is_open() );

		for (const auto& File : Zip)
		{
			if (!File.bIsDirectory)
			{
				Zip.Read(kFormat("{}/{}", OutputDirectory.Name(), File.SafeName()), File);
			}
		}

		{
			KDirectory Directory(OutputDirectory.Name());
			Directory.Sort();

			CHECK ( Directory.size() == 3 );
			auto File = Directory.begin();
			CHECK (     File->Filename() == "file1" );
			CHECK ( (++File)->Filename() == "file2" );
			CHECK ( (++File)->Filename() == "file3" );
		}

		{
			CHECK ( kReadAll(kFormat("{}/{}", OutputDirectory.Name(), "file1")) == "abcdefghijklmnopqrstuvwxyz\nabcdefghijklmnopqrstuvwxyz\n" );
			CHECK ( kReadAll(kFormat("{}/{}", OutputDirectory.Name(), "file2")) == "01234567890\n01234567890\n" );
			CHECK ( kReadAll(kFormat("{}/{}", OutputDirectory.Name(), "file3")) == "abcdefghijklmnopqrstuvwxyz\n01234567890\n" );
		}
	}

}

#endif
