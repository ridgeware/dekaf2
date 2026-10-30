/*
//
// DEKAF(tm): Lighter, Faster, Smarter (tm)
//
// Copyright (c) 2025, Ridgeware, Inc.
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


/// @file klogrotate.h
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/strings/kstringview.h>
#include <dekaf2/core/logging/kdatedlogfile.h>
#include <dekaf2/io/readwrite/kreader.h>
#include <dekaf2/io/readwrite/kwriter.h>
#include <dekaf2/time/clock/ktime.h>
#include <dekaf2/time/duration/kduration.h>
#include <dekaf2/time/duration/ktimer.h>
#include <dekaf2/system/filesystem/kfilesystem.h>
#include <dekaf2/containers/associative/klockmap.h>
#include <atomic>
#include <thread>
#include <functional>
#include <unordered_map>

DEKAF2_NAMESPACE_BEGIN

/// @addtogroup core_logging
/// @{

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Monitors and rotates growing files, typically log files, based on configurable
/// size and time thresholds.
///
/// Each monitored file has its own Config with independent parameters for maximum
/// size, rotation interval, keep size, compression, and cleanup policies.
///
/// ## Rotation Strategies
///
/// **Rename (default):** The log file is atomically renamed to the rotation target
/// and a new empty file is created at the original path. This is fast (O(1)) but
/// processes that hold the old file descriptor open will continue writing to the
/// renamed file. Use sCommandAfterRotation or AfterRotationCallback to signal
/// such processes to reopen the file (e.g. SIGHUP).
///
/// **CopyTrunc** (bCopyTrunc = true, or iKeepSize > 0): The oldest portion of the
/// file is copied to the rotation target while the newest portion (iKeepSize bytes)
/// is kept in the original file. The file inode stays the same, so processes with
/// open file descriptors continue writing correctly without any signal. This path
/// is protected by a KFileLock to prevent data loss from concurrent writers.
///
/// ## Rotation Naming
///
/// **Counter (default):** Rotated files are named `.1`, `.2`, etc. Older
/// rotations are bumped up, and files exceeding iRotations are deleted.
///
/// **Timestamp:** Rotated files are named with the UTC time of the rotation,
/// e.g. `.2025-03-31T14-30-00Z` - with dashes in the time, as NTFS does not allow
/// colons. Old files are cleaned up based on MaxAge. Set MaxAge to zero to keep all
/// rotated files indefinitely.
///
/// **Date:** Rotated files are named with the date of the day, week or month of
/// their content, e.g. `-20251031`, `-2025W44` or `-202510`, as KDatedLogFile names
/// them. With bWriterCuts, a KDatedLogFile cuts the file itself at the exact
/// boundary of each period, as KLog with SetDebugLogPeriod() and KHTTPLog with a
/// dated access log do, and KLogRotate only compresses and removes the files of
/// past periods. Without bWriterCuts, KLogRotate cuts the file at the first check
/// after the end of a period, which can be up to CheckEvery late - that mode is
/// meant for files of other writers. Files of past periods expire when MaxAge has
/// passed after the end of their period, also those of an earlier logrotate setup
/// with dateext naming.
///
/// ## Compression
///
/// Rotated files are compressed by default (bCompress = true), with zstd if
/// available (DEKAF2_HAS_LIBZSTD) and gzip otherwise, see CompressWith. New files
/// get the suffix .zst or .gz, files with .zstd of earlier versions are recognized.
/// The newest iKeepUncompressed rotated files stay uncompressed.
///
/// ## Timer Integration
///
/// Periodic checks are driven by a KTimer. By default the shared dekaf2 system
/// timer is used. Pass bOwnTimer = true to the constructor to create a dedicated
/// timer instance (useful when the system timer is not running). Use ForceRotate()
/// to trigger an immediate check independent of the timer.
///
/// With Date naming, the compression and removal of past files run once per day in
/// a work window, by default between 02:00 and 05:00 in the time zone of the files.
/// Each file gets a fixed time in the window from a hash of the host name and its
/// path, so that the files of a host and the hosts of a fleet do not all start at
/// the same time, see GetNextWorkTime(). See @ref dated_logs "Dated Log Files" for
/// an example.
///
/// ## Thread Safety
///
/// All public methods are thread-safe. The internal watch list is protected by
/// KThreadSafe, and per-file rotation is serialized via KShallowLockMap.
class DEKAF2_PUBLIC KLogRotate
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	/// Creates an instance of KLogRotate.
	/// @param bOwnTimer if true, creates a dedicated KTimer instance instead of using the
	/// shared dekaf2 system timer. Use this when the system timer is not running, e.g.
	/// in standalone tools or test environments. Defaults to false.
	KLogRotate(bool bOwnTimer = false);
	/// Destructor. Cancels all registered timer callbacks to prevent use-after-free,
	/// then destroys the own timer (if any).
	~KLogRotate();
	KLogRotate(const KLogRotate&) = delete;
	KLogRotate& operator=(const KLogRotate&) = delete;
	KLogRotate(KLogRotate&&) = delete;
	KLogRotate& operator=(KLogRotate&&) = delete;

	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// Configuration for a single monitored log file. Each parameter has a
	/// sensible default; at minimum set sLogFilename and either iMaxSize,
	/// RotateEvery or Date naming to enable rotation.
	struct Config
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		/// the naming of rotated files
		enum Naming
		{
			Counter,   ///< app.log.1, app.log.2, ... - the newest file has the lowest counter
			Timestamp, ///< app.log.2025-03-31T14-30-00Z, with the UTC time of the rotation
			Date       ///< app.log-20251031, with the date of the period of the content
		};

		/// the compression of rotated files
		enum Compression
		{
			Default,   ///< zstd if available, else gzip
			GZip,      ///< gzip, for tooling like zgrep or zless that expects it
			ZStd       ///< zstd, gzip if zstd is not available
		};

		/// Absolute path of the log file to monitor. Required.
		KString     sLogFilename;
		/// Maximum file size in bytes before rotation is triggered.
		/// Default: npos (off — rotation is only triggered by RotateEvery).
		std::size_t iMaxSize    { npos };
		/// Maximum age of the log file before rotation is forced, independent of size.
		/// A stamp file (sLogFilename + ".lastrotation") tracks the last rotation time.
		/// Default: zero (off — rotation is only triggered by iMaxSize).
		KDuration   RotateEvery { KDuration::zero()  };
		/// Number of bytes to keep at the end of the log file after rotation.
		/// The kept portion remains in the original file (same inode), making the
		/// most recent log entries always available for searching even right after
		/// rotation. Implies the CopyTrunc strategy.
		/// Default: 0 (rotate the entire file, use Rename strategy).
		std::size_t iKeepSize   { 0 };
		/// Maximum number of counter-based rotated files to keep before the oldest
		/// is deleted. Only used with Counter naming.
		/// Default: 1000.
		uint16_t    iRotations  { 1000 };
		/// Maximum age of rotated files before they are deleted. With Date naming, a
		/// file expires when MaxAge has passed after the end of the period in its name.
		/// Default: zero (never delete rotated files based on age).
		KDuration   MaxAge      { KDuration::zero()  };
		/// Interval between periodic rotation checks. For slowly growing files
		/// a longer interval reduces overhead.
		/// Default: 1 hour.
		KDuration   CheckEvery  { chrono::hours(1)   };
		/// Shell command to execute before rotation. The environment variable
		/// LOGFILE is set to the path of the file about to be rotated.
		/// Default: empty (no command).
		KString     sCommandBeforeRotation;
		/// Shell command to execute after rotation. The environment variable
		/// ROTATEDFILE is set to the path of the newest rotated file.
		/// Default: empty (no command).
		KString     sCommandAfterRotation;
		/// Callback invoked before rotation. Receives the log file path.
		/// Return false to abort the rotation.
		/// Default: empty (no callback).
		std::function<bool(KStringViewZ)> BeforeRotationCallback;
		/// Callback invoked after rotation. Receives the path of the newest
		/// rotated file, which may be empty if rotation failed.
		/// Default: empty (no callback).
		std::function<void(KStringViewZ)> AfterRotationCallback;
#if 0
		/// mail address for discarded or rotated sections
		KString     sMailTo;
		/// mail a discarded rotated section - defaults to false
		bool        bMailDiscarded { false };
		/// mail a newly rotated section - defaults to false
		bool        bMailRotated   { false };
#endif
		/// Compress rotated files. Uses zstd if available (DEKAF2_HAS_LIBZSTD),
		/// otherwise gzip. Compressed files get a .zstd or .gz suffix.
		/// Default: true.
		bool        bCompress      {  true };
		/// Use the CopyTrunc strategy even when iKeepSize is 0: copy all content
		/// to the rotation target and truncate the original file to zero. The file
		/// inode stays the same, so processes with open file descriptors continue
		/// writing to the correct file without needing a reopen signal.
		/// Default: false (use Rename strategy when iKeepSize is 0).
		bool        bCopyTrunc     { false };
		/// Only with Date naming: a KDatedLogFile writes the file and cuts it itself, so
		/// that KLogRotate only compresses and removes the files of past periods, in the
		/// work window. Without it, KLogRotate cuts the file at the first check after the
		/// end of a period.
		/// Default: false.
		bool        bWriterCuts    { false };
		/// The naming of rotated files.
		/// Default: Counter.
		Naming      RotationNaming { Counter };
		/// The compression of rotated files, if bCompress is set.
		/// Default: zstd if available, else gzip.
		Compression CompressWith   { Default };
		/// The number of the newest rotated files that stay uncompressed, if bCompress
		/// is set. With Counter naming a file is compressed when its counter grows beyond
		/// it, with Timestamp and Date naming all but the newest files are compressed.
		/// Default: 0.
		uint16_t    iKeepUncompressed { 0 };
		/// Only with Date naming without bWriterCuts: the period of a file.
		/// Default: a day.
		KDatedLogFile::Interval Period { KDatedLogFile::Day };
		/// Only with Date naming: the time zone of the periods and of the work window,
		/// nullptr for UTC - a time zone is only supported where DEKAF2_HAS_TIMEZONES is set.
		/// Default: UTC.
		const chrono::time_zone* TimeZone { nullptr };
		/// Only with Date naming: the start of the work window after midnight, in which
		/// the files of past periods are compressed and removed.
		/// Default: 2 hours.
		KDuration   WorkAt         { chrono::hours(2) };
		/// Only with Date naming: the length of the work window.
		/// Default: 3 hours.
		KDuration   WorkSpread     { chrono::hours(3) };

	}; // Config

	//-----------------------------------------------------------------------------
	/// add a new logfile to watch, or update the configuration of an already watched file
	/// @param conf the configuration for the logfile to watch
	/// @return true if the file was added successfully
	bool    Add         (const Config& conf);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// stop watching a logfile and cancel its periodic check timer
	/// @param sLogFilename the file to stop watching
	/// @return true if the file was being watched and was removed
	bool    Remove      (KStringViewZ sLogFilename);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// force an immediate rotation check for a watched logfile, independent
	/// of the periodic timer - with Date naming, the files of past periods are also
	/// compressed and removed right away
	/// @param sLogFilename the file to force-check for rotation
	/// @return true if the file was being watched and the check was triggered
	bool    ForceRotate (KStringViewZ sLogFilename);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// returns the time of the next compression and removal of past files for a
	/// file with Date naming: the start of the next day in conf.TimeZone plus
	/// conf.WorkAt plus an offset below conf.WorkSpread, which is fixed for the host
	/// name and the path of the file
	/// @param conf the configuration of the file
	/// @param tNow the time after which the next run is due
	/// @return the time of the next run, later than tNow
	static KUnixTime GetNextWorkTime(const Config& conf, KUnixTime tNow);
	//-----------------------------------------------------------------------------

//------
private:
//------

	void    Check        (KUnixTime tNow, const Config& conf);
	KString Rotate       (KUnixTime tNow, const Config& conf, const KFileStat& File);
	bool    MoveContent  (const Config& conf, const KFileStat& File, KStringViewZ sTarget);
	void    CompressOlder(const Config& conf);
	void    Housekeeping (KUnixTime tNow, const Config& conf);
	void    ScheduleWork (const KString& sLogFilename, uint64_t iGeneration, KUnixTime tAfter);
	void    Work         (const KString& sLogFilename, uint64_t iGeneration, KUnixTime tWork);

	static KString  Compress(KStringViewZ sUncompressedName, bool bRemoveAfterCompression, Config::Compression With);
	static uint16_t GetRotationCount(KStringView sSuffixes);
	KTimer* GetTimer();

	struct WatchedFile
	{
		Config       conf;
		/// the periodic check
		KTimer::ID_t TimerID     { KTimer::InvalidID };
		/// the next compression and removal of past files with Date naming
		KTimer::ID_t WorkTimerID { KTimer::InvalidID };
		/// changes with every Add(), so that a scheduled work of an earlier
		/// configuration does not schedule itself again
		uint64_t     iGeneration { 0 };
	};

	using WatchedFiles = std::unordered_map<KString, WatchedFile>;

	void    CancelTimers (const WatchedFile& Watched);

	std::unique_ptr<KTimer>   m_Timer;
	KShallowLockMap<KString>  m_LockMap;
	KThreadSafe<WatchedFiles> m_WatchedFiles;
	std::atomic<uint64_t>     m_iGeneration { 0 };

}; // KLogRotate


/// @}

DEKAF2_NAMESPACE_END
