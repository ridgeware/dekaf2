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

#include <dekaf2/core/logging/klogrotate.h>
#include <dekaf2/core/logging/klog.h>
#include <dekaf2/core/types/kctype.h>
#include <dekaf2/io/compression/kcompression.h>
#include <dekaf2/core/strings/kregex.h>
#include <dekaf2/system/process/kinshell.h>
#include <dekaf2/system/os/ksystem.h>
#include <dekaf2/core/init/dekaf2.h>
#include <algorithm>
#include <vector>

DEKAF2_NAMESPACE_BEGIN

namespace {

//-----------------------------------------------------------------------------
/// returns true if sFilename has the suffix of a compressed file
bool IsCompressed(KStringView sFilename)
//-----------------------------------------------------------------------------
{
	return sFilename.ends_with(".gz") || sFilename.ends_with(".zst") || sFilename.ends_with(".zstd");

} // IsCompressed

//-----------------------------------------------------------------------------
/// returns the rotated files of a file with Counter or Timestamp naming, unsorted
KDirectory ListRotated(const KLogRotate::Config& conf)
//-----------------------------------------------------------------------------
{
	KString    sDirname  = kDirname (conf.sLogFilename);
	auto       sBasename = kBasename(conf.sLogFilename);
	KDirectory Rotated(sDirname);

	if (conf.RotationNaming == KLogRotate::Config::Counter)
	{
		Rotated.Match(kFormat("{}\\.[0-9]+(\\.gz|\\.zst|\\.zstd|)", KRegex::EscapeText(sBasename)));
	}
	else
	{
		// "YYYY-MM-DDThh-mm-ssZ"
		Rotated.Match(kFormat("{}\\.[0-9]{{4}}-[0-9]{{2}}-[0-9]{{2}}T[0-9]{{2}}-[0-9]{{2}}-[0-9]{{2}}Z(\\.gz|\\.zst|\\.zstd|)", KRegex::EscapeText(sBasename)));
	}

	return Rotated;

} // ListRotated

} // end of anonymous namespace

//-----------------------------------------------------------------------------
KLogRotate::KLogRotate(bool bOwnTimer)
//-----------------------------------------------------------------------------
: m_Timer(bOwnTimer ? std::make_unique<KTimer>(chrono::minutes(1)) : nullptr)
{
} // ctor

//-----------------------------------------------------------------------------
KLogRotate::~KLogRotate()
//-----------------------------------------------------------------------------
{
	// cancel all registered timer callbacks before the timer (or this instance)
	// is destroyed, to prevent use-after-free in timer callbacks
	WatchedFiles Watched;

	{
		auto WatchedFiles = m_WatchedFiles.unique();
		Watched = std::move(WatchedFiles.get());
		WatchedFiles->clear();
	}

	for (const auto& it : Watched)
	{
		CancelTimers(it.second);
	}

} // dtor

//-----------------------------------------------------------------------------
KTimer* KLogRotate::GetTimer()
//-----------------------------------------------------------------------------
{
	auto Timer = m_Timer.get();

	if (!Timer)
	{
		Timer = &Dekaf::getInstance().GetTimer();
	}

	return Timer;

} // GetTimer

//-----------------------------------------------------------------------------
void KLogRotate::CancelTimers(const WatchedFile& Watched)
//-----------------------------------------------------------------------------
{
	// call this outside of the lock of m_WatchedFiles: Cancel() waits for a running
	// callback, and the work callback takes that lock
	auto Timer = GetTimer();

	for (auto ID : { Watched.TimerID, Watched.WorkTimerID })
	{
		if (ID != KTimer::InvalidID)
		{
			Timer->Cancel(ID);
		}
	}

} // CancelTimers

//-----------------------------------------------------------------------------
bool KLogRotate::Add(const Config& conf)
//-----------------------------------------------------------------------------
{
	if (conf.sLogFilename.empty())
	{
		kDebug(1, "empty log filename");
		return false;
	}

	if (conf.bWriterCuts && conf.RotationNaming != Config::Date)
	{
		kDebug(1, "a file that its writer cuts needs Date naming: {}", conf.sLogFilename);
		return false;
	}

	auto         Timer       = GetTimer();
	auto         iGeneration = ++m_iGeneration;
	KTimer::ID_t TimerID     = KTimer::InvalidID;

	if (!conf.bWriterCuts)
	{
		TimerID = Timer->CallEvery(conf.CheckEvery, [this, conf](KUnixTime tNow)
		{
			Check(tNow, conf);
		});

		if (TimerID == KTimer::InvalidID)
		{
			return false;
		}
	}

	WatchedFile Previous;

	{
		auto  WatchedFiles = m_WatchedFiles.unique();
		auto& Watched      = WatchedFiles.get()[conf.sLogFilename];

		Previous = std::move(Watched);
		Watched  = { conf, TimerID, KTimer::InvalidID, iGeneration };
	}

	// the timers of an earlier configuration of this file
	CancelTimers(Previous);

	if (conf.RotationNaming == Config::Date)
	{
		ScheduleWork(conf.sLogFilename, iGeneration, kNow());
	}

	return true;

} // Add

//-----------------------------------------------------------------------------
bool KLogRotate::Remove(KStringViewZ sLogFilename)
//-----------------------------------------------------------------------------
{
	WatchedFile Removed;

	{
		auto WatchedFiles = m_WatchedFiles.unique();

		auto it = WatchedFiles->find(KString(sLogFilename));

		if (it == WatchedFiles->end())
		{
			return false;
		}

		Removed = std::move(it->second);
		WatchedFiles->erase(it);
	}

	CancelTimers(Removed);

	return true;

} // Remove

//-----------------------------------------------------------------------------
bool KLogRotate::ForceRotate(KStringViewZ sLogFilename)
//-----------------------------------------------------------------------------
{
	Config conf;

	{
		auto WatchedFiles = m_WatchedFiles.shared();

		auto it = WatchedFiles->find(KString(sLogFilename));

		if (it == WatchedFiles->end())
		{
			return false;
		}

		conf = it->second.conf;
	}

	// call Check outside the lock to avoid holding it during I/O
	auto tNow = kNow();

	Check(tNow, conf);

	if (conf.RotationNaming == Config::Date)
	{
		Housekeeping(tNow, conf);
	}

	return true;

} // ForceRotate

//-----------------------------------------------------------------------------
KUnixTime KLogRotate::GetNextWorkTime(const Config& conf, KUnixTime tNow)
//-----------------------------------------------------------------------------
{
	// a fixed offset in the work window for each host and file, so that the files of a
	// host and the hosts of a fleet spread over the window
	auto iHash   = kFormat("{}:{}", kGetHostname(), conf.sLogFilename).Hash();
	auto iSpread = std::max<int64_t>(conf.WorkSpread.seconds().count(), 1);
	auto Offset  = conf.WorkAt + chrono::seconds(static_cast<int64_t>(iHash % static_cast<uint64_t>(iSpread)));

	auto Today   = detail::KDatedLogNames::GetSpan(tNow, KDatedLogFile::Day, conf.TimeZone);
	auto tWork   = Today.tStart + Offset;

	if (tWork <= tNow)
	{
		// the time of today has passed, take the one of tomorrow
		tWork = Today.tEnd + Offset;
	}

	return tWork;

} // GetNextWorkTime

//-----------------------------------------------------------------------------
void KLogRotate::ScheduleWork(const KString& sLogFilename, uint64_t iGeneration, KUnixTime tAfter)
//-----------------------------------------------------------------------------
{
	Config conf;

	{
		auto WatchedFiles = m_WatchedFiles.shared();

		auto it = WatchedFiles->find(sLogFilename);

		if (it == WatchedFiles->end() || it->second.iGeneration != iGeneration)
		{
			// the file was removed or added again
			return;
		}

		conf = it->second.conf;
	}

	auto tWork = GetNextWorkTime(conf, tAfter);
	auto Timer = GetTimer();

	auto WorkTimerID = Timer->CallOnce(tWork, [this, sLogFilename, iGeneration, tWork](KUnixTime)
	{
		Work(sLogFilename, iGeneration, tWork);
	});

	if (WorkTimerID == KTimer::InvalidID)
	{
		kDebug(1, "cannot schedule the work for {}", sLogFilename);
		return;
	}

	bool bStored { false };

	{
		auto WatchedFiles = m_WatchedFiles.unique();

		auto it = WatchedFiles->find(sLogFilename);

		if (it != WatchedFiles->end() && it->second.iGeneration == iGeneration)
		{
			it->second.WorkTimerID = WorkTimerID;
			bStored = true;
		}
	}

	if (!bStored)
	{
		// the file was removed or added again in the meantime
		Timer->Cancel(WorkTimerID);
	}

} // ScheduleWork

//-----------------------------------------------------------------------------
void KLogRotate::Work(const KString& sLogFilename, uint64_t iGeneration, KUnixTime tWork)
//-----------------------------------------------------------------------------
{
	Config conf;

	{
		auto WatchedFiles = m_WatchedFiles.shared();

		auto it = WatchedFiles->find(sLogFilename);

		if (it == WatchedFiles->end() || it->second.iGeneration != iGeneration)
		{
			return;
		}

		conf = it->second.conf;
	}

	auto tNow = kNow();

	Housekeeping(tNow, conf);

	// after the time of this run, also if the timer fired a little early
	ScheduleWork(sLogFilename, iGeneration, std::max(tNow, tWork));

} // Work

//-----------------------------------------------------------------------------
void KLogRotate::Check(KUnixTime tNow, const Config& conf)
//-----------------------------------------------------------------------------
{
	if (conf.bWriterCuts)
	{
		// the writer cuts the file itself
		return;
	}

	auto Lock = m_LockMap.Create(conf.sLogFilename);

	kDebug(2, "checking logfile: {}", conf.sLogFilename);

	// check size
	KFileStat File(conf.sLogFilename);

	if (!File.Exists())
	{
		// file does not (yet?) exist
		return;
	}

	bool    bForceRotation { false };
	// with Date naming the date of the content
	KString sDate;

	if (conf.RotationNaming == Config::Date)
	{
		// the stamp file holds the time of the last cut
		KString   sRotationStamp = conf.sLogFilename + ".lastrotation";
		KFileStat Stamp(sRotationStamp);
		auto      Current = detail::KDatedLogNames::GetSpan(tNow, conf.Period, conf.TimeZone);

		if (!Stamp.Exists())
		{
			// the content counts as content of the current period
			kTouchFile(sRotationStamp);
		}
		else
		{
			auto Last = detail::KDatedLogNames::GetSpan(Stamp.ModificationTime(), conf.Period, conf.TimeZone);

			if (Last.sDate != Current.sDate)
			{
				// a later period started, the content belongs to the period of the last cut
				kTouchFile(sRotationStamp);
				sDate = std::move(Last.sDate);
			}
		}

		if (File.Size() <= conf.iKeepSize)
		{
			// nothing to cut
			return;
		}

		if (sDate.empty() && File.Size() > conf.iMaxSize)
		{
			sDate = std::move(Current.sDate);
		}

		bForceRotation = !sDate.empty();
	}
	else
	{
		if (File.Size() <= conf.iKeepSize)
		{
			// file would not rotate
			return;
		}

		if (!conf.RotateEvery.IsZero())
		{
			// check last rotation
			KString sRotationStamp = conf.sLogFilename;
			sRotationStamp += ".lastrotation";

			KFileStat Stamp(sRotationStamp);

			if (!Stamp.Exists())
			{
				kTouchFile(sRotationStamp);
			}
			else
			{
				if (Stamp.ModificationTime() + conf.RotateEvery < tNow)
				{
					kTouchFile(sRotationStamp);
					bForceRotation = true;
				}
			}
		}

		if (File.Size() > conf.iMaxSize)
		{
			bForceRotation = true;
		}
	}

	if (bForceRotation)
	{
		if (!conf.sCommandBeforeRotation.empty())
		{
			KInShell(conf.sCommandBeforeRotation, "/bin/sh", {{"LOGFILE", conf.sLogFilename}});
		}

		if (conf.BeforeRotationCallback)
		{
			if (conf.BeforeRotationCallback(conf.sLogFilename) == false)
			{
				kDebug(2, "rotation stopped by callback: {}", conf.sLogFilename);
				return;
			}
		}

		KString sNewestRotatedFile;

		if (conf.RotationNaming == Config::Date)
		{
			// the compression follows in the work window
			auto sTarget = detail::KDatedLogNames::GetNextName(conf.sLogFilename, sDate);

			if (MoveContent(conf, File, sTarget))
			{
				sNewestRotatedFile = std::move(sTarget);
			}
		}
		else
		{
			sNewestRotatedFile = Rotate(tNow, conf, File);
		}

		if (conf.AfterRotationCallback)
		{
			conf.AfterRotationCallback(sNewestRotatedFile);
		}

		if (!conf.sCommandAfterRotation.empty())
		{
			KInShell(conf.sCommandAfterRotation, "/bin/sh", {{"ROTATEDFILE", sNewestRotatedFile}});
		}
	}

} // Check

//-----------------------------------------------------------------------------
KString KLogRotate::Rotate(KUnixTime tNow, const Config& conf, const KFileStat& File)
//-----------------------------------------------------------------------------
{
	kDebug(2, "file: {}, size: {}, keep: {}", conf.sLogFilename, kFormBytes(File.Size()), kFormBytes(conf.iKeepSize));

	if (conf.iMaxSize <= conf.iKeepSize)
	{
		kDebug(1, "bad configuration: max size: {} <= keep size: {}", conf.iMaxSize, conf.iKeepSize);
		return {};
	}

	bool bUseTimestamps = conf.RotationNaming == Config::Timestamp;

	if (!bUseTimestamps || !conf.MaxAge.IsZero())
	{
		auto       sBasename = kBasename(conf.sLogFilename);
		KDirectory Rotated   = ListRotated(conf);

		if (!bUseTimestamps)
		{
			std::sort(Rotated.begin(), Rotated.end(), [&sBasename](const KDirectory::value_type& left, const KDirectory::value_type& right)
			{
				// we sort reversed, as we will have to rename the files in that order
				return GetRotationCount(left.Filename().substr(sBasename.size())) > GetRotationCount(right.Filename().substr(sBasename.size()));
			});
		}

		bool bCheckAge { true };

		for (auto& Rotate : Rotated)
		{
			bool bDiscard { false };

			if (!bUseTimestamps)
			{
				// rotate all existing rotated files one level up
				auto iCount = GetRotationCount(Rotate.Filename().substr(sBasename.size()));

				if (iCount >= conf.iRotations)
				{
					bDiscard = true;
				}
				else if (!conf.MaxAge.IsZero() && bCheckAge)
				{
					KFileStat Rot(Rotate.Path());

					if (Rot.ModificationTime() + conf.MaxAge < tNow)
					{
						bDiscard = true;
					}
					else
					{
						// stop checking the age, all remaining files will be younger
						bCheckAge = false;
					}
				}

				if (!bDiscard)
				{
					// rotate ..
					auto sNewName = kFormat("{}.{}", conf.sLogFilename, iCount + 1);

					auto sSuffix = kExtension(Rotate.Filename());

					if (sSuffix == "zstd" || sSuffix == "zst" || sSuffix == "gz")
					{
						sNewName += '.';
						sNewName += sSuffix;
					}

					if (!kRename(Rotate.Path(), sNewName))
					{
						kDebug(1, "cannot rotate file: {}", Rotate.Path());
					}
				}
			}
			// no need to check the branch condition, this is the remaining case,
			// but for clarity leave it in - the compiler probably removes it anyway
			else if (!conf.MaxAge.IsZero())
			{
				// simply check the timestamps for expiration, no rotation needed
				constexpr KStringView sTimestampMask("YYYY-MM-DDThh-mm-ssZ");
				auto sTimestamp   = Rotate.Filename().substr(sBasename.size()+1, sTimestampMask.size());
				KUnixTime tRotate = kParseTimestamp(sTimestampMask, sTimestamp);

				if (tRotate + conf.MaxAge < tNow)
				{
					bDiscard = true;
				}
			}

			if (bDiscard)
			{
				if (!kRemoveFile(Rotate.Path()))
				{
					kDebug(1, "cannot remove file: {}", Rotate.Path());
				}
			}
		}
	}

	// finally rotate the log file as well

	KString sTempfile;

	// move/copy right into target rotation
	if (!bUseTimestamps)
	{
		sTempfile = kFormat("{}.1", conf.sLogFilename);
	}
	else
	{
		// note: '-' as time separator, not ':' - colons are illegal in Windows (NTFS) filenames
		sTempfile = kFormat("{}.{:%Y-%m-%dT%H-%M-%SZ}", conf.sLogFilename, tNow);
	}

	if (!MoveContent(conf, File, sTempfile))
	{
		return {};
	}

	if (!conf.bCompress)
	{
		return sTempfile;
	}

	CompressOlder(conf);

	if (conf.iKeepUncompressed > 0)
	{
		return sTempfile;
	}

	return Compress(sTempfile, true, conf.CompressWith);

} // Rotate

//-----------------------------------------------------------------------------
bool KLogRotate::MoveContent(const Config& conf, const KFileStat& File, KStringViewZ sTarget)
//-----------------------------------------------------------------------------
{
	if (conf.iKeepSize > 0 || conf.bCopyTrunc)
	{
		// take an exclusive lock to prevent concurrent writes during rotation
		KFileLock Lock(conf.sLogFilename, KFileLock::Exclusive);

		// keep the newest records in the old place, write the older ones into the temp location.
		// read the source through a dedicated read-only stream: interleaving reads and writes on
		// a single std::fstream is undefined behavior, and on MSVC the reads then return 0 bytes.
		// we rewrite the kept portion further down through a separate write-only handle.
		KInFile LogFile(conf.sLogFilename);
		// do not trim!
		LogFile.SetReaderTrim("", "");

		if (!LogFile.is_open())
		{
			kDebug(1, "cannot open log file: {}", conf.sLogFilename);
			return false;
		}

		KOutFile OutFile(sTarget);

		if (!OutFile.is_open())
		{
			kDebug(1, "cannot create tempfile: {}", sTarget);
			return false;
		}

		if (File.Size() <= conf.iKeepSize)
		{
			kDebug(1, "cannot rotate, file size {} <= {}", File.Size(), conf.iKeepSize);
			return false;
		}

		auto iCopy = File.Size() - conf.iKeepSize;

		OutFile.Write(LogFile, iCopy);

		// now copy until end of current line
		OutFile.Write(LogFile.ReadLine());

		// close the output
		OutFile.close();

		// read the remaining content into a buffer, then close the read handle
		auto sBuffer = LogFile.ReadRemaining();
		LogFile.close();

		// rewrite the kept portion in place through a separate write-only handle, opened
		// in|out (no truncation), so there is no read/write interleaving on the handle.
		// write the kept portion first, then truncate - if we crash between write and
		// truncate we have duplicate data (recoverable), not lost data.
		KFile KeepFile(conf.sLogFilename, std::ios_base::in | std::ios_base::out);

		if (!KeepFile.is_open())
		{
			kDebug(1, "cannot open log file for writing: {}", conf.sLogFilename);
			return false;
		}

		KeepFile.Write(sBuffer);
		KeepFile.close();

		// now truncate the stale tail
		kResizeFile(conf.sLogFilename, sBuffer.size());
	}
	else
	{
		if (!kRename(conf.sLogFilename, sTarget))
		{
			kDebug(1, "cannot rename file {} into temporary: {}", conf.sLogFilename, kBasename(sTarget));
			return false;
		}
		// and make sure the log file is existing again
		kTouchFile(conf.sLogFilename);
	}

	return true;

} // MoveContent

//-----------------------------------------------------------------------------
void KLogRotate::CompressOlder(const Config& conf)
//-----------------------------------------------------------------------------
{
	auto Rotated   = ListRotated(conf);
	auto sBasename = kBasename(conf.sLogFilename);

	// the newest first
	std::sort(Rotated.begin(), Rotated.end(), [&conf, &sBasename](const KDirectory::value_type& left, const KDirectory::value_type& right)
	{
		if (conf.RotationNaming == Config::Counter)
		{
			return GetRotationCount(left.Filename().substr(sBasename.size())) < GetRotationCount(right.Filename().substr(sBasename.size()));
		}

		// the timestamps sort by their names
		return left.Filename() > right.Filename();
	});

	// Rotate() compresses the newest file itself
	std::size_t iKeep  = std::max<std::size_t>(conf.iKeepUncompressed, 1);
	std::size_t iIndex { 0 };

	for (const auto& Entry : Rotated)
	{
		if (iIndex++ >= iKeep && !IsCompressed(Entry.Filename()))
		{
			Compress(Entry.Path(), true, conf.CompressWith);
		}
	}

} // CompressOlder

//-----------------------------------------------------------------------------
void KLogRotate::Housekeeping(KUnixTime tNow, const Config& conf)
//-----------------------------------------------------------------------------
{
	auto Lock = m_LockMap.Create(conf.sLogFilename);

	kDebug(2, "compressing and removing past files of: {}", conf.sLogFilename);

	// the number of kept files, the newest first
	std::size_t iKept { 0 };

	for (const auto& Past : detail::KDatedLogNames::GetPastFiles(conf.sLogFilename))
	{
		if (!conf.MaxAge.IsZero())
		{
			auto tEnd = detail::KDatedLogNames::GetEnd(Past.sDate, conf.TimeZone);

			if (tEnd.ok() && tEnd + conf.MaxAge < tNow)
			{
				if (!kRemoveFile(Past.sPath))
				{
					kDebug(1, "cannot remove file: {}", Past.sPath);
				}

				continue;
			}
		}

		if (conf.bCompress && !Past.bCompressed && iKept >= conf.iKeepUncompressed)
		{
			Compress(Past.sPath, true, conf.CompressWith);
		}

		++iKept;
	}

} // Housekeeping

//-----------------------------------------------------------------------------
KString KLogRotate::Compress(KStringViewZ sUncompressedName, bool bRemoveAfterCompression, Config::Compression With)
//-----------------------------------------------------------------------------
{
#if DEKAF2_HAS_LIBZSTD
	constexpr bool bHasZStd { true  };
#else
	constexpr bool bHasZStd { false };
#endif

	KStringView sSuffix;

	switch (With)
	{
		case Config::Default:
		case Config::ZStd:
			sSuffix = bHasZStd ? "zst" : "gz";
			break;

		case Config::GZip:
			sSuffix = "gz";
			break;
	}

	auto sCompfile = kFormat("{}.{}", sUncompressedName, sSuffix);

	KStopTime Timer;

	{
		KCompress Comp(sCompfile, 50, 1);
		KInFile InFile(sUncompressedName);

		if (!InFile.is_open())
		{
			kDebug(1, "cannot open input file: {}", sUncompressedName);
			return {};
		}

		if (!Comp.Write(InFile))
		{
			kDebug(1, "cannot compress tempfile into {}", sCompfile);
			return {};
		}
	}

	kDebug(2, "compression took {}", Timer.elapsed());

	if (bRemoveAfterCompression)
	{
		if (!kRemoveFile(sUncompressedName))
		{
			kDebug(1, "cannot remove input file: {}", sUncompressedName);
		}
	}

	return sCompfile;

} // Compress

//-----------------------------------------------------------------------------
uint16_t KLogRotate::GetRotationCount(KStringView sSuffixes)
//-----------------------------------------------------------------------------
{
	uint16_t iCount { 0 };

	// we get .124.zstd or .12 or .912456.gzip
	if (sSuffixes.remove_prefix('.'))
	{
		while (KASCII::kIsDigit(sSuffixes.front()))
		{
			auto iNew = static_cast<uint32_t>(iCount) * 10 + (sSuffixes.front() - '0');

			if (iNew > std::numeric_limits<uint16_t>::max())
			{
				return std::numeric_limits<uint16_t>::max();
			}

			iCount = static_cast<uint16_t>(iNew);
			sSuffixes.remove_prefix(1);
		}
		if (!(sSuffixes.empty() || sSuffixes.front() == '.'))
		{
			iCount = 0;
		}
	}

	return iCount;

} // GetRotationCount

DEKAF2_NAMESPACE_END
