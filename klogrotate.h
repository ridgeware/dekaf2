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

#include "kstring.h"
#include "kstringview.h"
#include "kreader.h"
#include "kwriter.h"
#include "ktime.h"
#include "kduration.h"
#include "ktimer.h"
#include "kfilesystem.h"
#include "klockmap.h"
#include <thread>
#include <functional>

DEKAF2_NAMESPACE_BEGIN

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// A class to rotate and truncate growing files, notably logs, and possibly discard or mail previously rotated files.
/// Every monitored file has its own parameters. Checks are executed at most every minute, but per default every
/// hour.
class DEKAF2_PUBLIC KLogRotate
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	/// Creates an instance of KLogRotate
	/// @param bOwnTimer set to true if you do not want to use the dekaf2 system timer, or if it is not running.
	/// Defaults to false.
	KLogRotate(bool bOwnTimer = false);
	KLogRotate(const KLogRotate&) = delete;
	KLogRotate(KLogRotate&&) = delete;

	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	struct Config
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		/// the file to watch
		KString     sLogFilename;
		/// max size of the file before rotation is triggered - default off
		std::size_t iMaxSize    { npos };
		/// max age of the file before rotation is triggered - default off
		KDuration   RotateEvery { KDuration::zero()  };
		/// size of the file to keep after rotation (and which is not rotated)
		/// - use this to keep a log file always searchable for the last
		/// records, even directly after rotation
		std::size_t iKeepSize   { npos };
		/// maximum rotation count before deletion
		uint16_t    iRotations  { 1000 };
		/// max age of rotated files before deletion
		KDuration   MaxAge      { KDuration::zero()  };
		/// interval to check for rotation - keep it low for slowly growing files
		KDuration   CheckEvery  { chrono::hours(1)   };
		/// command to execute before rotation, env LOGFILE will be set
		/// with the pathname of the logfile to process
		KString     sCommandBeforeRotation;
		/// command to execute after rotation, env ROTATEDFILE will be set
		/// with the pathname of the last rotated file (may be empty)
		KString     sCommandAfterRotation;
		/// callback to call before rotation - gets the logfile name as parameter,
		/// if return is false rotation is stopped
		std::function<bool(KStringViewZ)> BeforeRotationCallback;
		/// callback to call after rotation - gets the newest rotated file name as parameter
		/// which may be empty in case of rotation failure
		std::function<void(KStringViewZ)> AfterRotationCallback;
#if 0
		/// mail address for discarded or rotated sections
		KString     sMailTo;
		/// mail a discarded rotated section - defaults to false
		bool        bMailDiscarded { false };
		/// mail a newly rotated section - defaults to false
		bool        bMailRotated   { false };
#endif
		/// compress the rotations - defaults to true, will
		/// use zstd compression if available, else gzip
		bool        bCompress      {  true };
		/// keep the file open during rotation, and copy the
		/// rotated content, then trunc the file from the beginning,
		/// the inode will stay the same - defaults to false
		bool        bCopyTrunc     { false };
		/// use timestamps in rotation file names instead of simple counters
		/// - defaults to false
		bool        bUseTimestamps { false };

	}; // Config

	/// add a new logfile to watch
	bool    Add      (const Config& conf);

//------
private:
//------

	void    Check    (KUnixTime tNow, const Config& conf);
	KString Rotate   (KUnixTime tNow, const Config& conf, const KFileStat& File);
	KString Compress (KStringViewZ sUncompressedName, bool bRemoveAfterCompression);

	static uint16_t GetRotationCount(KStringView sSuffixes);

	std::unique_ptr<KTimer>  m_Timer;
	KShallowLockMap<KString> m_LockMap;

}; // KLogRotate

DEKAF2_NAMESPACE_END
