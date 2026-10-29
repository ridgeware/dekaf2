/*
//
// DEKAF(tm): Lighter, Faster, Smarter (tm)
//
// Copyright (c) 2026, Ridgeware, Inc.
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
*/

#include <dekaf2/core/logging/kdatedlogfile.h>
#include <dekaf2/core/logging/klog.h>
#include <dekaf2/core/format/kformat.h>
#include <dekaf2/system/filesystem/kfilesystem.h>

DEKAF2_NAMESPACE_BEGIN

namespace {

//-----------------------------------------------------------------------------
/// returns the date of Day as YYYYMMDD
KString FormDate(chrono::sys_days Day)
//-----------------------------------------------------------------------------
{
	chrono::year_month_day Date(Day);

	return kFormat("{:04}{:02}{:02}",
	               static_cast<int>(Date.year()),
	               static_cast<unsigned>(Date.month()),
	               static_cast<unsigned>(Date.day()));

} // FormDate

#if DEKAF2_HAS_TIMEZONES
//-----------------------------------------------------------------------------
/// returns the time at which the local day starting with Midnight begins
template<class LocalDays>
KUnixTime LocalMidnight(const chrono::time_zone* Zone, LocalDays Midnight)
//-----------------------------------------------------------------------------
{
	auto Info = Zone->get_info(Midnight);

	if (Info.result == Info.nonexistent)
	{
		// the midnight falls into the gap of a time change, the day starts at its end
		return KUnixTime(chrono::system_clock::time_point(Info.first.end));
	}

	// for a midnight that exists twice, the day starts with the first one
	return KUnixTime(chrono::system_clock::time_point(chrono::sys_seconds(Midnight.time_since_epoch() - Info.first.offset)));

} // LocalMidnight
#endif

//-----------------------------------------------------------------------------
/// returns true if sName exists, uncompressed or compressed
bool ExistsInAnyForm(KStringView sName)
//-----------------------------------------------------------------------------
{
	KString sCandidate(sName);

	if (kFileExists(sCandidate))
	{
		return true;
	}

	for (auto sSuffix : { ".gz"_ksv, ".zst"_ksv, ".zstd"_ksv })
	{
		sCandidate.erase(sName.size());
		sCandidate += sSuffix;

		if (kFileExists(sCandidate))
		{
			return true;
		}
	}

	return false;

} // ExistsInAnyForm

} // end of anonymous namespace

//-----------------------------------------------------------------------------
KDatedLogFile::KDatedLogFile(KString sFilename)
//-----------------------------------------------------------------------------
: KDatedLogFile(std::move(sFilename), Options())
{
} // ctor

//-----------------------------------------------------------------------------
KDatedLogFile::KDatedLogFile(KString sFilename, Options options)
//-----------------------------------------------------------------------------
: m_Options   (std::move(options))
, m_sFilename (std::move(sFilename))
{
	KFileStat File(m_sFilename);

	if (File.Exists() && File.Size() > 0)
	{
		// the file holds records up to its last modification, as after a restart - a record of a
		// later day cuts it
		m_Day = GetDay(File.ModificationTime());
	}

	Open();

} // ctor

//-----------------------------------------------------------------------------
KDatedLogFile::Day KDatedLogFile::GetDay(KUnixTime tRecord) const
//-----------------------------------------------------------------------------
{
	Day Result;

#if DEKAF2_HAS_TIMEZONES
	if (m_Options.TimeZone != nullptr)
	{
		auto LocalDay  = chrono::floor<chrono::days>(m_Options.TimeZone->to_local(tRecord.to_sys_time()));

		Result.tStart  = LocalMidnight(m_Options.TimeZone, LocalDay);
		Result.tEnd    = LocalMidnight(m_Options.TimeZone, LocalDay + chrono::days(1));
		Result.sDate   = FormDate(chrono::sys_days(LocalDay.time_since_epoch()));

		return Result;
	}
#endif

	auto UTCDay    = chrono::floor<chrono::days>(tRecord.to_sys_time());

	Result.tStart  = KUnixTime(chrono::system_clock::time_point(UTCDay));
	Result.tEnd    = KUnixTime(chrono::system_clock::time_point(UTCDay + chrono::days(1)));
	Result.sDate   = FormDate(UTCDay);

	return Result;

} // GetDay

//-----------------------------------------------------------------------------
bool KDatedLogFile::Open()
//-----------------------------------------------------------------------------
{
	bool bExisted = kFileExists(m_sFilename);

	m_File.open(m_sFilename, std::ios_base::app);

	if (!m_File.is_open())
	{
		kDebug(1, "cannot open log file: {}", m_sFilename);
		m_iSize = 0;
		return false;
	}

	if (!bExisted && m_Options.iMode != 0)
	{
		kChangeMode(m_sFilename, m_Options.iMode);
	}

	m_iSize = bExisted ? kFileSize(m_sFilename) : 0;

	if (m_iSize == npos)
	{
		m_iSize = 0;
	}

	return true;

} // Open

//-----------------------------------------------------------------------------
KString KDatedLogFile::NextFreeName(KStringView sDate) const
//-----------------------------------------------------------------------------
{
	auto sBase = kFormat("{}-{}", m_sFilename, sDate);
	auto sName = sBase;

	for (std::size_t iPart = 1; ExistsInAnyForm(sName); ++iPart)
	{
		sName = kFormat("{}.{}", sBase, iPart);
	}

	return sName;

} // NextFreeName

//-----------------------------------------------------------------------------
void KDatedLogFile::Cut(Day NewDay, bool bNewDay)
//-----------------------------------------------------------------------------
{
	m_File.close();

	auto sTarget = NextFreeName(m_Day.sDate);

	if (kRename(m_sFilename, sTarget))
	{
		if (bNewDay)
		{
			// late records of the day that just ended go into this file
			m_Previous.close();
			m_sPrevious   = std::move(sTarget);
			m_PreviousDay = m_Day;
		}
	}
	else
	{
		kDebug(1, "cannot rename {} to {}, continuing in the same file", m_sFilename, sTarget);
	}

	m_Day = std::move(NewDay);

	Open();

} // Cut

//-----------------------------------------------------------------------------
KOutFile& KDatedLogFile::Select(KUnixTime tRecord, std::size_t iSize)
//-----------------------------------------------------------------------------
{
	if (tRecord > m_tNewest)
	{
		m_tNewest = tRecord;
	}

	if (m_Previous.is_open() && m_tNewest - m_Day.tStart >= m_Options.Grace)
	{
		// the grace period for late records of the previous day has passed
		m_Previous.close();
	}

	if (m_Day.sDate.empty())
	{
		// the first record of a new or empty file
		m_Day = GetDay(tRecord);
	}
	else if (tRecord >= m_Day.tEnd)
	{
		// a record of a later day
		Cut(GetDay(tRecord), true);
	}
	else if (tRecord < m_Day.tStart)
	{
		// a record of an earlier day, which arrived after the cut
		if (!m_sPrevious.empty()
		    && tRecord >= m_PreviousDay.tStart
		    && m_tNewest - m_Day.tStart < m_Options.Grace)
		{
			if (!m_Previous.is_open())
			{
				m_Previous.open(m_sPrevious, std::ios_base::app);
			}

			if (m_Previous.is_open())
			{
				return m_Previous;
			}
		}

		// too late, or the file of its day is gone: the record goes into the current file
		return m_File;
	}

	if (m_Options.iMaxSize != npos && m_iSize > 0 && m_iSize + iSize > m_Options.iMaxSize)
	{
		// the file would grow too large, continue the day in a new file
		Cut(m_Day, false);
	}

	return m_File;

} // Select

//-----------------------------------------------------------------------------
bool KDatedLogFile::Write(KStringView sRecord, KUnixTime tRecord)
//-----------------------------------------------------------------------------
{
	auto& File = Select(tRecord, sRecord.size());

	File.Write(sRecord);

	if (&File == &m_File)
	{
		m_iSize += sRecord.size();
	}

	return File.Good();

} // Write

//-----------------------------------------------------------------------------
bool KDatedLogFile::WriteLine(KStringView sLine, KUnixTime tRecord)
//-----------------------------------------------------------------------------
{
	auto& File = Select(tRecord, sLine.size() + 1);

	File.WriteLine(sLine);

	if (&File == &m_File)
	{
		m_iSize += sLine.size() + 1;
	}

	return File.Good();

} // WriteLine

//-----------------------------------------------------------------------------
bool KDatedLogFile::Flush()
//-----------------------------------------------------------------------------
{
	if (m_Previous.is_open())
	{
		m_Previous.Flush();
	}

	m_File.Flush();

	return Good();

} // Flush

//-----------------------------------------------------------------------------
bool KDatedLogFile::Good() const
//-----------------------------------------------------------------------------
{
	return m_File.is_open() && m_File.Good();

} // Good

DEKAF2_NAMESPACE_END
