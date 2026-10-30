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

#pragma once

/// @file kdatedlogfile.h
/// a log file that starts a new file with every calendar day, week or month

#include <dekaf2/core/init/kdefinitions.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/strings/kstringview.h>
#include <dekaf2/io/readwrite/kwriter.h>
#include <dekaf2/time/clock/ktime.h>
#include <dekaf2/time/duration/kduration.h>
#include <vector>

DEKAF2_NAMESPACE_BEGIN

/// @addtogroup core_logging
/// @{

namespace detail {

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// one calendar period of a dated log file
struct KDatedLogSpan
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	KUnixTime tStart;
	KUnixTime tEnd;
	/// the date in the name, as YYYYMMDD, YYYYWww or YYYYMM, empty if the period is not known yet
	KString   sDate;

}; // KDatedLogSpan

} // end of namespace detail

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// A log file that holds the records of one calendar period: a day, an ISO week from Monday to
/// Sunday, or a month. Each record is written together with its time stamp, and the first record
/// of a later period closes the file, renames it to the name of its period, like app.log-20261029
/// for a day, app.log-2026W44 for a week or app.log-202610 for a month, and starts a new file
/// under the original name. All records stamped within one period are in the files of that
/// period, and only there. The periods start at midnight UTC, or at midnight of a configured time
/// zone.
///
/// A week belongs to the year of its Thursday, as defined by ISO 8601: the week from 2026-12-28 to
/// 2027-01-03 is named app.log-2026W53, and the week from 2025-12-29 to 2026-01-04 app.log-2026W01.
///
/// Records of the previous period that arrive after the cut, like a record that waited in a
/// queue, are appended to the file of the previous period, as long as the newest record time seen
/// is less than Options::Grace after the boundary. An even older record goes into the current
/// file.
///
/// A file that would grow beyond Options::iMaxSize is cut within the period as well. The parts of
/// a period are named app.log-20261029, app.log-20261029.1, app.log-20261029.2 in the order of
/// their content.
///
/// The class does not compress the files of past periods, KLogRotate does that. With
/// Options::iRotations, every cut removes the oldest past files beyond that number, also
/// compressed ones; without it, KLogRotate can remove them by age.
///
/// See @ref dated_logs "Dated Log Files" for the writers that use the class and for the
/// housekeeping with KLogRotate.
///
/// The class is not synchronized: the callers serialize their writes, as the writer thread of
/// kLogger() and KLog do.
class DEKAF2_PUBLIC KDatedLogFile
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	/// the calendar period that one file holds
	enum Interval
	{
		Day,   ///< a day, named like app.log-20261029
		Week,  ///< an ISO week from Monday to Sunday, named like app.log-2026W44
		Month  ///< a month, named like app.log-202610
	};

	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	struct Options
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		/// the time zone whose midnight starts the periods, nullptr for UTC - a time zone is only
		/// supported where DEKAF2_HAS_TIMEZONES is set
		const chrono::time_zone* TimeZone   { nullptr };
		/// the calendar period that one file holds
		Interval                 Period     { Day };
		/// the size beyond which a file is cut within the period, npos for no limit
		std::size_t              iMaxSize   { npos };
		/// the number of past files to keep, npos to keep all - every cut removes the oldest past
		/// files beyond it
		std::size_t              iRotations { npos };
		/// the time after the period boundary during which records of the previous period still go
		/// into the file of the previous period, measured in record time
		KDuration                Grace      { chrono::seconds(60) };
		/// the mode for newly created files, 0 to leave it to the umask
		int                      iMode      { 0 };

	}; // Options

	/// opens or creates the log file sFilename, with a file per day in UTC, no size limit, all past
	/// files kept and a grace period of 60 seconds - an existing file continues to be written, until
	/// a record of a later day than the day of its last modification arrives
	/// @param sFilename the path of the log file
	KDatedLogFile(KString sFilename);

	/// opens or creates the log file sFilename - an existing file continues to be written, until a
	/// record of a later period than the period of its last modification arrives
	/// @param sFilename the path of the log file
	/// @param options the options for the period and its time zone, the maximum size, the number of
	/// past files and the grace period
	KDatedLogFile(KString sFilename, Options options);

	KDatedLogFile(const KDatedLogFile&) = delete;
	KDatedLogFile& operator=(const KDatedLogFile&) = delete;

	/// writes sRecord as it is, after cutting the file if the record belongs to a later period or
	/// the file would grow too large
	/// @param sRecord the record to write
	/// @param tRecord the time stamp of the record
	/// @return true if the record was written
	bool           Write       (KStringView sRecord, KUnixTime tRecord);

	/// writes sLine and a line feed, after cutting the file if the line belongs to a later period
	/// or the file would grow too large
	/// @param sLine the line to write
	/// @param tRecord the time stamp of the line
	/// @return true if the line was written
	bool           WriteLine   (KStringView sLine, KUnixTime tRecord);

	/// flushes the current file, and the file of the previous period if it is open
	/// @return true if the current file is in a good state
	bool           Flush       ();

	/// @return true if the current file is open and in a good state
	DEKAF2_NODISCARD
	bool           Good        () const;

	/// @return the path of the current file
	DEKAF2_NODISCARD
	const KString& GetFilename () const { return m_sFilename; }

//------
private:
//------

	/// one calendar period in the configured time zone
	using Span = detail::KDatedLogSpan;

	DEKAF2_PRIVATE
	Span      GetSpan      (KUnixTime tRecord) const;
	DEKAF2_PRIVATE
	KOutFile& Select       (KUnixTime tRecord, std::size_t iSize);
	DEKAF2_PRIVATE
	void      Cut          (Span NewSpan, bool bNewSpan);
	DEKAF2_PRIVATE
	bool      Open         ();

	Options     m_Options;
	KString     m_sFilename;
	/// the file under the original name
	KOutFile    m_File;
	std::size_t m_iSize    { 0 };
	Span        m_Span;
	/// the last file of the previous period, opened when a late record arrives
	KOutFile    m_Previous;
	KString     m_sPrevious;
	Span        m_PreviousSpan;
	/// the newest record time seen, which ends the grace period
	KUnixTime   m_tNewest;

}; // KDatedLogFile

namespace detail {

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// the periods of a dated log file and the names of its past files, for KDatedLogFile and
/// KLogRotate
class DEKAF2_PUBLIC KDatedLogNames
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// a file of a past period, like app.log-20261029.1.gz
	struct PastFile
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		KString     sPath;
		/// the date as YYYYMMDD, YYYYWww or YYYYMM
		KString     sDate;
		/// the counter of the part of the period, 0 for the first part
		std::size_t iPart       { 0 };
		/// true for a file with the suffix .gz, .zst or .zstd
		bool        bCompressed { false };

	}; // PastFile

	/// returns the period that contains tTime
	/// @param tTime the time
	/// @param Period the length of the period
	/// @param TimeZone the time zone whose midnight starts the periods, nullptr for UTC
	static KDatedLogSpan GetSpan(KUnixTime tTime, KDatedLogFile::Interval Period, const chrono::time_zone* TimeZone);

	/// returns the end of the period with the date sDate - the form of the date tells the length
	/// of the period
	/// @param sDate the date as YYYYMMDD, YYYYWww or YYYYMM
	/// @param TimeZone the time zone whose midnight starts the periods, nullptr for UTC
	/// @return the end of the period, or KUnixTime() if sDate is no date
	static KUnixTime GetEnd(KStringView sDate, const chrono::time_zone* TimeZone);

	/// returns the files of past periods of the log file sFilename, uncompressed and compressed,
	/// the newest first - the date and the counter of the part order them
	static std::vector<PastFile> GetPastFiles(KStringView sFilename);

	/// returns the name for the next file of the period with the date sDate, with the counter
	/// after the highest existing part, so that the counters keep the order of the parts also
	/// when the oldest parts were removed
	static KString GetNextName(KStringView sFilename, KStringView sDate);

	/// removes the oldest files of past periods of the log file sFilename beyond iKeep
	static void RemovePastFiles(KStringView sFilename, std::size_t iKeep);

}; // KDatedLogNames

} // end of namespace detail

/// @}

DEKAF2_NAMESPACE_END
