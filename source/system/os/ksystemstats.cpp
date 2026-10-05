/*
//
// DEKAF(tm): Lighter, Faster, Smarter(tm)
//
// Copyright (c) 2017, Ridgeware, Inc.
//
// +-------------------------------------------------------------------------+
// | /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\|
// |/+---------------------------------------------------------------------+/|
// |/|                                                                     |/|
// |\|  ** THIS NOTICE MUST NOT BE REMOVED FROM THE SOURCE CODE MODULE **  |\|
// |/|                                                                     |/|
// |\|   OPEN SOURCE LICENSE                                               |\|
// |/|                                                                     |/|
// |\|   Permission is hereby granted, free of charge, to any person       |\|
// |/|   obtaining a copy of this software and associated                  |/|
// |\|   documentation files (the "Software"), to deal in the              |\|
// |/|   Software without restriction, including without limitation        |/|
// |\|   the rights to use, copy, modify, merge, publish,                  |\|
// |/|   distribute, sublicense, and/or sell copies of the Software,       |/|
// |\|   and to permit persons to whom the Software is furnished to        |\|
// |/|   do so, subject to the following conditions:                       |/|
// |\|                                                                     |\|
// |/|   The above copyright notice and this permission notice shall       |/|
// |\|   be included in all copies or substantial portions of the          |\|
// |/|   Software.                                                         |/|
// |\|                                                                     |\|
// |/|   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY         |/|
// |\|   KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE        |\|
// |/|   WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR           |/|
// |\|   PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS        |\|
// |/|   OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR          |/|
// |\|   OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR        |\|
// |/|   OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE         |/|
// |\|   SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.            |\|
// |/|                                                                     |/|
// |/+---------------------------------------------------------------------+/|
// |\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/ |
// +-------------------------------------------------------------------------+
//
*/

#include <dekaf2/system/os/ksystemstats.h>
#include <dekaf2/containers/sequential/kstack.h>
#include <dekaf2/core/strings/kstringutils.h>
#include <dekaf2/system/process/kinshell.h>
#include <dekaf2/core/strings/kregex.h>
#include <dekaf2/web/url/kurlencode.h>
#include <dekaf2/http/client/kwebclient.h>
#include <dekaf2/io/streams/kstream.h>
#include <dekaf2/data/json/kjson.h>
#include <dekaf2/system/filesystem/kfilesystem.h>
#include <dekaf2/time/clock/ktime.h>
#include <dekaf2/system/os/ksystem.h>
#include <dekaf2/containers/associative/kassociative.h>
#include <vector>
#ifdef DEKAF2_IS_WINDOWS
	#include <dekaf2/core/strings/kutf.h>
	#include <windows.h>
	#include <iphlpapi.h>
	#include <tlhelp32.h>
	#include <psapi.h>
#else
	#include <sys/statvfs.h>
	#include <cstdlib>
	#ifdef DEKAF2_IS_OSX
		#include <cstring>
		#include <sys/sysctl.h>
		#include <mach/mach.h>
	#endif
	#ifdef DEKAF2_HAS_LIBPROC
		#include <libproc.h>
		#include <array>
	#endif
#endif

DEKAF2_NAMESPACE_BEGIN

KStringViewZ KSystemStats::PROC_VERSION       = "/proc/version";
KStringViewZ KSystemStats::PROC_LOADAVG       = "/proc/loadavg";
KStringViewZ KSystemStats::PROC_UPTIME        = "/proc/uptime";
KStringViewZ KSystemStats::PROC_MISC          = "/proc/misc";
KStringViewZ KSystemStats::PROC_HOSTNAME      = "/proc/sys/kernel/hostname";
KStringViewZ KSystemStats::PROC_VMSTAT        = "/proc/vmstat";
KStringViewZ KSystemStats::PROC_DISKSTATS     = "/proc/diskstats";
KStringViewZ KSystemStats::PROC_DISKUSAGEINFO = "/proc/mounts";
KStringViewZ KSystemStats::PROC_CPUINFO       = "/proc/cpuinfo";
KStringViewZ KSystemStats::PROC_STAT          = "/proc/stat";
KStringViewZ KSystemStats::PROC_MEMINFO       = "/proc/meminfo";
KStringViewZ KSystemStats::CPUINFO_NUM_CORES  = "cpuinfo_num_cores";

//-----------------------------------------------------------------------------
int64_t NeverNegative (int64_t iN)
//-----------------------------------------------------------------------------
{
	if (iN < 0) 
	{
		return (0);
	}
	else 
	{
		return (iN);
	}

} // NeverNegative

namespace {

//-----------------------------------------------------------------------------
/// a process of the system, from the process list of the system, or from ps
struct ProcessInfo
//-----------------------------------------------------------------------------
{
	KString sPID;
	KString sPPID;
	KString sShortCmd; ///< the name of the program
	KString sFullCmd;  ///< its command line - on Windows the path of its program file
};

#ifdef DEKAF2_IS_WINDOWS

//-----------------------------------------------------------------------------
/// returns a FILETIME as a count of 100 nanosecond ticks
uint64_t Ticks(const FILETIME& Time)
//-----------------------------------------------------------------------------
{
	return (static_cast<uint64_t>(Time.dwHighDateTime) << 32) | Time.dwLowDateTime;

} // Ticks

//-----------------------------------------------------------------------------
/// returns the version of Windows, like 10.0.26100 - GetVersionEx() reports an
/// older version to a program without a manifest that declares a newer one
KString WindowsVersion()
//-----------------------------------------------------------------------------
{
	using RtlGetVersionFunc = LONG (WINAPI*)(OSVERSIONINFOW*);

	static RtlGetVersionFunc pRtlGetVersion = []() -> RtlGetVersionFunc
	{
		HMODULE hNtDll = ::GetModuleHandleW(L"ntdll.dll");
		return hNtDll ? reinterpret_cast<RtlGetVersionFunc>(::GetProcAddress(hNtDll, "RtlGetVersion")) : nullptr;
	}();

	OSVERSIONINFOW Info {};
	Info.dwOSVersionInfoSize = sizeof(Info);

	if (!pRtlGetVersion || pRtlGetVersion(&Info) != 0)
	{
		return {};
	}

	return kFormat("{}.{}.{}", Info.dwMajorVersion, Info.dwMinorVersion, Info.dwBuildNumber);

} // WindowsVersion

// the description of the first processor in the registry
constexpr wchar_t s_sProcessorKey[] = L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0";

//-----------------------------------------------------------------------------
/// returns a string of the description of the first processor, or an empty string
KString ProcessorString(const wchar_t* sName)
//-----------------------------------------------------------------------------
{
	wchar_t sValue[256];
	DWORD   iSize = sizeof(sValue);

	if (::RegGetValueW(HKEY_LOCAL_MACHINE, s_sProcessorKey, sName, RRF_RT_REG_SZ, nullptr, sValue, &iSize) != ERROR_SUCCESS)
	{
		return {};
	}

	auto sResult = kutf::Convert<KString>(std::wstring(sValue));
	sResult.Trim();

	return sResult;

} // ProcessorString

//-----------------------------------------------------------------------------
/// returns a number of the description of the first processor, or 0
DWORD ProcessorNumber(const wchar_t* sName)
//-----------------------------------------------------------------------------
{
	DWORD iValue { 0 };
	DWORD iSize = sizeof(iValue);

	if (::RegGetValueW(HKEY_LOCAL_MACHINE, s_sProcessorKey, sName, RRF_RT_REG_DWORD, nullptr, &iValue, &iSize) != ERROR_SUCCESS)
	{
		return 0;
	}

	return iValue;

} // ProcessorNumber

//-----------------------------------------------------------------------------
/// returns the path of the program file of a process, or an empty string without
/// the access to it
KString ProcessImagePath(DWORD iPID)
//-----------------------------------------------------------------------------
{
	HANDLE hProcess = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, iPID);

	if (!hProcess)
	{
		return {};
	}

	wchar_t sPath[MAX_PATH * 4];
	DWORD   iSize = MAX_PATH * 4;

	bool bHasPath = ::QueryFullProcessImageNameW(hProcess, 0, sPath, &iSize) != FALSE;

	::CloseHandle(hProcess);

	return bHasPath ? kutf::Convert<KString>(std::wstring(sPath, iSize)) : KString();

} // ProcessImagePath

//-----------------------------------------------------------------------------
/// counts the TCP connections of an address family by their state, under the
/// names of netstat on Linux
void CountTcpConnections(ULONG iFamily, KProps<KString, KSystemStats::int_t>& Netstat)
//-----------------------------------------------------------------------------
{
	std::vector<char> Table;
	DWORD             iSize { 0 };
	DWORD             iResult;

	// the table may grow between two calls
	do
	{
		Table.resize(iSize);
		iResult = ::GetExtendedTcpTable(Table.empty() ? nullptr : Table.data(), &iSize, FALSE, iFamily, TCP_TABLE_OWNER_PID_ALL, 0);
	}
	while (iResult == ERROR_INSUFFICIENT_BUFFER);

	if (iResult != NO_ERROR)
	{
		kDebug(2, "cannot get the TCP connections: {}", iResult);
		return;
	}

	auto Count = [&Netstat](DWORD iState)
	{
		KStringView sState;

		switch (iState)
		{
			case MIB_TCP_STATE_SYN_SENT:   sState = "syn_sent";    break;
			case MIB_TCP_STATE_SYN_RCVD:   sState = "syn_recv";    break;
			case MIB_TCP_STATE_ESTAB:      sState = "established"; break;
			case MIB_TCP_STATE_FIN_WAIT1:  sState = "fin_wait1";   break;
			case MIB_TCP_STATE_FIN_WAIT2:  sState = "fin_wait2";   break;
			case MIB_TCP_STATE_CLOSE_WAIT: sState = "close_wait";  break;
			case MIB_TCP_STATE_CLOSING:    sState = "closing";     break;
			case MIB_TCP_STATE_LAST_ACK:   sState = "last_ack";    break;
			case MIB_TCP_STATE_TIME_WAIT:  sState = "time_wait";   break;
			// as netstat -n, without listening and closed sockets
			default:                       return;
		}

		++Netstat[kFormat("netstat_tcp_{}", sState)];
	};

	if (iFamily == AF_INET)
	{
		auto* pTable = reinterpret_cast<const MIB_TCPTABLE_OWNER_PID*>(Table.data());

		for (DWORD i = 0; i < pTable->dwNumEntries; ++i)
		{
			Count(pTable->table[i].dwState);
		}
	}
	else
	{
		auto* pTable = reinterpret_cast<const MIB_TCP6TABLE_OWNER_PID*>(Table.data());

		for (DWORD i = 0; i < pTable->dwNumEntries; ++i)
		{
			Count(pTable->table[i].dwState);
		}
	}

} // CountTcpConnections

//-----------------------------------------------------------------------------
/// returns the processes of the system
std::vector<ProcessInfo> ListProcesses()
//-----------------------------------------------------------------------------
{
	std::vector<ProcessInfo> Processes;

	HANDLE hSnapshot = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

	if (hSnapshot == INVALID_HANDLE_VALUE)
	{
		kDebug(1, "cannot list the processes: {}", ::GetLastError());
		return Processes;
	}

	PROCESSENTRY32W Entry {};
	Entry.dwSize = sizeof(Entry);

	for (BOOL bHasEntry = ::Process32FirstW(hSnapshot, &Entry); bHasEntry; bHasEntry = ::Process32NextW(hSnapshot, &Entry))
	{
		auto sName = kutf::Convert<KString>(std::wstring(Entry.szExeFile));
		auto sPath = ProcessImagePath(Entry.th32ProcessID);

		Processes.push_back({ KString::to_string(Entry.th32ProcessID),
		                      KString::to_string(Entry.th32ParentProcessID),
		                      sName,
		                      sPath.empty() ? sName : sPath });
	}

	::CloseHandle(hSnapshot);

	return Processes;

} // ListProcesses

#else // DEKAF2_IS_WINDOWS

#ifdef DEKAF2_IS_OSX

//-----------------------------------------------------------------------------
/// returns a string value of the kernel, or an empty string
KString SysctlString(const char* sName)
//-----------------------------------------------------------------------------
{
	std::size_t iSize { 0 };

	if (::sysctlbyname(sName, nullptr, &iSize, nullptr, 0) != 0 || iSize == 0)
	{
		return {};
	}

	std::vector<char> Value(iSize);

	if (::sysctlbyname(sName, Value.data(), &iSize, nullptr, 0) != 0)
	{
		return {};
	}

	// the value ends with a NUL
	KString sValue(Value.data(), ::strnlen(Value.data(), iSize));
	sValue.Trim();

	return sValue;

} // SysctlString

//-----------------------------------------------------------------------------
/// returns a number of the kernel, or 0
template<typename T>
T SysctlNumber(const char* sName)
//-----------------------------------------------------------------------------
{
	T           iValue { 0 };
	std::size_t iSize = sizeof(iValue);

	if (::sysctlbyname(sName, &iValue, &iSize, nullptr, 0) != 0 || iSize != sizeof(iValue))
	{
		return 0;
	}

	return iValue;

} // SysctlNumber

#endif // DEKAF2_IS_OSX

//-----------------------------------------------------------------------------
/// reads a process from the /proc tables of Linux, returns false if there is no
/// such process
bool ReadProcProcess(KStringView sPID, ProcessInfo& Process)
//-----------------------------------------------------------------------------
{
	// % cat /proc/3002/stat
	// 3002 (crond) S 1 3002 3002 0 -1 4202816 92890 ...
	//
	// The name in the parentheses can have spaces and parentheses itself, therefore
	// the fields after it start after the last closing parenthesis: the state, and
	// then the parent process ID.
	KString sStat;

	if (!kReadTextFile(kFormat("/proc/{}/stat", sPID), sStat, false))
	{
		return false;
	}

	auto iOpen  = sStat.find('(');
	auto iClose = sStat.rfind(')');

	if (iOpen == KString::npos || iClose == KString::npos || iClose < iOpen)
	{
		return false;
	}

	auto Fields = KStringView(sStat).substr(iClose + 1).Split(' ');

	if (Fields.size() < 2)
	{
		return false;
	}

	Process.sPID      = sPID;
	Process.sPPID     = Fields[1];
	Process.sShortCmd = KStringView(sStat).substr(iOpen + 1, iClose - iOpen - 1);

	// The arguments are separated by NUL bytes. Kernel threads and zombies have no
	// arguments, and ps shows their name in brackets then.
	Process.sFullCmd.clear();
	kReadBinaryFile(kFormat("/proc/{}/cmdline", sPID), Process.sFullCmd);
	Process.sFullCmd.TrimRight('\0');
	Process.sFullCmd.Replace('\0', ' ');

	if (Process.sFullCmd.empty())
	{
		Process.sFullCmd = kFormat("[{}]", Process.sShortCmd);
	}

	return true;

} // ReadProcProcess

//-----------------------------------------------------------------------------
/// returns the processes of the system, from the /proc tables of Linux
std::vector<ProcessInfo> ListProcessesFromProc()
//-----------------------------------------------------------------------------
{
	std::vector<ProcessInfo> Processes;

	KDirectory Dir("/proc", KFileType::DIRECTORY);

	ProcessInfo Process;

	for (const auto& Entry : Dir)
	{
		auto sPID = Entry.Filename();

		// a process can end between the listing and the reading of its tables
		if (kIsInteger(sPID, false) && ReadProcProcess(sPID, Process))
		{
			Processes.push_back(std::move(Process));
		}
	}

	return Processes;

} // ListProcessesFromProc

#ifdef DEKAF2_HAS_LIBPROC

//-----------------------------------------------------------------------------
/// returns the command line of a process, or an empty string - the system tells
/// it only for the processes of the own user (or for all to root)
KString ProcessArguments(pid_t iPID)
//-----------------------------------------------------------------------------
{
	// the buffer holds the count of the arguments, the path of the program, NUL
	// bytes up to an aligned position, the arguments with a NUL byte after each,
	// and then the environment
	int Mib[3] { CTL_KERN, KERN_PROCARGS2, iPID };
	std::size_t iSize { 0 };

	if (sysctl(Mib, 3, nullptr, &iSize, nullptr, 0) != 0 || iSize <= sizeof(int))
	{
		return {};
	}

	KString sBuffer(iSize, '\0');

	if (sysctl(Mib, 3, sBuffer.data(), &iSize, nullptr, 0) != 0 || iSize <= sizeof(int))
	{
		return {};
	}

	int iArgs { 0 };
	std::memcpy(&iArgs, sBuffer.data(), sizeof(int));

	KStringView sRest(sBuffer.data() + sizeof(int), iSize - sizeof(int));

	// skip the path of the program and the NUL bytes after it
	auto iPos = sRest.find('\0');

	if (iPos != KStringView::npos)
	{
		iPos = sRest.find_first_not_of('\0', iPos);
	}

	if (iPos == KStringView::npos)
	{
		return {};
	}

	sRest.remove_prefix(iPos);

	KString sArgs;

	for (; iArgs > 0 && !sRest.empty(); --iArgs)
	{
		auto iEnd = sRest.find('\0');

		if (!sArgs.empty())
		{
			sArgs += ' ';
		}

		sArgs += sRest.substr(0, iEnd);

		if (iEnd == KStringView::npos)
		{
			break;
		}

		sRest.remove_prefix(iEnd + 1);
	}

	return sArgs;

} // ProcessArguments

//-----------------------------------------------------------------------------
/// returns the processes of the system, from libproc of macOS
std::vector<ProcessInfo> ListProcessesFromLibproc()
//-----------------------------------------------------------------------------
{
	std::vector<ProcessInfo> Processes;

	// the count can grow until the second call - leave room for new processes
	auto iCount = proc_listallpids(nullptr, 0);

	if (iCount <= 0)
	{
		return Processes;
	}

	std::vector<pid_t> PIDs(static_cast<std::size_t>(iCount) + 64);

	iCount = proc_listallpids(PIDs.data(), static_cast<int>(PIDs.size() * sizeof(pid_t)));

	if (iCount <= 0)
	{
		return Processes;
	}

	PIDs.resize(std::min(static_cast<std::size_t>(iCount), PIDs.size()));
	Processes.reserve(PIDs.size());

	for (auto iPID : PIDs)
	{
		// the short info is the one that the system gives also for the processes of
		// other users
		proc_bsdshortinfo Info;

		if (proc_pidinfo(iPID, PROC_PIDT_SHORTBSDINFO, 0, &Info, sizeof(Info)) != sizeof(Info))
		{
			continue;
		}

		ProcessInfo Process;

		Process.sPID  = KString::to_string(iPID);
		Process.sPPID = KString::to_string(Info.pbsi_ppid);

		// like ps on macOS: the path of the program as the short command
		std::array<char, PROC_PIDPATHINFO_MAXSIZE> aPath;
		auto iLen = proc_pidpath(iPID, aPath.data(), aPath.size());

		if (iLen > 0)
		{
			Process.sShortCmd.assign(aPath.data(), static_cast<std::size_t>(iLen));
		}
		else
		{
			Process.sShortCmd.assign(Info.pbsi_comm, strnlen(Info.pbsi_comm, sizeof(Info.pbsi_comm)));
		}

		// without the arguments of a process of another user, ps shows its program
		Process.sFullCmd = ProcessArguments(iPID);

		if (Process.sFullCmd.empty())
		{
			Process.sFullCmd = Process.sShortCmd;
		}

		Processes.push_back(std::move(Process));
	}

	return Processes;

} // ListProcessesFromLibproc

#else // DEKAF2_HAS_LIBPROC

//-----------------------------------------------------------------------------
/// runs ps for all processes with the given columns, and calls Row with the parts
/// of each line - the last column is the rest of the line, with its spaces
template<typename Callback>
void ReadProcessTable(KStringView sColumns, std::size_t iColumns, Callback Row)
//-----------------------------------------------------------------------------
{
	KInShell pipe;
	pipe.SetReaderRightTrim("\r\n\t ");

	if (!pipe.Open (kFormat("ps -e -o {}", sColumns)))
	{
		return;
	}

	KString sLine;

	while (pipe.ReadLine (sLine))
	{
		auto Parts = sLine.Split(' ');

		if (Parts.size() < iColumns)
		{
			kDebug (3, "SKIPPED Parsed Line = '{}'", sLine);
			continue;
		}

		// the parts are views into the line
		auto iLastStart = static_cast<std::size_t>(Parts[iColumns - 1].data() - sLine.data());
		Parts.resize(iColumns);
		Parts.back() = KStringView(sLine).substr(iLastStart);

		Row(Parts);
	}

} // ReadProcessTable

//-----------------------------------------------------------------------------
/// returns the processes of the system, from ps
std::vector<ProcessInfo> ListProcessesFromPS()
//-----------------------------------------------------------------------------
{
	std::vector<ProcessInfo> Processes;

	kDebug (4, "running ps ...");

	// both the short and the full command can have spaces (on macOS, the short command
	// is the path of the executable), therefore ps runs twice, with each of them as the
	// last column:
	//   324 cqueue/1
	//  2742 /usr/sbin/automount
	KUnorderedMap<KString, KString> ShortCmds;

	ReadProcessTable("pid=,comm=", 2, [&ShortCmds](const std::vector<KStringView>& Parts)
	{
		ShortCmds.emplace(Parts[0], Parts[1]);
	});

	//   324   151 [cqueue/1]
	//  2742     1 automount --pid-file /var/run/autofs.pid
	ReadProcessTable("pid=,ppid=,command=", 3, [&](const std::vector<KStringView>& Parts)
	{
		// a process that started between the two runs of ps is left out
		auto it = ShortCmds.find(Parts[0]);

		if (it != ShortCmds.end())
		{
			Processes.push_back({ Parts[0], Parts[1], it->second, Parts[2] });
		}
	});

	return Processes;

} // ListProcessesFromPS

#endif // DEKAF2_HAS_LIBPROC

//-----------------------------------------------------------------------------
/// returns the processes of the system
std::vector<ProcessInfo> ListProcesses()
//-----------------------------------------------------------------------------
{
	if (kDirExists("/proc"))
	{
		return ListProcessesFromProc();
	}

#ifdef DEKAF2_HAS_LIBPROC
	return ListProcessesFromLibproc();
#else
	return ListProcessesFromPS();
#endif

} // ListProcesses

#endif // DEKAF2_IS_WINDOWS

#if defined(DEKAF2_IS_WINDOWS) || defined(DEKAF2_IS_OSX)
//-----------------------------------------------------------------------------
/// returns the time of the start of the system - on Linux, /proc/stat tells it
KUnixTime BootTime()
//-----------------------------------------------------------------------------
{
#ifdef DEKAF2_IS_WINDOWS

	return KUnixTime::now() - chrono::milliseconds(::GetTickCount64());

#else

	struct timeval Boot {};
	std::size_t    iSize = sizeof(Boot);

	if (::sysctlbyname("kern.boottime", &Boot, &iSize, nullptr, 0) != 0)
	{
		return KUnixTime(std::time_t(0));
	}

	return KUnixTime(std::time_t(Boot.tv_sec));

#endif

} // BootTime
#endif

} // end of anonymous namespace

//-----------------------------------------------------------------------------
void KSystemStats::AddBootTime (std::time_t tBootTime)
//-----------------------------------------------------------------------------
{
	if (tBootTime <= 0)
	{
		return;
	}

	KUnixTime tBoot = KUnixTime(tBootTime);
	KDuration tAgo  = KUnixTime::now() - tBoot;

	Add("boot_time_unix", static_cast<int_t>(tBoot.to_time_t()), StatType::INTEGER);
	Add("boot_time_dtm",  kFormTimestamp(tBoot), StatType::STRING);
	Add("boot_time_ago",  static_cast<int_t>(tAgo.seconds().count()), StatType::INTEGER);

} // AddBootTime

//-----------------------------------------------------------------------------
bool KSystemStats::GatherAll ()
//-----------------------------------------------------------------------------
{
	kDebug (4, "...");

	bool bOK = true;
	if (!GatherProcInfo())          bOK = false;
	if (!GatherMiscInfo())          bOK = false;
	if (!GatherVmStatInfo())        bOK = false;
	if (!GatherDiskStats())         bOK = false;
	if (!GatherDiskUsage())         bOK = false;
	if (!GatherCpuInfo())           bOK = false;
	if (!GatherMemInfo())           bOK = false;
	if (!GatherNetstat())           bOK = false;
	if (!AddCalculations())         bOK = false;
	return (bOK);

} // GatherAll

//-----------------------------------------------------------------------------
KSystemStats::StatType KSystemStats::SenseType(KStringView sValue)
//-----------------------------------------------------------------------------
{
	if (kIsInteger(sValue))
	{
		return StatType::INTEGER;
	}

	if (kIsFloat(sValue))
	{
		return StatType::FLOAT;
	}

	return StatType::STRING;

} // SenseType

//-----------------------------------------------------------------------------
void KSystemStats::AddIntStatIfFileExists(KStringViewZ sStatName, KStringViewZ sStatFilePath)
//-----------------------------------------------------------------------------
{
	// Check to see if the requested proc stats file exists and output the
	// data it contains if it does exist, else do not output anything

	if (true == kFileExists (sStatFilePath))
	{
		KString sContents;
		KInFile oStatFile(sStatFilePath);
#ifdef DEKAF2_WITH_KLOG
		size_t iRead = oStatFile.Read(sContents, 256);
		kDebug (3, "Read {} bytes from stat file '{}' : '{}'", iRead, sStatFilePath, sContents);
#endif
		sContents.CollapseAndTrim();
		Add(sStatName, sContents, StatType::AUTO);
	}
	else
	{
		kDebug (3, "Skipping stat: '{}' because the '{}' file does not exist", sStatName, sStatFilePath);
	}

} // AddIntStatIfFileExists

//-----------------------------------------------------------------------------
bool KSystemStats::Add (KStringView sStatName, KStringView sStatValue, StatType iStatType)
//-----------------------------------------------------------------------------
{

	if (sStatName.empty())
	{
		kDebug (2, "Error: missing stat name");
		return false;
	}

	if (sStatValue.empty())
	{
		kDebug (2, "Error: missing stat value for '{}'", sStatName);
		return false;
	}

	m_Stats.Add(sStatName, StatValueType(sStatValue, iStatType));

	return true;

} // Add

//-----------------------------------------------------------------------------
bool KSystemStats::Add (KStringView sStatName, int_t iStatValue, StatType iStatType)
//-----------------------------------------------------------------------------
{
	if (sStatName.empty())
	{
		kDebug (2, "Error: missing stat name");
		return (false);
	}

	if (0 > iStatValue)
	{
		iStatValue = 0;
	}

	m_Stats.Add (sStatName, StatValueType(KString::to_string(iStatValue), iStatType));

	return true;

} // KSystemStats::Add

//-----------------------------------------------------------------------------
bool KSystemStats::Add(KStringView sStatName, double iStatValue, StatType iStatType)
//-----------------------------------------------------------------------------
{
	if (sStatName.empty())
	{
		kDebug(2, "Error: missing stat name");
		return (false);;
	}

	if (0 > iStatValue)
	{
		iStatValue = 0;
	}

	m_Stats.Add(sStatName, StatValueType(KString::to_string(iStatValue), iStatType));

	return (true);

} // KSystemStats::Add

//-----------------------------------------------------------------------------
KStringView KSystemStats::StatTypeToString(StatType iStatType)
//-----------------------------------------------------------------------------
{
	switch (iStatType)
	{
		case StatType::INTEGER:
			return "integer";
			break;

		case StatType::FLOAT:
			return "float";
			break;

		case StatType::AUTO:
		case StatType::STRING:
			return "string";
			break;
	}

	return "";

} // StatTypeToString

//-----------------------------------------------------------------------------
bool KSystemStats::GatherProcInfo ()
//-----------------------------------------------------------------------------
{
	kDebug (4, "...");

#ifdef DEKAF2_IS_WINDOWS

	// Windows has no load average
	auto iUptime = static_cast<int_t>(::GetTickCount64() / 1000);

	Add ("uptime_seconds", iUptime, StatType::INTEGER);

	FILETIME Idle, Kernel, User;

	if (::GetSystemTimes(&Idle, &Kernel, &User) && iUptime > 0)
	{
		// the idle time of all processors, as in /proc/uptime of Linux
		auto iIdle = static_cast<int_t>(Ticks(Idle) / 10000000);

		Add ("uptime_idle_seconds", iIdle,                   StatType::INTEGER);
		Add ("uptime_idle_percent", (iIdle * 100) / iUptime, StatType::INTEGER);
	}

	PERFORMANCE_INFORMATION Performance {};

	if (::GetPerformanceInfo(&Performance, sizeof(Performance)))
	{
		Add ("threads_total",   static_cast<int_t>(Performance.ThreadCount),  StatType::INTEGER);
		Add ("processes_total", static_cast<int_t>(Performance.ProcessCount), StatType::INTEGER);
	}

	auto sWindowsVersion = WindowsVersion();

	if (!sWindowsVersion.empty())
	{
		Add ("windows_version", sWindowsVersion, StatType::STRING);
	}

#else

	KString sVersion;
	KString sLoadAvg;
	KString sUptime;

	if (kDirExists ("/proc"))
	{
		{
			kDebug (3, "reading {} ...", PROC_VERSION);
			KInFile oProcFile(PROC_VERSION);
			oProcFile.ReadAll(sVersion);
			kDebug(3, "contents of {}: {}", PROC_VERSION, sVersion);
		}
		{
			kDebug (3, "reading {} ...", PROC_LOADAVG);
			KInFile oLoadFile(PROC_LOADAVG);
			oLoadFile.ReadAll(sLoadAvg);
		}
		{
			kDebug (3, "reading {} ...", PROC_UPTIME);
			KInFile oUptimeFile(PROC_UPTIME);
			oUptimeFile.ReadAll(sUptime);
		}
	
		if (!sLoadAvg.empty())
		{
			sVersion.TrimRight();
			kDebug (3, "LOADAVG: {}", sLoadAvg);
			sLoadAvg.Replace('/', ' ',true);
			auto Parts = sLoadAvg.Split(' ');
	
			// 4th item had a slash and became 2 items with the Replace+kSplit above, and last field is just most recent PID (disregard)
			if (Parts.size() == 6)
			{
				Add ("load_average_1min",  Parts.at(0), StatType::FLOAT);
				Add ("load_average_5min",  Parts.at(1), StatType::FLOAT);
				Add ("load_average_15min", Parts.at(2), StatType::FLOAT);
				Add ("threads_runnable",   Parts.at(3), StatType::INTEGER);
				Add ("threads_total",      Parts.at(4), StatType::INTEGER);
			}
		}
	
		if (!sUptime.empty())
		{
			sUptime.TrimRight();
			kDebug (3, "UPTIME: {}", sUptime);
			auto Parts = sUptime.Split(' ');
			if (Parts.size() == 2)
			{
				double nTotal = Parts.at(0).Double();
				double nIdle  = Parts.at(1).Double();
				if (nTotal > 0.0)
				{
					KStringView sTotal (Parts.at(0));
					sTotal.ClipAt(".");
					KStringView sIdle (Parts.at(1));
					sIdle.ClipAt(".");
					Add ("uptime_seconds",      sTotal,                   StatType::INTEGER);
					Add ("uptime_idle_seconds", sIdle,                    StatType::INTEGER);
					Add ("uptime_idle_percent", ((nIdle * 100) / nTotal), StatType::INTEGER);
				}
				else
				{
					kDebug(2, "invalid value for {}: {}", "uptime_seconds", nTotal);
				}
			}
		}

		if (!sVersion.empty())
		{
			sVersion.TrimRight();
			Add ("unix_version", sVersion, StatType::STRING);
		}	
	}
	else
	{
		// without the /proc tables, like on macOS: the load average from the kernel
		double LoadAverage[3];

		if (::getloadavg(LoadAverage, 3) == 3)
		{
			Add ("load_average_1min",  LoadAverage[0], StatType::FLOAT);
			Add ("load_average_5min",  LoadAverage[1], StatType::FLOAT);
			Add ("load_average_15min", LoadAverage[2], StatType::FLOAT);
		}

#ifdef DEKAF2_IS_OSX
		auto tBoot = BootTime();

		if (tBoot.to_time_t() > 0)
		{
			Add ("uptime_seconds", static_cast<int_t>((KUnixTime::now() - tBoot).seconds().count()), StatType::INTEGER);
		}

		// the version of the kernel, as /proc/version of Linux, and the one of macOS
		sVersion = SysctlString("kern.version");

		if (!sVersion.empty())
		{
			Add ("unix_version", sVersion, StatType::STRING);
		}

		auto sMacOSVersion = SysctlString("kern.osproductversion");

		if (!sMacOSVersion.empty())
		{
			Add ("macos_version", sMacOSVersion, StatType::STRING);
		}
#endif
	}

#endif // DEKAF2_IS_WINDOWS

	kDebug (3, "sizing {} ...", KLog::getInstance().GetDebugLog());
	Add ("bytes_klog",             NeverNegative (kFileSize (KLog::getInstance().GetDebugLog())),StatType::INTEGER);

#ifndef DEKAF2_IS_WINDOWS
	kDebug (3, "sizing a bunch of files in /var/log ...");
	Add ("bytes_var_log_cron",     NeverNegative (kFileSize ("/var/log/cron")),     StatType::INTEGER);
	Add ("bytes_var_log_dmesg",    NeverNegative (kFileSize ("/var/log/dmesg")),    StatType::INTEGER);
	Add ("bytes_var_log_faillog",  NeverNegative (kFileSize ("/var/log/faillog")),  StatType::INTEGER);
	Add ("bytes_var_log_lastlog",  NeverNegative (kFileSize ("/var/log/lastlog")),  StatType::INTEGER);
	Add ("bytes_var_log_maillog",  NeverNegative (kFileSize ("/var/log/maillog")),  StatType::INTEGER);
	Add ("bytes_var_log_messages", NeverNegative (kFileSize ("/var/log/messages")), StatType::INTEGER);
	Add ("bytes_var_log_rpmpkgs",  NeverNegative (kFileSize ("/var/log/rpmpkgs")),  StatType::INTEGER);
	Add ("bytes_var_log_secure",   NeverNegative (kFileSize ("/var/log/secure")),   StatType::INTEGER);
	Add ("bytes_var_log_wtmp",     NeverNegative (kFileSize ("/var/log/wtmp")),     StatType::INTEGER);
#endif

	return (true);

} // GatherProcInfo

//-----------------------------------------------------------------------------
bool KSystemStats::GatherMiscInfo ()
//-----------------------------------------------------------------------------
{
	kDebug (4, "...");

/*
// PROC_MISC file contents look like this
	59 autofs
	60 device-mapper
	61 network_throughput
	62 network_latency
	63 cpu_dma_latency
	175 agpgart
	144 nvram
	228 hpet
	135 rtc
	231 snapshot
	227 mcelog
*/

	kDebug (3, "reading {} ...", PROC_MISC);

	KInFile file(PROC_MISC);
	file.SetReaderRightTrim("\r\n\t ");

	KString sLine;

	while (file.ReadLine(sLine))
	{
		kDebug (3, "{}: raw: {}", PROC_MISC, sLine);

		auto Parts = sLine.Split(' ');
		if (Parts.size() != 2)
		{
			kDebug (3, "Got an unexpected line: {}", sLine);
			continue;
		}
		// Note: the value is on the LHS and the name is on the RHS:
		KString sName = "misc_";
		sName += Parts.at(1).ToLower();
		sName.Replace('-', '_',true);
		kDebugLog (3, "{}:  ok: {}={}", PROC_MISC, sName, Parts.at(0));
		Add (sName, Parts.at(0), StatType::INTEGER);
	}

	file.close();

	Add ("hostname",  kGetHostname (/*khostname=*/false), StatType::STRING); // physical hostname
	if (kFileExists ("/etc/khostname"))
	{
		Add ("khostname", kGetHostname (/*khostname=*/true),  StatType::STRING); // logical hostname
	}

	return (true);

} // GatherMiscInfo

//-----------------------------------------------------------------------------
bool KSystemStats::GatherVmStatInfo ()
//-----------------------------------------------------------------------------
{
	kDebug (4, "...");

/*
// PROC_VMSTAT contents look like this
	nr_anon_pages 143454
	nr_mapped 10753
	nr_file_pages 3311744
	nr_slab 239293
	nr_page_table_pages 3878
	nr_dirty 15
	nr_writeback 0
	nr_unstable 0
	nr_bounce 0
	numa_hit 9398792144
	numa_miss 0
	numa_foreign 0
	numa_interleave 173115
	numa_local 9398792144
	numa_other 0
	pgpgin 276614177
	pgpgout 1000497471
	pswpin 0
	pswpout 30
	pgalloc_dma 11
	pgalloc_dma32 887259513
	pgalloc_normal 8523582940
	pgalloc_high 0
	pgfree 9411247260
	pgactivate 540296860
	pgdeactivate 24792570
	pgfault 11703492704
	pgmajfault 13491
	pgrefill_dma 0
	pgrefill_dma32 5228538
	pgrefill_normal 24589132
	pgrefill_high 0
	pgsteal_dma 0
	pgsteal_dma32 27479825
	pgsteal_normal 117106795
	pgsteal_high 0
	pgscan_kswapd_dma 0
	pgscan_kswapd_dma32 28967840
	pgscan_kswapd_normal 123811776
	pgscan_kswapd_high 0
	pgscan_direct_dma 0
	pgscan_direct_dma32 12160
	pgscan_direct_normal 49152
	pgscan_direct_high 0
	pginodesteal 24
	slabs_scanned 20908160
	kswapd_steal 144530732
	kswapd_inodesteal 8820541
	pageoutrun 273602
	allocstall 64
	pgrotated 1374
*/

	kDebug (3, "reading {} ...", PROC_VMSTAT);

	KInFile file(PROC_VMSTAT);
	file.SetReaderRightTrim("\r\n\t ");

	KString sLine;
	while (file.ReadLine(sLine))
	{
		if (sLine.empty())
		{
			continue;
		}

		kDebug (3, "{}: raw: {}", PROC_VMSTAT, sLine);

		auto Parts = sLine.Split(' ');

		if (Parts.size() != 2)
		{
			kDebug (3, "Got unexpected line from {}", PROC_VMSTAT);
			continue;
		}

		KString sName = "vmstat_";
		sName += Parts.at(0);

		kDebug (3, "{}:  ok: {}={}", PROC_VMSTAT, sName, Parts.at(1));
		Add (sName, Parts.at(1), StatType::INTEGER);
	}

	return (true);

} // GatherVmStatInfo

//-----------------------------------------------------------------------------
void KSystemStats::AddDiskStat (KStringView sValue, KStringView sDevice, KStringView sStat)
//-----------------------------------------------------------------------------
{
	KString sName;
	sName.Format ("disk_{}_{}", sDevice, sStat);

	Add (sName, sValue, StatType::INTEGER);

} // AddDiskStat

//-----------------------------------------------------------------------------
bool KSystemStats::GatherDiskStats ()
//-----------------------------------------------------------------------------
{
	kDebug (4, "...");

/*
 * PROC_DISKSTATS file contents look like this
	1    0 ram0 0 0 0 0 0 0 0 0 0 0 0
	1    1 ram1 0 0 0 0 0 0 0 0 0 0 0
	1    2 ram2 0 0 0 0 0 0 0 0 0 0 0
	1    3 ram3 0 0 0 0 0 0 0 0 0 0 0
	1    4 ram4 0 0 0 0 0 0 0 0 0 0 0
	1    5 ram5 0 0 0 0 0 0 0 0 0 0 0
	1    6 ram6 0 0 0 0 0 0 0 0 0 0 0
	1    7 ram7 0 0 0 0 0 0 0 0 0 0 0
	1    8 ram8 0 0 0 0 0 0 0 0 0 0 0
	1    9 ram9 0 0 0 0 0 0 0 0 0 0 0
	1   10 ram10 0 0 0 0 0 0 0 0 0 0 0
	1   11 ram11 0 0 0 0 0 0 0 0 0 0 0
	1   12 ram12 0 0 0 0 0 0 0 0 0 0 0
	1   13 ram13 0 0 0 0 0 0 0 0 0 0 0
	1   14 ram14 0 0 0 0 0 0 0 0 0 0 0
	1   15 ram15 0 0 0 0 0 0 0 0 0 0 0
	8    0 sda 3654320 1625959 144740703 137322189 70269836 109296419 1436552406 966516648 0 328918727 1103914695
	8    1 sda1 90 1140 2476 3044 7 0 14 173 0 2919 3217
	8    2 sda2 565227 229884 20559115 25297996 8401973 23777683 257439336 2625334387 0 155249916 2650640067
	8    3 sda3 2035765 1080468 97134776 66935073 31379303 26017663 459188008 693235760 0 150233850 760217592
	8    4 sda4 1053214 314443 27043952 45084548 30488553 59501073 719925048 1942913624 0 154409901 1988019873
	253    0 dm-0 5278807 0 144735650 179426846 179569019 0 1436552152 2691479835 0 328979561 2871517735
	253    1 dm-1 124 0 992 55914 30 0 240 1050 0 4422 56964
	2    0 fd0 0 0 0 0 0 0 0 0 0 0 0
	9    0 md0 0 0 0 0 0 0 0 0 0 0 0
	8   16 sdb 3256566 1809200 408476820 100691205 23356710 71293985 564331152 534532608 0 78906068 635224837
	8   17 sdb1 3256514 1808694 408475704 100689752 23356709 71293985 564331150 534532579 0 78904588 635223494
*/

	kDebug (3, "reading {} ...", PROC_DISKSTATS);

	KInFile file(PROC_DISKSTATS);
	file.SetReaderRightTrim("\r\n\t ");

	KString sLine;
	while (file.ReadLine(sLine))
	{
		if (sLine.empty())
		{
			continue;
		}

		kDebug (3, "{}: raw: {}", PROC_DISKSTATS, sLine);

		sLine.TrimLeft();
		sLine.ReplaceRegex("[ ]{2,}", " ", true);

		auto Parts = sLine.Split(' ');
		if (Parts.size() != 14)
		{
			kDebug(3, "Got unexpected line from {}", PROC_DISKSTATS);
			continue;
		}
		KStringView sDisk (Parts.at(3-1));

		// AWS uses xv instead of s as the leading character
		// These are the 3 types of drives we care about e.g. sda, xvda, nvme
		if (!sDisk.MatchRegex("^sd[a-z]", 0).empty() || !sDisk.MatchRegex("^xvd[a-x]", 0).empty() || !sDisk.MatchRegex("^nvme", 0).empty())
		{
			// see: https://www.kernel.org/doc/Documentation/ABI/testing/procfs-diskstats
			AddDiskStat (Parts.at(4-1),  sDisk, "reads_completed");
			AddDiskStat (Parts.at(5-1),  sDisk, "reads_merged");
			AddDiskStat (Parts.at(6-1),  sDisk, "sectors_read");
			AddDiskStat (Parts.at(7-1),  sDisk, "msecs_spent_reading");
			AddDiskStat (Parts.at(8-1),  sDisk, "writes_completed");
			AddDiskStat (Parts.at(9-1),  sDisk, "writes_merged");
			AddDiskStat (Parts.at(10-1), sDisk, "sectors_written");
			AddDiskStat (Parts.at(11-1), sDisk, "msecs_spent_writing");
			AddDiskStat (Parts.at(12-1), sDisk, "ios_in_progress");
			AddDiskStat (Parts.at(13-1), sDisk, "msecs_spent_io");
			AddDiskStat (Parts.at(14-1), sDisk, "msecs_spent_io_weighted");
		}
	}

	return (true);

} // GatherDiskStats

//-----------------------------------------------------------------------------
bool KSystemStats::GatherDiskUsage ()
//-----------------------------------------------------------------------------
{
	kDebug (4, "...");

	uint64_t iTotalBytes { 0 };
	uint64_t iFreeBytes  { 0 }; // the free space for an unprivileged user

#ifdef DEKAF2_IS_WINDOWS

	// the system drive, in place of the root of Unix
	wchar_t      sDrive[MAX_PATH];
	auto         iLength = ::GetEnvironmentVariableW(L"SystemDrive", sDrive, MAX_PATH);
	std::wstring wsRoot  = (iLength > 0 && iLength < MAX_PATH) ? std::wstring(sDrive, iLength) : std::wstring(L"C:");

	wsRoot += L'\\';

	ULARGE_INTEGER Available, Total, Free;

	if (!::GetDiskFreeSpaceExW(wsRoot.c_str(), &Available, &Total, &Free))
	{
		kDebug(2, "GetDiskFreeSpaceEx() failed: {}", ::GetLastError());
		return false;
	}

	iTotalBytes = Total.QuadPart;
	iFreeBytes  = Available.QuadPart;

#else

	struct statvfs vfs {};

	if (0 != statvfs("/", &vfs))
	{
		kDebug(2, "statvfs('/') failed");
		return false;
	}

	const uint64_t iBlockSize = static_cast<uint64_t>(vfs.f_frsize ? vfs.f_frsize : vfs.f_bsize);

	iTotalBytes = static_cast<uint64_t>(vfs.f_blocks) * iBlockSize;
	iFreeBytes  = static_cast<uint64_t>(vfs.f_bavail) * iBlockSize;

#endif

	const uint64_t iUsedBytes = (iTotalBytes >= iFreeBytes) ? (iTotalBytes - iFreeBytes) : 0;

	double nUsedPct = 0.0;
	double nFreePct = 0.0;

	if (iTotalBytes > 0)
	{
		nUsedPct = (static_cast<double>(iUsedBytes) * 100.0) / static_cast<double>(iTotalBytes);
		nFreePct = (static_cast<double>(iFreeBytes) * 100.0) / static_cast<double>(iTotalBytes);
	}

	Add("disk_root_total_kb",      static_cast<int64_t>(iTotalBytes / 1024), StatType::INTEGER);
	Add("disk_root_used_kb",       static_cast<int64_t>(iUsedBytes  / 1024), StatType::INTEGER);
	Add("disk_root_free_kb",       static_cast<int64_t>(iFreeBytes  / 1024), StatType::INTEGER);
	Add("disk_root_used_percent",  nUsedPct,                                 StatType::FLOAT);
	Add("disk_root_free_percent",  nFreePct,                                 StatType::FLOAT);

#ifndef DEKAF2_IS_WINDOWS

	// NTFS has no inodes
	const uint64_t iTotalInodes = static_cast<uint64_t>(vfs.f_files);
	const uint64_t iFreeInodes  = static_cast<uint64_t>(vfs.f_favail);
	const uint64_t iUsedInodes  = (iTotalInodes >= iFreeInodes) ? (iTotalInodes - iFreeInodes) : 0;

	double nUsedInodePct = 0.0;
	double nFreeInodePct = 0.0;

	if (iTotalInodes > 0)
	{
		nUsedInodePct = (static_cast<double>(iUsedInodes) * 100.0) / static_cast<double>(iTotalInodes);
		nFreeInodePct = (static_cast<double>(iFreeInodes) * 100.0) / static_cast<double>(iTotalInodes);
	}

	Add("inode_root_total",        static_cast<int64_t>(iTotalInodes), StatType::INTEGER);
	Add("inode_root_used",         static_cast<int64_t>(iUsedInodes),  StatType::INTEGER);
	Add("inode_root_free",         static_cast<int64_t>(iFreeInodes),  StatType::INTEGER);
	Add("inode_root_used_percent", nUsedInodePct,                       StatType::FLOAT);
	Add("inode_root_free_percent", nFreeInodePct,                       StatType::FLOAT);

#endif

	return true;

} // GatherDiskUsage

//-----------------------------------------------------------------------------
bool KSystemStats::GatherCpuInfo ()
//-----------------------------------------------------------------------------
{
	kDebug (4, "...");

#if defined(DEKAF2_IS_WINDOWS)

	Add (CPUINFO_NUM_CORES, static_cast<int_t>(kGetCPUCount()), StatType::INTEGER);

	auto sModelName = ProcessorString(L"ProcessorNameString");

	if (!sModelName.empty())
	{
		Add ("cpuinfo_model_name", sModelName, StatType::STRING);
	}

	auto sVendor = ProcessorString(L"VendorIdentifier");

	if (!sVendor.empty())
	{
		Add ("cpuinfo_vendor_id", sVendor, StatType::STRING);
	}

	auto iMHz = ProcessorNumber(L"~MHz");

	if (iMHz)
	{
		Add ("cpuinfo_cpu_mhz", static_cast<int_t>(iMHz), StatType::INTEGER);
	}

	FILETIME Idle, Kernel, User;

	if (::GetSystemTimes(&Idle, &Kernel, &User))
	{
		// the times of all processors in 1/100 seconds, as the jiffies of Linux - the
		// kernel time includes the idle time
		Add ("procs_user_mode",   static_cast<int_t>(Ticks(User) / 100000),                 StatType::INTEGER);
		Add ("procs_kernel_mode", static_cast<int_t>((Ticks(Kernel) - Ticks(Idle)) / 100000), StatType::INTEGER);
		Add ("procs_idle",        static_cast<int_t>(Ticks(Idle) / 100000),                 StatType::INTEGER);
	}

	AddBootTime(BootTime().to_time_t());

	return true;

#elif defined(DEKAF2_IS_OSX)

	Add (CPUINFO_NUM_CORES, static_cast<int_t>(kGetCPUCount()), StatType::INTEGER);

	auto sModelName = SysctlString("machdep.cpu.brand_string");

	if (!sModelName.empty())
	{
		Add ("cpuinfo_model_name", sModelName, StatType::STRING);
	}

	// the vendor and the frequency exist for Intel processors only
	auto sVendor = SysctlString("machdep.cpu.vendor");

	if (!sVendor.empty())
	{
		Add ("cpuinfo_vendor_id", sVendor, StatType::STRING);
	}

	auto iFrequency = SysctlNumber<uint64_t>("hw.cpufrequency");

	if (iFrequency)
	{
		Add ("cpuinfo_cpu_mhz", static_cast<int_t>(iFrequency / 1000000), StatType::INTEGER);
	}

	// the times of all processors in ticks of 1/100 seconds, as the jiffies of Linux
	host_cpu_load_info_data_t Load;
	mach_msg_type_number_t    iCount = HOST_CPU_LOAD_INFO_COUNT;
	mach_port_t               Host   = ::mach_host_self();

	if (::host_statistics(Host, HOST_CPU_LOAD_INFO, reinterpret_cast<host_info_t>(&Load), &iCount) == KERN_SUCCESS)
	{
		Add ("procs_user_mode",   static_cast<int_t>(Load.cpu_ticks[CPU_STATE_USER]),   StatType::INTEGER);
		Add ("procs_user_niced",  static_cast<int_t>(Load.cpu_ticks[CPU_STATE_NICE]),   StatType::INTEGER);
		Add ("procs_kernel_mode", static_cast<int_t>(Load.cpu_ticks[CPU_STATE_SYSTEM]), StatType::INTEGER);
		Add ("procs_idle",        static_cast<int_t>(Load.cpu_ticks[CPU_STATE_IDLE]),   StatType::INTEGER);
	}

	::mach_port_deallocate(::mach_task_self(), Host);

	AddBootTime(BootTime().to_time_t());

	return true;

#else

/*
// RHEL5 PROC_CPUINFO file looks like this
	processor	: 0
	vendor_id	: GenuineIntel
	cpu family	: 6
	model		: 44
	model name	: Intel(R) Xeon(R) CPU           X5690  @ 3.47GHz
	stepping	: 2
	cpu MHz		: 3457.999
	cache size	: 12288 KB
	fpu		: yes
	fpu_exception	: yes
	cpuid level	: 11
	wp		: yes
	flags		: fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss syscall nx rdtscp lm constant_tsc ida nonstop_tsc arat pni ssse3 cx16 sse4_1 sse4_2 popcnt lahf_lm
	bogomips	: 6915.99
	clflush size	: 64
	cache_alignment	: 64
	address sizes	: 40 bits physical, 48 bits virtual
	power management: [8]

	processor	: 1
	vendor_id	: GenuineIntel
	cpu family	: 6
	model		: 44
	o o o (rinse and repeat) o o o
*/

/*
// RHEL6 PROC_CPUINFO file contents look like this:
	address sizes   : 40 bits physical, 48 bits virtual
	apicid          : 0
	bogomips        : 5786.05
	cache size      : 20480 KB
	cache_alignment : 64
	clflush size    : 64
	core id         : 0
	cpu MHz         : 2893.029
	cpu cores       : 2
	cpu family      : 6
	cpuid level     : 13
	flags           : fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts mmx fxsr sse sse2 ss ht syscall nx rdtscp lm constant_tsc arch_perfmon pebs bts xtopology tsc_reliable nonstop_tsc aperfmperf unfair_spinlock pni pclmulqdq ssse3 cx16 sse4_1 sse4_2 popcnt aes xsave avx hypervisor lahf_lm ida arat epb xsaveopt pln pts dts
	fpu             : yes
	fpu_exception   : yes
	initial apicid  : 0
	nn                                  model           : 45
	model name      : Intel(R) Xeon(R) CPU E5-2690 0 @ 2.90GHz
	physical id     : 0
	power management:
	processor       : 0
	siblings        : 2
	stepping        : 7
	vendor_id       : GenuineIntel
	wp              : yes
*/

/*
// RHEL7 PROC_CPUINFO file contents look like this
	processor       : 0
	vendor_id       : GenuineIntel
	cpu family      : 6
	model           : 44
	model name      : Intel(R) Xeon(R) CPU           X5680  @ 3.33GHz
	stepping        : 2
	microcode       : 0xc
	cpu MHz         : 3324.999
	cache size      : 12288 KB
	physical id     : 0
	siblings        : 1
	core id         : 0
	cpu cores       : 1
	apicid          : 0
	initial apicid  : 0
	fpu             : yes
	fpu_exception   : yes
	cpuid level     : 11
	wp              : yes
	flags           : fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss syscall nx rdtscp lm constant_tsc arch_perfmon pebs bts nopl xtopology tsc_reliable nonstop_tsc aperfmperf pni pclmulqdq ssse3 cx16 sse4_1 sse4_2 popcnt aes hypervisor lahf_lm ida arat dtherm
	bogomips        : 6649.99
	clflush size    : 64
	cache_alignment : 64
	address sizes   : 40 bits physical, 48 bits virtual
	power management:
#endif
*/
	kDebug (3, "reading {} ...", PROC_CPUINFO);

	int_t iNumCores { 0 };

	KInFile file(PROC_CPUINFO);
	file.SetReaderRightTrim("\r\n\t ");

	KString sLine;
	while (file.ReadLine(sLine))
	{
		if (sLine.empty())
		{
			continue;
		}

		kDebug (3, "{}: raw: {}", PROC_CPUINFO, sLine);

		auto Parts = sLine.Split(":");
		if (Parts.size() != 2)
		{
			kDebug(2, "Got unexpected line from {}", PROC_CPUINFO);
			continue;
		}

		KString sName (Parts.at(0).ToLower());
		KString sValue (Parts.at(1));

		sName.Replace(' ', '_', 0/*from beginning*/, true/*globally*/);

		if (sValue.EndsWith(" KB"))
		{
			sName += "_kb";
			sValue.ClipAt (" ");
		}

		// restrict what we capture for cpuinfo to match the schema:
		if (!kStrIn (sName, "address_sizes,bogomips,cache_alignment,cache_size_kb,clflush_size,cpu_family,cpu_mhz,cpuid_level,flags,fpu,fpu_exception,model,model_name,power_management,processor,stepping,vendor_id,wp,microcache"))
		{
			kDebug (3, "ignoring newer cpuinfo field: {}", sName);
			continue;
		}

		if (sName == "processor")
		{
			iNumCores++;
		}
		else if (1 == iNumCores) // we only need to store CPU info for one core since every process is identical
		{
			sName.insert(0, "cpuinfo_");
			kDebug (3, "{}:  ok: {}={}", PROC_CPUINFO, sName, sValue);
			Add (sName, sValue);
		}
	}

	Add (CPUINFO_NUM_CORES, iNumCores, StatType::INTEGER);
	file.close();

	kDebug (3, "reading {} ...", PROC_STAT);
	file.open (PROC_STAT);

	while (file.ReadLine(sLine))
	{
		if (sLine.empty())
		{
			continue;
		}

/*
// PROC_STAT file contents look like this:
	cpu  1066906 27020 252903 432082575 15189156 19883 52371 0
	cpu0 309398 22481 99870 103148199 8527518 18746 48368 0
	cpu1 249791 892 57770 109875547 1985014 761 2257 0
	cpu2 245379 1766 47833 109519748 2355734 340 1319 0
	cpu3 262336 1879 47428 109539080 2320888 35 425 0
	intr 1138754577 1092858647 83 0 3 3 0 5 0 0 0 0 0 752 0 0 0 0 0 0 0 0 0 0 0 0...
	ctxt 1951140112
	btime 1432653604
	processes 896829
	procs_running 2
	procs_blocked 0
*/

		kDebug (3, "{}: raw: {}", PROC_STAT, sLine);

		auto Parts = sLine.Split(' ');

		if (Parts.at(0) == "cpu")
		{
			kDebug (3, "{}: {} parts: {}", PROC_STAT, Parts.size(), sLine);

			uint64_t idx = 0;
			Add ("procs_user_mode",           (++idx < Parts.size()) ? Parts.at(idx) : "0", StatType::INTEGER);
			Add ("procs_user_niced",          (++idx < Parts.size()) ? Parts.at(idx) : "0", StatType::INTEGER);
			Add ("procs_kernel_mode",         (++idx < Parts.size()) ? Parts.at(idx) : "0", StatType::INTEGER);
			Add ("procs_idle",                (++idx < Parts.size()) ? Parts.at(idx) : "0", StatType::INTEGER);
			Add ("procs_iowait",              (++idx < Parts.size()) ? Parts.at(idx) : "0", StatType::INTEGER);
			Add ("procs_hardware_interrupts", (++idx < Parts.size()) ? Parts.at(idx) : "0", StatType::INTEGER);
			Add ("procs_software_interrupts", (++idx < Parts.size()) ? Parts.at(idx) : "0", StatType::INTEGER);
			Add ("procs_steal_wait",          (++idx < Parts.size()) ? Parts.at(idx) : "0", StatType::INTEGER);
			Add ("procs_guest",               (++idx < Parts.size()) ? Parts.at(idx) : "2", StatType::INTEGER);
			Add ("procs_guest_nice",          (++idx < Parts.size()) ? Parts.at(idx) : "2", StatType::INTEGER);
		}
		else if (Parts.at(0) == "btime")
		{
			AddBootTime(std::time_t(Parts.at(1).Int64()));
		}
	}

	return (true);

#endif

} // GatherCpuInfo

//-----------------------------------------------------------------------------
bool KSystemStats::GatherMemInfo ()
//-----------------------------------------------------------------------------
{
	kDebug (4, "...");

#if defined(DEKAF2_IS_WINDOWS)

	MEMORYSTATUSEX Status {};
	Status.dwLength = sizeof(Status);

	if (::GlobalMemoryStatusEx(&Status))
	{
		Add ("meminfo_memtotal_kb",     static_cast<int_t>(Status.ullTotalPhys / 1024), StatType::INTEGER);
		// the available memory of Windows includes the file cache (the standby list) -
		// the free memory is the same, and there is no cache that AddCalculations() adds
		Add ("meminfo_memfree_kb",      static_cast<int_t>(Status.ullAvailPhys / 1024), StatType::INTEGER);
		Add ("meminfo_memavailable_kb", static_cast<int_t>(Status.ullAvailPhys / 1024), StatType::INTEGER);
	}

	PERFORMANCE_INFORMATION Performance {};

	if (::GetPerformanceInfo(&Performance, sizeof(Performance)))
	{
		// the committed memory and its limit (memory and paging files), as Committed_AS
		// and CommitLimit of Linux
		auto iPageSize = static_cast<uint64_t>(Performance.PageSize);

		Add ("meminfo_committed_as_kb", static_cast<int_t>(Performance.CommitTotal * iPageSize / 1024), StatType::INTEGER);
		Add ("meminfo_commitlimit_kb",  static_cast<int_t>(Performance.CommitLimit * iPageSize / 1024), StatType::INTEGER);
	}

	return true;

#elif defined(DEKAF2_IS_OSX)

	auto iTotal = SysctlNumber<uint64_t>("hw.memsize");

	if (iTotal)
	{
		Add ("meminfo_memtotal_kb", static_cast<int_t>(iTotal / 1024), StatType::INTEGER);
	}

	vm_statistics64_data_t VM;
	mach_msg_type_number_t iCount    = HOST_VM_INFO64_COUNT;
	mach_port_t            Host      = ::mach_host_self();
	vm_size_t              iPageSize { 0 };

	if (::host_page_size(Host, &iPageSize) == KERN_SUCCESS
	 && ::host_statistics64(Host, HOST_VM_INFO64, reinterpret_cast<host_info64_t>(&VM), &iCount) == KERN_SUCCESS)
	{
		auto KB = [iPageSize](uint64_t iPages)
		{
			return static_cast<int_t>(iPages * iPageSize / 1024);
		};

		// as on Linux, the free memory does not include the cache - the inactive pages,
		// which macOS frees first, count as the cache
		Add ("meminfo_memfree_kb",      KB(VM.free_count + VM.speculative_count),                     StatType::INTEGER);
		Add ("meminfo_cached_kb",       KB(VM.inactive_count),                                        StatType::INTEGER);
		Add ("meminfo_memavailable_kb", KB(VM.free_count + VM.speculative_count + VM.inactive_count), StatType::INTEGER);
		Add ("meminfo_active_kb",       KB(VM.active_count),                                          StatType::INTEGER);
		Add ("meminfo_inactive_kb",     KB(VM.inactive_count),                                        StatType::INTEGER);
		Add ("meminfo_wired_kb",        KB(VM.wire_count),                                            StatType::INTEGER);
		Add ("meminfo_compressed_kb",   KB(VM.compressor_page_count),                                 StatType::INTEGER);
	}

	::mach_port_deallocate(::mach_task_self(), Host);

	xsw_usage   Swap {};
	std::size_t iSize = sizeof(Swap);

	if (::sysctlbyname("vm.swapusage", &Swap, &iSize, nullptr, 0) == 0)
	{
		Add ("meminfo_swaptotal_kb", static_cast<int_t>(Swap.xsu_total / 1024), StatType::INTEGER);
		Add ("meminfo_swapfree_kb",  static_cast<int_t>(Swap.xsu_avail / 1024), StatType::INTEGER);
	}

	return true;

#else

/*
// PROC_MEMINFO file contents look like this
	MemTotal:     16436048 kB
	MemFree:       1649276 kB
	Buffers:       1638960 kB
	Cached:       11479084 kB
	SwapCached:          0 kB
	Active:        6451164 kB
	Inactive:      7336724 kB
	Active(anon):    1660716 kB
	Inactive(anon):    40876 kB
	Active(file):    5182252 kB
	Inactive(file):  3019156 kB
	HighTotal:           0 kB
	HighFree:            0 kB
	LowTotal:     16436048 kB
	LowFree:       1649276 kB
	SwapTotal:     2031608 kB
	SwapFree:      2031496 kB
	Dirty:           68096 kB
	Writeback:           0 kB
	AnonPages:      668920 kB
	Mapped:          45616 kB
	Slab:           956300 kB
	PageTables:      15728 kB
	NFS_Unstable:        0 kB
	Bounce:              0 kB
	CommitLimit:  10249632 kB
	Committed_AS:  1305360 kB
	VmallocTotal: 34359738367 kB
	VmallocUsed:    263940 kB
	VmallocChunk: 34359473943 kB
	HugePages_Total:     0
	HugePages_Free:      0
	HugePages_Rsvd:      0
	Hugepagesize:     2048 kB
*/

	kDebug (3, "reading {} ...", PROC_MEMINFO);

	KInFile file(PROC_MEMINFO);
	file.SetReaderRightTrim("\r\n\t ");

	KString sLine;
	while(file.ReadLine(sLine))
	{
		if (sLine.empty())
		{
			continue;
		}

		kDebug (3, "{}: raw: {}", PROC_MEMINFO, sLine);

		auto Parts = sLine.Split(":");

		if (Parts.size() != 2)
		{
			kDebug (2, "Got unexpected line from {}", PROC_MEMINFO);
			continue;
		}

		KString sName  (Parts.at(0).ToLower());
		KStringView sValue (Parts.at(1));

		// Put KB label in Name and remove from value
		if (sValue.EndsWith(" kB"))
		{
			sName += KString ("_kb");
			sValue.ClipAt (" ");
		}

		sName.insert(0,"meminfo_");
		// handle these anomolies:
		//	Active(anon):    1660716 kB
		//	Inactive(anon):    40876 kB
		//	Active(file):    5182252 kB
		//	Inactive(file):  3019156 kB

		sName.Replace ('(','_');
		sName.ClipAt (")");

		kDebug (3, "{}:  ok: {}={}", PROC_MEMINFO, sName, sValue);
		Add (sName, sValue, StatType::INTEGER);
	}

	return (true);

#endif

} // GatherMemInfo

//-----------------------------------------------------------------------------
bool KSystemStats::GatherNetstat ()
//-----------------------------------------------------------------------------
{
	kDebug (4, "counting the connections ...");

#ifndef DEKAF2_IS_WINDOWS
	KInShell pipe;
	pipe.SetReaderRightTrim("\r\n\t ");

	if (!pipe.Open ("netstat -n")) // <-- we need to add --numeric to netstat so that it runs faster and does not try to resolve hostnames, etc.
	{
		m_sLastError = "command failed: netstat";
		return (false);
	}
#endif

	// iniitialize all possible values coming back from netstat so that missing ones have zeros instead of nulls:
	KProps<KString, int_t> Netstat;
	Netstat.Add ("netstat_tcp_close_wait",  0);
	Netstat.Add ("netstat_tcp_closing",     0);
	Netstat.Add ("netstat_tcp_established", 0);
	Netstat.Add ("netstat_tcp_fin_wait1",   0);
	Netstat.Add ("netstat_tcp_fin_wait2",   0);
	Netstat.Add ("netstat_tcp_last_ack",    0);
	Netstat.Add ("netstat_tcp_syn_recv",    0);
	Netstat.Add ("netstat_tcp_syn_sent",    0);
	Netstat.Add ("netstat_tcp_time_wait",   0);
	Netstat.Add ("netstat_unix_dgram",      0);
	Netstat.Add ("netstat_unix_stream",     0);
	Netstat.Add ("netstat_unix_unknown",    0);

#ifdef DEKAF2_IS_WINDOWS

	// the TCP connections from the system - Windows cannot list unix domain sockets
	CountTcpConnections(AF_INET,  Netstat);
	CountTcpConnections(AF_INET6, Netstat);

#else

	// now run "netstat" command and increment any occurances of these:
	KString sLine;

	while (pipe.ReadLine(sLine))
	{
		if (sLine.empty())
		{
			continue;
		}

		kDebug (3, "{}: raw: {}", "netstat", sLine);

		// Proto Recv-Q Send-Q Local Address               Foreign Address             State
		// tcp        0      0 stow:54051                  207-223-245-134.conte:https TIME_WAIT
		// tcp        0    196 stow:ssh                    maynard-dhcp-227-78-1:62799 ESTABLISHED

		// Active UNIX domain sockets (w/o servers)
		// Proto RefCnt Flags       Type       State         I-Node Path
		// unix  16     [ ]         DGRAM                    6438   /dev/log
		// unix  2      [ ]         DGRAM                    1739   @/org/kernel/udev/udevd
		// unix  3      [ ]         STREAM     CONNECTED     27941272

		// macOS:
		// tcp4       0      0  192.168.1.5.50124      17.57.146.23.5223      ESTABLISHED
		// Address          Type   Recv-Q Send-Q    Inode     Conn     Refs  Nextref Addr
		// a2b4c46e1d08ad03 stream      0      0        0 a2b4c46e...        0        0

		auto Parts = sLine.Split(' ');

		KString sName("netstat_");

		if (kStrIn (Parts.at(0), "tcp,tcp4,tcp6,tcp46"))
		{
			KString sState = Parts.at (Parts.size() - 1).ToLower();

			// the states that BSD (macOS) names differently than Linux
			if (sState == "fin_wait_1")
			{
				sState = "fin_wait1";
			}
			else if (sState == "fin_wait_2")
			{
				sState = "fin_wait2";
			}
			else if (sState == "syn_received")
			{
				sState = "syn_recv";
			}

			sName += "tcp_";
			sName += sState;
			Netstat[sName]++;
		}
		else if (Parts.at(0) == "unix" && Parts.size() >= 5)
		{
			sName += "unix_";
			sName += Parts.at (5 - 1).ToLower();
			Netstat[sName]++;
		}
#ifdef DEKAF2_IS_OSX
		else if (Parts.size() >= 2 && kStrIn (Parts.at(1), "stream,dgram"))
		{
			// a unix domain socket of macOS, with its address and its type
			sName += "unix_";
			sName += Parts.at(1);
			Netstat[sName]++;
		}
#endif
	}

#endif // DEKAF2_IS_WINDOWS

	for (const auto& stat : Netstat)
	{
		Add(stat.first, stat.second, StatType::INTEGER);
	}

#ifndef DEKAF2_IS_WINDOWS
	// two random parms we care about from experience:
	AddIntStatIfFileExists ("ipv4_tcp_timestamps",       "/proc/sys/net/ipv4/tcp_timestamps");
	AddIntStatIfFileExists ("ipv4_tcp_window_scaling",   "/proc/sys/net/ipv4/tcp_window_scaling");

	// mentioned on http://www.outsystems.com/forums/discussion/6956/how-to-tune-the-tcp-ip-stack-for-high-volume-of-web-requests/
	AddIntStatIfFileExists ("ipv4_tcp_fin_timeout",      "/proc/sys/net/ipv4/tcp_fin_timeout");
	AddIntStatIfFileExists ("ipv4_ip_local_port_range",  "/proc/sys/net/ipv4/ip_local_port_range");  //-- min max, see below

	// anything else that has the word "time" in it:
	AddIntStatIfFileExists ("ipv4_inet_peer_gc_maxtime", "/proc/sys/net/ipv4/inet_peer_gc_maxtime");
	AddIntStatIfFileExists ("ipv4_inet_peer_gc_mintime", "/proc/sys/net/ipv4/inet_peer_gc_mintime");
	AddIntStatIfFileExists ("ipv4_ipfrag_time",          "/proc/sys/net/ipv4/ipfrag_time");
	AddIntStatIfFileExists ("ipv4_tcp_keepalive_time",   "/proc/sys/net/ipv4/tcp_keepalive_time");

	if (m_Stats.contains("ipv4_ip_local_port_range"))
	{
		// the ip_local_port_range file has a min and max:
		auto Parts = m_Stats["ipv4_ip_local_port_range"].sValue.Split(' ');
		// TODO re-examine logic here... converting atoi and then "AddNonEmpty" stat converts itoa...
		// But there is also the NeverNegative logic
		Add ("ipv4_ip_local_port_min", NeverNegative (Parts.at(0).Int32()), StatType::INTEGER);
		Add ("ipv4_ip_local_port_max", NeverNegative (Parts.at(1).Int32()), StatType::INTEGER);
	}
#endif

	return (true);

} // GatherNetstat

//-----------------------------------------------------------------------------
bool KSystemStats::AddCalculations ()
//-----------------------------------------------------------------------------
{
	// add a few hand-picked calculations (if we have the stats that compose them) - the
	// values are read with Get(), as operator[] would add the missing ones (like the
	// load average on Windows) with empty values:

	kDebug (4, "computing a few parms ...");
	if (m_Stats.contains ("cpuinfo_num_cores") && m_Stats.contains ("load_average_1min"))
	{
		auto   nLoad    = m_Stats.Get("load_average_1min").sValue.Double();
		auto   nCores   = m_Stats.Get("cpuinfo_num_cores").sValue.Double();
		double nLPC     = (nCores > 0.0) ? (nLoad / nCores) : 0.0;

		Add ("load_per_core", nLPC, StatType::FLOAT);
	}

	if (m_Stats.contains ("meminfo_memfree_kb") && m_Stats.contains ("meminfo_memtotal_kb"))
	{
		int_t  iTotal     = m_Stats.Get("meminfo_memtotal_kb").sValue.Int64();
		int_t  iFree      = m_Stats.Get("meminfo_memfree_kb").sValue.Int64();
		int_t  iBuffers   = m_Stats.Get("meminfo_buffers_kb").sValue.Int64();
		int_t  iCached    = m_Stats.Get("meminfo_cached_kb").sValue.Int64();
		int_t  iTotalFree = iFree + iBuffers + iCached;

		if (iTotal > 0)
		{
			int_t  iUsed      = iTotal - iTotalFree;
			double nPctUsed   = ((double)iUsed * 100.0) / ((double)iTotal);
			double nPctFree   = ((double)iTotalFree * 100.0) / ((double)iTotal);

			Add ("meminfo_memused_kb",      iUsed,    StatType::INTEGER);
			Add ("meminfo_memused_percent", nPctUsed, StatType::FLOAT);
			Add ("meminfo_memfree_percent", nPctFree, StatType::FLOAT);
		}
		else
		{
			kDebug(2, "invalid value for {}: {}", "meminfo_memtotal_kb", iTotal);
		}
	}

	if (m_Stats.contains ("meminfo_swapfree_kb") && m_Stats.contains ("meminfo_swaptotal_kb"))
	{
#ifdef DEKAF2_HAS_INT128
		int_t  iTotal   = m_Stats.Get("meminfo_swaptotal_kb").sValue.Int128();
		int_t  iFree    = m_Stats.Get("meminfo_swapfree_kb").sValue.Int128();
#else
		int_t  iTotal   = m_Stats.Get("meminfo_swaptotal_kb").sValue.Int64();
		int_t  iFree    = m_Stats.Get("meminfo_swapfree_kb").sValue.Int64();
#endif
		if (iTotal > 0)
		{
			int_t  iUsed    = iTotal - iFree;
			double nPctUsed = ((double)iUsed * 100.0) / ((double)iTotal);
			double nPctFree = ((double)iFree * 100.0) / ((double)iTotal);

			Add ("meminfo_swapused_kb",      iUsed,    StatType::INTEGER);
			Add ("meminfo_swapused_percent", nPctUsed, StatType::FLOAT);
			Add ("meminfo_swapfree_percent", nPctFree, StatType::FLOAT);
		}
		else
		{
			kDebug(2, "invalid value for {}: {}", "meminfo_swaptotal_kb", iTotal);
		}
	}

	return (true);

} // AddCalculations

//-----------------------------------------------------------------------------
size_t KSystemStats::GatherProcs (KStringView sCommandRegex/*=""*/, bool bDoNoShowMyself/*=true*/)
//-----------------------------------------------------------------------------
{
	m_Procs.clear();

	KRegex kregex(sCommandRegex);
	KString sWhat;

	auto Processes = ListProcesses();

	pid_t iMyPID  = kGetPid();
	pid_t iMyPPID = kGetPpid();

#ifdef DEKAF2_IS_WINDOWS
	// Windows tells the parent only in the process list
	for (const auto& Process : Processes)
	{
		if (Process.sPID.Int32() == iMyPID)
		{
			iMyPPID = Process.sPPID.Int32();
			break;
		}
	}
#endif

	for (const auto& Process : Processes)
	{
		// convert PID to an integer
		pid_t iPID = Process.sPID.Int32();

		if (bDoNoShowMyself && (iPID == iMyPID))
		{
			sWhat.Format ("me:{}/{}", iMyPID, iMyPPID);
		}
		else if (sCommandRegex.empty())
		{
			m_Procs.Add(Process.sPID, ProcValueType(Process.sFullCmd, Process.sPPID, Process.sShortCmd, StatType::STRING));
			sWhat = "show all procs";
		}
		else if (kregex.Matches (Process.sShortCmd))
		{
			m_Procs.Add(Process.sPID, ProcValueType(Process.sFullCmd, Process.sPPID, Process.sShortCmd, StatType::STRING));
			sWhat = "MATCHES";
		}
		else
		{
			sWhat = "does not match";
		}

		kDebug (3, "{:<15} | {:<6} | {:<6} | {:<15} | {}", sWhat, Process.sPID, Process.sPPID, Process.sShortCmd, Process.sFullCmd);
	}

	return (m_Procs.size());

} // GatherProcs

//-----------------------------------------------------------------------------
void KSystemStats::DumpStats (KOutStream& stream, DumpFormat iFormat/*=DumpFormat::TEXT*/, KStringView sGrepString/*=""*/)
//-----------------------------------------------------------------------------
{
	kDebug (4, "...");

	KRegex kregex(sGrepString);

	switch (iFormat)
	{
		case DumpFormat::JSON:
		{
			KJSON js = KJSON::array();
			for (const auto& it : m_Stats)
			{
				if (sGrepString.empty() || kregex.Matches (it.first))
				{
					KJSON jj;
					jj["name"] = it.first;
					jj["type"] =  StatTypeToString(it.second.type);
					jj["value"] = it.second.sValue;
					js.push_back(std::move(jj));
				}
			}
			stream << js.dump(1, '\t') << "\n";
			break;
		}

		case DumpFormat::SHELL:
		{
			for (const auto& it : m_Stats)
			{
				if (sGrepString.empty() || kregex.Matches (it.first))
				{
					stream.FormatLine("{}='{}'", it.first, it.second.sValue);
				}
			}
			break;
		}

		case DumpFormat::TEXT:
		default:
		{
			for (const auto& it : m_Stats)
			{
				if (sGrepString.empty() || kregex.Matches (it.first))
				{
					stream.FormatLine("{:<7} | {:<50} = {}", StatTypeToString(it.second.type), it.first, it.second.sValue);
				}
			}
			break;
		}
	}

} // DumpStats

//-----------------------------------------------------------------------------
void KSystemStats::DumpProcs (KOutStream& stream, DumpFormat iFormat/*=DumpFormat::TEXT*/)
//-----------------------------------------------------------------------------
{
	kDebug (4, "...");

	switch (iFormat)
	{
		case DumpFormat::JSON:
		{
			KJSON js = KJSON::array();
			for (const auto& it : m_Procs)
			{
				KJSON jj;
				jj["pid"] = it.first;
				jj["ppid"] = it.second.sExtra1;
				jj["shortcmd"] = it.second.sExtra2;
				jj["fullcmd"] = it.second.sValue;
				js.push_back(std::move(jj));
			}
			stream << js.dump(1, '\t') << 'n';
			break;
		}

		case DumpFormat::SHELL:
		{
			for (const auto& it : m_Procs)
			{
				stream.FormatLine("procs[{}]=\"{}\"", it.first, it.second.sExtra2);
			}
			break;
		}

		case DumpFormat::TEXT:
		{

			stream.FormatLine("{:<6} {:<6} {}", "PID", "PPID", "FULL-COMMAND");
			for (const auto& it : m_Procs)
			{
				stream.FormatLine("{:<6} {:<6} {}", it.first, it.second.sExtra1, it.second.sValue);
			}

			break;
		}
	} // end switch

} // DumpProcs

//-----------------------------------------------------------------------------
void KSystemStats::DumpProcTree (KOutStream& stream, uint64_t iStartWithPID/*=0*/)
//-----------------------------------------------------------------------------
{
	kDebug (4, "...");

	DumpPidTree (stream, iStartWithPID, 0);

} // DumpProcTree

//-----------------------------------------------------------------------------
void KSystemStats::DumpPidTree (KOutStream& stream, uint64_t iFromPID, uint64_t iLevel)
//-----------------------------------------------------------------------------
{
	kDebug (4, "...");

	kDebug (3, "ppid={}, level={}", iFromPID, iLevel);

	// indentation:
	if (iLevel > 0)
	{
		stream.Write("  ");
		for (uint64_t jj=1; jj < iLevel; ++jj)
		{
			stream.Write ("|   ");
		}
		stream.Write("+-- ");
	}

	// show this pid:
	int idx = m_Procs[std::to_string(iFromPID)].sValue.Int32();
	if (idx < 0)
	{
		if (!iFromPID)
		{
			stream.Write("[boot]\n");
		}
		else
		{
			stream.Format("invalid pid: {}\n", iFromPID);
			return;
		}
	}
	else
	{
		KStringViewZ sCmd = m_Procs.at(idx).second.sValue;
		stream.Format ("{} {}\n", iFromPID, sCmd);
	}

	// recurse through children:
	for (const auto& proc : m_Procs)
	{
		uint64_t    iPID   = proc.first.UInt64();
		uint64_t    iPPID  = proc.second.sExtra1.UInt64();

		if (iPPID == iFromPID)
		{
#ifdef DEKAF2_WITH_KLOG
			KStringView sCmd = proc.second.sValue;
			kDebug (3, "child: ppid={}, pid={}, cmd={}", iPPID, iPID, sCmd);
#endif
			DumpPidTree (stream, iPID, iLevel+1);
		}
	}

} // DumpPidTree

//-----------------------------------------------------------------------------
uint16_t KSystemStats::PushStats (KString/*copy*/ sURL, const KMIME& iMime, KStringView sMyUniqueIP, KStringRef& sResponse)
//-----------------------------------------------------------------------------
{
	kDebug (4, "...");

	KJSON   oBody;
	KString sBody;

	if (iMime == KMIME::WWW_FORM_URLENCODED)
	{
		sBody.Format ("internal_ip:s={}", sMyUniqueIP);
	}
	else if (iMime == KMIME::JSON)
	{
		if (!sURL.EndsWith("/"))
		{
			sURL += "/";
		}
		sURL += sMyUniqueIP;
	}
	else
	{
		sResponse = kFormat ("BUG: KSystem::PushStats only accepts iMime = KMIME::JSON or KMIME::WWW_FORM_URLENCODED").ToStdString(); // <-- ToStdString() is needed for C++ < 17 (and does not hurt above)
		return 0;
	}

	if (true) // add unique tracker on the url
	{
		sURL += kFormat ("?uniq={}", kFormat ("{}|{}", time(nullptr), sMyUniqueIP).Hash());
	}

	for (const auto& it : m_Stats)
	{
		KString sValue (it.second.sValue);
		if (sValue.empty())
		{
			// blank numbers are a problem:
			switch (it.second.type)
			{
				case StatType::INTEGER: // integer
					sValue = "0";
					break;

				case StatType::FLOAT: // float
					sValue = "0.0";
					break;

				case StatType::AUTO:
				case StatType::STRING: // already empty string...
					break;
			}
		}

		if (iMime == KMIME::WWW_FORM_URLENCODED)
		{
			KString sEncValue;
			kUrlEncode(sValue.ToView(), sEncValue);
			KString sPair;
			sPair.Format ("&{}:{:.1}={}", it.first, StatTypeToString(it.second.type), sEncValue);
			sBody += sPair;
		}
		else
		{
			oBody[it.first] = sValue; // transport everything as a string
		}

	} // for each stat

	if (iMime == KMIME::JSON)
	{
		sBody = oBody.dump();
	}

	kDebug (3, "sending POST to: {}", sURL);
	KWebClient HTTP;
	sResponse = HTTP.Post(sURL, sBody, iMime).ToStdString(); // <-- ToStdString() is needed for C++ < 17 (and does not hurt above)

	if (!sResponse.empty())
	{
		kTrimRight(sResponse);
		kDebug (3, "HTTP-{}: {}", HTTP.GetStatusCode(), sResponse);
		return HTTP.GetStatusCode();
	}
	else
	{
		sResponse = "(got timeout)";
		kDebug (3, "got timeout");
		return 0;
	}
} // PushStats

//-----------------------------------------------------------------------------
KString KSystemStats::Backtrace (pid_t iPID)
//-----------------------------------------------------------------------------
{
	kDebug (4, "...");

	// A process ID can belong to a later process, which can make the chain a
	// circle - it ends at a process that is in the chain already.
	KString sChain;
	KUnorderedSet<pid_t> Seen;

#ifndef DEKAF2_IS_WINDOWS
	if (kDirExists ("/proc"))
	{
		// the /proc tables tell each process of the chain on its own
		ProcessInfo Process;

		while (iPID != 0 && Seen.insert(iPID).second && ReadProcProcess(KString::to_string(iPID), Process))
		{
			sChain += kFormat("{}{}:{}", sChain.empty() ? "" : " <- ", iPID, Process.sFullCmd);

			iPID = Process.sPPID.Int32();
		}

		return sChain;
	}
#endif

	// otherwise the parents come from the process list
	auto Processes = ListProcesses();

	KUnorderedMap<pid_t, const ProcessInfo*> ByPID;

	for (const auto& Process : Processes)
	{
		ByPID.emplace(Process.sPID.Int32(), &Process);
	}

	while (iPID != 0 && Seen.insert(iPID).second)
	{
		auto it = ByPID.find(iPID);

		if (it == ByPID.end())
		{
			break;
		}

		sChain += kFormat("{}{}:{}", sChain.empty() ? "" : " <- ", iPID, it->second->sFullCmd);

		iPID = it->second->sPPID.Int32();
	}

	return sChain;

} // Backtrace

DEKAF2_NAMESPACE_END

