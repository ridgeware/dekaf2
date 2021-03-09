#include "catch.hpp"

#include <dekaf2/kfilesystem.h>
#include <dekaf2/ksystem.h>
#include <vector>
#include <fstream>
#include <dekaf2/kreader.h>
#include <dekaf2/kwriter.h>

using namespace dekaf2;

// make this a global, as otherwise the different test sections would create
// and remove the temp dir again and again, but we want persistence for the tests
KTempDir TempDir;

TEST_CASE("KFilesystem")
{
	KString sDirectory = TempDir.Name();
	sDirectory += "/test/depth/three";

	KString sDirectorySlash = sDirectory;
	sDirectorySlash += kDirSep;

	KString sFile = sDirectory;
	sFile += kDirSep;
	sFile += "KFilesystem.test";

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
		CHECK( kDirExists(sDirectory) == true );
		CHECK( kGetSize(sFile) == 63 );
#ifndef DEKAF2_IS_WINDOWS
		CHECK( kGetSize(sDirectory) == npos );
#else
		CHECK( kGetSize(sDirectory) == -1 );
#endif
		CHECK( kFileSize(sFile) == 63 );
		CHECK( kFileSize(sDirectory) == npos );
	}

	SECTION("name manipulations")
	{
		KStringView sPathname = "/this/is/a/name.txt";
		CHECK ( kExtension(sPathname) == "txt" );
		CHECK ( kExtension("/this.is/a./name") == "" );
		CHECK ( kExtension("/this.is/a./.name") == "" );
		CHECK ( kBasename(sPathname) == "name.txt" );
		CHECK ( kDirname(sPathname) == "/this/is/a" );
		CHECK ( kDirname(sPathname, true) == "/this/is/a/" );
		CHECK ( kRemoveExtension(sPathname) == "/this/is/a/name" );
		CHECK ( kRemoveExtension("/this.is/a./name") == "/this.is/a./name" );
		CHECK ( kRemoveExtension("/this.is/a./.name") == "/this.is/a./.name" );
#ifdef DEKAF2_IS_WINDOWS
		CHECK ( kNormalizePath("/this.is/a./.name") == "\\this.is\\a.\\.name" );
		CHECK ( kNormalizePath("//this.is/a./name") == "\\\\this.is\\a.\\name" );
		CHECK ( kNormalizePath("/this..is////a/.././name") == "\\this..is\\name" );
		CHECK ( kNormalizePath("/this.is/../../../wrong") == "\\wrong" );
		auto sCWD = kGetCWD();
		KString sCompare (sCWD);
		sCompare += "\\this..is\\name";
		CHECK ( kNormalizePath("this..is////a/.././name") == sCompare );

		CHECK ( kNormalizePath("\\this.is\\a.\\.name") == "\\this.is\\a.\\.name" );
		CHECK ( kNormalizePath("\\this..is\\\\\\\\a\\..\\.\\name") == "\\this..is\\name" );
		CHECK ( kNormalizePath("\\this.is\\..\\..\\..\\wrong") == "\\wrong" );
		sCWD = kGetCWD();
		sCompare = sCWD;
		sCompare += "\\this..is\\name";
		CHECK ( kNormalizePath("this..is\\\\\\\\a\\..\\.\\name") == sCompare );

		CHECK ( kNormalizePath("C:\\this.is\\a.\\.name") == "C:\\this.is\\a.\\.name" );
		CHECK ( kNormalizePath("c:\\this..is\\\\\\\\a\\..\\.\\name") == "C:\\this..is\\name" );
		CHECK ( kNormalizePath("C:\\this.is\\..\\..\\..\\wrong") == "C:\\wrong" );
		sCWD = kGetCWD();
#else
		CHECK ( kNormalizePath("/this.is/a./.name") == "/this.is/a./.name" );
		CHECK ( kNormalizePath("/this..is////a/.././name") == "/this..is/name" );
		CHECK ( kNormalizePath("/this.is/../../../wrong") == "/wrong" );
		auto sCWD = kGetCWD();
		KString sCompare (sCWD);
		sCompare += "/this..is/name";
		CHECK ( kNormalizePath("this..is////a/.././name") == sCompare );
#endif
	}

	SECTION("KDirectory")
	{
		KDirectory Dir(sDirectory);
		CHECK ( Dir.size() == 2 );
		CHECK ( Dir.Match( KFileType::REGULAR) == 2);
		CHECK ( Dir.Match( KFileType::REGULAR, true) == 2);
		CHECK ( Dir.empty() == true );
		CHECK ( Dir.Open(sDirectory) == 2 );
		Dir.RemoveHidden();
		CHECK ( Dir.size() == 2 );
		CHECK ( Dir.Match(".*\\.test") == 1 );
		CHECK ( Dir.Match(".*\\.test", true) == 1 );
		CHECK ( Dir.empty() == true );
		CHECK ( Dir.Open(sDirectory, KFileType::REGULAR) == 2 );
		CHECK ( Dir.size() == 2 );
		CHECK ( Dir.Find("KFilesystem.test") != Dir.end() );
		CHECK ( Dir.Find("test.txt") != Dir.end() );
		CHECK ( Dir.Find("KFi*ystem.t?st") != Dir.end() );
		CHECK ( Dir.WildCardMatch("KFilesystem.test") == 1 );
		CHECK ( Dir.WildCardMatch("*.t?st") == 1 );
		CHECK ( Dir.WildCardMatch("*.t?st", true) == 1 );
		CHECK ( Dir.empty() == true );
	}

	SECTION("KDirectory2")
	{
		KDirectory Dir(sDirectorySlash);
		CHECK ( Dir.size() == 2 );
		CHECK ( Dir.Match( KFileType::REGULAR) == 2);
		Dir.Sort();
		CHECK ( Dir.begin()->Path().find(kFormat("{}{}", kDirSep, kDirSep)) == KString::npos );
	}

	SECTION("KDiskStat")
	{
		KDiskStat Stat(sDirectory);
		CHECK ( Stat.Total() > 100000ULL );
		CHECK ( Stat.Error() == "" );
	}

	SECTION("kTouchFile")
	{
		KString sNested { sDirectory };
		sNested += "/far/down/here.txt";

		CHECK ( kTouchFile(sNested)   );
		CHECK ( kFileExists(sNested)  );
		CHECK ( kTouchFile(sNested)   );
		CHECK ( kRemoveFile(sNested)  );
		CHECK ( !kFileExists(sNested) );
		CHECK ( kTouchFile(sNested)   );
		CHECK ( kFileExists(sNested)  );
	}

	SECTION("kRename")
	{
		KString sBaseDir = kGetTemp();
		sBaseDir += kDirSep;
		sBaseDir += "fs9238w78";

		KString sNested { sBaseDir };
		sNested += "/farer/down/here.txt";

		KString sRenamed { kDirname(sNested) + "/new.txt" };

		KString sNestedDir { sBaseDir };
		sNestedDir += "/farer/down";

		KString sRenamedDir { sBaseDir };
		sRenamedDir += "/farer/up";

		KString sFileinRenamedDir { sBaseDir };
		sFileinRenamedDir += "/farer/up/new.txt";


		CHECK ( kTouchFile(sNested)              );
		CHECK ( kFileExists(sNested)             );
		CHECK ( kRename(sNested, sRenamed)       );
		CHECK ( kFileExists(sRenamed)            );
		CHECK ( kRename(sNestedDir, sRenamedDir) );
		CHECK ( kFileExists(sFileinRenamedDir)   );
		CHECK ( kRemoveDir(sBaseDir) == true     );
	}

	SECTION("kIsSafeFilename")
	{
		CHECK ( kIsSafeFilename("hello-world.txt")       == true  );
		CHECK ( kIsSafeFilename("hel.l-o-wor-ld.txt")    == true  );
		CHECK ( kIsSafeFilename("hello/world.txt")       == false );
		CHECK ( kIsSafeFilename("hel.l-o/wor-ld.txt")    == false );
		CHECK ( kIsSafeFilename("hel.l-o/wor_ld.txt")    == false );
		CHECK ( kIsSafeFilename("hel.-lo/wor-ld.txt")    == false );
		CHECK ( kIsSafeFilename("")                      == false );
		CHECK ( kIsSafeFilename("/hello/world.txt")      == false );
		CHECK ( kIsSafeFilename("hello/../../world.txt") == false );
		CHECK ( kIsSafeFilename("../../world.txt")       == false );
		CHECK ( kIsSafeFilename("world.txt/")            == false );
		CHECK ( kIsSafeFilename("world.txt..")           == false );
	}

	SECTION("kIsSafePathname")
	{
		CHECK ( kIsSafePathname("hello-world.txt")       == true  );
		CHECK ( kIsSafePathname("hel.l-o-wor-ld.txt")    == true  );
#ifdef DEKAF2_IS_WINDOWS
		CHECK(kIsSafePathname("hello/world.txt")         == false );
		CHECK(kIsSafePathname("hel.l-o/wor-ld.txt")      == false );
		CHECK(kIsSafePathname("hello\\world.txt")        == true  );
		CHECK(kIsSafePathname("hel.l-o\\wor-ld.txt")     == true  );
#else
		CHECK ( kIsSafePathname("hello/world.txt")       == true  );
		CHECK ( kIsSafePathname("hel.l-o/wor-ld.txt")    == true  );
#endif
		CHECK ( kIsSafePathname("hel.l-o/wor_ld.txt")    == true  );
		CHECK ( kIsSafePathname("hel.-lo/wor-ld.txt")    == false );
		CHECK ( kIsSafePathname("")                      == false );
		CHECK ( kIsSafePathname("/hello/world.txt")      == false );
		CHECK ( kIsSafePathname("hello/../../world.txt") == false );
		CHECK ( kIsSafePathname("../../world.txt")       == false );
		CHECK ( kIsSafePathname("world.txt/")            == false );
		CHECK ( kIsSafePathname("world.txt..")           == false );
	}

	SECTION("kMakeSafeFilename")
	{
#ifdef DEKAF2_IS_WINDOWS
		CHECK ( kMakeSafeFilename("C:hello/world.txt")     == "hello-world.txt"     );
		CHECK ( kMakeSafeFilename("C:\\hello\\world.txt")  == "hello-world.txt"     );
		CHECK ( kMakeSafeFilename("hello/world.txt")       == "hello-world.txt"     );
		CHECK ( kMakeSafeFilename("C:/hello/world.txt")    == "hello-world.txt"     );
		CHECK ( kMakeSafeFilename("hel.-lo/wÖr_ld.txt")    == "hel.lo-wör_ld.txt"   );
		CHECK ( kMakeSafeFilename("?hel.-lo/wo?r_ld.txt")  == "hel.lo-wo-r-ld.txt"  );
		CHECK ( kMakeSafeFilename("/hello/world.txt")      == "hello-world.txt"     );
		CHECK ( kMakeSafeFilename("///hello/world.txt")    == "hello-world.txt"     );
		CHECK ( kMakeSafeFilename("hello/../../world.txt") == "hello.world.txt"     );
#else
		CHECK ( kMakeSafeFilename("hello/world.txt")       == "hello-world.txt"      );
		CHECK ( kMakeSafeFilename("C:/hello/world.txt")    == "c-hello-world.txt"    );
		CHECK ( kMakeSafeFilename("hel.-lo/wÖr_ld.txt")    == "hel.lo-wör_ld.txt"    );
		CHECK ( kMakeSafeFilename("?hel.-lo/wo?r_ld.txt")  == "hel.lo-wo-r_ld.txt"   );
		CHECK ( kMakeSafeFilename("/hello/world.txt")      == "hello-world.txt"      );
		CHECK ( kMakeSafeFilename("///hello/world.txt")    == "hello-world.txt"      );
		CHECK ( kMakeSafeFilename("hello/../../world.txt") == "hello.world.txt"      );
#endif
		CHECK ( kMakeSafeFilename("")                      == "none"                 );
		CHECK ( kMakeSafeFilename("&*&^%$#")               == "none"                 );
		CHECK ( kMakeSafeFilename("&*&^%$#", false, "default") == "default"          );
		CHECK ( kMakeSafeFilename("../../world.txt")       == "world.txt"            );
		CHECK ( kMakeSafeFilename("world.txt/")            == "world.txt"            );
		CHECK ( kMakeSafeFilename("WÖRLD.txt..")           == "wörld.txt"            );
		CHECK ( kMakeSafeFilename("world.txt.*.")          == "world.txt"            );
		CHECK ( kMakeSafeFilename("world.txt..")           == "world.txt"            );
		CHECK ( kMakeSafeFilename("WÖRLD.txt..", false)    == "WÖRLD.txt"            );
	}

	SECTION("kMakeSafePathname")
	{
#ifdef DEKAF2_IS_WINDOWS
		CHECK ( kMakeSafePathname("C:hello/world.txt")     == "hello\\world.txt"     );
		CHECK ( kMakeSafePathname("C:\\hello\\world.txt")  == "hello\\world.txt"     );
		CHECK ( kMakeSafePathname("hello/world.txt")       == "hello\\world.txt"     );
		CHECK ( kMakeSafePathname("C:/hello/world.txt")    == "hello\\world.txt"     );
		CHECK ( kMakeSafePathname("hel.-lo/wÖr_ld.txt")    == "hel.lo\\wör-ld.txt"   );
		CHECK ( kMakeSafePathname("?hel.-lo/wo?r_ld.txt")  == "hel.lo\\wo-r_ld.txt"  );
		CHECK ( kMakeSafePathname("/hello/world.txt")      == "hello\\world.txt"     );
		CHECK ( kMakeSafePathname("///hello/world.txt")    == "hello\\world.txt"     );
		CHECK ( kMakeSafePathname("hello/../../world.txt") == "hello\\world.txt"     );
#else
		CHECK ( kMakeSafePathname("hello/world.txt")       == "hello/world.txt"      );
		CHECK ( kMakeSafePathname("C:/hello/world.txt")    == "c/hello/world.txt"    );
		CHECK ( kMakeSafePathname("hel.-lo/wÖr_ld.txt")    == "hel.lo/wör_ld.txt"    );
		CHECK ( kMakeSafePathname("?hel.-lo/wo?r_ld.txt")  == "hel.lo/wo-r_ld.txt"   );
		CHECK ( kMakeSafePathname("/hello/world.txt")      == "hello/world.txt"      );
		CHECK ( kMakeSafePathname("///hello/world.txt")    == "hello/world.txt"      );
		CHECK ( kMakeSafePathname("hello/../../world.txt") == "hello/world.txt"      );
#endif
		CHECK ( kMakeSafePathname("")                      == "none"                 );
		CHECK ( kMakeSafePathname("&*&^%$#")               == "none"                 );
		CHECK ( kMakeSafePathname("&*&^%$#", false, "default") == "default"          );
		CHECK ( kMakeSafePathname("../../world.txt")       == "world.txt"            );
		CHECK ( kMakeSafePathname("world.txt/")            == "world.txt"            );
		CHECK ( kMakeSafePathname("WÖRLD.txt..")           == "wörld.txt"            );
		CHECK ( kMakeSafePathname("world.txt.*.")          == "world.txt"            );
		CHECK ( kMakeSafePathname("world.txt..")           == "world.txt"            );
		CHECK ( kMakeSafePathname("WÖRLD.txt..", false)    == "WÖRLD.txt"            );
	}

}
