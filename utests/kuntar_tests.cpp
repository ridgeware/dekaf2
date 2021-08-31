#include "catch.hpp"

#include <dekaf2/kuntar.h>
#include <dekaf2/kwriter.h>
#include <dekaf2/kfilesystem.h>
#include <dekaf2/ksystem.h>
#include <dekaf2/kinshell.h>
#include <vector>

#ifndef DEKAF2_IS_WINDOWS

using namespace dekaf2;

namespace {
KTempDir TempDir;
}

TEST_CASE("KUnTar")
{
	KString sBaseDir = TempDir.Name();
	sBaseDir += kDirSep;

	SECTION("Create archive")
	{
		KString sTarDir = sBaseDir + "myfolder";

		CHECK ( kCreateDir(sTarDir) == true );

		sTarDir += kDirSep;

		kSetCWD(sBaseDir);

		{
			KOutFile File(sTarDir + "file1.txt");
			File.WriteLine("this is line 1");
			File.WriteLine("this is line 2");
		}
		CHECK ( kFileExists(sTarDir + "file1.txt") );
		{
			KInShell Shell(kFormat("cd {} && tar -c -f test1.tar myfolder", sBaseDir));
		}
		{
			KOutFile File(sTarDir + "file2.txt");
			File.WriteLine("this is another line 1");
			File.WriteLine("this is another line 2");
		}
		CHECK ( kFileExists(sTarDir + "file2.txt") );
		{
			KInShell Shell(kFormat("cd {} && tar -r -f test1.tar myfolder/file2.txt", sBaseDir));
		}
		{
			KOutFile File(sTarDir + "filé3.txt");
			File.WriteLine("this is yet another line 1");
			File.WriteLine("this is yet another line 2");
		}
		CHECK ( kFileExists(sTarDir + "filé3.txt") );
		{
			kSetCWD(sTarDir);
			kCreateSymlink ("file2.txt", "symlink.txt");
			kCreateHardlink("file2.txt", "hardlink.txt");
			kSetCWD(sBaseDir);
		}
		{
			KInShell Shell(kFormat("cd {} && tar -r -f test1.tar myfolder/filé3.txt", sBaseDir));
		}
		{
			KInShell Shell(kFormat("cd {} && tar -r -f test1.tar myfolder/symlink.txt", sBaseDir));
		}
		{
			KInShell Shell(kFormat("cd {} && tar -r -f test1.tar myfolder/hardlink.txt", sBaseDir));
		}
		{
			KInShell Shell(kFormat("cd {} && cp test1.tar test2.tar && cp test1.tar test3.tar", sBaseDir));
		}
		{
			KInShell Shell(kFormat("cd {} && gzip test2.tar && mv test2.tar.gz test2.tgz", sBaseDir));
		}
		{
			KInShell Shell(kFormat("cd {} && bzip2 test3.tar", sBaseDir));
		}

		CHECK ( kFileExists(sBaseDir + "test1.tar") );
		CHECK ( kFileExists(sBaseDir + "test2.tgz") );
		CHECK ( kFileExists(sBaseDir + "test3.tar.bz2") );
	}

	SECTION("uncompressed")
	{
		KUnTar untar(sBaseDir + "test1.tar");

		CHECK ( untar.Next() );
		CHECK ( untar.Filename() == "myfolder/" );
		CHECK ( untar.Filesize() == 0 );
		CHECK ( untar.Type() == tar::Directory );
		CHECK ( untar.Error() == "" );

		CHECK ( untar.Next() );
		CHECK ( untar.Filename() == "myfolder/file1.txt" );
		CHECK ( untar.Filesize() == 30 );
		CHECK ( untar.Type() == tar::File );
		KString sContent;
		CHECK ( untar.Read(sContent) );
		CHECK ( sContent == "this is line 1\nthis is line 2\n" );
		CHECK ( untar.Error() == "" );

		CHECK ( untar.Next() );
		CHECK ( untar.Filename() == "myfolder/file2.txt" );
		CHECK ( untar.Filesize() == 46 );
		CHECK ( untar.Type() == tar::File );
		CHECK ( untar.Read(sContent) );
		CHECK ( sContent == "this is another line 1\nthis is another line 2\n" );
		CHECK ( untar.Error() == "" );

		CHECK ( untar.Next() );
		CHECK ( untar.Filename() == "myfolder/filé3.txt" );
		CHECK ( untar.Filesize() == 54 );
		CHECK ( untar.Type() == tar::File );
		CHECK ( untar.Read(sContent) );
		CHECK ( sContent == "this is yet another line 1\nthis is yet another line 2\n" );
		CHECK ( untar.Error() == "" );

		CHECK ( untar.Next() );
		CHECK ( untar.Filename() == "myfolder/symlink.txt" );
		CHECK ( untar.Filesize() == 0 );
		CHECK ( untar.Type() == tar::Symlink );
		CHECK ( untar.Error() == "" );

		CHECK ( untar.Next() );
		CHECK ( untar.Filename() == "myfolder/hardlink.txt" );
		CHECK ( untar.Filesize() == 46 );
		CHECK ( untar.Type() == tar::File );
		CHECK ( untar.Error() == "" );

		CHECK ( untar.Next() == false );
	}

	SECTION("gzip compressed")
	{
		KUnTarCompressed untar(sBaseDir + "test2.tgz");

		CHECK ( untar.Next() );
		CHECK ( untar.Filename() == "myfolder/" );
		CHECK ( untar.Filesize() == 0 );
		CHECK ( untar.Type() == tar::Directory );
		CHECK ( untar.Error() == "" );

		CHECK ( untar.Next() );
		CHECK ( untar.Filename() == "myfolder/file1.txt" );
		CHECK ( untar.Filesize() == 30 );
		CHECK ( untar.Type() == tar::File );
		KString sContent;
		CHECK ( untar.Read(sContent) );
		CHECK ( sContent == "this is line 1\nthis is line 2\n" );
		CHECK ( untar.Error() == "" );

		CHECK ( untar.Next() );
		CHECK ( untar.Filename() == "myfolder/file2.txt" );
		CHECK ( untar.Filesize() == 46 );
		CHECK ( untar.Type() == tar::File );
		CHECK ( untar.Read(sContent) );
		CHECK ( sContent == "this is another line 1\nthis is another line 2\n" );
		CHECK ( untar.Error() == "" );

		CHECK ( untar.Next() );
		CHECK ( untar.Filename() == "myfolder/filé3.txt" );
		CHECK ( untar.Filesize() == 54 );
		CHECK ( untar.Type() == tar::File );
		CHECK ( untar.Read(sContent) );
		CHECK ( sContent == "this is yet another line 1\nthis is yet another line 2\n" );
		CHECK ( untar.Error() == "" );

		CHECK ( untar.Next() );
		CHECK ( untar.Filename() == "myfolder/symlink.txt" );
		CHECK ( untar.Filesize() == 0 );
		CHECK ( untar.Type() == tar::Symlink );
		CHECK ( untar.Error() == "" );

		CHECK ( untar.Next() );
		CHECK ( untar.Filename() == "myfolder/hardlink.txt" );
		CHECK ( untar.Filesize() == 46 );
		CHECK ( untar.Type() == tar::File );
		CHECK ( untar.Error() == "" );

		CHECK ( untar.Next() == false );
		CHECK ( untar.Error() == "" );
	}

	SECTION("bzip2 compressed")
	{
		KUnTarBZip2 untar(sBaseDir + "test3.tar.bz2");

		CHECK ( untar.Next() );
		CHECK ( untar.Filename() == "myfolder/" );
		CHECK ( untar.Filesize() == 0 );
		CHECK ( untar.Type() == tar::Directory );
		CHECK ( untar.Error() == "" );

		CHECK ( untar.Next() );
		CHECK ( untar.Filename() == "myfolder/file1.txt" );
		CHECK ( untar.Filesize() == 30 );
		CHECK ( untar.Type() == tar::File );
		KString sContent;
		CHECK ( untar.Read(sContent) );
		CHECK ( sContent == "this is line 1\nthis is line 2\n" );
		CHECK ( untar.Error() == "" );

		CHECK ( untar.Next() );
		CHECK ( untar.Filename() == "myfolder/file2.txt" );
		CHECK ( untar.Filesize() == 46 );
		CHECK ( untar.Type() == tar::File );
		CHECK ( untar.Read(sContent) );
		CHECK ( sContent == "this is another line 1\nthis is another line 2\n" );
		CHECK ( untar.Error() == "" );

		CHECK ( untar.Next() );
		CHECK ( untar.Filename() == "myfolder/filé3.txt" );
		CHECK ( untar.Filesize() == 54 );
		CHECK ( untar.Type() == tar::File );
		CHECK ( untar.Read(sContent) );
		CHECK ( sContent == "this is yet another line 1\nthis is yet another line 2\n" );
		CHECK ( untar.Error() == "" );

		CHECK ( untar.Next() );
		CHECK ( untar.Filename() == "myfolder/symlink.txt" );
		CHECK ( untar.Filesize() == 0 );
		CHECK ( untar.Type() == tar::Symlink );
		CHECK ( untar.Error() == "" );

		CHECK ( untar.Next() );
		CHECK ( untar.Filename() == "myfolder/hardlink.txt" );
		CHECK ( untar.Filesize() == 46 );
		CHECK ( untar.Type() == tar::File );
		CHECK ( untar.Error() == "" );

		CHECK ( untar.Next() == false );
		CHECK ( untar.Error() == "" );
	}

	SECTION("bzip2 compressed, all types")
	{
		KUnTarBZip2 untar(sBaseDir + "test3.tar.bz2");

		CHECK ( untar.Next() );
		CHECK ( untar.Filename() == "myfolder/" );
		CHECK ( untar.Filesize() == 0 );
		CHECK ( untar.Type() == tar::Directory );
		CHECK ( untar.Error() == "" );

		CHECK ( untar.Next() );
		CHECK ( untar.Filename() == "myfolder/file1.txt" );
		CHECK ( untar.Filesize() == 30 );
		CHECK ( untar.Type() == tar::File );
		KString sContent;
		CHECK ( untar.Read(sContent) );
		CHECK ( sContent == "this is line 1\nthis is line 2\n" );
		CHECK ( untar.Error() == "" );

		CHECK ( untar.Next() );
		CHECK ( untar.Filename() == "myfolder/file2.txt" );
		CHECK ( untar.Filesize() == 46 );
		CHECK ( untar.Type() == tar::File );
		CHECK ( untar.Read(sContent) );
		CHECK ( sContent == "this is another line 1\nthis is another line 2\n" );
		CHECK ( untar.Error() == "" );

		CHECK ( untar.Next() );
		CHECK ( untar.Filename() == "myfolder/filé3.txt" );
		CHECK ( untar.Filesize() == 54 );
		CHECK ( untar.Type() == tar::File );
		CHECK ( untar.Read(sContent) );
		CHECK ( sContent == "this is yet another line 1\nthis is yet another line 2\n" );
		CHECK ( untar.Error() == "" );

		CHECK ( untar.Next() );
		CHECK ( untar.Filename() == "myfolder/symlink.txt" );
		CHECK ( untar.Filesize() == 0 );
		CHECK ( untar.Type() == tar::Symlink );
		CHECK ( untar.Error() == "" );

		CHECK ( untar.Next() );
		CHECK ( untar.Filename() == "myfolder/hardlink.txt" );
		CHECK ( untar.Filesize() == 46 );
		CHECK ( untar.Type() == tar::File );
		CHECK ( untar.Error() == "" );

		CHECK ( untar.Next() == false );
		CHECK ( untar.Error() == "" );
	}

	SECTION("uncompressed, iterator access")
	{
		std::size_t iCount = 0;
		KUnTar untar(sBaseDir + "test1.tar");

		for (auto& File : untar)
		{
			KString sContent;

			switch (++iCount)
			{
				case 1:
					CHECK ( File.Filename() == "myfolder/" );
					CHECK ( File.Filesize() == 0 );
					CHECK ( File.Type()     == tar::Directory );
					break;
				case 2:
					CHECK ( File.Filename() == "myfolder/file1.txt" );
					CHECK ( File.Filesize() == 30 );
					CHECK ( File.Type()     == tar::File );
					CHECK ( File.Read(sContent) );
					CHECK ( sContent == "this is line 1\nthis is line 2\n" );
					break;
				case 3:
					CHECK ( File.Filename() == "myfolder/file2.txt" );
					CHECK ( File.Filesize() == 46 );
					CHECK ( File.Type()     == tar::File );
					CHECK ( File.Read(sContent) );
					CHECK ( sContent == "this is another line 1\nthis is another line 2\n" );
					break;
				case 4:
					CHECK ( File.Filename() == "myfolder/filé3.txt" );
					CHECK ( File.Filesize() == 54 );
					CHECK ( File.Type()     == tar::File );
					CHECK ( File.Read(sContent) );
					CHECK ( sContent == "this is yet another line 1\nthis is yet another line 2\n" );
					break;

				case 5:
					CHECK ( File.Filename() == "myfolder/symlink.txt" );
					CHECK ( File.Filesize() == 0 );
					CHECK ( File.Type()     == tar::Symlink );
					break;

				case 6:
					CHECK ( File.Filename() == "myfolder/hardlink.txt" );
					CHECK ( File.Filesize() == 46 );
					CHECK ( File.Type()     == tar::File );
					break;
			}

			CHECK ( File.Error() == "" );
		}
	}

	SECTION("bzip2 compressed, iterator access")
	{
		std::size_t iCount = 0;
		KUnTarBZip2 untar(sBaseDir + "test3.tar.bz2");

		for (auto& File : untar)
		{
			KString sContent;

			switch (++iCount)
			{
				case 1:
					CHECK ( File.Filename() == "myfolder/" );
					CHECK ( File.Filesize() == 0 );
					CHECK ( File.Type()     == tar::Directory );
					break;
				case 2:
					CHECK ( File.Filename() == "myfolder/file1.txt" );
					CHECK ( File.Filesize() == 30 );
					CHECK ( File.Type()     == tar::File );
					CHECK ( File.Read(sContent) );
					CHECK ( sContent == "this is line 1\nthis is line 2\n" );
					break;
				case 3:
					CHECK ( File.Filename() == "myfolder/file2.txt" );
					CHECK ( File.Filesize() == 46 );
					CHECK ( File.Type()     == tar::File );
					CHECK ( File.Read(sContent) );
					CHECK ( sContent == "this is another line 1\nthis is another line 2\n" );
					break;
				case 4:
					CHECK ( File.Filename() == "myfolder/filé3.txt" );
					CHECK ( File.Filesize() == 54 );
					CHECK ( File.Type()     == tar::File );
					CHECK ( File.Read(sContent) );
					CHECK ( sContent == "this is yet another line 1\nthis is yet another line 2\n" );
					break;
				case 5:
					CHECK ( File.Filename() == "myfolder/symlink.txt" );
					CHECK ( File.Filesize() == 0 );
					CHECK ( File.Type()     == tar::Symlink );
					break;
				case 6:
					CHECK ( File.Filename() == "myfolder/hardlink.txt" );
					CHECK ( File.Filesize() == 46 );
					CHECK ( File.Type()     == tar::File );
					break;
			}

			CHECK ( File.Error() == "" );
		}
	}

	SECTION("uncompressed, extract all files")
	{
		KTempDir TempDir;
		KUnTar untar(sBaseDir + "test1.tar");
		CHECK ( untar.ReadAll(TempDir.Name()) );

		// get a recursive directory listing
		KDirectory Files(TempDir.Name(), KFileTypes::ALL, true);
		Files.Sort();
		CHECK ( Files.size() == 6 );

		std::size_t iCount = 0;
		for (auto& File : Files)
		{
			auto sPath = File.Path().ToView(TempDir.Name().size() + 1);

			switch (++iCount)
			{
				case 1:
					CHECK ( sPath       == "myfolder" );
					CHECK ( File.Size() == 0 );
					CHECK ( File.Type() == KFileType::DIRECTORY );
					break;
				case 2:
					CHECK ( sPath       == "myfolder/file1.txt" );
					CHECK ( File.Size() == 30 );
					CHECK ( File.Type() == KFileType::FILE );
					CHECK ( kReadAll(File.Path()) == "this is line 1\nthis is line 2\n" );
					break;
				case 3:
					CHECK ( sPath       == "myfolder/file2.txt" );
					CHECK ( File.Size() == 46 );
					CHECK ( File.Type() == KFileType::FILE );
					CHECK ( kReadAll(File.Path()) == "this is another line 1\nthis is another line 2\n" );
					break;
				case 4:
					CHECK ( sPath       == "myfolder/filé3.txt" );
					CHECK ( File.Size() == 54 );
					CHECK ( File.Type() == KFileType::FILE );
					CHECK ( kReadAll(File.Path()) == "this is yet another line 1\nthis is yet another line 2\n" );
					break;
				case 5:
					CHECK ( sPath       == "myfolder/hardlink.txt" );
					CHECK ( File.Size() == 46 );
					CHECK ( File.Type() == KFileType::FILE );
					CHECK ( kReadAll(File.Path()) == "this is another line 1\nthis is another line 2\n" );
					break;
				case 6:
					CHECK ( sPath       == "myfolder/symlink.txt" );
					CHECK ( File.Size() == 46 );
					CHECK ( File.Type() == KFileType::SYMLINK );
					CHECK ( kReadAll(File.Path()) == "this is another line 1\nthis is another line 2\n" );
					break;
			}
		}
	}

	SECTION("bzip2 compressed, extract all files")
	{
		KTempDir TempDir;
		KUnTarBZip2 untar(sBaseDir + "test3.tar.bz2");
		CHECK ( untar.ReadAll(TempDir.Name()) );

		// get a recursive directory listing
		KDirectory Files(TempDir.Name(), KFileTypes::ALL, true);
		Files.Sort();
		CHECK ( Files.size() == 6 );

		std::size_t iCount = 0;
		for (auto& File : Files)
		{
			auto sPath = File.Path().ToView(TempDir.Name().size() + 1);

			switch (++iCount)
			{
				case 1:
					CHECK ( sPath       == "myfolder" );
					CHECK ( File.Size() == 0 );
					CHECK ( File.Type() == KFileType::DIRECTORY );
					break;
				case 2:
					CHECK ( sPath       == "myfolder/file1.txt" );
					CHECK ( File.Size() == 30 );
					CHECK ( File.Type() == KFileType::FILE );
					CHECK ( kReadAll(File.Path()) == "this is line 1\nthis is line 2\n" );
					break;
				case 3:
					CHECK ( sPath       == "myfolder/file2.txt" );
					CHECK ( File.Size() == 46 );
					CHECK ( File.Type() == KFileType::FILE );
					CHECK ( kReadAll(File.Path()) == "this is another line 1\nthis is another line 2\n" );
					break;
				case 4:
					CHECK ( sPath       == "myfolder/filé3.txt" );
					CHECK ( File.Size() == 54 );
					CHECK ( File.Type() == KFileType::FILE );
					CHECK ( kReadAll(File.Path()) == "this is yet another line 1\nthis is yet another line 2\n" );
					break;
				case 5:
					CHECK ( sPath       == "myfolder/hardlink.txt" );
					CHECK ( File.Size() == 46 );
					CHECK ( File.Type() == KFileType::FILE );
					CHECK ( kReadAll(File.Path()) == "this is another line 1\nthis is another line 2\n" );
					break;
				case 6:
					CHECK ( sPath       == "myfolder/symlink.txt" );
					CHECK ( File.Size() == 46 );
					CHECK ( File.Type() == KFileType::SYMLINK );
					CHECK ( kReadAll(File.Path()) == "this is another line 1\nthis is another line 2\n" );
					break;
			}
		}
	}
}

#endif
