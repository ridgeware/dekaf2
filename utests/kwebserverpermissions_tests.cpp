#include "catch.hpp"
#include <dekaf2/rest/serving/kwebserverpermissions.h>
#include <dekaf2/http/protocol/khttp_method.h>

using namespace dekaf2;

TEST_CASE("KWebServerPermissions")
{
	SECTION("ParsePermissions")
	{
		CHECK(KWebServerPermissions::ParsePermissions("none")  == KWebServerPermissions::None);
		CHECK(KWebServerPermissions::ParsePermissions("read")  == KWebServerPermissions::Read);
		CHECK(KWebServerPermissions::ParsePermissions("write") == KWebServerPermissions::Write);
		CHECK(KWebServerPermissions::ParsePermissions("erase") == KWebServerPermissions::Erase);
		CHECK(KWebServerPermissions::ParsePermissions("delete") == KWebServerPermissions::Erase);
		CHECK(KWebServerPermissions::ParsePermissions("browse") == KWebServerPermissions::Browse);
		CHECK(KWebServerPermissions::ParsePermissions("all")   == KWebServerPermissions::All);

		CHECK(KWebServerPermissions::ParsePermissions("read|write") == (KWebServerPermissions::Read | KWebServerPermissions::Write));
		CHECK(KWebServerPermissions::ParsePermissions("read|browse") == (KWebServerPermissions::Read | KWebServerPermissions::Browse));
		CHECK(KWebServerPermissions::ParsePermissions("READ|WRITE") == (KWebServerPermissions::Read | KWebServerPermissions::Write));
		CHECK(KWebServerPermissions::ParsePermissions(" read | write ") == (KWebServerPermissions::Read | KWebServerPermissions::Write));
	}

	SECTION("SerializePermissions")
	{
		CHECK(KWebServerPermissions::SerializePermissions(KWebServerPermissions::None) == "none");
		CHECK(KWebServerPermissions::SerializePermissions(KWebServerPermissions::All)  == "all");
		CHECK(KWebServerPermissions::SerializePermissions(KWebServerPermissions::Read) == "read");
		CHECK(KWebServerPermissions::SerializePermissions(KWebServerPermissions::Read | KWebServerPermissions::Browse) == "read|browse");
	}

	SECTION("MethodToPermission")
	{
		CHECK(KWebServerPermissions::MethodToPermission(KHTTPMethod::GET)     == KWebServerPermissions::Read);
		CHECK(KWebServerPermissions::MethodToPermission(KHTTPMethod::HEAD)    == KWebServerPermissions::Read);
		CHECK(KWebServerPermissions::MethodToPermission(KHTTPMethod::OPTIONS) == KWebServerPermissions::Read);
		CHECK(KWebServerPermissions::MethodToPermission(KHTTPMethod::PUT)     == KWebServerPermissions::Write);
		CHECK(KWebServerPermissions::MethodToPermission(KHTTPMethod::POST)    == KWebServerPermissions::Write);
		CHECK(KWebServerPermissions::MethodToPermission(KHTTPMethod::PATCH)   == KWebServerPermissions::Write);
		CHECK(KWebServerPermissions::MethodToPermission(KHTTPMethod::DELETE)  == KWebServerPermissions::Erase);
	}

	SECTION("DefaultPermissions")
	{
		KWebServerPermissions Perms;

		// default is READ | BROWSE
		CHECK(Perms.GetDefaultPermissions() == (KWebServerPermissions::Read | KWebServerPermissions::Browse));
		CHECK(Perms.IsAllowed("", KHTTPMethod::GET,    "/") == true);
		CHECK(Perms.IsAllowed("", KHTTPMethod::HEAD,   "/") == true);
		CHECK(Perms.IsAllowed("", KHTTPMethod::PUT,    "/") == false);
		CHECK(Perms.IsAllowed("", KHTTPMethod::DELETE, "/") == false);

		Perms.SetDefaultPermissions(KWebServerPermissions::All);
		CHECK(Perms.IsAllowed("", KHTTPMethod::PUT,    "/") == true);
		CHECK(Perms.IsAllowed("", KHTTPMethod::DELETE, "/") == true);
	}

	SECTION("DirectoryPermissions")
	{
		KWebServerPermissions Perms;
		Perms.SetDefaultPermissions(KWebServerPermissions::Read | KWebServerPermissions::Browse);
		Perms.AddDirectoryPermission("/uploads", KWebServerPermissions::All);
		Perms.AddDirectoryPermission("/private", KWebServerPermissions::None);

		// default paths
		CHECK(Perms.IsAllowed("", KHTTPMethod::GET,    "/index.html")        == true);
		CHECK(Perms.IsAllowed("", KHTTPMethod::PUT,    "/index.html")        == false);

		// /uploads allows everything
		CHECK(Perms.IsAllowed("", KHTTPMethod::GET,    "/uploads/file.txt")  == true);
		CHECK(Perms.IsAllowed("", KHTTPMethod::PUT,    "/uploads/file.txt")  == true);
		CHECK(Perms.IsAllowed("", KHTTPMethod::DELETE, "/uploads/file.txt")  == true);

		// /private allows nothing
		CHECK(Perms.IsAllowed("", KHTTPMethod::GET,    "/private/secret.txt") == false);

		// path boundary: /uploadsx should NOT match /uploads
		CHECK(Perms.IsAllowed("", KHTTPMethod::PUT,    "/uploadsx/file.txt") == false);
	}

	SECTION("DirectoryPermissionsLongestMatch")
	{
		KWebServerPermissions Perms;
		Perms.SetDefaultPermissions(KWebServerPermissions::Read | KWebServerPermissions::Browse);
		Perms.AddDirectoryPermission("/data",        KWebServerPermissions::Read | KWebServerPermissions::Write | KWebServerPermissions::Browse);
		Perms.AddDirectoryPermission("/data/locked", KWebServerPermissions::Read | KWebServerPermissions::Browse);

		// /data allows write
		CHECK(Perms.IsAllowed("", KHTTPMethod::PUT, "/data/file.txt")        == true);
		// /data/locked does NOT allow write (more specific match)
		CHECK(Perms.IsAllowed("", KHTTPMethod::PUT, "/data/locked/file.txt") == false);
		CHECK(Perms.IsAllowed("", KHTTPMethod::GET, "/data/locked/file.txt") == true);
	}

	SECTION("UserPermissions")
	{
		KWebServerPermissions Perms;
		Perms.SetDefaultPermissions(KWebServerPermissions::All);

		CHECK(Perms.AddUser("alice:secret:/:read|browse") == true);
		CHECK(Perms.AddUser("alice:secret:/uploads:read|write|browse|erase") == true);
		CHECK(Perms.AddUser("bob:pass123:/:all") == true);

		CHECK(Perms.HasUsers() == true);

		// authentication
		CHECK(Perms.Authenticate("alice", "secret")  == true);
		CHECK(Perms.Authenticate("alice", "wrong")   == false);
		CHECK(Perms.Authenticate("bob",   "pass123") == true);
		CHECK(Perms.Authenticate("eve",   "hack")    == false);

		// alice: read-only on /, read-write on /uploads
		CHECK(Perms.IsAllowed("alice", KHTTPMethod::GET,    "/index.html")       == true);
		CHECK(Perms.IsAllowed("alice", KHTTPMethod::PUT,    "/index.html")       == false);
		CHECK(Perms.IsAllowed("alice", KHTTPMethod::GET,    "/uploads/file.txt") == true);
		CHECK(Perms.IsAllowed("alice", KHTTPMethod::PUT,    "/uploads/file.txt") == true);
		CHECK(Perms.IsAllowed("alice", KHTTPMethod::DELETE, "/uploads/file.txt") == true);

		// bob: all permissions everywhere
		CHECK(Perms.IsAllowed("bob", KHTTPMethod::PUT,    "/index.html")       == true);
		CHECK(Perms.IsAllowed("bob", KHTTPMethod::DELETE, "/anything")         == true);

		// unauthenticated: no access when users are configured
		CHECK(Perms.IsAllowed("", KHTTPMethod::GET, "/index.html") == false);

		// unknown user: no access
		CHECK(Perms.IsAllowed("eve", KHTTPMethod::GET, "/index.html") == false);
	}

	SECTION("IntersectionOfDirAndUserPermissions")
	{
		KWebServerPermissions Perms;
		// directory: /uploads allows write, /docs does not
		Perms.SetDefaultPermissions(KWebServerPermissions::Read | KWebServerPermissions::Browse);
		Perms.AddDirectoryPermission("/uploads", KWebServerPermissions::All);

		// user has write everywhere
		CHECK(Perms.AddUser("alice:secret:/:all") == true);

		// alice can write to /uploads (dir allows, user allows)
		CHECK(Perms.IsAllowed("alice", KHTTPMethod::PUT, "/uploads/file.txt") == true);

		// alice cannot write to /docs (dir does NOT allow write, intersection = no write)
		CHECK(Perms.IsAllowed("alice", KHTTPMethod::PUT, "/docs/file.txt")    == false);

		// alice can read from /docs (both dir and user allow read)
		CHECK(Perms.IsAllowed("alice", KHTTPMethod::GET, "/docs/file.txt")    == true);
	}

	SECTION("AddUserFormatValidation")
	{
		KWebServerPermissions Perms;

		// too few parts
		CHECK(Perms.AddUser("alice") == false);
		CHECK(Perms.AddUser("alice:pass") == false);

		// minimum valid: user:pass:/path (defaults to read|browse)
		CHECK(Perms.AddUser("alice:pass:/") == true);

		// full format
		CHECK(Perms.AddUser("bob:pass:/uploads:read|write|browse") == true);
	}

	SECTION("JSONConfig")
	{
		KJSON jConfig = {
			{ "permissions", "read|browse" },
			{ "directories", {
				{ "/uploads", "read|write|browse|erase" },
				{ "/private", "none" }
			}}
		};

		KWebServerPermissions Perms(jConfig);

		CHECK(Perms.GetDefaultPermissions() == (KWebServerPermissions::Read | KWebServerPermissions::Browse));
		CHECK(Perms.IsAllowed("", KHTTPMethod::GET,    "/index.html")        == true);
		CHECK(Perms.IsAllowed("", KHTTPMethod::PUT,    "/uploads/file.txt")  == true);
		CHECK(Perms.IsAllowed("", KHTTPMethod::DELETE, "/uploads/file.txt")  == true);
		CHECK(Perms.IsAllowed("", KHTTPMethod::GET,    "/private/secret.txt") == false);
	}

	SECTION("Resolve")
	{
		KWebServerPermissions Perms;
		Perms.SetDefaultPermissions(KWebServerPermissions::Read | KWebServerPermissions::Browse);
		Perms.AddDirectoryPermission("/uploads", KWebServerPermissions::All);

		// no users: resolve returns directory permissions
		auto iPerms = Perms.Resolve("", "/uploads/file.txt");
		CHECK(iPerms == KWebServerPermissions::All);

		iPerms = Perms.Resolve("", "/docs/file.txt");
		CHECK(iPerms == (KWebServerPermissions::Read | KWebServerPermissions::Browse));
	}

	SECTION("RootPathMatch")
	{
		KWebServerPermissions Perms;
		Perms.AddUser("alice:pass:/:read|browse");

		// root path should match everything
		CHECK(Perms.IsAllowed("alice", KHTTPMethod::GET, "/") == true);
		CHECK(Perms.IsAllowed("alice", KHTTPMethod::GET, "/any/path") == true);
	}
}
