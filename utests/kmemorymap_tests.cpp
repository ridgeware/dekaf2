#include "catch.hpp"

#include <dekaf2/io/readwrite/kmemorymap.h>
#include <dekaf2/system/filesystem/kfilesystem.h>
#include <dekaf2/io/readwrite/kreader.h>
#include <dekaf2/core/strings/kstring.h>
#include <utility>
#include <cstdint>

using namespace dekaf2;

TEST_CASE("KMemoryMap")
{
	KTempDir TempDir;
	KString  sDir = TempDir.Name();
	REQUIRE( !sDir.empty() );

	auto MakeFile = [&](KStringView sName, KStringView sContent) -> KString
	{
		KString sPath = sDir;
		sPath += '/';
		sPath += sName;
		REQUIRE( kWriteFile (sPath, sContent) );
		return sPath;
	};

	SECTION("read-only mapping of a file")
	{
		KString sContent = "Hello, memory-mapped world!\nsecond line\n";
		KString sPath    = MakeFile ("ro.bin", sContent);

		KConstMemoryMap Map (sPath);
		REQUIRE( Map.is_open() );
		CHECK ( Map.size() == sContent.size() );
		CHECK ( Map.ToView() == sContent );
		CHECK ( Map.ToConstBuffer().size() == sContent.size() );
		CHECK ( KStringView (Map.data(), Map.size()) == sContent );
	}

	SECTION("read/write shared mapping persists to disk")
	{
		KString sContent = "AAAAAAAAAA"; // 10 bytes
		KString sPath    = MakeFile ("rw.bin", sContent);

		{
			KMemoryMap Map (sPath); // Sharing::Shared by default
			REQUIRE( Map.is_open() );

			auto Buf = Map.Buffer();
			REQUIRE( Buf.size() == sContent.size() );

			Buf.CharData()[0]  = 'Z';
			Buf.UInt8Data()[1] = static_cast<uint8_t>('Y');

			CHECK( Map.Sync() );
		} // Close() flushes and unmaps here

		CHECK( kReadAll (sPath) == "ZYAAAAAAAA" );
	}

	SECTION("copy-on-write mapping leaves the file unchanged")
	{
		KString sContent = "0123456789";
		KString sPath    = MakeFile ("cow.bin", sContent);

		{
			KMemoryMap Map (sPath, KMemoryMap::Sharing::Private);
			REQUIRE( Map.is_open() );

			Map.Buffer().CharData()[0] = 'X';
			CHECK( Map.data()[0] == 'X' );  // visible in our private copy
			CHECK( Map.Sync() );            // no-op for copy-on-write
		}

		CHECK( kReadAll (sPath) == sContent ); // the file on disk is unchanged
	}

	SECTION("anonymous shared mapping")
	{
		auto Map = KMemoryMap::Anonymous (4096);
		REQUIRE( Map.is_open() );
		CHECK ( Map.size() == 4096 );
		CHECK ( Map.data()[0] == 0 ); // anonymous memory is zero initialised

		auto Buf = Map.Buffer();
		REQUIRE( Buf.size() == 4096 );

		for (std::size_t ii = 0; ii < 256; ++ii)
		{
			Buf.UInt8Data()[ii] = static_cast<uint8_t>(ii);
		}

		for (std::size_t ii = 0; ii < 256; ++ii)
		{
			CHECK( static_cast<uint8_t>(Map.data()[ii]) == static_cast<uint8_t>(ii) );
		}
	}

	SECTION("anonymous private mapping")
	{
		auto Map = KMemoryMap::Anonymous (1024, KMemoryMap::Sharing::Private);
		REQUIRE( Map.is_open() );
		CHECK ( Map.size() == 1024 );

		Map.Buffer().CharData()[0] = 'Q';
		CHECK( Map.data()[0] == 'Q' );
	}

	SECTION("opening a missing file fails and reports an error")
	{
		KConstMemoryMap Map;
		CHECK_FALSE( Map.Open (sDir + "/does-not-exist") );
		CHECK_FALSE( Map.is_open() );
		CHECK      ( Map.HasError() );

		KMemoryMap WMap;
		CHECK_FALSE( WMap.Open (sDir + "/also-missing") );
		CHECK_FALSE( WMap.is_open() );
		CHECK      ( WMap.HasError() );
	}

	SECTION("an anonymous mapping of size 0 is rejected")
	{
		auto Map = KMemoryMap::Anonymous (0);
		CHECK_FALSE( Map.is_open() );
		CHECK      ( Map.HasError() );
	}

	SECTION("move transfers ownership and leaves the source closed")
	{
		KString sContent = "movable mapped content";
		KString sPath    = MakeFile ("move.bin", sContent);

		KConstMemoryMap A (sPath);
		REQUIRE( A.is_open() );

		KConstMemoryMap B (std::move(A));
		CHECK      ( B.is_open() );
		CHECK      ( B.ToView() == sContent );
		CHECK_FALSE( A.is_open() );
	}
}
