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
/// a log file that starts a new file with every calendar day

#include <dekaf2/core/init/kdefinitions.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/strings/kstringview.h>
#include <dekaf2/io/readwrite/kwriter.h>
#include <dekaf2/time/clock/ktime.h>
#include <dekaf2/time/duration/kduration.h>

DEKAF2_NAMESPACE_BEGIN

/// @addtogroup core_logging
/// @{

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// A log file that holds the records of one calendar day. Each record is written together with
/// its time stamp, and the first record of a later day closes the file, renames it to the name
/// of its day, like app.log-20261029, and starts a new file under the original name. All
/// records stamped on one day are in the files of that day, and only there. The days are
/// separated at midnight UTC, or at midnight of a configured time zone.
///
/// Records of the previous day that arrive after the cut, like a record that waited in a queue,
/// are appended to the file of the previous day, as long as the newest record time seen is less
/// than Options::Grace after the day boundary. An even older record goes into the current file.
///
/// A file that would grow beyond Options::iMaxSize is cut within the day as well. The parts of a
/// day are named app.log-20261029, app.log-20261029.1, app.log-20261029.2 in the order of their
/// content.
///
/// The class does not compress or remove the files of past days, KLogRotate does that.
///
/// The class is not synchronized: the callers serialize their writes, as the writer thread of
/// kLogger() and KLog do.
class DEKAF2_PUBLIC KDatedLogFile
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	struct Options
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		/// the time zone whose midnight separates the days, nullptr for UTC - a time zone is only
		/// supported where DEKAF2_HAS_TIMEZONES is set
		const chrono::time_zone* TimeZone { nullptr };
		/// the size beyond which a file is cut within the day, npos for no limit
		std::size_t              iMaxSize { npos };
		/// the time after the day boundary during which records of the previous day still go into
		/// the file of the previous day, measured in record time
		KDuration                Grace    { chrono::seconds(60) };
		/// the mode for newly created files, 0 to leave it to the umask
		int                      iMode    { 0 };

	}; // Options

	/// opens or creates the log file sFilename, with days in UTC, no size limit and a grace period
	/// of 60 seconds - an existing file continues to be written, until a record of a later day
	/// than the day of its last modification arrives
	/// @param sFilename the path of the log file
	KDatedLogFile(KString sFilename);

	/// opens or creates the log file sFilename - an existing file continues to be written, until a
	/// record of a later day than the day of its last modification arrives
	/// @param sFilename the path of the log file
	/// @param options the options for the day boundaries, the maximum size and the grace period
	KDatedLogFile(KString sFilename, Options options);

	KDatedLogFile(const KDatedLogFile&) = delete;
	KDatedLogFile& operator=(const KDatedLogFile&) = delete;

	/// writes sRecord as it is, after cutting the file if the record belongs to a later day or the
	/// file would grow too large
	/// @param sRecord the record to write
	/// @param tRecord the time stamp of the record, which decides about its day
	/// @return true if the record was written
	bool           Write       (KStringView sRecord, KUnixTime tRecord);

	/// writes sLine and a line feed, after cutting the file if the line belongs to a later day or
	/// the file would grow too large
	/// @param sLine the line to write
	/// @param tRecord the time stamp of the line, which decides about its day
	/// @return true if the line was written
	bool           WriteLine   (KStringView sLine, KUnixTime tRecord);

	/// flushes the current file, and the file of the previous day if it is open
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

	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// a calendar day in the configured time zone
	struct Day
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		KUnixTime tStart;
		KUnixTime tEnd;
		/// the date as YYYYMMDD, empty if the day is not known yet
		KString   sDate;

	}; // Day

	DEKAF2_PRIVATE
	Day       GetDay       (KUnixTime tRecord) const;
	DEKAF2_PRIVATE
	KOutFile& Select       (KUnixTime tRecord, std::size_t iSize);
	DEKAF2_PRIVATE
	void      Cut          (Day NewDay, bool bNewDay);
	DEKAF2_PRIVATE
	KString   NextFreeName (KStringView sDate) const;
	DEKAF2_PRIVATE
	bool      Open         ();

	Options     m_Options;
	KString     m_sFilename;
	/// the file under the original name
	KOutFile    m_File;
	std::size_t m_iSize    { 0 };
	Day         m_Day;
	/// the last file of the previous day, opened when a late record arrives
	KOutFile    m_Previous;
	KString     m_sPrevious;
	Day         m_PreviousDay;
	/// the newest record time seen, which ends the grace period
	KUnixTime   m_tNewest;

}; // KDatedLogFile

/// @}

DEKAF2_NAMESPACE_END
