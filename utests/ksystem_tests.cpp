#include "catch.hpp"

#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/system/os/ksystem.h>
#include <dekaf2/system/filesystem/kfilesystem.h>
#include <dekaf2/core/logging/klog.h>
#include <fcntl.h> // for open()
#ifdef DEKAF2_IS_WINDOWS
	#include <io.h>
#endif

using namespace dekaf2;

namespace {
KTempDir TempDir;
}

TEST_CASE("KSystem")
{
	SECTION("EnvVars")
	{
		CHECK ( kUnsetEnv("DEKAF2_UTEST_VAR1") == true );
		CHECK ( kSetEnv("DEKAF2_UTEST_VAR1", "ThisIsATest") == true );
		CHECK ( kGetEnv("DEKAF2_UTEST_VAR1") == "ThisIsATest" );
		CHECK ( kUnsetEnv("DEKAF2_UTEST_VAR1") == true );
		CHECK ( kGetEnv("DEKAF2_UTEST_VAR1") == "" );
		{
			// tests if setenv does a copy (it does on unix)
			KString sVar("Copied");
			KString sName("Var1");
			kSetEnv(sName, sVar);
			sVar  = "xxxxxx";
			sName = "2222";
		}
		CHECK ( kGetEnv("Var1") == "Copied" );

	}

	SECTION("CWD")
	{
		auto sTemp = kGetTemp();
		CHECK ( sTemp != "" );
		auto sDir = kGetCWD();
		CHECK ( sDir != "" );
		CHECK ( sTemp != sDir );
		CHECK ( kSetCWD(sTemp) );
		auto sCD = kGetCWD();
#ifdef DEKAF2_IS_OSX
		sCD.remove_prefix("/private");
#endif
		CHECK ( sTemp == sCD );
		CHECK ( kSetCWD(sDir) );
		CHECK ( kGetCWD() == sDir );
	}

	SECTION("Home")
	{
		auto sHome = kGetHome();
		CHECK ( sHome != "" );
	}

	SECTION("WhoAmI")
	{
		auto sWho = kGetWhoAmI();
		CHECK ( sWho != "" );
	}

	SECTION("Hostname")
	{
		auto sHostname = kGetHostname();
		CHECK ( sHostname != "" );
	}

	SECTION("TID")
	{
		auto i = kGetTid();
		CHECK ( i != 0 );
	}

	SECTION("ThreadName")
	{
		// run in a joined thread to not rename the utest main thread
		std::thread([]()
		{
			// an unnamed thread reports an empty name (on Linux it inherits the
			// process name, which kGetThreadName() filters out)
			CHECK ( kGetThreadName().empty() );

#ifdef DEKAF2_IS_WINDOWS
			// needs Windows 10 1607 for Set/GetThreadDescription()
			if (kSetThreadName("utestthread"))
			{
				CHECK ( kGetThreadName() == "utestthread" );
			}
#else
			CHECK ( kSetThreadName("utestthread") );
			char szName[64] {};
			REQUIRE ( pthread_getname_np(pthread_self(), szName, sizeof(szName)) == 0 );
			CHECK ( KStringView(szName) == "utestthread" );
			CHECK ( kGetThreadName() == "utestthread" );

			// longer names get truncated to the 15 chars of the kernel's comm field
			CHECK ( kSetThreadName("averylongthreadname") );
			REQUIRE ( pthread_getname_np(pthread_self(), szName, sizeof(szName)) == 0 );
			CHECK ( KStringView(szName) == "averylongthread" );
			CHECK ( kGetThreadName() == "averylongthread" );
#endif
		}).join();
	}

	SECTION("kSystem")
	{
		KString sOutput;
		auto iRet = kSystem("echo this is some text", sOutput);
		CHECK ( iRet == 0 );
		CHECK ( sOutput == "this is some text\n" );

		iRet = kSystem("echo this is some text && exit 123", sOutput);
		CHECK ( iRet == 123 );
#ifdef DEKAF2_IS_WINDOWS
		// Windows 'echo' includes any trailing space
		CHECK(sOutput == "this is some text \n");
#else
		CHECK ( sOutput == "this is some text\n" );
#endif

		iRet = kSystem("echo this is some && echo multiline text && echo output", sOutput);
		CHECK ( iRet == 0 );
#ifdef DEKAF2_IS_WINDOWS
		CHECK ( sOutput.starts_with("this is some") );
		CHECK ( sOutput.ends_with("output\n") );
#else
		CHECK ( sOutput == "this is some\nmultiline text\noutput\n" );
#endif

#ifdef DEKAF2_IS_WINDOWS
		iRet = kSystem("echo 123 && timeout /t 1 && echo this is some text", sOutput);
		CHECK ( iRet == 0 );
		CHECK ( sOutput.starts_with("123") );
		CHECK ( sOutput.ends_with("this is some text\n") );
#else
		iRet = kSystem("echo 123 && sleep 1 && echo this is some text", sOutput);
		CHECK ( iRet == 0 );
		CHECK ( sOutput == "123\nthis is some text\n" );
#endif

#ifdef DEKAF2_IS_WINDOWS
		iRet = kSystem("dir", sOutput);
#else
		iRet = kSystem("ls -al ./", sOutput);
#endif
		CHECK ( iRet == 0 );
		CHECK ( sOutput != "" );

		iRet = kSystem("", sOutput);
		CHECK ( iRet == EINVAL );
		CHECK ( sOutput == "" );

		iRet = kSystem("abasdkhjfgbsarkjghvasgskufhse", sOutput);

		// on debian, we occasionally get 141, which is the shell's return value after a SIGPIPE
		if (iRet != 141)
		{

			CHECK ( iRet == DEKAF2_POPEN_COMMAND_NOT_FOUND );
			INFO  ( sOutput );
			CHECK ( sOutput.contains("abasdkhjfgbsarkjghvasgskufhse") );
#ifdef DEKAF2_IS_WINDOWS
			CHECK ( sOutput.contains("not recognized") );
#else
			CHECK ( sOutput.contains("not found") );
#endif
		}

		iRet = kSystem("echo hello world");
		CHECK ( iRet == 0 );

		iRet = kSystem("exit 123");
		CHECK ( iRet == 123 );

		iRet = kSystem("abasdkhjfgbsarkjghvasgskufhse");
		CHECK ( iRet == DEKAF2_POPEN_COMMAND_NOT_FOUND);
	}

	SECTION("rusage")
	{
		auto iStart1 = kGetMicroTicksPerProcess();
		auto iStart2 = kGetMicroTicksPerThread();
		auto iStart3 = kGetMicroTicksPerChildProcesses();
		KString s;
#ifdef DEKAF2_IS_WINDOWS
		for (auto i = 0; i < 1000000; ++i)
#else
		for (auto i = 0; i < 100000; ++i)
#endif
		{
			s += 'a';
		}
		if (s[10000] != 'a')
		{
			kDebug(1, "false");
		}
		auto iTicks1 = kGetMicroTicksPerProcess()        - iStart1;
		auto iTicks2 = kGetMicroTicksPerThread()         - iStart2;
		auto iTicks3 = kGetMicroTicksPerChildProcesses() - iStart3;

#ifdef DEKAF2_IS_WINDOWS
		// CPU time on Windows comes from GetProcessTimes()/GetThreadTimes(), which only
		// resolve at the scheduler tick (~15ms), so a short workload can legitimately
		// measure 0 elapsed ticks - just sanity-check the counters did not run backwards
		CHECK ( iTicks1 < 1000000000 );
		CHECK ( iTicks2 < 1000000000 );
#else
		CHECK ( iTicks1 > 0 );
		CHECK ( iTicks2 > 0 );
#endif
	}

	SECTION("GetFileNameFromFileDescriptor")
	{
		auto sFilename = kFormat("{}{}{}", TempDir.Name(), kDirSep, "test日本語abc中文Русский.file");
#ifdef DEKAF2_IS_WINDOWS
		auto sUTF16Filename = kutf::Convert<std::wstring>(sFilename);
		int fd = _wopen(sUTF16Filename.c_str(), _O_CREAT | _O_RDWR | _O_BINARY, DEKAF2_MODE_CREATE_FILE);
#else
		int fd = open(sFilename.c_str(), O_CREAT | O_RDWR, DEKAF2_MODE_CREATE_FILE);
#endif
		if (fd < 0)
		{
#ifdef DEKAF2_IS_WINDOWS
			FAIL_CHECK( dekaf2::strerror(errno) );
#else
			FAIL_CHECK( strerror(errno) );
#endif
		}

		auto sName = kGetFileNameFromFileDescriptor(fd);

#ifdef DEKAF2_IS_OSX
		if (sName != sFilename)
		{
			// MacOS creates the temp folder under /private, but does normally
			// not tell so, except when being asked with fcntl()
			sName.remove_prefix("/private");
		}
#endif

		CHECK ( sName == sFilename );

		auto handle = kGetHandleFromFileDescriptor(fd);
		auto fd2    = kGetFileDescriptorFromHandle(handle);
		auto sName2 = kGetFileNameFromFileDescriptor(fd2);
		auto sName3 = kGetFileNameFromFileHandle(handle);

#ifdef DEKAF2_IS_OSX
		if (sName != sName2)
		{
			sName2.remove_prefix("/private");
		}
		if (sName != sName3)
		{
			sName3.remove_prefix("/private");
		}
#endif

		CHECK ( sName == sName2 );
		CHECK ( sName == sName3 );

#ifndef DEKAF2_IS_WINDOWS
		CHECK ( fd2 == fd );
#endif

		close(fd);
	}

	SECTION("GetTerminalSize")
	{
		auto TTY = kGetTerminalSize();
		CHECK ( TTY.lines   > 0 );
		CHECK ( TTY.columns > 0 );
	}

	SECTION("GetCPU")
	{
		auto iCount = kGetCPUCount();

		if (iCount > 0)
		{
			auto iCPU = kGetCPU();
			uint16_t iNewCPU = iCPU == 0 ? 1 : 0;

			if (kSetProcessCPU({iNewCPU}))
			{
//				CHECK ( kGetCPU() == iNewCPU );
			}
		}
	}

	SECTION("GetThreadCPU")
	{
		auto iCount = kGetCPUCount();

		if (iCount > 0)
		{
			auto CPUs = kGetThreadCPU();

			CHECK ( CPUs.size() < 2 );

			if (kSetThreadCPU({0,1}))
			{
				CPUs = kGetThreadCPU();
				CHECK ( CPUs.size() == 2 );
				if (CPUs.size() == 2)
				{
					CHECK ( CPUs[0] == 0 );
					CHECK ( CPUs[1] == 1 );
				}
			}
		}
	}

	SECTION("IsInsideDataSegment")
	{
		CHECK ( kIsInsideDataSegment("I am a static string literal stored in the data segment") == true );
		KString sTest("I am a dynamic string whose class is stored on the stack and whose content is stored on the heap");
		CHECK ( kIsInsideDataSegment(&sTest) == false );
		CHECK ( kIsInsideDataSegment(sTest.c_str()) == false );
		auto sDynamic = std::make_unique<char[]>(41);
		strncpy(sDynamic.get(), "I am a dynamic string stored on the heap", 41);
		CHECK ( kIsInsideDataSegment(sDynamic.get()) == false );
	}

	SECTION("KUName")
	{
		KUName Info;
		CHECK ( Info.nodename != "" );
#ifndef DEKAF2_IS_WINDOWS
		CHECK ( Info.release  != "" );
#endif
		CHECK ( Info.sysname  != "" );
		CHECK ( Info.machine  != "" );
		CHECK ( Info.version  != "" );

		auto ii = std::make_unique<KUName>();
		KUName I2 = *ii;
		ii.reset();
		CHECK ( I2.sysname != "" );
	}

	SECTION("kGetUptime")
	{
		CHECK ( kGetUptime().seconds().count() > 5 );
	}

	SECTION("KSocketBufferLimits")
	{
#if defined(DEKAF2_IS_LINUX) || defined(DEKAF2_IS_MACOS)
		KSocketBufferLimits udp(/*bForUDP=*/true);

		// Some sandboxed environments do not expose the socket buffer limits at
		// all - e.g. a container's own network namespace on kernels that do not
		// namespace net.core.*mem_* (and where the tcp_*mem / getsockopt
		// fallbacks are unavailable too). Treat an all-zero read as "unavailable"
		// and skip rather than fail.
		if (udp.iSendMax == 0)
		{
			WARN("socket buffer limits not exposed in this environment - skipping");
		}
		else
		{
			CHECK ( udp.iSendMax     > 0 );
			CHECK ( udp.iSendDefault > 0 );
			CHECK ( udp.iRecvMax     > 0 );
			CHECK ( udp.iRecvDefault > 0 );

			KSocketBufferLimits tcp(/*bForUDP=*/false);
			CHECK ( tcp.iSendMax     > 0 );
			CHECK ( tcp.iSendDefault > 0 );
			CHECK ( tcp.iRecvMax     > 0 );
			CHECK ( tcp.iRecvDefault > 0 );
		}
#endif
	}

	SECTION("KSharedMemoryLimits")
	{
#if defined(DEKAF2_IS_LINUX) || defined(DEKAF2_IS_MACOS)
		KSharedMemoryLimits shm;
		shm.Read();
		CHECK ( shm.iMaxSegmentBytes > 0 );
		CHECK ( shm.iTotalPages      > 0 );
		CHECK ( shm.iMaxSegments     > 0 );
#endif
	}

	SECTION("KFileDescriptorLimits")
	{
#if defined(DEKAF2_IS_LINUX) || defined(DEKAF2_IS_MACOS)
		KFileDescriptorLimits fds;
		fds.Read();
		CHECK ( fds.iKernelMax   > 0 );
		CHECK ( fds.iProcessSoft > 0 );
		CHECK ( fds.iProcessHard > 0 );
#endif
	}

	SECTION("kWhich")
	{
		// should find common executables
#ifdef DEKAF2_IS_WINDOWS
		// ls/sh do not exist on Windows; cmd.exe and where.exe live in System32 (always on PATH)
		auto sCmd = kWhich("cmd");
		CHECK ( sCmd.empty() == false );
		CHECK ( sCmd.contains(kDirSep) );

		auto sWhere = kWhich("where");
		CHECK ( sWhere.empty() == false );
		CHECK ( sWhere.contains(kDirSep) );
#else
		auto sLS = kWhich("ls");
		CHECK ( sLS.empty() == false );
		CHECK ( sLS.front() == '/' );

		auto sSH = kWhich("sh");
		CHECK ( sSH.empty() == false );
		CHECK ( sSH.front() == '/' );
#endif

		// should return empty for nonexistent commands
		CHECK ( kWhich("thiscommanddoesnotexist99") == "" );

		// should return empty for empty input
		CHECK ( kWhich("") == "" );

		// should reject commands with invalid characters
		CHECK ( kWhich("ls -la")       == "" );
		CHECK ( kWhich("$(whoami)")    == "" );
		CHECK ( kWhich("file;rm")      == "" );
		CHECK ( kWhich("cat /etc/pwd") == "" );

		// should accept commands with hyphens, underscores, dots
		// (these may or may not be found, but should not be rejected by validation)
		auto sResult = kWhich("apt-get");
		// just verify it doesn't crash - result depends on platform

		sResult = kWhich("python3.11");
		// just verify it doesn't crash - result depends on platform
	}
}
