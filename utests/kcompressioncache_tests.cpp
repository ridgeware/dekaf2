#include "catch.hpp"

#include <dekaf2/rest/serving/kcompressioncache.h>
#include <dekaf2/io/compression/kcompression.h>
#include <dekaf2/io/streams/koutstringstream.h>
#include <dekaf2/system/filesystem/kfilesystem.h>
#include <future>
#include <random>
#include <sstream>
#include <thread>
#include <vector>

using namespace dekaf2;

namespace {

//-----------------------------------------------------------------------------
KString CompressibleText(std::size_t iLines)
//-----------------------------------------------------------------------------
{
	KString sText;

	for (std::size_t i = 0; i < iLines; ++i)
	{
		sText += kFormat("line {} of a text that compresses well\n", i);
	}

	return sText;

} // CompressibleText

//-----------------------------------------------------------------------------
KString RandomBytes(std::size_t iSize)
//-----------------------------------------------------------------------------
{
	std::mt19937 Random(42);
	KString sBytes;
	sBytes.reserve(iSize);

	for (std::size_t i = 0; i < iSize; ++i)
	{
		sBytes += static_cast<char>(Random() & 0xff);
	}

	return sBytes;

} // RandomBytes

//-----------------------------------------------------------------------------
KString Uncompress(KStringView sData, KHTTPCompression::COMP Compression)
//-----------------------------------------------------------------------------
{
	KUnCompressIStream::COMPRESSION Method { KUnCompressIStream::GZIP };

	switch (Compression)
	{
#ifdef DEKAF2_HAS_LIBZSTD
		case KHTTPCompression::ZSTD:
			Method = KUnCompressIStream::ZSTD;
			break;
#endif
#ifdef DEKAF2_HAS_LIBBROTLI
		case KHTTPCompression::BROTLI:
			Method = KUnCompressIStream::BROTLI;
			break;
#endif
		default:
			break;
	}

	KUnCompress UnCompress(sData, Method);

	return UnCompress.ReadAll();

} // Uncompress

//-----------------------------------------------------------------------------
std::vector<KHTTPCompression::COMP> Compressors()
//-----------------------------------------------------------------------------
{
	return
	{
		KHTTPCompression::GZIP,
#ifdef DEKAF2_HAS_LIBZSTD
		KHTTPCompression::ZSTD,
#endif
#ifdef DEKAF2_HAS_LIBBROTLI
		KHTTPCompression::BROTLI,
#endif
	};

} // Compressors

} // end of anonymous namespace

TEST_CASE("KCompressionCache")
{
	KTempDir DocumentRoot;
	KTempDir CacheDirectory;

	KCompressionCache Cache(CacheDirectory.Name());

	auto sRoot   = DocumentRoot.Name();
	auto sSource = kFormat("{}/js/app.js", sRoot);
	auto sText   = CompressibleText(2000);

	REQUIRE ( kCreateDir(kFormat("{}/js/lib", sRoot)) );
	REQUIRE ( kWriteFile(sSource, sText) );

	SECTION("supported compressors")
	{
		auto Supported = KCompressionCache::GetSupportedCompressors();
		CHECK ( (Supported & KHTTPCompression::GZIP) != 0 );
		CHECK ( (Supported & KHTTPCompression::ZLIB) == 0 );
	}

	SECTION("Get compresses a missing entry, Lookup finds it")
	{
		for (auto Compression : Compressors())
		{
			INFO ( KHTTPCompression::ToString(Compression) );

			KFileStat Stat(sSource);

			auto Entry = Cache.Lookup(sRoot, "js/app.js", Stat, Compression);
			CHECK ( Entry.Status == KCompressionCache::State::Miss );

			Entry = Cache.Get(sRoot, "js/app.js", Stat, Compression, chrono::seconds(10), nullptr);
			REQUIRE ( Entry.Status == KCompressionCache::State::Hit );
			CHECK ( Entry.iSize == kFileSize(Entry.sPath) );
			CHECK ( Entry.iSize < sText.size() );
			CHECK ( Uncompress(kReadAll(Entry.sPath), Compression) == sText );

			auto Found = Cache.Lookup(sRoot, "/js/app.js", Stat, Compression);
			CHECK ( Found.Status == KCompressionCache::State::Hit );
			CHECK ( Found.sPath  == Entry.sPath );
			CHECK ( Found.iSize  == Entry.iSize );
		}
	}

	SECTION("a changed source file gets a new entry, the old one is removed")
	{
		auto Entry = Cache.Get(sRoot, "js/app.js", KFileStat(sSource), KHTTPCompression::GZIP, chrono::seconds(10), nullptr);
		REQUIRE ( Entry.Status == KCompressionCache::State::Hit );

		auto sChanged = CompressibleText(2100);
		REQUIRE ( kWriteFile(sSource, sChanged) );

		KFileStat Stat(sSource);
		CHECK ( Cache.Lookup(sRoot, "js/app.js", Stat, KHTTPCompression::GZIP).Status == KCompressionCache::State::Miss );

		auto NewEntry = Cache.Get(sRoot, "js/app.js", Stat, KHTTPCompression::GZIP, chrono::seconds(10), nullptr);
		REQUIRE ( NewEntry.Status == KCompressionCache::State::Hit );
		CHECK ( NewEntry.sPath != Entry.sPath );
		CHECK ( kFileExists(Entry.sPath) == false );
		CHECK ( Uncompress(kReadAll(NewEntry.sPath), KHTTPCompression::GZIP) == sChanged );
	}

	SECTION("a file replaced by a rename with the same content and time gets a new entry")
	{
		KFileStat Stat(sSource);
		auto Entry = Cache.Get(sRoot, "js/app.js", Stat, KHTTPCompression::GZIP, chrono::seconds(10), nullptr);
		REQUIRE ( Entry.Status == KCompressionCache::State::Hit );

		// like rsync -a: same content and modification time, but a new file
		auto sOther = kFormat("{}/js/other.js", sRoot);
		REQUIRE ( kWriteFile(sOther, sText) );
		REQUIRE ( kSetLastMod(sOther, Stat.ModificationTime()) );
		REQUIRE ( kRename(sOther, sSource) );

		CHECK ( Cache.Lookup(sRoot, "js/app.js", KFileStat(sSource), KHTTPCompression::GZIP).Status == KCompressionCache::State::Miss );
	}

	SECTION("a file that does not get smaller gets a negative entry")
	{
		auto sBinary = kFormat("{}/random.txt", sRoot);
		REQUIRE ( kWriteFile(sBinary, RandomBytes(64 * 1024)) );

		KFileStat Stat(sBinary);

		auto Entry = Cache.Get(sRoot, "random.txt", Stat, KHTTPCompression::GZIP, chrono::seconds(10), nullptr);
		CHECK ( Entry.Status == KCompressionCache::State::Negative );
		CHECK ( Cache.Lookup(sRoot, "random.txt", Stat, KHTTPCompression::GZIP).Status == KCompressionCache::State::Negative );
	}

	SECTION("the transmission starts after the deadline")
	{
		// larger than one block, so that the transmission can start during the compression
		auto sLarge = kFormat("{}/large.txt", sRoot);
		auto sLargeText = CompressibleText(100000);
		REQUIRE ( kWriteFile(sLarge, sLargeText) );

		for (auto Deadline : { chrono::milliseconds(0), chrono::milliseconds(1) })
		{
			INFO ( "deadline: " << Deadline.count() << " ms" );

			Cache.Forget(sRoot, "large.txt");

			KString          sTransmitted;
			KOutStringStream Response(sTransmitted);
			int              iCalls { 0 };

			auto Entry = Cache.Get(sRoot, "large.txt", KFileStat(sLarge), KHTTPCompression::GZIP, Deadline, [&]() -> KOutStream*
			{
				++iCalls;
				return &Response;
			});

			CHECK ( Entry.Status == KCompressionCache::State::Transmitted );
			CHECK ( iCalls == 1 );
			CHECK ( Uncompress(sTransmitted, KHTTPCompression::GZIP) == sLargeText );

			// the entry was completed during the transmission
			auto Found = Cache.Lookup(sRoot, "large.txt", KFileStat(sLarge), KHTTPCompression::GZIP);
			REQUIRE ( Found.Status == KCompressionCache::State::Hit );
			CHECK ( kReadAll(Found.sPath) == sTransmitted );
		}
	}

	SECTION("before the deadline there is no transmission")
	{
		bool bCalled { false };

		auto Entry = Cache.Get(sRoot, "js/app.js", KFileStat(sSource), KHTTPCompression::GZIP, chrono::seconds(60), [&]() -> KOutStream*
		{
			bCalled = true;
			return nullptr;
		});

		CHECK ( Entry.Status == KCompressionCache::State::Hit );
		CHECK ( bCalled == false );
	}

	SECTION("a transmitter without a stream completes the entry")
	{
		auto Entry = Cache.Get(sRoot, "js/app.js", KFileStat(sSource), KHTTPCompression::GZIP, chrono::seconds(0), []() -> KOutStream*
		{
			return nullptr;
		});

		CHECK ( Entry.Status == KCompressionCache::State::Hit );
	}

	SECTION("a disconnected client does not stop the entry")
	{
		std::ostringstream Failing;
		Failing.setstate(std::ios::badbit);
		KOutStream Response(Failing);

		auto Entry = Cache.Get(sRoot, "js/app.js", KFileStat(sSource), KHTTPCompression::GZIP, chrono::seconds(0), [&]() -> KOutStream*
		{
			return &Response;
		});

		CHECK ( Entry.Status == KCompressionCache::State::Transmitted );

		auto Found = Cache.Lookup(sRoot, "js/app.js", KFileStat(sSource), KHTTPCompression::GZIP);
		REQUIRE ( Found.Status == KCompressionCache::State::Hit );
		CHECK ( Uncompress(kReadAll(Found.sPath), KHTTPCompression::GZIP) == sText );
	}

	SECTION("Forget removes the entries of a file and of a directory")
	{
		auto sOther = kFormat("{}/js/lib/other.js", sRoot);
		REQUIRE ( kWriteFile(sOther, sText) );

		KFileStat Stat(sSource);
		KFileStat OtherStat(sOther);

		CHECK ( Cache.Get(sRoot, "js/app.js",      Stat,      KHTTPCompression::GZIP, chrono::seconds(10), nullptr).Status == KCompressionCache::State::Hit );
		CHECK ( Cache.Get(sRoot, "js/lib/other.js", OtherStat, KHTTPCompression::GZIP, chrono::seconds(10), nullptr).Status == KCompressionCache::State::Hit );

		Cache.Forget(sRoot, "js/lib/other.js");
		CHECK ( Cache.Lookup(sRoot, "js/lib/other.js", OtherStat, KHTTPCompression::GZIP).Status == KCompressionCache::State::Miss );
		CHECK ( Cache.Lookup(sRoot, "js/app.js",       Stat,      KHTTPCompression::GZIP).Status == KCompressionCache::State::Hit  );

		Cache.Forget(sRoot, "js");
		CHECK ( Cache.Lookup(sRoot, "js/app.js", Stat, KHTTPCompression::GZIP).Status == KCompressionCache::State::Miss );
	}

	SECTION("Sweep removes the entries of missing and changed files")
	{
		auto sOther = kFormat("{}/other.txt", sRoot);
		REQUIRE ( kWriteFile(sOther, sText) );

		CHECK ( Cache.Get(sRoot, "js/app.js", KFileStat(sSource), KHTTPCompression::GZIP, chrono::seconds(10), nullptr).Status == KCompressionCache::State::Hit );
		CHECK ( Cache.Get(sRoot, "other.txt", KFileStat(sOther),  KHTTPCompression::GZIP, chrono::seconds(10), nullptr).Status == KCompressionCache::State::Hit );

		// nothing to remove
		CHECK ( Cache.Sweep(sRoot) == 0 );

		REQUIRE ( kWriteFile(sSource, CompressibleText(10)) );
		REQUIRE ( kRemoveFile(sOther) );

		CHECK ( Cache.Sweep(sRoot) == 2 );

		// the cache directory has no files and no directories left
		CHECK ( KDirectory(CacheDirectory.Name(), KFileTypes::ALL, true).empty() );
	}

	SECTION("a second thread waits for the running pass")
	{
		std::promise<void> Started;
		std::promise<void> Release;
		auto ReleaseFuture = Release.get_future().share();

		KString          sTransmitted;
		KOutStringStream Response(sTransmitted);

		// Catch2 assertions are not thread safe, the threads only return their results
		auto First = std::async(std::launch::async, [&]()
		{
			return Cache.Get(sRoot, "js/app.js", KFileStat(sSource), KHTTPCompression::GZIP, chrono::seconds(0), [&]() -> KOutStream*
			{
				// the pass is registered now - block it until the main thread has checked
				Started.set_value();
				ReleaseFuture.wait();
				return &Response;
			});
		});

		Started.get_future().wait();

		// with a transmitter the second request gives up at its deadline
		auto Busy = Cache.Get(sRoot, "js/app.js", KFileStat(sSource), KHTTPCompression::GZIP, chrono::milliseconds(50), []() -> KOutStream* { return nullptr; });
		CHECK ( Busy.Status == KCompressionCache::State::Busy );

		// without a transmitter it waits for the running pass
		auto Waiting = std::async(std::launch::async, [&]()
		{
			return Cache.Get(sRoot, "js/app.js", KFileStat(sSource), KHTTPCompression::GZIP, chrono::seconds(0), nullptr);
		});

		Release.set_value();

		CHECK ( First.get().Status   == KCompressionCache::State::Transmitted );
		CHECK ( Waiting.get().Status == KCompressionCache::State::Hit );
		CHECK ( Uncompress(sTransmitted, KHTTPCompression::GZIP) == sText );
	}

	SECTION("another process that compresses the file holds the lock")
	{
		KFileStat Stat(sSource);

		auto Entry = Cache.Get(sRoot, "js/app.js", Stat, KHTTPCompression::GZIP, chrono::seconds(10), nullptr);
		REQUIRE ( Entry.Status == KCompressionCache::State::Hit );
		REQUIRE ( kRemoveFile(Entry.sPath) );

		auto sLock = Entry.sPath + ".lock";
		REQUIRE ( kTouchFile(sLock) );

		{
			KFileLock Lock(sLock, KFileLock::Exclusive);
			REQUIRE ( static_cast<bool>(Lock) );

			CHECK ( Cache.Get(sRoot, "js/app.js", Stat, KHTTPCompression::GZIP, chrono::seconds(10), nullptr).Status == KCompressionCache::State::Busy );
		}

		CHECK ( Cache.Get(sRoot, "js/app.js", Stat, KHTTPCompression::GZIP, chrono::seconds(10), nullptr).Status == KCompressionCache::State::Hit );
	}

	SECTION("invalid requests")
	{
		KFileStat Stat(sSource);

		CHECK ( Cache.Lookup(sRoot, "../js/app.js",     Stat, KHTTPCompression::GZIP).Status == KCompressionCache::State::Failed );
		CHECK ( Cache.Lookup(sRoot, "js/../../app.js",  Stat, KHTTPCompression::GZIP).Status == KCompressionCache::State::Failed );
		CHECK ( Cache.Lookup(sRoot, "",                 Stat, KHTTPCompression::GZIP).Status == KCompressionCache::State::Failed );
		CHECK ( Cache.Lookup(sRoot, "js/app.js",        Stat, KHTTPCompression::ZLIB).Status == KCompressionCache::State::Failed );
		CHECK ( Cache.Lookup(sRoot, "js",  KFileStat(kFormat("{}/js", sRoot)), KHTTPCompression::GZIP).Status == KCompressionCache::State::Failed );
		CHECK ( Cache.Get(sRoot, "../js/app.js", Stat, KHTTPCompression::GZIP, chrono::seconds(10), nullptr).Status == KCompressionCache::State::Failed );

		// nothing was created outside of the cache directory
		CHECK ( kDirExists(kFormat("{}/../js", CacheDirectory.Name())) == false );
	}
}
