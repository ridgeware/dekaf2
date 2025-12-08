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
namespace {
KTempDir TempDir;
}

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

	SECTION("create existing dir")
	{
		CHECK ( kDirExists(TempDir.Name()) == true );
		CHECK ( kCreateDir(TempDir.Name()) == true );
	}

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

		CHECK( kNonEmptyFileExists(sFile) == true );
		CHECK( kGetSize(sFile) == 63 * 2 );

		kRemoveFile(sFile);
		CHECK( kFileExists(sFile) == false );
	}

	SECTION("setup test file")
	{
		auto now = KUnixTime::now();

		KOutFile fWriter(sFile, std::ios_base::trunc);
		CHECK( fWriter.is_open() == true );

		CHECK ( kGetLastMod(sFile) - now < chrono::seconds(2) );

		if (fWriter.is_open())
		{
			fWriter.Write(sOut);
		}

		KOutFile fOut(sDirectory + "/test.txt");
	}

	SECTION("change mode")
	{
		CHECK( kNonEmptyFileExists(sFile) == true );

		auto iOldMode = kGetMode(sFile);
		auto iNewMode = iOldMode | 0700;

		CHECK ( kChangeMode(sFile, iNewMode) );

		auto iVerifyMode = kGetMode(sFile);

		CHECK ( iVerifyMode == iNewMode );

		CHECK ( kChangeMode(sFile, iOldMode) );

		iVerifyMode = kGetMode(sFile);

		CHECK ( iVerifyMode == iOldMode );
	}

	SECTION("KFile stats")
	{
		CHECK( kNonEmptyFileExists(sFile) == true );
		CHECK( kDirExists(sDirectory)   == true );
		CHECK( kGetSize(sFile)          == 63   );
#ifndef DEKAF2_IS_WINDOWS
		CHECK( kGetSize(sDirectory)     == npos );
#else
		CHECK( kGetSize(sDirectory)     == -1   );
#endif
		CHECK( kFileSize(sFile)         == 63   );
		CHECK( kFileSize(sDirectory)    == npos );
	}

	SECTION("KFileType")
	{
		KFileType ft(KFileType::DIRECTORY);
		CHECK ( ft == KFileType::DIRECTORY );
		ft = KFileType::PIPE;
		CHECK ( ft == KFileType::PIPE );
		CHECK ( ft.Serialize() == "pipe" );
	}

	SECTION("KFileTypes")
	{
		auto ft3(KFileType::PIPE | KFileType::BLOCK);
		CHECK ( kJoined(ft3.Serialize(), "|") == "pipe|block" );
		auto ft4 = KFileType::DIRECTORY | KFileType::FILE;
		CHECK ( kJoined(ft4.Serialize(), "|") == "file|directory" );
		auto ft5(KFileType::PIPE | KFileType::BLOCK | KFileType::CHARACTER);
		CHECK ( kJoined(ft5.Serialize(), "|") == "pipe|block|character" );
		auto ft6(KFileTypes::ALL);
		CHECK ( kJoined(ft6.Serialize(), "|") == "all" );
		auto ft8(KFileType::PIPE | KFileType::BLOCK | KFileType::CHARACTER | KFileType::FILE);
		CHECK ( kJoined(ft8.Serialize(), "|") == "file|pipe|block|character" );
	}

	SECTION("KFileStat")
	{
		KFileStat fs(sFile);
		CHECK( fs.Exists()      == true  );
		CHECK( fs.Type()        == KFileType::FILE );
		CHECK( fs.IsFile()      == true  );
		CHECK( fs.IsDirectory() == false );
		CHECK( fs.Size()        == 63    );

		fs = KFileStat(sDirectory);
		CHECK( fs.Exists()      == true  );
		CHECK( fs.Type()        == KFileType::DIRECTORY );
		CHECK( fs.IsFile()      == false );
		CHECK( fs.IsDirectory() == true  );
		CHECK( fs.Size()        == 0     );
	}

	SECTION("name manipulations")
	{
		CHECK ( kDirname("") == "." );
#ifdef DEKAF2_IS_WINDOWS
		CHECK ( kDirname("", true) == ".\\" );
#else
		CHECK ( kDirname("", true) == "./" );
#endif
		CHECK ( kDirname("",  true, false) == "" );
		CHECK ( kDirname("", false, false) == "" );
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
		CHECK ( kNormalizePath("/this.is/../../../wrong") == "" );
		auto sCWD = kGetCWD();
		KString sCompare (sCWD);
		sCompare += "/this..is/name";
		CHECK ( kNormalizePath("this..is////a/.././name") == sCompare );
		CHECK ( kNormalizePath("this..is/../..//a/.././name", true) == "" );
#endif
	}

	SECTION("KDirectory")
	{
		KDirectory Dir(sDirectory);
		CHECK ( Dir.size() == 2 );
		CHECK ( Dir.Match( KFileType::FILE) == 2);
		CHECK ( Dir.Match( KFileType::FILE, true) == 2);
		CHECK ( Dir.empty() == true );
		CHECK ( Dir.Open(sDirectory) == 2 );
		Dir.RemoveHidden();
		CHECK ( Dir.size() == 2 );
		CHECK ( Dir.Match(".*\\.test") == 1 );
		CHECK ( Dir.Match(".*\\.test", true) == 1 );
		CHECK ( Dir.empty() == true );
		CHECK ( Dir.Open(sDirectory, KFileType::FILE) == 2 );
		CHECK ( Dir.size() == 2 );
		CHECK ( Dir.Find("KFilesystem.test") != Dir.end() );
		CHECK ( Dir.Find("KFilesystem.test")->FileStat().Size() == 63 );
		CHECK ( Dir.Find("KFilesystem.test")->FileStat().Type() == KFileType::FILE );
		CHECK ( Dir.Find("KFilesystem.test")->Type() == KFileType::FILE );
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
		CHECK ( Dir.Match( KFileType::FILE) == 2);
		Dir.Sort(KDirectory::SortBy::SIZE, true);
		CHECK ( Dir[0].Filename() == "KFilesystem.test" );
		CHECK ( Dir[0].Size() == 63 );
		CHECK ( Dir[1].Filename() == "test.txt" );
		CHECK ( Dir[1].Size() == 0 );
		CHECK ( Dir[2].Filename() == "" );
		CHECK ( Dir[2].Size() == 0 );
		CHECK ( Dir.begin()->Path().find(kFormat("{}{}", kDirSep, kDirSep)) == KString::npos );
	}

	SECTION("KDirectory3")
	{
		KDirectory Dir(sDirectory);
		CHECK ( Dir.size() == 2 );
		Dir.Sort(KDirectory::SortBy::SIZE, true);
		CHECK ( Dir[0].Filename() == "KFilesystem.test" );
		CHECK ( Dir[1].Filename() == "test.txt" );
		Dir.push_back({ "/a/b/c", "new.txt", KFileType::FILE});
		CHECK ( Dir.size() == 3 );
		auto it = Dir.Find("test.txt");
		CHECK ( it != Dir.end() );
		it = Dir.erase(it);
		CHECK ( Dir.size() == 2 );
		CHECK ( it != Dir.end() );
		CHECK ( it->Filename() == "new.txt" );

		KDirectory Dir2;
		Dir2.push_back({ "/a/b/c/d", "new.txt", KFileType::FILE});
		Dir2.push_back({ "/a/b/c/d", "new2.txt", KFileType::FILE});
		Dir2.push_back({ "/a/b/c/d", "new3.txt", KFileType::FILE});

		Dir += std::move(Dir2);
		CHECK ( Dir.size() == 5 );
		Dir.erase(Dir.begin(), Dir.begin() + 2);
		CHECK ( Dir.size() == 3 );
		// note that this is one after the end
		Dir.erase(Dir.begin(), Dir.end());
		CHECK ( Dir.size() == 0 );
		CHECK ( Dir.empty() );
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

		auto tNow1 = kNow();
#ifndef DEKAF2_HAS_MUSL
		kSleep(chrono::milliseconds(100));
#else
		kSleep(chrono::milliseconds(1500));
#endif
		CHECK ( kTouchFile(sNested)   );
		auto tNow2 = kNow();
		CHECK ( kFileExists(sNested)  );
		auto tFirstTouch = KFileStat(sNested).ModificationTime();
		CHECK ( tNow1 < tFirstTouch  );
		CHECK ( tNow2 > tFirstTouch  );
#ifndef DEKAF2_HAS_MUSL
		kSleep(chrono::milliseconds(100));
#else
		kSleep(chrono::milliseconds(1500));
#endif
		CHECK ( kTouchFile(sNested)   );
		auto tSecondTouch = KFileStat(sNested).ModificationTime();
		CHECK ( tFirstTouch < tSecondTouch );
		CHECK ( kRemoveFile(sNested)  );
		CHECK ( !kFileExists(sNested) );
		CHECK ( kTouchFile(sNested)   );
		CHECK ( kFileExists(sNested)  );
	}

	SECTION("kTouchFile existing")
	{
		KString sNested { sDirectory };
		sNested += "/far/down/here2.txt";
		kWriteFile(sNested, "this is a test file");
		CHECK ( kFileExists(sNested)  );
		auto tFirstTouch = KFileStat(sNested).ModificationTime();
#ifndef DEKAF2_HAS_MUSL
		kSleep(chrono::milliseconds(100));
#else
		kSleep(chrono::milliseconds(1500));
#endif
		CHECK ( kTouchFile(sNested)   );
		CHECK ( KFileStat(sNested).Size() == 19 );
		auto tSecondTouch = KFileStat(sNested).ModificationTime();
		CHECK ( tFirstTouch < tSecondTouch );
		CHECK ( kRemoveFile(sNested)  );
		CHECK ( !kFileExists(sNested) );
		CHECK ( kTouchFile(sNested)   );
		CHECK ( kFileExists(sNested)  );
	}

	SECTION("kRename")
	{
		KString sBaseDir { sDirectory };
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

	SECTION("kCopy")
	{
		KString sBaseDir { sDirectory };
		sBaseDir += kDirSep;
		sBaseDir += "fs9238w79";

		KString sNested { sBaseDir };
		sNested += "/farer/down/here.txt";

		KString sRenamed { kDirname(sNested) + "/new.txt" };

		KString sFileinRenamedDir { sBaseDir };
		sFileinRenamedDir += "/farer/up/new.txt";

		KString sSymlink = sBaseDir;
		sSymlink += "/mysymlink";

		CHECK ( kTouchFile(sNested)              );
		CHECK ( kFileExists(sNested)             );
		CHECK ( kCreateSymlink(sNested, sSymlink));
		CHECK ( kCopy(sNested, sRenamed)         );
		CHECK ( kFileExists(sNested)             );
		CHECK ( kFileExists(sSymlink)            );
		CHECK ( kFileExists(sRenamed)            );
		CHECK ( kRemoveDir(sBaseDir) == true     );
	}

	SECTION("kCopyFile")
	{
		KString sBaseDir { sDirectory };
		sBaseDir += kDirSep;
		sBaseDir += "fs9238w79a";

		KString sNested { sBaseDir };
		sNested += "/farer/down/here.txt";

		KString sRenamed { kDirname(sNested) + "/new.txt" };

		KString sFileinRenamedDir { sBaseDir };
		sFileinRenamedDir += "/farer/up/new.txt";

		KString sSymlink = sBaseDir;
		sSymlink += "/mysymlink";

		CHECK ( kTouchFile(sNested)              );
		CHECK ( kFileExists(sNested)             );
		CHECK ( kCreateSymlink(sNested, sSymlink));
		CHECK ( kCopyFile(sNested, sRenamed)     );
		CHECK ( kCopyFile(kFormat("{}{}", sBaseDir, "/does/not/exist.txt"), kFormat("{}{}", sBaseDir, "some.txt")) == false );
		CHECK ( kFileExists(sNested)             );
		CHECK ( kFileExists(sSymlink)            );
		CHECK ( kFileExists(sRenamed)            );
		CHECK ( kRemoveDir(sBaseDir) == true     );
	}

	SECTION("kCopy Directory")
	{
		KString sBaseDir { sDirectory };
		sBaseDir += kDirSep;
		sBaseDir += "fs9238w7b";
		CHECK ( kCreateDir(kFormat("{}{}", sBaseDir, "/farer/down/and/even/more"           )) );
		CHECK ( kTouchFile(kFormat("{}{}", sBaseDir, "/farer/down/test1.txt"               )) );
		CHECK ( kTouchFile(kFormat("{}{}", sBaseDir, "/farer/down/and/test2.txt"           )) );
		CHECK ( kWriteFile(kFormat("{}{}", sBaseDir, "/farer/down/and/even/test3.txt"      ), "12345678901234567890123456789012345") );
		CHECK ( kTouchFile(kFormat("{}{}", sBaseDir, "/farer/down/and/even/more/test4.txt" )) );
		CHECK ( kCreateSymlink(kFormat("{}{}", sBaseDir, "/farer/down/and/even/test3.txt"), kFormat("{}{}", sBaseDir, "/farer/down/symlink1")) );
		CHECK ( kCopy(kFormat("{}{}", sBaseDir, "/farer/down"), kFormat("{}{}", sBaseDir, "/farer/up")) );
		CHECK ( kFileExists(kFormat("{}{}", sBaseDir, "/farer/up/and/even/more/test4.txt"  )) );
		CHECK ( kFileExists(kFormat("{}{}", sBaseDir, "/farer/up/symlink1"                 )) );
		CHECK ( KFileStat  (kFormat("{}{}", sBaseDir, "/farer/up/symlink1"), false).Type() == KFileType::FILE    );
		CHECK ( KFileStat  (kFormat("{}{}", sBaseDir, "/farer/up/symlink1"), true ).Type() == KFileType::SYMLINK );
		CHECK ( KFileStat  (kFormat("{}{}", sBaseDir, "/farer/up/symlink1"), false).Size() == 35 );
		CHECK ( KFileStat  (kFormat("{}{}", sBaseDir, "/farer/up/symlink1"), true ).Size() == 35 );
		auto sTarget = kReadLink(kFormat("{}{}", sBaseDir, "/farer/up/symlink1"), false);
		CHECK ( sTarget.remove_prefix(sBaseDir) );
		CHECK ( sTarget == "/farer/up/and/even/test3.txt" );
		auto sCanonicalBaseDir = kReadLink(sBaseDir, true);
		sTarget = kReadLink(kFormat("{}{}", sBaseDir, "/farer/up/symlink1"), true);
		CHECK ( sTarget.remove_prefix(sCanonicalBaseDir) );
		CHECK ( sTarget == "/farer/up/and/even/test3.txt" );
		CHECK ( kRemoveDir(sBaseDir) == true );
	}

	SECTION("kMove")
	{
		KString sBaseDir { sDirectory };
		sBaseDir += kDirSep;
		sBaseDir += "fs9238w75";

		KString sNested { sBaseDir };
		sNested += "/farer/down/here.txt";

		KString sRenamed { kDirname(sNested) + "/new.txt" };

		KString sFileinRenamedDir { sBaseDir };
		sFileinRenamedDir += "/farer/up/new.txt";

		CHECK ( kTouchFile(sNested)              );
		CHECK ( kFileExists(sNested)             );
		CHECK ( kMove(sNested, sRenamed)         );
		CHECK ( kFileExists(sRenamed)            );
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
		CHECK ( kIsSafePathname("hello/world.txt")       == false ); // we should allow forward slashes
		CHECK ( kIsSafePathname("hel.l-o/wor-ld.txt")    == false ); // we should allow forward slashes
		CHECK ( kIsSafePathname("hello\\world.txt")      == true  );
		CHECK ( kIsSafePathname("hel.l-o\\wor-ld.txt")   == true  );
		CHECK ( kIsSafePathname("hel.l-o\\wor_ld.txt")   == true  );
		CHECK ( kIsSafePathname("\\hello\\world.txt", true)== true  );
		CHECK ( kIsSafePathname("hello\\world\\", false, true) == true  );
		CHECK ( kIsSafePathname("\\hello\\world\\", true, true) == true  );
		CHECK ( kIsSafePathname("\\hello\\world\\\\", true, true) == false );
#else
		CHECK ( kIsSafePathname("hello/world.txt")       == true  );
		CHECK ( kIsSafePathname("hel.l-o/wor-ld.txt")    == true  );
		CHECK ( kIsSafePathname("hel.l-o/wor_ld.txt")    == true  );
		CHECK ( kIsSafePathname("/hello/world.txt")      == false );
		CHECK ( kIsSafePathname("hello/world/", false, true) == true  );
		CHECK ( kIsSafePathname("/hello/world/", true, true) == true  );
		CHECK ( kIsSafePathname("/hello/world//", true, true) == false );
#endif
		CHECK ( kIsSafePathname("hel.-lo/wor-ld.txt")    == false );
		CHECK ( kIsSafePathname("")                      == false );
		CHECK ( kIsSafePathname("//hello/world.txt", true)== false );
		CHECK ( kIsSafePathname("hello/../../world.txt") == false );
		CHECK ( kIsSafePathname("../../world.txt")       == false );
		CHECK ( kIsSafePathname("world.txt/")            == false );
		CHECK ( kIsSafePathname("world.txt..")           == false );
	}

	SECTION("kMakeSafeFilename")
	{
#ifdef DEKAF2_IS_WINDOWS
		CHECK ( kMakeSafeFilename("C:hello/world.txt")     == "hello-world.txt"      );
		CHECK ( kMakeSafeFilename("C:\\hello\\world.txt")  == "hello-world.txt"      );
		CHECK ( kMakeSafeFilename("hello/world.txt")       == "hello-world.txt"      );
		CHECK ( kMakeSafeFilename("C:/hello/world.txt")    == "hello-world.txt"      );
		CHECK ( kMakeSafeFilename("hel.-lo/wÖr_ld.txt")    == "hel.lo-wör_ld.txt"    );
		CHECK ( kMakeSafeFilename("?hel.-lo/wo?r_ld.txt")  == "hel.lo-wo-r_ld.txt"   );
		CHECK ( kMakeSafeFilename("/hello/world.txt")      == "hello-world.txt"      );
		CHECK ( kMakeSafeFilename("///hello/world.txt")    == "hello-world.txt"      );
		CHECK ( kMakeSafeFilename("hello/../../world.txt") == "hello.world.txt"      );
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
		CHECK ( kMakeSafePathname("hel.-lo/wÖr_ld.txt")    == "hel.lo\\wör_ld.txt"   );
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

	SECTION("kCreateBackupFile")
	{
		KString sDirectory = TempDir.Name();
		sDirectory += kDirSep;
		sDirectory += "backups";
		sDirectory += kDirSep;

		KString sFilename = sDirectory;
		sFilename += "test.txt";
		kTouchFile(sFilename);
		sFilename += ".old";
		kTouchFile(sFilename);
		sFilename += ".old";
		kTouchFile(sFilename);

		sFilename = sDirectory;
		sFilename += "test.txt";

		auto File = kCreateFileWithBackup(sFilename);
		CHECK ( File != nullptr );

		if (File != nullptr)
		{
			CHECK ( File->is_open() );
			File->Write("new content");
			File->close();
			KString sNewFile = kReadAll(sFilename);
			CHECK (sNewFile == "new content");
		}

		KDirectory Dir(sDirectory);

		CHECK ( Dir.size() == 4 );
		CHECK ( Dir.Find("test.txt") != Dir.end() );
		CHECK ( Dir.Find("test.txt.old") != Dir.end() );
		CHECK ( Dir.Find("test.txt.old.old") != Dir.end() );
		CHECK ( Dir.Find("test.txt.old.old.old") != Dir.end() );

		File = kCreateFileWithBackup(sFilename, "s.df");
		CHECK ( File == nullptr );
		File = kCreateFileWithBackup(sFilename, "s\\df");
		CHECK ( File == nullptr );
		File = kCreateFileWithBackup(sFilename, "s/df");
		CHECK ( File == nullptr );
		File = kCreateFileWithBackup(sFilename, "");
		CHECK ( File == nullptr );

		sFilename = sDirectory;
		sFilename += "new.txt";

		File = kCreateFileWithBackup(sFilename);
		CHECK ( File != nullptr );
		if (File != nullptr)
		{
			File->close();
		}

		Dir.Open(sDirectory);
		CHECK ( Dir.size() == 5 );
		CHECK ( Dir.Find("new.txt") != Dir.end() );

		CHECK ( kRemoveFile(sFilename) );
		CHECK ( kCreateDir(sFilename) );

		File = kCreateFileWithBackup(sFilename);
		CHECK ( File == nullptr );
		Dir.Open(sDirectory);
		CHECK ( Dir.size() == 5 );

		CHECK ( kRemoveDir(sFilename) );
		CHECK ( kTouchFile(sFilename) );
		CHECK ( kCreateDir(sFilename + ".old") );

		File = kCreateFileWithBackup(sFilename);
		CHECK ( File == nullptr );
		Dir.Open(sDirectory);
		CHECK ( Dir.size() == 6 );
	}
}
