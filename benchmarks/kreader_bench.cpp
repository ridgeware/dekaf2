
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dekaf2/kwriter.h>
#include <dekaf2/kreader.h>
#include <dekaf2/kfdstream.h>
#include <dekaf2/kstring.h>
#include <dekaf2/kprof.h>
#include <dekaf2/kfilesystem.h>
#include <dekaf2/ksystem.h>
#include <dekaf2/kbufferedreader.h>
#include <dekaf2/kinstringstream.h>
#include <iostream>
#include <fstream>
#ifdef DEKAF2_IS_WINDOWS
	#include <io.h>
#else
	#include <unistd.h>
#endif

using namespace dekaf2;

void compare_readers()
{
	KString filename = kGetTemp();
	filename += kDirSep;
	filename += "dekaf2_reader_test";
	KString s("1234567890123456789012345678901234567890123456789\n");

	KProf pp("-Readers");

	{
		// create a test file
		KOutFile of(filename);
		for (int ct = 0; ct < 100000; ++ct)
		{
			of.Write(s);
		}
	}

	{
		int fd = ::open(filename.c_str(), O_RDONLY);
		if (fd >= 0)
		{
			char Ch;
			KProf prof("read FileDesc, single chars");
			prof.SetMultiplier(500000);
			for (int ct = 0; ct < 500000; ++ct)
			{
				::read(fd, &Ch, 1);
				prof.Force();
			}
			if (Ch == 'x') std::cout << "nope";
		}
		close(fd);
	}
	{
		FILE* fp = fopen(filename.c_str(), "r");
		if (fp)
		{
			char Ch;
			KProf prof("read FILE*, single chars");
			prof.SetMultiplier(500000);
			for (int ct = 0; ct < 500000; ++ct)
			{
				Ch = fgetc(fp);
				prof.Force();
			}
			if (Ch == 'x') std::cout << "nope";
		}
		fclose(fp);
	}
	{
		for (int x = 0; x < 100; ++x)
		{
			std::ifstream is(filename.c_str());
			if (is.is_open())
			{
				char Ch;
				KProf prof("read std::ifstream, single chars");
				prof.SetMultiplier(500000);
				for (int ct = 0; ct < 500000; ++ct)
				{
					Ch = is.get();
					prof.Force();
				}
				if (Ch == 'x') std::cout << "nope";
			}
		}
	}
	{
		for (int x = 0; x < 100; ++x)
		{
			KInFile is(filename);
			if (is.is_open())
			{
				char Ch;
				KProf prof("read KInFile, single chars");
				prof.SetMultiplier(500000);
				for (int ct = 0; ct < 500000; ++ct)
				{
					Ch = is.Read();
					prof.Force();
				}
				if (Ch == 'x') std::cout << "nope";
			}
		}
	}
	{
		for (int x = 0; x < 100; ++x)
		{
			KInFile is(filename);
			if (is.is_open())
			{
				std::istream::int_type Ch;
				std::streambuf* sb = is.istream().rdbuf();
				if (sb)
				{
					KProf prof("read std::streambuf, single chars");
					prof.SetMultiplier(500000);
					for (int ct = 0; ct < 500000; ++ct)
					{
						Ch = sb->sbumpc();
						prof.Force();
					}
					if (Ch == 'x') std::cout << "nope";
				}
			}
		}
	}
	{
		for (int x = 0; x < 100; ++x)
		{
			KInFile File(filename);
			KBufferedStreamReader is(File);
			std::istream::int_type Ch;
			KProf prof("read KBufferedStreamReader, single chars");
			prof.SetMultiplier(500000);
			for (int ct = 0; ct < 500000; ++ct)
			{
				Ch = is.Read();
				prof.Force();
			}
			if (Ch == 'x') std::cout << "nope";
		}
	}
	{
		for (int x = 0; x < 100; ++x)
		{
			KBufferedFileReader is(filename);
			std::istream::int_type Ch;
			KProf prof("read KBufferedFileReader, single chars");
			prof.SetMultiplier(500000);
			for (int ct = 0; ct < 500000; ++ct)
			{
				Ch = is.Read();
				prof.Force();
			}
			if (Ch == 'x') std::cout << "nope";
		}
	}
	{
		KString sFile;
		kReadAll(filename, sFile);
		for (int x = 0; x < 100; ++x)
		{
			KInStringStream is(sFile);
			std::istream::int_type Ch;
			KProf prof("read KInStringStream, single chars");
			prof.SetMultiplier(500000);
			for (int ct = 0; ct < 500000; ++ct)
			{
				Ch = is.Read();
				prof.Force();
			}
			if (Ch == 'x') std::cout << "nope";
		}
	}
	{
		KString sFile;
		kReadAll(filename, sFile);
		for (int x = 0; x < 100; ++x)
		{
			KBufferedStringReader is(sFile);
			std::istream::int_type Ch;
			KProf prof("read KBufferedStringReader on string, single chars");
			prof.SetMultiplier(500000);
			for (int ct = 0; ct < 500000; ++ct)
			{
				Ch = is.Read();
				prof.Force();
			}
			if (Ch == 'x') std::cout << "nope";
		}
	}
	{
		FILE* fp = std::fopen(filename.c_str(), "r");
		KFPReader is(fp);
		if (is.is_open())
		{
			char Ch;
			KProf prof("read KFPReader, single chars");
			prof.SetMultiplier(500000);
			for (int ct = 0; ct < 500000; ++ct)
			{
				Ch = is.Read();
				prof.Force();
			}
			if (Ch == 'x') std::cout << "nope";
		}
		is.close();
	}
	{
		char Ch;
		int fd = ::open(filename.c_str(), O_RDONLY);
		KFDReader is(fd);
		if (is.is_open())
		{
			KProf prof("read KFDReader, single chars");
			prof.SetMultiplier(500000);
			for (int ct = 0; ct < 500000; ++ct)
			{
				Ch = is.Read();
				prof.Force();
			}
			if (Ch == 'x') std::cout << "nope";
		}
		is.close();
	}
	{
		int fd = ::open(filename.c_str(), O_RDONLY);
		if (fd >= 0)
		{
			char Ch;
			KString line;
			KProf prof("read FileDesc, single lines");
			prof.SetMultiplier(10000);
			for (int ct = 0; ct < 10000; ++ct)
			{
				line.clear();
				for(;;)
				{
					::read(fd, &Ch, 1);
					line += Ch;
					if (Ch == '\n')
					{
						break;
					}
					prof.Force();
				}
			}
		}
		close(fd);
	}
#ifndef DEKAF2_IS_WINDOWS
	{
		for (int x = 0; x < 100; ++x)
		{
			FILE* fp = fopen(filename.c_str(), "r");
			if (fp)
			{
				char* szLine = static_cast<char*>(malloc(1000));
				KProf prof("read FILE*, single lines w/o copy");
				prof.SetMultiplier(10000);
				for (int ct = 0; ct < 10000; ++ct)
				{
					size_t rb = 999;
					::getline(&szLine, &rb, fp);
					prof.Force();
				}
				free(szLine);
			}
			fclose(fp);
		}
	}
#endif
#ifndef DEKAF2_IS_WINDOWS
	{
		for (int x = 0; x < 100; ++x)
		{
			FILE* fp = fopen(filename.c_str(), "r");
			if (fp)
			{
				KString line;
				char* szLine = static_cast<char*>(malloc(1000));
				KProf prof("read FILE*, single lines w/ copy");
				prof.SetMultiplier(10000);
				for (int ct = 0; ct < 10000; ++ct)
				{
					line.clear();
					size_t rb = 999;
					::getline(&szLine, &rb, fp);
					line = szLine;
					prof.Force();
				}
				free(szLine);
			}
			fclose(fp);
		}
	}
#endif
	{
		for (int x = 0; x < 100; ++x)
		{
			std::ifstream is(filename.c_str());
			if (is.is_open())
			{
				KString line;
				KProf prof("read std::ifstream, single lines");
				prof.SetMultiplier(10000);
				for (int ct = 0; ct < 10000; ++ct)
				{
					std::getline(is, line);
					prof.Force();
				}
			}
			is.close();
		}
	}
	{
		for (int x = 0; x < 100; ++x)
		{
			KInFile is(filename);
			if (is.is_open())
			{
				is.SetReaderRightTrim("");
				KString line;
				KProf prof("read KInFile, single lines");
				prof.SetMultiplier(10000);
				for (int ct = 0; ct < 10000; ++ct)
				{
					is.ReadLine(line);
					prof.Force();
				}
			}
			is.close();
		}
	}
	{
		for (int x = 0; x < 100; ++x)
		{
			KInFile File(filename);
			KBufferedStreamReader is(File);
			KStringView line;
			KProf prof("read KBufferedStreamReader, single lines into views");
			prof.SetMultiplier(10000);
			for (int ct = 0; ct < 10000; ++ct)
			{
				is.ReadLine(line, '\n', "");
				prof.Force();
			}
		}
	}
	{
		for (int x = 0; x < 100; ++x)
		{
			KInFile File(filename);
			KBufferedStreamReader is(File);
			KString line;
			KProf prof("read KBufferedStreamReader, single lines into strings");
			prof.SetMultiplier(10000);
			for (int ct = 0; ct < 10000; ++ct)
			{
				is.ReadLine(line, '\n', "");
				prof.Force();
			}
		}
	}
	{
		for (int x = 0; x < 100; ++x)
		{
			KBufferedFileReader is(filename);
			KStringView line;
			KProf prof("read KBufferedFileReader, single lines into views");
			prof.SetMultiplier(10000);
			for (int ct = 0; ct < 10000; ++ct)
			{
				is.ReadLine(line, '\n', "");
				prof.Force();
			}
			if (line != s && x == 0) { KErr.FormatLine("sv line != s: {}", line); }
		}
	}
	{
		for (int x = 0; x < 100; ++x)
		{
			KBufferedFileReader is(filename);
			KString line;
			KProf prof("read KBufferedFileReader, single lines into strings");
			prof.SetMultiplier(10000);
			for (int ct = 0; ct < 10000; ++ct)
			{
				is.ReadLine(line, '\n', "");
				prof.Force();
			}
			if (line != s && x == 0) { KErr.FormatLine("line != s: {}", line); }
		}
	}
	{
		for (int x = 0; x < 100; ++x)
		{
			FILE* fp = std::fopen(filename.c_str(), "r");
			KFPReader is(fp);
			if (is.is_open())
			{
				KString line;
				KProf prof("read KFPReader, single lines");
				prof.SetMultiplier(10000);
				for (int ct = 0; ct < 10000; ++ct)
				{
					is.ReadLine(line);
					prof.Force();
				}
			}
			is.close();
		}
	}
	{
		for (int x = 0; x < 100; ++x)
		{
			int fd = ::open(filename.c_str(), O_RDONLY);
			KFDReader is(fd);
			if (is.is_open())
			{
				KString line;
				KProf prof("read KFDReader, single lines");
				prof.SetMultiplier(10000);
				for (int ct = 0; ct < 10000; ++ct)
				{
					is.ReadLine(line);
					prof.Force();
				}
			}
			is.close();
		}
	}
	{
		size_t fsize = kGetSize(filename.c_str());
		KProf prof("read FileDesc, full file");
		prof.SetMultiplier(1000);
		for (int ct = 0; ct < 1000; ++ct)
		{
			int fd = ::open(filename.c_str(), O_RDONLY);
			if (fd >= 0)
			{
				char* szBuffer = static_cast<char*>(malloc(fsize));
				read(fd, szBuffer, fsize);
				KProf::Force(szBuffer);
				close(fd);
				prof.Force();
				free(szBuffer);
			}
		}
	}
#if 0
	{
		size_t fsize = kGetSize(filename.c_str());
		KProf prof("read FileDesc, full file into KString");
		prof.SetMultiplier(1000);
		for (int ct = 0; ct < 1000; ++ct)
		{
			int fd = ::open(filename.c_str(), O_RDONLY);
			if (fd >= 0)
			{
				char* szBuffer = static_cast<char*>(malloc(fsize+1));
				read(fd, szBuffer, fsize);
				KProf::Force(szBuffer);
				KString str(szBuffer, fsize, fsize+1, KString::AcquireMallocatedString{});
				close(fd);
				KProf::Force(&str);
				prof.Force();
			}
		}
	}
#endif
	{
		size_t fsize = kGetSize(filename.c_str());
		KProf prof("read FileDesc, full file into resized KString");
		prof.SetMultiplier(1000);
		for (int ct = 0; ct < 1000; ++ct)
		{
			int fd = ::open(filename.c_str(), O_RDONLY);
			if (fd >= 0)
			{
				KString str;
				str.resize(fsize);
				read(fd, str.data(), fsize);
				close(fd);
				KProf::Force(&str);
				prof.Force();
			}
		}
	}
	{
		size_t fsize = kGetSize(filename.c_str());
		KProf prof("read FileDesc, full file into resized uninit KString");
		prof.SetMultiplier(1000);
		for (int ct = 0; ct < 1000; ++ct)
		{
			int fd = ::open(filename.c_str(), O_RDONLY);
			if (fd >= 0)
			{
				KString str;
				str.resize_uninitialized(fsize);
				read(fd, str.data(), fsize);
				close(fd);
				KProf::Force(&str);
				prof.Force();
			}
		}
	}
	{
		size_t fsize = kGetSize(filename.c_str());
		KProf prof("read FILE*, full file");
		prof.SetMultiplier(1000);
		for (int ct = 0; ct < 1000; ++ct)
		{
			FILE* fp = fopen(filename.c_str(), "r");
			if (fp)
			{
				char* szBuffer = static_cast<char*>(malloc(fsize));
				fread(szBuffer, fsize, 1, fp);
				KProf::Force(szBuffer);
				prof.Force();
				free(szBuffer);
				fclose(fp);
			}
		}
	}
	{
		KProf prof("read KInFile, full file");
		prof.SetMultiplier(1000);
		for (int ct = 0; ct < 1000; ++ct)
		{
			KInFile is(filename);
			if (is.is_open())
			{
				KString buffer;
				is.ReadRemaining(buffer);
				KProf::Force(&buffer);
				prof.Force();
			}
		}
	}
#if 0
	{
		size_t fsize = kGetSize(filename.c_str());
		KProf prof("read KInFile, full file manually");
		prof.SetMultiplier(1000);
		for (int ct = 0; ct < 1000; ++ct)
		{
			KInFile is(filename);
			if (is.is_open())
			{
				KString buffer(static_cast<char*>(malloc(fsize+1)), fsize, fsize+1, KString::AcquireMallocatedString{});
				is.Read(&buffer[0], fsize);
				KProf::Force(&buffer);
				prof.Force();
			}
		}
	}
#endif
	{
		KProf prof("kReadAll, full file");
		prof.SetMultiplier(1000);
		for (int ct = 0; ct < 1000; ++ct)
		{
			KString buffer;
			kReadAll(filename, buffer);
			KProf::Force(&buffer);
			prof.Force();
		}
	}
	{
		KProf prof("kReadAll from KInFile, full file");
		prof.SetMultiplier(1000);
		for (int ct = 0; ct < 1000; ++ct)
		{
			KString buffer;
			KInFile is(filename);
			kReadAll(is, buffer);
			KProf::Force(&buffer);
			prof.Force();
		}
	}


	kRemoveFile(filename);

}

void kreader_bench()
{
	compare_readers();
}

