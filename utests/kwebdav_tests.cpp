#include "catch.hpp"
#include <dekaf2/kwebdav.h>
#include <dekaf2/kfilesystem.h>
#include <dekaf2/khttp_method.h>
#include <dekaf2/kwebserverpermissions.h>
#include <dekaf2/khttperror.h>
#include <dekaf2/kstring.h>

using namespace dekaf2;

TEST_CASE("KWebDAV")
{
	SECTION("GenerateETag")
	{
		// create a temp file and stat it
		KTempFile<> TempFile(".txt");
		*TempFile << "hello world";
		TempFile.Close();

		KFileStat Stat(TempFile.Name());
		REQUIRE ( Stat.IsFile() );

		auto sETag = KWebDAV::GenerateETag(Stat);
		// ETag format: "<inode>-<mtime>-<size>" in hex, quoted
		CHECK ( sETag.front() == '"' );
		CHECK ( sETag.back()  == '"' );
		// must contain two dashes inside the quotes
		auto iDashes = std::count(sETag.begin(), sETag.end(), '-');
		CHECK ( iDashes == 2 );
	}

	SECTION("ResolveFilesystemPath")
	{
		// route "/*" gets stripped to "" by KHTTPAnalyzedPath
		// so sRoute passed to ResolveFilesystemPath is ""
		auto sPath = KWebDAV::ResolveFilesystemPath("/var/www", "/files/test.txt", "");
		CHECK ( sPath == "/var/www/files/test.txt" );

		// route "/dav/*" gets stripped to "/dav"
		sPath = KWebDAV::ResolveFilesystemPath("/var/www", "/dav/test.txt", "/dav");
		CHECK ( sPath == "/var/www/test.txt" );

		// root path with empty relative
		sPath = KWebDAV::ResolveFilesystemPath("/var/www", "/dav", "/dav");
		CHECK ( sPath == "/var/www" );

		// path traversal should throw
		CHECK_THROWS_AS (
			KWebDAV::ResolveFilesystemPath("/var/www", "/../etc/passwd", ""),
			const KHTTPError&
		);
	}

	SECTION("MethodToPermission for WebDAV methods")
	{
		CHECK ( KWebServerPermissions::MethodToPermission(KHTTPMethod::PROPFIND) == KWebServerPermissions::Read );
		CHECK ( KWebServerPermissions::MethodToPermission(KHTTPMethod::MKCOL)    == KWebServerPermissions::Write );
		CHECK ( KWebServerPermissions::MethodToPermission(KHTTPMethod::COPY)     == KWebServerPermissions::Write );
		CHECK ( KWebServerPermissions::MethodToPermission(KHTTPMethod::MOVE)     == (KWebServerPermissions::Write | KWebServerPermissions::Erase) );
		CHECK ( KWebServerPermissions::MethodToPermission(KHTTPMethod::OPTIONS)  == KWebServerPermissions::Read );
	}

	SECTION("KHTTPMethod WebDAV methods parse and serialize")
	{
		CHECK ( KHTTPMethod("PROPFIND") == KHTTPMethod::PROPFIND );
		CHECK ( KHTTPMethod("MKCOL")    == KHTTPMethod::MKCOL );
		CHECK ( KHTTPMethod("COPY")     == KHTTPMethod::COPY );
		CHECK ( KHTTPMethod("MOVE")     == KHTTPMethod::MOVE );

		CHECK ( KHTTPMethod(KHTTPMethod::PROPFIND).Serialize() == "PROPFIND" );
		CHECK ( KHTTPMethod(KHTTPMethod::MKCOL).Serialize()    == "MKCOL" );
		CHECK ( KHTTPMethod(KHTTPMethod::COPY).Serialize()     == "COPY" );
		CHECK ( KHTTPMethod(KHTTPMethod::MOVE).Serialize()     == "MOVE" );
	}
}
