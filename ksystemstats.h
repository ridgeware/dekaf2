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

#pragma once

#include <dekaf2/kstring.h>
#include <dekaf2/kprops.h>

namespace dekaf2 {

/////////////////////////////////////////////////////////////////////////////
class KSystemStats
/////////////////////////////////////////////////////////////////////////////
{
//----------
public:
//----------
	typedef enum  {
		STRING = 1,
		INTEGER = 2,
		FLOAT = 3
	} STAT_TYPE;

	typedef int64_t int_t;

	struct StatValueType {
		KString sValue;
		KString sExtra1;
		KString sExtra2;
		STAT_TYPE type = STRING;

		StatValueType(){}
		~StatValueType(){}
		StatValueType(KString value)
		{
			this->sValue = value;
			this->type = STRING;
		}
		StatValueType(KString value, STAT_TYPE iStatType)
		{
			this->sValue = value;
			this->type = iStatType;
		}
		StatValueType(KString sValue, KString sExtra1)
		{
			this->sValue = sValue;
			this->sExtra1 = sExtra1;
			this->type = STRING;
		}
		StatValueType(KString sValue, KString sExtra1, STAT_TYPE iStatType)
		{
			this->sValue = sValue;
			this->sExtra1 = sExtra1;
			this->type = iStatType;
		}
		StatValueType(KString sValue, KString sExtra1, KString sExtra2)
		{
			this->sValue = sValue;
			this->sExtra1 = sExtra1;
			this->sExtra2 = sExtra2;
			this->type = STRING;
		}
		StatValueType(KString sValue, KString sExtra1, KString sExtra2, STAT_TYPE iStatType)
		{
			this->sValue = sValue;
			this->sExtra1 = sExtra1;
			this->sExtra2 = sExtra2;
			this->type = iStatType;
		}
	};
	typedef struct StatValueType StatValueType;

	typedef KProps <KString, StatValueType, /*order-matters=*/true, /*unique-keys*/true> StatType; // KProps type

	const KStringView CPUINFO_NUM_CORES = "cpuinfo_num_cores";

	enum DumpFormat{
		DUMP_TEXT   = 'T',
		DUMP_JSON   = 'J',
		DUMP_SHELL  = 'S'
	};

	KStringView StatTypeToString(STAT_TYPE statType);

	KSystemStats();

	bool GatherProcInfo ();
	bool GatherMiscInfo ();
	bool GatherVmStatInfo ();
	bool GatherDiskStats();
	bool GatherCpuInfo();
	bool GatherMemInfo ();
	bool GatherNetstat ();
	bool AddCalculations ();
	bool GatherAll ();

	void      DumpStats    (KOutStream& fp, DumpFormat iFormat=DUMP_TEXT, KStringView sGrepString="");
	StatType& GetStats     ()   { return m_Stats; }
	uint16_t  PushStats    (KStringView sURL, KStringView sMyUniqueIP, KString& sResponse);

	uint64_t  GatherProcs  (KStringView sCommandRegex="", bool bDoNoShowMyself=true);
	void      DumpProcs    (KOutStream& stream, DumpFormat DumpFormat=DUMP_TEXT);
	void      DumpProcTree (KOutStream& stream, uint64_t iStartWithPID=0);
	StatType& GetProcs     ()   { return m_Procs; }

	static KString Backtrace    (pid_t iPID);

	KStringView GetLastError () { return (m_sLastError.c_str()); }

	// we declare these as variables so that unit tests can override them:
	static KStringViewZ PROC_VERSION;
	static KStringViewZ PROC_LOADAVG;
	static KStringViewZ PROC_UPTIME;
	static KStringViewZ PROC_MISC;
	static KStringViewZ PROC_HOSTNAME;
	static KStringViewZ PROC_VMSTAT;
	static KStringViewZ PROC_DISKSTATS;
	static KStringViewZ PROC_DISKUSAGEINFO;
	static KStringViewZ PROC_CPUINFO;
	static KStringViewZ PROC_STAT;
	static KStringViewZ PROC_MEMINFO;

	bool Add        (KStringView sStatName, KStringView  sStatValue, STAT_TYPE iStatType);
	bool Add        (KStringView sStatName, int64_t     iStatValue, STAT_TYPE iStatType);
	bool Add        (KStringView sStatName, double       dStatValue, STAT_TYPE iStatType);

//----------
private:
//----------
	StatType  m_Stats;
	StatType  m_Procs;
	KString   m_sLastError;

	void AddDiskStat            (KStringView sValue, KStringView sDevice, KStringView sStat);
	void DumpPidTree            (KOutStream& stream, uint64_t iPPID, uint64_t iLevel);
	void AddIntStatIfFileExists (KStringViewZ sStatName, KStringViewZ  sStatFilePath);

	// For gathering CPU info
	int m_iNumCores = 0;

}; // KSystemStats

} // end namespace dekaf2
