#include "catch.hpp"

#include <dekaf2/web/acme/kacmemanager.h>
#include <dekaf2/crypto/rsa/krsacert.h>
#include <dekaf2/crypto/rsa/krsakey.h>
#include <dekaf2/net/tcp/ktcpserver.h>
#include <dekaf2/system/filesystem/kfilesystem.h>

using namespace dekaf2;

namespace {

// an unreachable directory URL, guarding the tests against ever ordering
// from a real CA
constexpr KStringViewZ NoDirectory = "https://localhost:1/dir";

//-----------------------------------------------------------------------------
void StoreCert(KStringViewZ sDir, KStringView sDomain, KUnixTime ValidFrom = KUnixTime())
//-----------------------------------------------------------------------------
{
	KRSAKey  Key(2048);
	KRSACert Cert(Key, sDomain, "", "", "", chrono::years(1), ValidFrom);

	REQUIRE ( Cert.HasError() == false );
	REQUIRE ( kWriteFile(kFormat("{}{}acme-cert.pem"   , sDir, kDirSep), Cert.GetPEM()    , 0644) );
	REQUIRE ( kWriteFile(kFormat("{}{}acme-privkey.pem", sDir, kDirSep), Key.GetPEM(true) , 0600) );

} // StoreCert

//-----------------------------------------------------------------------------
KAcmeManager::Options TestOptions(KStringViewZ sDir, KStringView sDomain)
//-----------------------------------------------------------------------------
{
	KAcmeManager::Options Options;
	Options.Domains            = { KString(sDomain) };
	Options.sStorageDir        = sDir;
	Options.Acme.sDirectoryURL = NoDirectory;
	Options.Acme.bVerifyTLS    = false;
	return Options;

} // TestOptions

} // end of anonymous namespace

TEST_CASE("KAcmeManager")
{
#if OPENSSL_VERSION_NUMBER >= 0x10101000L && !defined(LIBRESSL_VERSION_NUMBER)

	SECTION("LoadStored")
	{
		// a stored, valid and covering certificate is installed without any order
		KTempDir Tmp;
		StoreCert(Tmp.Name(), "alpha.test");

		KTLSContext Context(true);
		KAcmeManager Manager(TestOptions(Tmp.Name(), "alpha.test"));

		CHECK ( Manager.Start(Context, /*bBlockUntilIssued*/true) == true );
		CHECK ( Manager.ValidUntil() > KUnixTime::now() );

		// installed both per SNI dispatch and as the default certificate
		CHECK ( Context.GetSNIContext("alpha.test") != nullptr );

		Manager.Stop();
	}

	SECTION("RejectNonCovering")
	{
		// a stored certificate for another domain is not installed, and the
		// order that becomes necessary fails on the unreachable directory
		KTempDir Tmp;
		StoreCert(Tmp.Name(), "other.test");

		KTLSContext Context(true);
		KAcmeManager Manager(TestOptions(Tmp.Name(), "alpha.test"));

		CHECK ( Manager.Start(Context, true) == false );
		CHECK ( Manager.HasError()           == true  );
		CHECK ( Manager.ValidUntil()         == KUnixTime() );
		CHECK ( Context.GetSNIContext("alpha.test") == nullptr );
	}

	SECTION("RejectExpired")
	{
		// an expired certificate is not installed
		KTempDir Tmp;
		StoreCert(Tmp.Name(), "alpha.test", KUnixTime::now() - chrono::days(730));

		KTLSContext Context(true);
		KAcmeManager Manager(TestOptions(Tmp.Name(), "alpha.test"));

		CHECK ( Manager.Start(Context, true) == false );
		CHECK ( Manager.ValidUntil()         == KUnixTime() );
	}

	SECTION("Errors")
	{
		KTempDir Tmp;
		KTLSContext Context(true);

		auto Options = TestOptions(Tmp.Name(), "alpha.test");
		Options.Domains.clear();
		KAcmeManager Empty(std::move(Options));
		CHECK ( Empty.Start(Context) == false );
	}

	SECTION("SetACME")
	{
		KTempDir Tmp;

		// plain TCP servers cannot do ACME
		KTCPServer Plain(30307, /*bTLS*/false, 5, false);
		CHECK ( Plain.SetACME({ "alpha.test" }) == false );

		// a TLS server accepts the configuration
		KTCPServer Server(30307, /*bTLS*/true, 5, false);
		CHECK ( Server.SetACME({})              == false );
		CHECK ( Server.SetACME({ "alpha.test" }, "", NoDirectory, Tmp.Name(), false) == true );
	}

#endif
}
