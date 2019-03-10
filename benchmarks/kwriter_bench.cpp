
#include <stdio.h>
#include <dekaf2/kwriter.h>
#include <dekaf2/kfdstream.h>
#include <dekaf2/kstring.h>
#include <dekaf2/kprof.h>
#include <dekaf2/kfilesystem.h>
#include <fstream>
#include <fcntl.h>
#ifdef DEKAF2_IS_WINDOWS
	#include <sys/stat.h>
	#include <io.h>
	#define S_IRUSR _S_IREAD
	#define S_IWUSR _S_IWRITE
#else
	#include <unistd.h>
#endif

using namespace dekaf2;

void compare_writers()
{
	std::string filename("/tmp/dekaf2_writer_test");
	std::string s("123456789012345678901234567890123456789\n");
	kRemoveFile(filename);

	KProf pp("-Writers");

	{
		int fd = open(filename.c_str(), O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
		if (fd >= 0)
		{
			KProf prof("FileDesc, 40 bytes");
			prof.SetMultiplier(100000);
			for (int ct = 0; ct < 100000; ++ct)
			{
				write(fd, s.data(), s.size());
			}
		}
		close(fd);
	}
	kRemoveFile(filename);
	{
		FILE* fp = fopen(filename.c_str(), "w");
		if (fp)
		{
			KProf prof("FILE*, 40 bytes");
			prof.SetMultiplier(100000);
			for (int ct = 0; ct < 100000; ++ct)
			{
				fwrite(s.data(), 1, s.size(), fp);
			}
			fflush(fp);
		}
		fclose(fp);
	}
	kRemoveFile(filename);
	{
		std::ofstream os(filename);
		if (os.is_open())
		{
			KProf prof("std::ofstream, 40 bytes");
			prof.SetMultiplier(100000);
			for (int ct = 0; ct < 100000; ++ct)
			{
				os.write(s.data(), s.size());
			}
			os.flush();
		}
		os.close();
	}
	kRemoveFile(filename);
	{
		KOutFile os(filename);
		if (os.is_open())
		{
			KProf prof("KFileWriter, 40 bytes");
			prof.SetMultiplier(100000);
			for (int ct = 0; ct < 100000; ++ct)
			{
				os.Write(s.data(), s.size());
			}
			os.flush();
		}
		os.close();
	}
	kRemoveFile(filename);
	{
		FILE* fp = std::fopen(filename.c_str(), "w");
		KFPWriter os(fp);
		if (os.is_open())
		{
			KProf prof("KFPWriter, 40 bytes");
			prof.SetMultiplier(100000);
			for (int ct = 0; ct < 100000; ++ct)
			{
				os.Write(s.data(), s.size());
			}
			os.flush();
		}
		os.close();
	}
	kRemoveFile(filename);
	{
		int fd = ::open(filename.c_str(), O_WRONLY | O_CREAT, S_IWUSR | S_IRUSR);
		KFDWriter os(fd);
		if (os.is_open())
		{
			KProf prof("KFileDescWriter, 40 bytes");
			prof.SetMultiplier(100000);
			for (int ct = 0; ct < 100000; ++ct)
			{
				os.Write(s.data(), s.size());
			}
			os.flush();
		}
		os.close();
	}
	kRemoveFile(filename);

	{
		s = "123456789012345678901234567890123456789\n";
		std::string tmp;
		tmp.reserve(40 * 100);
		for (int x = 0; x < 100; ++x)
		{
			tmp += s;
		}
		s = tmp;
	}

	kRemoveFile(filename);
	{
		int fd = open(filename.c_str(), O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
		if (fd >= 0)
		{
			KProf prof("FileDesc, 4000 bytes");
			prof.SetMultiplier(1000);
			for (int ct = 0; ct < 1000; ++ct)
			{
				write(fd, s.data(), s.size());
			}
		}
		close(fd);
	}
	kRemoveFile(filename);
	{
		FILE* fp = fopen(filename.c_str(), "w");
		if (fp)
		{
			KProf prof("FILE*, 4000 bytes");
			prof.SetMultiplier(1000);
			for (int ct = 0; ct < 1000; ++ct)
			{
				fwrite(s.data(), 1, s.size(), fp);
			}
			fflush(fp);
		}
		fclose(fp);
	}
	kRemoveFile(filename);
	{
		std::ofstream os(filename);
		if (os.is_open())
		{
			KProf prof("std::ofstream, 4000 bytes");
			prof.SetMultiplier(1000);
			for (int ct = 0; ct < 1000; ++ct)
			{
				os.write(s.data(), s.size());
			}
			os.flush();
		}
		os.close();
	}
	kRemoveFile(filename);
	{
		KOutFile os(filename);
		if (os.is_open())
		{
			KProf prof("KFileWriter, 4000 bytes");
			prof.SetMultiplier(1000);
			for (int ct = 0; ct < 1000; ++ct)
			{
				os.Write(s.data(), s.size());
			}
			os.flush();
		}
		os.close();
	}
	kRemoveFile(filename);
	{
		FILE* fp = std::fopen(filename.c_str(), "w");
		KFPWriter os(fp);
		if (os.is_open())
		{
			KProf prof("KFPWriter, 4000 bytes");
			prof.SetMultiplier(1000);
			for (int ct = 0; ct < 1000; ++ct)
			{
				os.Write(s.data(), s.size());
			}
			os.flush();
		}
		os.close();
	}
	kRemoveFile(filename);
	{
		int fd = ::open(filename.c_str(), O_WRONLY | O_CREAT, S_IWUSR | S_IRUSR);
		KFDWriter os(fd);
		if (os.is_open())
		{
			KProf prof("KFileDescWriter, 4000 bytes");
			prof.SetMultiplier(1000);
			for (int ct = 0; ct < 1000; ++ct)
			{
				os.Write(s.data(), s.size());
			}
			os.flush();
		}
		os.close();
	}
	unlink(filename.c_str());

	{
		s = "123456789012345678901234567890123456789\n";
		std::string tmp;
		tmp.reserve(40 * 1000);
		for (int x = 0; x < 1000; ++x)
		{
			tmp += s;
		}
		s = tmp;
	}

	kRemoveFile(filename);
	{
		int fd = open(filename.c_str(), O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
		if (fd >= 0)
		{
			KProf prof("FileDesc, 40000 bytes");
			prof.SetMultiplier(100);
			for (int ct = 0; ct < 100; ++ct)
			{
				write(fd, s.data(), s.size());
			}
		}
		close(fd);
	}
	kRemoveFile(filename);
	{
		FILE* fp = fopen(filename.c_str(), "w");
		if (fp)
		{
			KProf prof("FILE*, 40000 bytes");
			prof.SetMultiplier(100);
			for (int ct = 0; ct < 100; ++ct)
			{
				fwrite(s.data(), 1, s.size(), fp);
			}
			fflush(fp);
		}
		fclose(fp);
	}
	kRemoveFile(filename);
	{
		std::ofstream os(filename);
		if (os.is_open())
		{
			KProf prof("std::ofstream, 40000 bytes");
			prof.SetMultiplier(100);
			for (int ct = 0; ct < 100; ++ct)
			{
				os.write(s.data(), s.size());
			}
			os.flush();
		}
		os.close();
	}
	kRemoveFile(filename);
	{
		KOutFile os(filename);
		if (os.is_open())
		{
			KProf prof("KFileWriter, 40000 bytes");
			prof.SetMultiplier(100);
			for (int ct = 0; ct < 100; ++ct)
			{
				os.Write(s.data(), s.size());
			}
			os.flush();
		}
		os.close();
	}
	kRemoveFile(filename);
	{
		FILE* fp = std::fopen(filename.c_str(), "w");
		KFPWriter os(fp);
		if (os.is_open())
		{
			KProf prof("KFPWriter, 40000 bytes");
			prof.SetMultiplier(100);
			for (int ct = 0; ct < 100; ++ct)
			{
				os.Write(s.data(), s.size());
			}
			os.flush();
		}
		os.close();
	}
	kRemoveFile(filename);
	{
		int fd = ::open(filename.c_str(), O_WRONLY | O_CREAT, S_IWUSR | S_IRUSR);
		KFDWriter os(fd);
		if (os.is_open())
		{
			KProf prof("KFileDescWriter, 40000 bytes");
			prof.SetMultiplier(100);
			for (int ct = 0; ct < 100; ++ct)
			{
				os.Write(s.data(), s.size());
			}
			os.flush();
		}
		os.close();
	}
	kRemoveFile(filename);

}

void kwriter_bench()
{
	compare_writers();
}

