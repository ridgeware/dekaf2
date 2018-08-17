
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dekaf2/kwriter.h>
#include <dekaf2/kreader.h>
#include <dekaf2/kfdstream.h>
#include <dekaf2/kstring.h>
#include <dekaf2/kprof.h>
#include <dekaf2/kfilesystem.h>
#include <fstream>

using namespace dekaf2;

void compare_readers()
{
	std::string filename("/tmp/dekaf2_reader_test");
	std::string s("1234567890123456789012345678901234567890123456789\n");

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
			}
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
			}
		}
		fclose(fp);
	}
	{
		std::ifstream is(filename);
		if (is.is_open())
		{
			char Ch;
			KProf prof("read std::ifstream, single chars");
			prof.SetMultiplier(500000);
			for (int ct = 0; ct < 500000; ++ct)
			{
				Ch = is.get();
			}
		}
		is.close();
	}
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
			}
		}
		is.close();
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
			}
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
			}
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
				}
			}
		}
		close(fd);
	}
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
			}
		}
		fclose(fp);
	}
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
			}
		}
		fclose(fp);
	}
	{
		std::ifstream is(filename);
		if (is.is_open())
		{
			KString line;
			KProf prof("read std::ifstream, single lines");
			prof.SetMultiplier(10000);
			for (int ct = 0; ct < 10000; ++ct)
			{
				std::getline(is, line);
			}
		}
		is.close();
	}
	{
		KInFile is(filename);
		if (is.is_open())
		{
			KString line;
			KProf prof("read KInFile, single lines");
			prof.SetMultiplier(10000);
			for (int ct = 0; ct < 10000; ++ct)
			{
				is.ReadLine(line);
			}
		}
		is.close();
	}
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
			}
		}
		is.close();
	}
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
			}
		}
		is.close();
	}

	kRemoveFile(filename);

}

void kreader_bench()
{
	compare_readers();
}

