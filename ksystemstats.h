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

/// @file ksystemstats.h
/// Collects information about system parameters in domains like network,
/// memory, cpu

#include <dekaf2/kstring.h>
#include <dekaf2/kprops.h>
#include <dekaf2/kmime.h>

namespace dekaf2 {

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC KSystemStats
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	enum class StatType
	{
		AUTO,
		STRING,
		INTEGER,
		FLOAT
	};

	using int_t = int64_t;

	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	class StatValueType
	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{

	//----------
	public:
	//----------

		KString sValue;
		KString sExtra1;
		KString sExtra2;
		StatType type { StatType::AUTO };

		StatValueType() = default;

		StatValueType(KString _sValue, StatType iStatType = StatType::AUTO)
		: sValue(std::move(_sValue))
		, type(iStatType)
		{
			CheckType();
		}

		StatValueType(KString _sValue, KString _sExtra1, KString _sExtra2 = KString{}, StatType iStatType = StatType::AUTO)
		: sValue(std::move(_sValue))
		, sExtra1(std::move(_sExtra1))
		, sExtra2(std::move(_sExtra2))
		, type(iStatType)
		{
			CheckType();
		}

		static StatType SenseType(KStringView sValue);

	//----------
	private:
	//----------

		DEKAF2_PRIVATE void CheckType()
		{
			if (type == StatType::AUTO)
			{
				type = SenseType(sValue);
			}
		}

	};

	using Stats = KProps <KString, StatValueType, /*order-matters=*/true, /*unique-keys*/true>; // KProps type

	enum class DumpFormat
	{
		TEXT,
		JSON,
		SHELL
	};

	KStringView StatTypeToString(StatType statType);

	bool GatherProcInfo ();
	bool GatherMiscInfo ();
	bool GatherVmStatInfo ();
	bool GatherDiskStats();
	bool GatherCpuInfo();
	bool GatherMemInfo ();
	bool GatherNetstat ();
	bool AddCalculations ();
	bool GatherAll ();

	void      DumpStats    (KOutStream& fp, DumpFormat iFormat=DumpFormat::TEXT, KStringView sGrepString="");
	Stats&    GetStats     () { return m_Stats; }

	/// push stats as KMIME::WWW_URL_ENCODED or KMIME::JSON
	uint16_t  PushStats    (KString/*copy*/ sURL, const KMIME& iMime, KStringView sMyUniqueIP, KString& sResponse);

	size_t    GatherProcs  (KStringView sCommandRegex="", bool bDoNoShowMyself=true);
	void      DumpProcs    (KOutStream& stream, DumpFormat DumpFormat=DumpFormat::TEXT);
	void      DumpProcTree (KOutStream& stream, uint64_t iStartWithPID=0);
	Stats&    GetProcs     () { return m_Procs; }

	static KString Backtrace (pid_t iPID);

	KStringView GetLastError () { return (m_sLastError.c_str()); }

//----------
protected:
//----------

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
	static KStringViewZ CPUINFO_NUM_CORES;

	bool Add (KStringView sStatName, KStringView  sStatValue, StatType iStatType = StatType::AUTO);
	bool Add (KStringView sStatName, int_t        iStatValue, StatType iStatType = StatType::AUTO);
	bool Add (KStringView sStatName, double       dStatValue, StatType iStatType = StatType::AUTO);

//----------
private:
//----------

	Stats     m_Stats;
	Stats     m_Procs;
	KString   m_sLastError;

	DEKAF2_PRIVATE
	void AddDiskStat (KStringView sValue, KStringView sDevice, KStringView sStat);
	DEKAF2_PRIVATE
	void DumpPidTree (KOutStream& stream, uint64_t iPPID, uint64_t iLevel);
	DEKAF2_PRIVATE
	void AddIntStatIfFileExists (KStringViewZ sStatName, KStringViewZ  sStatFilePath);

}; // KSystemStats

} // end namespace dekaf2
