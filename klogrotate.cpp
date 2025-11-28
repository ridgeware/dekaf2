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

#include "klogrotate.h"
#include "klog.h"
#include "kctype.h"
#include "kcompression.h"
#include "kregex.h"
#include "kinshell.h"

DEKAF2_NAMESPACE_BEGIN

//-----------------------------------------------------------------------------
KLogRotate::KLogRotate(bool bOwnTimer)
//-----------------------------------------------------------------------------
: m_Timer(bOwnTimer ? std::make_unique<KTimer>(chrono::minutes(1)) : nullptr)
{
} // ctor


//-----------------------------------------------------------------------------
bool KLogRotate::Add(const Config& conf)
//-----------------------------------------------------------------------------
{
	if (conf.sLogFilename.empty())
	{
		kDebug(1, "empty log filename");
		return false;
	}

	auto Timer = m_Timer.get();

	if (!Timer)
	{
		Timer = &Dekaf::getInstance().GetTimer();
	}

	Timer->CallEvery(conf.CheckEvery, [this, conf](KUnixTime tNow)
	{
		Check(tNow, conf);
	});

	return true;

} // Add

//-----------------------------------------------------------------------------
void KLogRotate::Check(KUnixTime tNow, const Config& conf)
//-----------------------------------------------------------------------------
{
	auto Lock = m_LockMap.Create(conf.sLogFilename);

	kDebug(2, "checking logfile: {}", conf.sLogFilename);

	// check size
	KFileStat File(conf.sLogFilename);

	if (!File.Exists() || File.Size() <= conf.iKeepSize)
	{
		// file does not (yet?) exist or would not rotate
		return;
	}

	bool bForceRotation { false };

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

		auto sNewestRotatedFile = Rotate(tNow, conf, File);

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

	if (!conf.bUseTimestamps || !conf.MaxAge.IsZero())
	{
		KString    sDirname  = kDirname (conf.sLogFilename);
		auto       sBasename = kBasename(conf.sLogFilename);
		KDirectory Rotated(sDirname);

		if (!conf.bUseTimestamps)
		{
			Rotated.Match(kFormat("{}\\.[0-9]+(\\.gz|\\.zstd|)", KRegex::EscapeText(sBasename)));

			std::sort(Rotated.begin(), Rotated.end(), [&sBasename](const KDirectory::value_type& left, const KDirectory::value_type& right)
			{
				// we sort reversed, as we will have to rename the files in that order
				return GetRotationCount(left.Filename().substr(sBasename.size())) > GetRotationCount(right.Filename().substr(sBasename.size()));
			});
		}
		else
		{
			// "YYYY-MM-DDThh:mm:ssZ"
			Rotated.Match(kFormat("{}\\.[0-9]{{4}}-[0-9]{{2}}-[0-9]{{2}}T[0-9]{{2}}:[0-9]{{2}}:[0-9]{{2}}Z(\\.gz|\\.zstd|)", KRegex::EscapeText(sBasename)));
		}

		bool bCheckAge { true };

		for (auto& Rotate : Rotated)
		{
			bool bDiscard { false };

			if (!conf.bUseTimestamps)
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

					if (sSuffix == "zstd" || sSuffix == "gz")
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
				constexpr KStringView sTimestampMask("YYYY-MM-DDThh:mm:ssZ");
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
	if (!conf.bUseTimestamps)
	{
		sTempfile = kFormat("{}.1", conf.sLogFilename);
	}
	else
	{
		sTempfile = kFormat("{}.{:%Y-%m-%dT%H:%M:%SZ}", conf.sLogFilename, tNow);
	}

	if (conf.iKeepSize > 0 || conf.bCopyTrunc)
	{
		// keep the newest records in the old place, write the older ones into the temp location
		KFile LogFile(conf.sLogFilename);
		// do not trim!
		LogFile.SetReaderTrim("", "");

		if (!LogFile.is_open())
		{
			kDebug(1, "cannot open log file: {}", conf.sLogFilename);
			return {};
		}

		KOutFile OutFile(sTempfile);

		if (!OutFile.is_open())
		{
			kDebug(1, "cannot create tempfile: {}", sTempfile);
			return {};
		}

		if (File.Size() <= conf.iKeepSize)
		{
			kDebug(1, "cannot rotate, file size {} <= {}", File.Size(), conf.iKeepSize);
			return {};
		}

		auto iCopy = File.Size() - conf.iKeepSize;

		OutFile.Write(LogFile, iCopy);

		// now copy until end of current line
		OutFile.Write(LogFile.ReadLine());

		// close the output
		OutFile.close();

		// read the remaining content into a buffer
		auto sBuffer = LogFile.ReadRemaining();

		// truncate the file early
		kResizeFile(conf.sLogFilename, sBuffer.size());

		if (!LogFile.KOutFile::Rewind())
		{
			kDebug(1, "cannot seek to start: {}", conf.sLogFilename);
			return {};
		}

		// now write the rest of the logfile to its own start
		LogFile.Write(sBuffer);
		// and close it
		LogFile.close();
	}
	else
	{
		if (!kRename(conf.sLogFilename, sTempfile))
		{
			kDebug(1, "cannot rename file {} into temporary: {}", conf.sLogFilename, kBasename(sTempfile));
			return {};
		}
		// and make sure the log file is existing again
		kTouchFile(conf.sLogFilename);
	}

	if (!conf.bCompress)
	{
		return sTempfile;
	}

	return Compress(sTempfile, true);

} // Rotate

//-----------------------------------------------------------------------------
KString KLogRotate::Compress(KStringViewZ sUncompressedName, bool bRemoveAfterCompression)
//-----------------------------------------------------------------------------
{
#if DEKAF2_HAS_LIBZSTD
	auto sCompfile = kFormat("{}.zstd", sUncompressedName);
#else
	auto sCompfile = kFormat("{}.gz", sUncompressedName);
#endif

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
			iCount *= 10;
			iCount += sSuffixes.front() - '0';
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
