#include "catch.hpp"

#include <dekaf2/kstream.h>
#include <dekaf2/kfdstream.h>
#include <dekaf2/ksystem.h>
#include <dekaf2/kfilesystem.h>
#include <vector>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#ifdef DEKAF2_IS_WINDOWS
	#include <io.h>
#else
	#include <unistd.h>
#endif


using namespace dekaf2;

namespace {
KTempDir TempDir;
}

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
			KInFile File(TempDir.Name() + "/this_file_should_not_exist_ASKJFHsdkfgj37r6");
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

	KString sFile = TempDir.Name();
	sFile += kDirSep;
	sFile += "KReader.test";

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
			CHECK( it.starts_with("line ") == true  );
			CHECK( it.ends_with  ("\n")    == false );
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

	SECTION("KInFile via KInStream")
	{
		std::unique_ptr<KInStream> File = std::make_unique<KInFile>(sFile);
		File->SetReaderRightTrim("");
		auto it = File->begin();
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
			CHECK( it.starts_with("line ") == true  );
			CHECK( it.ends_with  ("\n")    == false );
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
			CHECK( it.starts_with("line ") == true  );
			CHECK( it.ends_with  ("\n")    == false );
		}
		CHECK( File.eof() == true );
		CHECK( iCount == 9 );
		File.close();
		CHECK( File.is_open() == false );
	}


	SECTION("KFile read all")
	{
		KInFile File(sFile);
		KString sRead;
		CHECK( File.GetContent(sRead) == true );
		CHECK( sRead == sOut );
	}

	SECTION("KFile read iterator 1")
	{
		KInFile File(sFile);
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
		KInFile File(sFile);
		for (const auto& it : File)
		{
			CHECK( it.starts_with("line ") == true );
		}
	}

	SECTION("readline with limit")
	{
		KInFile File(sFile);
		File.SetReaderEndOfLine('|');
		File.SetReaderRightTrim("|");
		auto sLine = File.ReadLine(0);
		CHECK ( sLine.size() == 0 );
		sLine = File.ReadLine(1);
		CHECK ( sLine.size() == 1 );
		sLine = File.ReadLine(20);
		CHECK ( sLine.size() == 20 );
		sLine = File.ReadLine();
		CHECK ( sLine.size() == 41 );
		File.close();

		File.open(sFile);
		File.SetReaderEndOfLine('|');
		File.SetReaderRightTrim("");
		sLine = File.ReadLine(0);
		CHECK ( sLine.size() == 0 );
		sLine = File.ReadLine(1);
		CHECK ( sLine.size() == 1 );
		sLine = File.ReadLine(20);
		CHECK ( sLine.size() == 20 );
		sLine = File.ReadLine();
		CHECK ( sLine.size() == 41 );
	}

	SECTION("move")
	{
		static_assert(std::is_move_constructible<std::ifstream>::value, "std::ifstream has to be move constructible");

		{
			std::ifstream F1;
			F1.open(sFile.c_str());
			CHECK ( F1.is_open() );

			auto F2 = std::move(F1);
			CHECK ( F2.is_open() );

			KInStream instream(F2);
			KInStream instream2 = std::move(instream);
			KInStream& instream3 = instream2;
			auto s = instream2.ReadLine();
			CHECK ( s == "line 1" );
		}
		{
			KReader<std::ifstream> File(sFile);
			CHECK ( File.is_open() );
			auto s = File.ReadLine();
			CHECK ( s == "line 1" );
			auto File2 = std::move(File);
			CHECK ( File2.is_open() );
			s = File2.ReadLine();
			CHECK ( s == "line 2" );
			File2.close();
		}
		{
			KInFile InFile(sFile);
			CHECK ( InFile.is_open() );
			auto s = InFile.ReadLine();
			CHECK ( s == "line 1" );
			auto InFile2 = std::move(InFile);
			CHECK ( InFile2.is_open() );
			s = InFile2.ReadLine();
			CHECK ( s == "line 2" );
			InFile2.close();
		}
		{
			KInFile InFile(sFile);
			CHECK ( InFile.is_open() );
			auto s = InFile.ReadLine();
			CHECK ( s == "line 1" );
			KInStream InStream(InFile.InStream());
			CHECK ( InStream.Good() );
			s = InStream.ReadLine();
			CHECK ( s == "line 2" );
			auto InStream2 = std::move(InStream);
			CHECK ( InStream2.Good() );
			s = InStream2.ReadLine();
			CHECK ( s == "line 3" );
			auto InFile2 = std::move(InFile);
			CHECK ( InFile2.is_open() );
			s = InFile2.ReadLine();
			CHECK ( s == "line 4" );
			InFile2.close();
		}
	}

	SECTION("Seek")
	{
		KString sFile = kFormat("{}{}seek-tests", TempDir.Name(), kDirSep);
		kWriteFile(sFile, "0123456789012345678901234567890123456789012345678901234567890123456789");
		KInFile File(sFile);
		KString sBuffer;

		CHECK ( kGetReadPosition(File)     == 0    );
		CHECK ( kSetReadPosition(File, 55) == true );
		CHECK ( kGetReadPosition(File)     == 55   );
		CHECK ( kReadAll(File, false, 10)  == "5678901234" );
		CHECK ( kGetReadPosition(File)     == 65   );
		CHECK ( kRewind(File, 5)           == true );
		CHECK ( kGetReadPosition(File)     == 60   );
		CHECK ( kForward(File, 5)          == true );
		CHECK ( kGetReadPosition(File)     == 65   );
		CHECK ( kForward(File)             == true );
		CHECK ( kGetReadPosition(File)     == 70   );
		CHECK ( kRewind(File)              == true );
		CHECK ( kGetReadPosition(File)     == 0    );

		CHECK ( File.GetReadPosition()     == 0    );
		CHECK ( File.SetReadPosition(54)   == true );
		CHECK ( kGetReadPosition(File)     == 54   );
		CHECK ( File.Read(sBuffer, 10)     == 10   );
		CHECK ( sBuffer == "4567890123" );
		CHECK ( File.GetReadPosition()     == 64   );
		CHECK ( File.Rewind(5)             == true );
		CHECK ( File.GetReadPosition()     == 59   );
		CHECK ( File.Forward(5)            == true );
		CHECK ( File.GetReadPosition()     == 64   );
		CHECK ( File.Forward()             == true );
		CHECK ( File.GetReadPosition()     == 70   );
		CHECK ( File.Rewind()              == true );
		CHECK ( File.GetReadPosition()     == 0    );
	}

	SECTION("kGoToLine forward")
	{
		// 5 lines: "aaa\nbbb\nccc\nddd\neee"
		// line starts at byte: 0, 4, 8, 12, 16
		KString sPath = kFormat("{}{}gotoline-tests", TempDir.Name(), kDirSep);
		kWriteFile(sPath, "aaa\nbbb\nccc\nddd\neee");
		KInFile File(sPath);

		KString sLine;

		CHECK ( kGoToLine(File, 0) == true  );
		CHECK ( kGetReadPosition(File) == 0 );
		kReadLine(File, sLine);
		CHECK ( sLine == "aaa" );

		CHECK ( kGoToLine(File, 1) == true  );
		CHECK ( kGetReadPosition(File) == 4 );
		File.ReadLine(sLine);
		CHECK ( sLine == "bbb" );

		CHECK ( kGoToLine(File, 2) == true  );
		CHECK ( kGetReadPosition(File) == 8 );
		kReadLine(File, sLine);
		CHECK ( sLine == "ccc" );

		CHECK ( kGoToLine(File, 3) == true  );
		CHECK ( kGetReadPosition(File) == 12 );
		File.ReadLine(sLine);
		CHECK ( sLine == "ddd" );

		CHECK ( kGoToLine(File, 4) == true  );
		CHECK ( kGetReadPosition(File) == 16 );
		kReadLine(File, sLine);
		CHECK ( sLine == "eee" );

		CHECK ( kGoToLine(File, 5) == false );
	}

	SECTION("kGoToLine backward")
	{
		KString sPath = kFormat("{}{}gotoline-back-tests", TempDir.Name(), kDirSep);
		kWriteFile(sPath, "aaa\nbbb\nccc\nddd\neee");
		KInFile File(sPath);

		KString sLine;

		// line 0 from end = last line ("eee") at pos 16
		CHECK ( kGoToLine(File, 0, true) == true  );
		CHECK ( kGetReadPosition(File) == 16 );
		kReadLine(File, sLine);
		CHECK ( sLine == "eee" );

		// line 1 from end = "ddd" at pos 12
		CHECK ( kGoToLine(File, 1, true) == true  );
		CHECK ( kGetReadPosition(File) == 12 );
		File.ReadLine(sLine);
		CHECK ( sLine == "ddd" );

		// line 2 from end = "ccc" at pos 8
		CHECK ( kGoToLine(File, 2, true) == true  );
		CHECK ( kGetReadPosition(File) == 8 );
		kReadLine(File, sLine);
		CHECK ( sLine == "ccc" );

		// line 3 from end = "bbb" at pos 4
		CHECK ( kGoToLine(File, 3, true) == true  );
		CHECK ( kGetReadPosition(File) == 4 );
		File.ReadLine(sLine);
		CHECK ( sLine == "bbb" );

		// line 4 from end = "aaa" at pos 0
		CHECK ( kGoToLine(File, 4, true) == true  );
		CHECK ( kGetReadPosition(File) == 0 );
		kReadLine(File, sLine);
		CHECK ( sLine == "aaa" );

		// line 5 from end = out of range
		CHECK ( kGoToLine(File, 5, true) == false );
	}

	SECTION("kMoveToLine forward")
	{
		KString sPath = kFormat("{}{}movetoline-tests", TempDir.Name(), kDirSep);
		kWriteFile(sPath, "aaa\nbbb\nccc\nddd\neee");
		KInFile File(sPath);

		KString sLine;

		// move 0 lines = stay at current position
		CHECK ( kMoveToLine(File, 0) == true );
		CHECK ( kGetReadPosition(File) == 0 );
		kReadLine(File, sLine);
		CHECK ( sLine == "aaa" );

		// move forward 2 lines from start
		kRewind(File);
		CHECK ( kMoveToLine(File, 2) == true );
		CHECK ( kGetReadPosition(File) == 8 );
		File.ReadLine(sLine);
		CHECK ( sLine == "ccc" );

		// move forward 1 more line (ReadLine consumed up to pos 12)
		CHECK ( kMoveToLine(File, 1) == true );
		CHECK ( kGetReadPosition(File) == 16 );
		kReadLine(File, sLine);
		CHECK ( sLine == "eee" );

		// try to move forward from last line - should fail
		CHECK ( kGoToLine(File, 4) == true );
		CHECK ( kMoveToLine(File, 2) == false );
	}

	SECTION("kMoveToLine backward")
	{
		KString sPath = kFormat("{}{}movetoline-back-tests", TempDir.Name(), kDirSep);
		kWriteFile(sPath, "aaa\nbbb\nccc\nddd\neee");
		KInFile File(sPath);

		KString sLine;

		// seek to end, then backward 0 = seek to end (special case)
		kForward(File);
		CHECK ( kMoveToLine(File, 0, true) == true );
		CHECK ( kGetReadPosition(File) == 19 );

		// seek to end, then backward 1 = start of last line
		kForward(File);
		CHECK ( kMoveToLine(File, 1, true) == true );
		CHECK ( kGetReadPosition(File) == 16 );
		kReadLine(File, sLine);
		CHECK ( sLine == "eee" );

		// seek to end, then backward 3
		kForward(File);
		CHECK ( kMoveToLine(File, 3, true) == true );
		CHECK ( kGetReadPosition(File) == 8 );
		File.ReadLine(sLine);
		CHECK ( sLine == "ccc" );

		// from beginning, backward should fail
		kRewind(File);
		CHECK ( kMoveToLine(File, 1, true) == false );
	}

	SECTION("kGoToLine with trailing newline")
	{
		// file ends with newline: "aaa\nbbb\nccc\n"
		// lines: "aaa", "bbb", "ccc", and arguably an empty trailing line
		KString sPath = kFormat("{}{}gotoline-trailing-tests", TempDir.Name(), kDirSep);
		kWriteFile(sPath, "aaa\nbbb\nccc\n");
		KInFile File(sPath);

		KString sLine;

		CHECK ( kGoToLine(File, 0) == true );
		CHECK ( kGetReadPosition(File) == 0 );
		kReadLine(File, sLine);
		CHECK ( sLine == "aaa" );

		CHECK ( kGoToLine(File, 2) == true );
		CHECK ( kGetReadPosition(File) == 8 );
		File.ReadLine(sLine);
		CHECK ( sLine == "ccc" );

		CHECK ( kGoToLine(File, 3) == true );
		CHECK ( kGetReadPosition(File) == 12 );
		kReadLine(File, sLine);
		CHECK ( sLine == "" );

		// from end, line 0 = empty trailing content at pos 12
		CHECK ( kGoToLine(File, 0, true) == true );
		CHECK ( kGetReadPosition(File) == 12 );
		File.ReadLine(sLine);
		CHECK ( sLine == "" );

		// from end, line 1 = "ccc" at pos 8
		CHECK ( kGoToLine(File, 1, true) == true );
		CHECK ( kGetReadPosition(File) == 8 );
		kReadLine(File, sLine);
		CHECK ( sLine == "ccc" );
	}

	SECTION("kGoToLine single line")
	{
		KString sPath = kFormat("{}{}gotoline-single-tests", TempDir.Name(), kDirSep);
		kWriteFile(sPath, "hello");
		KInFile File(sPath);

		KString sLine;

		CHECK ( kGoToLine(File, 0) == true );
		CHECK ( kGetReadPosition(File) == 0 );
		kReadLine(File, sLine);
		CHECK ( sLine == "hello" );

		CHECK ( kGoToLine(File, 1) == false );

		CHECK ( kGoToLine(File, 0, true) == true );
		CHECK ( kGetReadPosition(File) == 0 );
		File.ReadLine(sLine);
		CHECK ( sLine == "hello" );

		CHECK ( kGoToLine(File, 1, true) == false );
	}

	SECTION("kGoToLine empty file")
	{
		KString sPath = kFormat("{}{}gotoline-empty-tests", TempDir.Name(), kDirSep);
		kWriteFile(sPath, "");
		KInFile File(sPath);

		CHECK ( kGoToLine(File, 0) == true );
		CHECK ( kGetReadPosition(File) == 0 );

		CHECK ( kGoToLine(File, 1) == false );
	}

	SECTION("KInStream::Read with count")
	{
		// exercises the resize_and_overwrite path in KInStream::Read
		KString sPath = kFormat("{}{}read-count-tests", TempDir.Name(), kDirSep);
		kWriteFile(sPath, "abcdefghijklmnopqrstuvwxyz");
		KInFile File(sPath);

		KString sBuffer;
		auto iRead = File.Read(sBuffer, 10);
		CHECK ( iRead == 10 );
		CHECK ( sBuffer.size() == 10 );
		CHECK ( sBuffer == "abcdefghij" );

		// append more
		iRead = File.Read(sBuffer, 5);
		CHECK ( iRead == 5 );
		CHECK ( sBuffer.size() == 15 );
		CHECK ( sBuffer == "abcdefghijklmno" );

		// read beyond remaining content
		iRead = File.Read(sBuffer, 100);
		CHECK ( iRead == 11 );
		CHECK ( sBuffer.size() == 26 );
		CHECK ( sBuffer == "abcdefghijklmnopqrstuvwxyz" );
	}

	SECTION("KInStream::Read large")
	{
		// exercises resize_and_overwrite with a buffer beyond SSO
		KString sPath = kFormat("{}{}read-large-tests", TempDir.Name(), kDirSep);
		KString sExpected;
		for (int i = 0; i < 100; ++i)
		{
			sExpected += "0123456789";
		}
		kWriteFile(sPath, sExpected);
		KInFile File(sPath);

		KString sBuffer;
		auto iRead = File.Read(sBuffer, 1000);
		CHECK ( iRead == 1000 );
		CHECK ( sBuffer == sExpected );
	}

	SECTION("kReadAll from file")
	{
		// exercises the resize_and_overwrite path in kAppendAll
		KString sPath = kFormat("{}{}readall-tests", TempDir.Name(), kDirSep);
		kWriteFile(sPath, "the quick brown fox");

		KString sContent = kReadAll(sPath);
		CHECK ( sContent == "the quick brown fox" );
	}

	SECTION("kAppendAll preserves existing content")
	{
		KString sPath = kFormat("{}{}appendall-tests", TempDir.Name(), kDirSep);
		kWriteFile(sPath, " jumps over the lazy dog");

		KString sContent = "the quick brown fox";
		KInFile File(sPath);
		kAppendAll(File.istream(), sContent, true);
		CHECK ( sContent == "the quick brown fox jumps over the lazy dog" );
	}
}
