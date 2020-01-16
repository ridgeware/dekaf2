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

#include <dekaf2/ksystemstats.h>
#include <dekaf2/kstack.h>
#include <dekaf2/kstringutils.h>
#include <dekaf2/kinshell.h>
#include <dekaf2/kregex.h>
#include <dekaf2/kurlencode.h>
#include <dekaf2/kwebclient.h>
#include <dekaf2/kstream.h>
#include <dekaf2/kjson.h>
#include <dekaf2/kfilesystem.h>

namespace dekaf2 {

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

//-----------------------------------------------------------------------------
bool KSystemStats::GatherAll ()
//-----------------------------------------------------------------------------
{
	bool bOK = true;
	if (!GatherProcInfo())          bOK = false;
	if (!GatherMiscInfo())          bOK = false;
	if (!GatherVmStatInfo())        bOK = false;
	if (!GatherDiskStats())         bOK = false;
	if (!GatherCpuInfo())           bOK = false;
	if (!GatherMemInfo())           bOK = false;
	if (!GatherNetstat())           bOK = false;
	if (!AddCalculations())         bOK = false;
	return (bOK);

} // GatherAll

//-----------------------------------------------------------------------------
KSystemStats::StatType KSystemStats::StatValueType::SenseType(KStringView sValue)
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
		size_t iRead = oStatFile.Read(sContents, 256);
		sContents.CollapseAndTrim();

		kDebug (3, "Read {} bytes from stat file '{}' : '{}'", iRead, sStatFilePath, sContents);
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
	KString sVersion;
	KString sLoadAvg;
	KString sUptime;

	{
		kDebug (2, "reading {} ...", PROC_VERSION);
		KInFile oProcFile(PROC_VERSION);
		oProcFile.ReadAll(sVersion);
		kDebug(3, "contents of {}: {}", PROC_VERSION, sVersion);
	}
	{
		kDebug (2, "reading {} ...", PROC_LOADAVG);
		KInFile oLoadFile(PROC_LOADAVG);
		oLoadFile.ReadAll(sLoadAvg);
	}
	{
		kDebug (2, "reading {} ...", PROC_UPTIME);
		KInFile oUptimeFile(PROC_UPTIME);
		oUptimeFile.ReadAll(sUptime);
	}

	if (!sLoadAvg.empty())
	{
		sVersion.TrimRight();
		kDebug (2, "LOADAVG: {}", sLoadAvg);
		sLoadAvg.Replace('/', ' ',true);
		auto Parts = sLoadAvg.Split(" ");

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
		kDebug (2, "UPTIME: {}", sUptime);
		auto Parts = sUptime.Split(" ");
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

	kDebug (2, "sizing {} ...", KLog::getInstance().GetDebugLog());
	Add ("bytes_klog",             NeverNegative (kFileSize (KLog::getInstance().GetDebugLog())),StatType::INTEGER);
	kDebug (2, "sizing a bunch of files in /var/log ...");
	Add ("bytes_var_log_cron",     NeverNegative (kFileSize ("/var/log/cron")),     StatType::INTEGER);
	Add ("bytes_var_log_dmesg",    NeverNegative (kFileSize ("/var/log/dmesg")),    StatType::INTEGER);
	Add ("bytes_var_log_faillog",  NeverNegative (kFileSize ("/var/log/faillog")),  StatType::INTEGER);
	Add ("bytes_var_log_lastlog",  NeverNegative (kFileSize ("/var/log/lastlog")),  StatType::INTEGER);
	Add ("bytes_var_log_maillog",  NeverNegative (kFileSize ("/var/log/maillog")),  StatType::INTEGER);
	Add ("bytes_var_log_messages", NeverNegative (kFileSize ("/var/log/messages")), StatType::INTEGER);
	Add ("bytes_var_log_rpmpkgs",  NeverNegative (kFileSize ("/var/log/rpmpkgs")),  StatType::INTEGER);
	Add ("bytes_var_log_secure",   NeverNegative (kFileSize ("/var/log/secure")),   StatType::INTEGER);
	Add ("bytes_var_log_wtmp",     NeverNegative (kFileSize ("/var/log/wtmp")),     StatType::INTEGER);

/*
// might be too heavy - lets think about this some more...
	UCHAR8 Md5Passwd[16];
	UCHAR8 Md5Shadow[16];
	UCHAR8 Md5Group[16];
	char   szHexDigest[32+1];

	kDebugLog ("KSystemStats: computing hashes on a bunch of files in /etc ...");
	kmd5_file ("/etc/passwd", Md5Passwd);
	kmd5_file ("/etc/shadow", Md5Shadow);
	kmd5_file ("/etc/group",  Md5Group);

	Add ("md5_passwd", kmd5_tohex (Md5Passwd, szHexDigest), STRING);
	Add ("md5_shadow", kmd5_tohex (Md5Shadow, szHexDigest), STRING);
	Add ("md5_group",  kmd5_tohex (Md5Group,  szHexDigest), STRING);
*/
	return (true);

} // GatherProcInfo

//-----------------------------------------------------------------------------
bool KSystemStats::GatherMiscInfo ()
//-----------------------------------------------------------------------------
{

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

		auto Parts = sLine.Split(" ");
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

	// physical hostname and logical hostname:
	kDebug (3, "reading {} ...", PROC_HOSTNAME);

	file.open (PROC_HOSTNAME);

	while (file.ReadLine(sLine))
	{
		if (!sLine.empty()) 
		{
			Add ("hostname", sLine, StatType::STRING);
		}
	}

	file.close();

	kDebug (3, "reading {} ...", "/etc/khostname");

	file.open("/etc/khostname");

	if (file.ReadLine(sLine) && !sLine.empty()) 
	{
		Add ("khostname", sLine, StatType::STRING);
	}

	return (true);

} // GatherMiscInfo

//-----------------------------------------------------------------------------
bool KSystemStats::GatherVmStatInfo ()
//-----------------------------------------------------------------------------
{

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

	kDebug (2, "reading {} ...", PROC_VMSTAT);

	KInFile file(PROC_VMSTAT);
	file.SetReaderRightTrim("\r\n\t ");

	KString sLine;
	while (file.ReadLine(sLine))
	{
		if (sLine.empty())
		{
			continue;
		}

		kDebug (2, "{}: raw: {}", PROC_VMSTAT, sLine);

		auto Parts = sLine.Split(" ");

		if (Parts.size() != 2)
		{
			kDebug (2, "Got unexpected line from {}", PROC_VMSTAT);
			continue;
		}

		KString sName = "vmstat_";
		sName += Parts.at(0);

		kDebug (2, "{}:  ok: {}={}", PROC_VMSTAT, sName, Parts.at(1));
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

	kDebug (2, "reading {} ...", PROC_DISKSTATS);

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

		auto Parts = sLine.Split(" ");
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
bool KSystemStats::GatherCpuInfo ()
//-----------------------------------------------------------------------------
{

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
	kDebug (2, "reading {} ...", PROC_CPUINFO);

	int_t m_iNumCores { 0 };

	KInFile file(PROC_CPUINFO);
	file.SetReaderRightTrim("\r\n\t ");

	KString sLine;
	while (file.ReadLine(sLine))
	{
		if (sLine.empty())
		{
			continue;
		}

		kDebug (2, "{}: raw: {}", PROC_CPUINFO, sLine);

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
			kDebug (2, "ignoring newer cpuinfo field: {}", sName);
			continue;
		}

		if (sName == "processor")
		{
			m_iNumCores++;
		}
		else if (1 == m_iNumCores) // we only need to store CPU info for one core since every process is identical
		{
			sName.insert(0, "cpuinfo_");
			kDebug (2, "{}:  ok: {}={}", PROC_CPUINFO, sName, sValue);
			Add (sName, sValue);
		}
	}

	Add (CPUINFO_NUM_CORES, m_iNumCores, StatType::INTEGER);
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

		auto Parts = sLine.Split(" ");

		if (Parts.at(0) == "cpu")
		{
			kDebug (2, "{}: {} parts: {}", PROC_STAT, Parts.size(), sLine);

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
			time_t tBoot = Parts.at(1).Int64();
			time_t tNow  = time(nullptr);
			time_t tAgo  = tNow - tBoot;

			Add("boot_time_unix", static_cast<int_t>(tBoot), StatType::INTEGER);
			Add("boot_time_dtm",  kFormTimestamp(tBoot),     StatType::STRING);
			Add("boot_time_ago",  static_cast<int_t>(tAgo),  StatType::INTEGER);
		}
	}

	return (true);

} // GatherCpuInfo

//-----------------------------------------------------------------------------
bool KSystemStats::GatherMemInfo ()
//-----------------------------------------------------------------------------
{

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

	kDebug (2, "reading {} ...", PROC_MEMINFO);

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

} // GatherMemInfo

//-----------------------------------------------------------------------------
bool KSystemStats::GatherNetstat ()
//-----------------------------------------------------------------------------
{
	kDebug (3, "running netstat ...");

	KInShell pipe;
	pipe.SetReaderRightTrim("\r\n\t ");

	if (!pipe.Open ("netstat -n")) // <-- we need to add --numeric to netstat so that it runs faster and does not try to resolve hostnames, etc.
	{
		m_sLastError.Format ("command failed: netstat");
		return (false);
	}

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

		auto Parts = sLine.Split(" ");

		KString sName("netstat_");

		if (kStrIn (Parts.at(0), "tcp,tcp6"))
		{
			sName += "tcp_";
			sName += Parts.at (Parts.size() - 1).ToLower();
			Netstat[sName]++;
		}
		else if (Parts.at(0) == "unix")
		{
			sName += "unix_";
			sName += Parts.at (5 - 1).ToLower();
			Netstat[sName]++;
		}
	}

	for (const auto& stat : Netstat)
	{
		Add(stat.first, stat.second, StatType::INTEGER);
	}

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

	if (m_Stats.Contains("ipv4_ip_local_port_range"))
	{
		// the ip_local_port_range file has a min and max:
		auto Parts = m_Stats["ipv4_ip_local_port_range"].sValue.Split(" ");
		// TODO re-examine logic here... converting atoi and then "AddNonEmpty" stat converts itoa...
		// But there is also the NeverNegative logic
		Add ("ipv4_ip_local_port_min", NeverNegative (Parts.at(0).Int32()), StatType::INTEGER);
		Add ("ipv4_ip_local_port_max", NeverNegative (Parts.at(1).Int32()), StatType::INTEGER);
	}

	return (true);

} // GatherNetstat

//-----------------------------------------------------------------------------
bool KSystemStats::AddCalculations ()
//-----------------------------------------------------------------------------
{
	// add a few hand-picked calculations (if we have the stats that compose them):

	kDebug (3, "computing a few parms ...");

	if (m_Stats.Contains ("meminfo_memfree_kb") && m_Stats.Contains ("meminfo_memtotal_kb"))
	{
		int_t  iTotal     = m_Stats["meminfo_memtotal_kb"].sValue.Int64();
		int_t  iFree      = m_Stats["meminfo_memfree_kb"].sValue.Int64();
		int_t  iBuffers   = m_Stats["meminfo_buffers_kb"].sValue.Int64();
		int_t  iCached    = m_Stats["meminfo_cached_kb"].sValue.Int64();
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

	if (m_Stats.Contains ("meminfo_swapfree_kb") && m_Stats.Contains ("meminfo_swaptotal_kb"))
	{
#ifdef DEKAF2_HAS_INT128
		int_t  iTotal   = m_Stats["meminfo_swaptotal_kb"].sValue.Int128();
		int_t  iFree    = m_Stats["meminfo_swapfree_kb"].sValue.Int128();
#else
		int_t  iTotal   = m_Stats["meminfo_swaptotal_kb"].sValue.Int64();
		int_t  iFree    = m_Stats["meminfo_swapfree_kb"].sValue.Int64();
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
	KInShell pipe;
	KString sWhat;

	KStringView sPID;
	KStringView sPPID;
	KStringView sShortCmd;
	KStringView sFullCmd;
	KString sLine;

	kDebug (3, "running ps ...");

	if (pipe.Open ("ps -e -o pid,ppid,comm,command 2>&1"))
	{
		enum { MAX = 5000 };
		enum { TIMEOUT_SEC = 10 };

		sLine.clear();

		pid_t iMyPID  = getpid();
#ifdef DEKAF2_IS_WINDOWS
		pid_t iMyPPID = 0;
#else
		pid_t iMyPPID = getppid();
#endif
		while (pipe.ReadLine (sLine))
		{
			//   PID  PPID COMMAND         COMMAND
			//   324   151 cqueue/1        [cqueue/1]
			//  2742     1 automount       automount --pid-file /var/run/autofs.pid
			// 22463 13273 httpd           /usr/local/packages/onelink/apache.redhat64/bin/http

			auto Parts = sLine.Split(" ", " \r\n\t\b", '\0', false, false);

			// Did we get the correct number of parts?
			if (4 != Parts.size())
			{
				kDebug (2, "ERROR: Unexpected number of parsed results. Expected four (4) elements got '{}' element(s)", Parts.size());

				if (Parts.size() > 4)
				{
					// 4th part has spaces.
					kDebug ( 3, "Parsed Line = '{}'", sLine);
					// Here I need the 4th part merged with all the rest after
					KStringView sTemp = Parts.at(3);
					Parts.at(3) = sLine;
					Parts.at(3).ClipAtReverse(sTemp);
				}
				else
				{
					// move on to the next line of input
					kDebug ( 3, "SKIPPED Parsed Line = '{}'", sLine)
					continue;
				}
			}

			sPID = Parts.at(0);
			sPPID = Parts.at(1);
			sShortCmd = Parts.at(2);
			sFullCmd = Parts.at(3);

			// convert PID to an integer
			pid_t iPID = sPID.Int32();

			if (sShortCmd.compare("COMMAND") == 0)
			{
				sWhat = "PARSED AS";
			}
			else if (bDoNoShowMyself && (iPID == iMyPID))
			{
				sWhat.Format ("me:{}/{}", iMyPID, iMyPPID);
			}
			else if (sCommandRegex.empty())
			{
				m_Procs.Add(sPID, StatValueType(sFullCmd, sPPID, sShortCmd, StatType::STRING));
				sWhat = "show all procs";
			}
			else if (kregex.Matches (sShortCmd))
			{
				m_Procs.Add(sPID, StatValueType(sFullCmd, sPPID, sShortCmd, StatType::STRING));
				sWhat = "MATCHES";
			}
			else
			{
				sWhat = "does not match";
			}

			kDebug (2, "{:<15} | {:<6} | {:<6} | {:<15} | {}", sWhat, sPID, sPPID, sShortCmd, sFullCmd);
		}
	}

	return (m_Procs.size());

} // GatherProcs

//-----------------------------------------------------------------------------
void KSystemStats::DumpStats (KOutStream& stream, DumpFormat iFormat/*=DumpFormat::TEXT*/, KStringView sGrepString/*=""*/)
//-----------------------------------------------------------------------------
{
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
	DumpPidTree (stream, iStartWithPID, 0);

} // DumpProcTree

//-----------------------------------------------------------------------------
void KSystemStats::DumpPidTree (KOutStream& stream, uint64_t iFromPID, uint64_t iLevel)
//-----------------------------------------------------------------------------
{
	kDebug (3, "ppid={}, level={}", iFromPID, iLevel);

	// indentation:
	if (iLevel > 0)
	{
		stream.Printf("  ");
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
			stream.Printf("invalid pid: %lu\n", iFromPID);
			return;
		}
	}
	else
	{
		KStringViewZ sCmd = m_Procs.at(idx).second.sValue;
		stream.Printf ("%lu %s\n", iFromPID, sCmd);
	}

	// recurse through children:
	for (const auto& proc : m_Procs)
	{
		uint64_t    iPID   = proc.first.UInt64();
		KStringView sCmd = proc.second.sValue;
		uint64_t    iPPID  = proc.second.sExtra1.UInt64();

		if (iPPID == iFromPID)
		{
			kDebug (2, "child: ppid={}, pid={}, cmd={}", iPPID, iPID, sCmd);
			DumpPidTree (stream, iPID, iLevel+1);
		}
	}

} // DumpPidTree

//-----------------------------------------------------------------------------
uint16_t KSystemStats::PushStats (KStringView sURL, KStringView sMyUniqueIP, KString& sResponse)
//-----------------------------------------------------------------------------
{
	KString sPostData;
	sPostData.Format ("internal_ip:s={}", sMyUniqueIP);

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

		KString sEncValue;
		kUrlEncode(sValue.ToView(), sEncValue);
		KString sPair;
		sPair.Format ("&{}:{:.1}={}", it.first, StatTypeToString(it.second.type), sEncValue);
		sPostData += sPair;
	}

	kDebug (2, "sending POST to: {}", sURL);
	KWebClient HTTP;
	sResponse = HTTP.Post(sURL, sPostData, KMIME::WWW_FORM_URLENCODED);

	if (!sResponse.empty())
	{
		kDebug (2, "HTTP-{}: {}", HTTP.GetStatusCode(), sResponse.TrimRight());
		return HTTP.GetStatusCode();
	}
	else
	{
		sResponse = "(got timeout)";
		kDebug (2, "got timeout");
		return 0;
	}
} // PushStats

//-----------------------------------------------------------------------------
KString KSystemStats::Backtrace (pid_t iPID)
//-----------------------------------------------------------------------------
{
	KString sChain;

	do
	{
		KString sPath;
		sPath.Format ("/proc/{}/cmdline", iPID);

		KString sCMD;
		kReadFile (sPath, sCMD, true);

		KString sAdd;
		sAdd.Format ("{}{}:{}", sChain.empty() ? "" : " <- ", iPID, sCMD);
		sChain += sAdd;

		// recurse to parent pid:

		// % cat /proc/3002/stat
		//                v
		// 3002 (crond) S 1 3002 3002 0 -1 4202816 92890 ...
		//                ^
		//                +---- ppid is the 4th word

		pid_t iPPID = 0;
		KString sLine;
		sPath.Format ("/proc/{}/stat", iPID);
		if (kReadFile (sPath, sLine, true))
		{
			auto Words = sLine.Split(" ");
			iPPID = Words.at(4-1).UInt32();
			if (!iPPID) 
			{
				break; // do..while
			}
		}

		iPID = iPPID;
	}
	while (iPID != 0);

	return (sChain);

} // Backtrace

} // end namespace dekaf2

