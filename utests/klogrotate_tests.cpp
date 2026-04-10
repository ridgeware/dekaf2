#include "catch.hpp"

#include <dekaf2/core/logging/klogrotate.h>
#include <dekaf2/system/filesystem/kfilesystem.h>
#include <dekaf2/io/readwrite/kwriter.h>
#include <dekaf2/io/readwrite/kreader.h>
#include <dekaf2/core/strings/kstring.h>

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
		CHECK ( kFileExists(conf.sLogFilename + ".1.zstd") == true );
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
		conf.bUseTimestamps = true;
		conf.CheckEvery    = chrono::hours(24);

		kWriteFile(conf.sLogFilename, KString(100, 'T'));

		CHECK ( Rotator.Add(conf) == true );
		CHECK ( Rotator.ForceRotate(conf.sLogFilename) == true );

		// should not have .1 file
		CHECK ( kFileExists(conf.sLogFilename + ".1") == false );

		// should have a timestamp-named file
		KDirectory Dir(sDir);
		Dir.Match("timestamp\\.log\\.[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}Z");
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
}
