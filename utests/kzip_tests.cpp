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

		KString sZipFile1 = kFormat("{}/{}", ZipDir.Name(), "input1.zip");
		KString sZipFile2 = kFormat("{}/{}", ZipDir.Name(), "input2.zip");
		KString sZipFile3 = kFormat("{}/{}", ZipDir.Name(), "input3.zip");

		{
			// create the archive manually
			KZip Zip(sZipFile1, true);

			CHECK (Zip.is_open() );

			{
				CHECK ( Zip.WriteBuffer(kFormat("{}\n{}\n", sAlpha, sAlpha), "file1"        ) );
				CHECK ( Zip.WriteFile(sFile2, "subdir/file2" ) );
				sFile2 = "helloolkdfsjlkdfjglksfjglfkdsjglkdjlgkjfdlgkjdsflkgjldskjglksnfjlkdfkhtklfjgksdjfjdgslkjdsglkhjslckhjsldhh";
				KInFile File3(sFile3);
				CHECK ( File3.is_open() );
				CHECK ( Zip.Write(File3, "file3" ) );
			}
		}

		{
			// create the archive by recursive parsing
			KZip Zip(sZipFile2, true);

			CHECK (Zip.is_open() );

			{
				KDirectory Files(InputDirectory.Name(), KFileType::FILE, true);
				Files.Sort();
				CHECK ( Files.size() == 3 );
				CHECK ( Zip.WriteFiles(Files, InputDirectory.Name()) );
			}
		}

		{
			// create the archive by recursive parsing
			KZip Zip(sZipFile3, true);

			CHECK (Zip.is_open() );

			{
				CHECK ( Zip.WriteFiles(InputDirectory.Name()) );
			}
		}

		{
			// check the created archive from the manual creation
			KTempDir OutputDirectory;

			KZip Zip  (sZipFile1);

			CHECK ( Zip.is_open() );
			CHECK ( Zip.size() == 3 );

			Zip.ReadAll(OutputDirectory.Name());

			{
				KDirectory Directory(OutputDirectory.Name(), KFileType::FILE, true);
				Directory.Sort();

				CHECK ( Directory.size() == 3 );
				auto File = Directory.begin();
				CHECK (     File->Filename() == "file1" );
				CHECK ( (++File)->Filename() == "file3" );
				CHECK ( (++File)->Filename() == "file2" );
			}

			{
				CHECK ( kReadAll(kFormat("{}/{}",    OutputDirectory.Name(), "file1")) == "abcdefghijklmnopqrstuvwxyz\nabcdefghijklmnopqrstuvwxyz\n" );
				CHECK ( kReadAll(kFormat("{}/{}/{}", OutputDirectory.Name(), "subdir", "file2")) == "01234567890\n01234567890\n" );
				CHECK ( kReadAll(kFormat("{}/{}",    OutputDirectory.Name(), "file3")) == "abcdefghijklmnopqrstuvwxyz\n01234567890\n" );
			}
		}

		{
			// check the created archive from recursive creation
			KTempDir OutputDirectory;

			KZip Zip  (sZipFile2);

			CHECK ( Zip.is_open() );
			CHECK ( Zip.size() == 3 );

			for (const auto& File : Zip)
			{
				if (!File.IsDirectory())
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

		{
			// check the created archive from recursive creation
			KTempDir OutputDirectory;

			KZip Zip  (sZipFile3);

			CHECK ( Zip.is_open() );
			CHECK ( Zip.size() == 4 );  // we have 4, because we have the subdirectory as a sinle directory entry as well

			for (const auto& File : Zip)
			{
				if (!File.IsDirectory())
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

}

#endif
