#include "catch.hpp"

#include <dekaf2/core/logging/klogrotate.h>
#include <dekaf2/system/filesystem/kfilesystem.h>
#include <dekaf2/io/readwrite/kwriter.h>
#include <dekaf2/io/readwrite/kreader.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/format/kformat.h>
#include <dekaf2/time/clock/ktime.h>

using namespace dekaf2;

TEST_CASE("KLogRotate")
{
	KTempDir TempDir;
	KString sDir = TempDir.Name();

	SECTION("Add with empty filename fails")
	{
		KLogRotate Rotator;
		KLogRotate::Config conf;
		CHECK ( Rotator.Add(conf) == false );
	}

	SECTION("Add with valid config succeeds")
	{
		KLogRotate Rotator;
		KLogRotate::Config conf;
		conf.sLogFilename = sDir + "/test.log";
		conf.iMaxSize     = 100;
		conf.CheckEvery   = chrono::hours(24);
		kWriteFile(conf.sLogFilename, "hello");
		CHECK ( Rotator.Add(conf) == true );
	}

	SECTION("Remove unknown file returns false")
	{
		KLogRotate Rotator;
		CHECK ( Rotator.Remove("/nonexistent.log") == false );
	}

	SECTION("Remove known file returns true")
	{
		KLogRotate Rotator;
		KLogRotate::Config conf;
		conf.sLogFilename = sDir + "/test.log";
		conf.iMaxSize     = 100;
		conf.CheckEvery   = chrono::hours(24);
		kWriteFile(conf.sLogFilename, "hello");
		CHECK ( Rotator.Add(conf) == true );
		CHECK ( Rotator.Remove(conf.sLogFilename) == true );
		CHECK ( Rotator.Remove(conf.sLogFilename) == false );
	}

	SECTION("ForceRotate unknown file returns false")
	{
		KLogRotate Rotator;
		CHECK ( Rotator.ForceRotate("/nonexistent.log") == false );
	}

	SECTION("Re-Add same file updates config")
	{
		KLogRotate Rotator;
		KLogRotate::Config conf;
		conf.sLogFilename = sDir + "/test.log";
		conf.iMaxSize     = 100;
		conf.CheckEvery   = chrono::hours(24);
		kWriteFile(conf.sLogFilename, "hello");
		CHECK ( Rotator.Add(conf) == true );

		conf.iMaxSize = 200;
		CHECK ( Rotator.Add(conf) == true );
		CHECK ( Rotator.Remove(conf.sLogFilename) == true );
	}

	SECTION("Rename rotation by size")
	{
		KLogRotate Rotator;
		KLogRotate::Config conf;
		conf.sLogFilename = sDir + "/test.log";
		conf.iMaxSize     = 50;
		conf.iKeepSize    = 0;
		conf.bCompress    = false;
		conf.CheckEvery   = chrono::hours(24);

		// write enough data to exceed iMaxSize
		KString sData(100, 'X');
		kWriteFile(conf.sLogFilename, sData);
		CHECK ( kFileSize(conf.sLogFilename) == 100 );

		CHECK ( Rotator.Add(conf) == true );
		CHECK ( Rotator.ForceRotate(conf.sLogFilename) == true );

		// original file should be recreated (empty)
		CHECK ( kFileExists(conf.sLogFilename) == true );
		CHECK ( kFileSize(conf.sLogFilename) == 0 );

		// rotated file should exist
		KString sRotated = conf.sLogFilename + ".1";
		CHECK ( kFileExists(sRotated) == true );
		CHECK ( kFileSize(sRotated) == 100 );
	}

	SECTION("No rotation when file is below iMaxSize")
	{
		KLogRotate Rotator;
		KLogRotate::Config conf;
		conf.sLogFilename = sDir + "/small.log";
		conf.iMaxSize     = 1000;
		conf.iKeepSize    = 0;
		conf.bCompress    = false;
		conf.CheckEvery   = chrono::hours(24);

		kWriteFile(conf.sLogFilename, "tiny");

		CHECK ( Rotator.Add(conf) == true );
		CHECK ( Rotator.ForceRotate(conf.sLogFilename) == true );

		// no rotation should have happened
		CHECK ( kFileExists(conf.sLogFilename) == true );
		CHECK ( kFileSize(conf.sLogFilename) == 4 );
		CHECK ( kFileExists(conf.sLogFilename + ".1") == false );
	}

	SECTION("CopyTrunc rotation with iKeepSize")
	{
		KLogRotate Rotator;
		KLogRotate::Config conf;
		conf.sLogFilename = sDir + "/copytrunc.log";
		conf.iMaxSize     = 50;
		conf.iKeepSize    = 20;
		conf.bCompress    = false;
		conf.CheckEvery   = chrono::hours(24);

		// write lines to easily verify kept content
		KString sData;
		for (int i = 0; i < 20; ++i)
		{
			sData += kFormat("line {:02d}\n", i);
		}
		kWriteFile(conf.sLogFilename, sData);
		auto iOrigSize = kFileSize(conf.sLogFilename);
		CHECK ( iOrigSize > 50 );

		CHECK ( Rotator.Add(conf) == true );
		CHECK ( Rotator.ForceRotate(conf.sLogFilename) == true );

		// rotated file should exist with old content
		KString sRotated = conf.sLogFilename + ".1";
		CHECK ( kFileExists(sRotated) == true );

		// original file should still exist (same inode) with kept portion
		CHECK ( kFileExists(conf.sLogFilename) == true );
		auto iKeptSize = kFileSize(conf.sLogFilename);
		CHECK ( iKeptSize > 0 );
		CHECK ( iKeptSize < iOrigSize );

		// rotated size + kept size >= original size (due to line boundary rounding)
		auto iRotatedSize = kFileSize(sRotated);
		CHECK ( iRotatedSize + iKeptSize >= iOrigSize );
	}

	SECTION("CopyTrunc with bCopyTrunc and iKeepSize 0")
	{
		KLogRotate Rotator;
		KLogRotate::Config conf;
		conf.sLogFilename = sDir + "/copytrunc0.log";
		conf.iMaxSize     = 50;
		conf.iKeepSize    = 0;
		conf.bCopyTrunc   = true;
		conf.bCompress    = false;
		conf.CheckEvery   = chrono::hours(24);

		KString sData(100, 'Y');
		kWriteFile(conf.sLogFilename, sData);

		CHECK ( Rotator.Add(conf) == true );
		CHECK ( Rotator.ForceRotate(conf.sLogFilename) == true );

		// original file should be truncated to 0 (inode preserved)
		CHECK ( kFileExists(conf.sLogFilename) == true );
		CHECK ( kFileSize(conf.sLogFilename) == 0 );

		// rotated file should have all old content
		KString sRotated = conf.sLogFilename + ".1";
		CHECK ( kFileExists(sRotated) == true );
		CHECK ( kFileSize(sRotated) == 100 );
	}

	SECTION("Multiple rotations bump counter")
	{
		KLogRotate Rotator;
		KLogRotate::Config conf;
		conf.sLogFilename = sDir + "/multi.log";
		conf.iMaxSize     = 50;
		conf.iKeepSize    = 0;
		conf.bCompress    = false;
		conf.CheckEvery   = chrono::hours(24);

		CHECK ( Rotator.Add(conf) == true );

		// first rotation
		kWriteFile(conf.sLogFilename, KString(100, 'A'));
		CHECK ( Rotator.ForceRotate(conf.sLogFilename) == true );
		CHECK ( kFileExists(conf.sLogFilename + ".1") == true );

		// second rotation — existing .1 should become .2
		kWriteFile(conf.sLogFilename, KString(100, 'B'));
		CHECK ( Rotator.ForceRotate(conf.sLogFilename) == true );
		CHECK ( kFileExists(conf.sLogFilename + ".1") == true );
		CHECK ( kFileExists(conf.sLogFilename + ".2") == true );

		// verify content
		CHECK ( kReadAll(conf.sLogFilename + ".1") == KString(100, 'B') );
		CHECK ( kReadAll(conf.sLogFilename + ".2") == KString(100, 'A') );
	}

	SECTION("Rotation with compression")
	{
		KLogRotate Rotator;
		KLogRotate::Config conf;
		conf.sLogFilename = sDir + "/compressed.log";
		conf.iMaxSize     = 50;
		conf.iKeepSize    = 0;
		conf.bCompress    = true;
		conf.CheckEvery   = chrono::hours(24);

		kWriteFile(conf.sLogFilename, KString(100, 'Z'));

		CHECK ( Rotator.Add(conf) == true );
		CHECK ( Rotator.ForceRotate(conf.sLogFilename) == true );

		// should have compressed rotation
#if DEKAF2_HAS_LIBZSTD
		CHECK ( kFileExists(conf.sLogFilename + ".1.zst") == true );
#else
		CHECK ( kFileExists(conf.sLogFilename + ".1.gz") == true );
#endif
		// uncompressed rotation should not exist
		CHECK ( kFileExists(conf.sLogFilename + ".1") == false );
	}

	SECTION("BeforeRotationCallback can stop rotation")
	{
		KLogRotate Rotator;
		KLogRotate::Config conf;
		conf.sLogFilename = sDir + "/callback.log";
		conf.iMaxSize     = 50;
		conf.iKeepSize    = 0;
		conf.bCompress    = false;
		conf.CheckEvery   = chrono::hours(24);
		conf.BeforeRotationCallback = [](KStringViewZ) { return false; };

		kWriteFile(conf.sLogFilename, KString(100, 'X'));

		CHECK ( Rotator.Add(conf) == true );
		CHECK ( Rotator.ForceRotate(conf.sLogFilename) == true );

		// rotation should NOT have happened
		CHECK ( kFileSize(conf.sLogFilename) == 100 );
		CHECK ( kFileExists(conf.sLogFilename + ".1") == false );
	}

	SECTION("AfterRotationCallback receives rotated filename")
	{
		KString sRotatedFile;

		KLogRotate Rotator;
		KLogRotate::Config conf;
		conf.sLogFilename = sDir + "/aftercb.log";
		conf.iMaxSize     = 50;
		conf.iKeepSize    = 0;
		conf.bCompress    = false;
		conf.CheckEvery   = chrono::hours(24);
		conf.AfterRotationCallback = [&sRotatedFile](KStringViewZ sFile)
		{
			sRotatedFile = sFile;
		};

		kWriteFile(conf.sLogFilename, KString(100, 'X'));

		CHECK ( Rotator.Add(conf) == true );
		CHECK ( Rotator.ForceRotate(conf.sLogFilename) == true );

		CHECK ( sRotatedFile == conf.sLogFilename + ".1" );
		CHECK ( kFileExists(sRotatedFile) == true );
	}

	SECTION("Rotation with timestamps")
	{
		KLogRotate Rotator;
		KLogRotate::Config conf;
		conf.sLogFilename  = sDir + "/timestamp.log";
		conf.iMaxSize      = 50;
		conf.iKeepSize     = 0;
		conf.bCompress     = false;
		conf.RotationNaming = KLogRotate::Config::Timestamp;
		conf.CheckEvery    = chrono::hours(24);

		kWriteFile(conf.sLogFilename, KString(100, 'T'));

		CHECK ( Rotator.Add(conf) == true );
		CHECK ( Rotator.ForceRotate(conf.sLogFilename) == true );

		// should not have .1 file
		CHECK ( kFileExists(conf.sLogFilename + ".1") == false );

		// should have a timestamp-named file
		KDirectory Dir(sDir);
		Dir.Match("timestamp\\.log\\.[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}-[0-9]{2}-[0-9]{2}Z");
		CHECK ( Dir.size() == 1 );
	}

	SECTION("Rotation count limits old files")
	{
		KLogRotate Rotator;
		KLogRotate::Config conf;
		conf.sLogFilename = sDir + "/limited.log";
		conf.iMaxSize     = 50;
		conf.iKeepSize    = 0;
		conf.iRotations   = 2;
		conf.bCompress    = false;
		conf.CheckEvery   = chrono::hours(24);

		CHECK ( Rotator.Add(conf) == true );

		// rotate 3 times
		for (int i = 0; i < 3; ++i)
		{
			kWriteFile(conf.sLogFilename, KString(100, static_cast<char>('A' + i)));
			CHECK ( Rotator.ForceRotate(conf.sLogFilename) == true );
		}

		// .1 and .2 should exist, .3 should have been discarded
		CHECK ( kFileExists(conf.sLogFilename + ".1") == true );
		CHECK ( kFileExists(conf.sLogFilename + ".2") == true );
		CHECK ( kFileExists(conf.sLogFilename + ".3") == false );
	}

	SECTION("the newest rotated files stay uncompressed")
	{
		KLogRotate Rotator;
		KLogRotate::Config conf;
		conf.sLogFilename      = sDir + "/uncompressed.log";
		conf.iMaxSize          = 50;
		conf.iKeepUncompressed = 1;
		conf.CompressWith      = KLogRotate::Config::GZip;
		conf.CheckEvery        = chrono::hours(24);

		CHECK ( Rotator.Add(conf) == true );

		for (int i = 0; i < 3; ++i)
		{
			kWriteFile(conf.sLogFilename, KString(100, static_cast<char>('A' + i)));
			CHECK ( Rotator.ForceRotate(conf.sLogFilename) == true );
		}

		CHECK ( kReadAll(conf.sLogFilename + ".1") == KString(100, 'C') );
		CHECK ( kFileExists(conf.sLogFilename + ".2.gz") == true );
		CHECK ( kFileExists(conf.sLogFilename + ".3.gz") == true );
		CHECK ( kFileExists(conf.sLogFilename + ".2")    == false );
	}

	SECTION("a file that its writer cuts needs Date naming")
	{
		KLogRotate Rotator;
		KLogRotate::Config conf;
		conf.sLogFilename = sDir + "/cut.log";
		conf.bWriterCuts  = true;
		CHECK ( Rotator.Add(conf) == false );
	}

	// the date of the day iDaysAgo days ago, in UTC
	auto Date = [](int iDaysAgo, KDatedLogFile::Interval Period = KDatedLogFile::Day)
	{
		return detail::KDatedLogNames::GetSpan(kNow() - chrono::hours(24 * iDaysAgo), Period, nullptr).sDate;
	};

	SECTION("the past files of a dated log file that its writer cuts")
	{
		KLogRotate Rotator;
		KLogRotate::Config conf;
		conf.sLogFilename      = sDir + "/dated.log";
		conf.RotationNaming    = KLogRotate::Config::Date;
		conf.bWriterCuts       = true;
		conf.iKeepUncompressed = 1;
		conf.CompressWith      = KLogRotate::Config::GZip;
		conf.MaxAge            = chrono::hours(24 * 30);

		auto sYesterday = kFormat("{}-{}", conf.sLogFilename, Date(1));
		auto sTwoDays   = kFormat("{}-{}", conf.sLogFilename, Date(2));
		// a file of an earlier logrotate setup, older than MaxAge
		auto sOld       = kFormat("{}-{}.gz", conf.sLogFilename, Date(40));
		auto sOther     = kFormat("{}-backup", conf.sLogFilename);

		for (const auto& sFile : { conf.sLogFilename, sYesterday, sTwoDays, sOld, sOther })
		{
			REQUIRE ( kWriteFile(sFile, "content\n") );
		}

		CHECK ( Rotator.Add(conf) == true );
		CHECK ( Rotator.ForceRotate(conf.sLogFilename) == true );

		// the writer cuts the current file, not KLogRotate
		CHECK ( kReadAll(conf.sLogFilename) == "content\n" );
		// the newest past file stays uncompressed, the older ones are compressed
		CHECK ( kReadAll(sYesterday) == "content\n" );
		CHECK ( kFileExists(sTwoDays + ".gz") == true  );
		CHECK ( kFileExists(sTwoDays)         == false );
		CHECK ( kFileExists(sOld)             == false );
		CHECK ( kReadAll(sOther) == "content\n" );
	}

	SECTION("MaxAge counts from the end of the period of a file")
	{
		KLogRotate Rotator;
		KLogRotate::Config conf;
		conf.sLogFilename   = sDir + "/weekly.log";
		conf.RotationNaming = KLogRotate::Config::Date;
		conf.bWriterCuts    = true;
		conf.bCompress      = false;
		conf.MaxAge         = chrono::hours(24 * 30);

		// the week of 40 days ago ended at least 33 days ago, the one of 20 days ago at
		// most 20 days ago
		auto sExpired = kFormat("{}-{}", conf.sLogFilename, Date(40, KDatedLogFile::Week));
		auto sKept    = kFormat("{}-{}", conf.sLogFilename, Date(20, KDatedLogFile::Week));

		REQUIRE ( kWriteFile(sExpired, "content\n") );
		REQUIRE ( kWriteFile(sKept   , "content\n") );

		CHECK ( Rotator.Add(conf) == true );
		CHECK ( Rotator.ForceRotate(conf.sLogFilename) == true );

		CHECK ( kFileExists(sExpired) == false );
		CHECK ( kFileExists(sKept)    == true  );
	}

	SECTION("a dated log file that KLogRotate cuts")
	{
		KLogRotate Rotator;
		KLogRotate::Config conf;
		conf.sLogFilename   = sDir + "/foreign.log";
		conf.RotationNaming = KLogRotate::Config::Date;
		conf.bCompress      = false;
		conf.CheckEvery     = chrono::hours(24);

		// the last cut was yesterday
		auto sStamp = kFormat("{}.lastrotation", conf.sLogFilename);
		REQUIRE ( kWriteFile(sStamp, "") );
		REQUIRE ( kSetLastMod(sStamp, kNow() - chrono::hours(24)) );
		REQUIRE ( kWriteFile(conf.sLogFilename, "content of yesterday\n") );

		CHECK ( Rotator.Add(conf) == true );
		CHECK ( Rotator.ForceRotate(conf.sLogFilename) == true );

		CHECK ( kReadAll(kFormat("{}-{}", conf.sLogFilename, Date(1))) == "content of yesterday\n" );
		CHECK ( kFileSize(conf.sLogFilename) == 0 );

		// a second check within the same period does not cut again
		REQUIRE ( kWriteFile(conf.sLogFilename, "content of today\n") );
		CHECK ( Rotator.ForceRotate(conf.sLogFilename) == true );
		CHECK ( kReadAll(conf.sLogFilename) == "content of today\n" );
	}

	SECTION("the time of the work lies in the work window")
	{
		KLogRotate::Config conf;
		conf.sLogFilename   = "/var/log/app/access.log";
		conf.RotationNaming = KLogRotate::Config::Date;

		auto Day29 = KUnixTime(chrono::system_clock::time_point(chrono::sys_days(chrono::year(2026) / chrono::month(10) / chrono::day(29))));

		// after the window of the 29th, the next run is in the window of the 30th, between
		// 02:00 and 05:00 UTC
		auto tWork = KLogRotate::GetNextWorkTime(conf, Day29 + chrono::hours(12));
		CHECK ( tWork >= Day29 + chrono::hours(24 + 2) );
		CHECK ( tWork <  Day29 + chrono::hours(24 + 5) );
		// the time within the window is fixed for a file
		CHECK ( KLogRotate::GetNextWorkTime(conf, Day29 + chrono::hours(13)) == tWork );
		// before the window, the next run is on the same day
		CHECK ( KLogRotate::GetNextWorkTime(conf, Day29 + chrono::hours(1))  == tWork - chrono::hours(24) );

#if DEKAF2_HAS_TIMEZONES
		auto Zone = kFindTimezone("Europe/Berlin", false);

		if (Zone != nullptr)
		{
			// in Berlin the 30th starts at 23:00 UTC on the 29th, after the end of summer time
			conf.TimeZone = Zone;
			CHECK ( KLogRotate::GetNextWorkTime(conf, Day29 + chrono::hours(12)) == tWork - chrono::hours(1) );
		}
#endif
	}
}
