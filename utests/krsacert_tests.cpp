#include "catch.hpp"

#include <dekaf2/crypto/rsa/krsacert.h>
#include <dekaf2/crypto/rsa/krsakey.h>
#include <dekaf2/time/clock/ktime.h>
#include <dekaf2/time/duration/kduration.h>

using namespace dekaf2;

TEST_CASE("KRSACert")
{
	SECTION("CreateAndVerify")
	{
		KRSAKey Key(1024);
		CHECK ( Key.empty() == false );

		KRSACert Cert(Key, "localhost", "US", "TestOrg");
		CHECK ( Cert.empty() == false );
		CHECK ( Cert.IsValidNow() == true );
		CHECK ( Cert.CheckDomain(KString("localhost")) == true );
		CHECK ( Cert.CheckCountryCode(KString("US")) == true );
		CHECK ( Cert.CheckOrganization(KString("TestOrg")) == true );
	}

	SECTION("MoveConstructCert")
	{
		KRSAKey Key(1024);
		KRSACert Cert(Key, "localhost", "US");
		CHECK ( Cert.empty() == false );

		KRSACert Moved(std::move(Cert));
		CHECK ( Moved.empty() == false );
		CHECK ( Cert.empty()  == true  );
		CHECK ( Moved.IsValidNow() == true );
	}

	SECTION("MoveAssignOverExisting")
	{
		// move assign over an existing cert must not leak
		KRSAKey Key1(1024);
		KRSACert Cert1(Key1, "example.com", "US");
		CHECK ( Cert1.empty() == false );

		KRSAKey Key2(1024);
		KRSACert Cert2(Key2, "other.com", "DE");
		CHECK ( Cert2.empty() == false );

		Cert2 = std::move(Cert1);
		CHECK ( Cert2.empty() == false );
		CHECK ( Cert1.empty() == true  );
		CHECK ( Cert2.CheckDomain(KString("example.com")) == true );
	}

	SECTION("ValidFromFuture")
	{
		// notAfter must be ValidFrom + ValidFor, not now + ValidFor
		KRSAKey Key(1024);

		auto FutureStart = KUnixTime::now() + chrono::hours(24 * 30); // 30 days from now
		auto ValidFor    = chrono::hours(24 * 365); // 1 year

		KRSACert Cert(Key, "localhost", "US", "", KDuration(ValidFor), FutureStart);
		CHECK ( Cert.empty() == false );

		// the cert should NOT be valid now (starts 30 days from now)
		CHECK ( Cert.IsValidNow() == false );

		// the cert should be valid at FutureStart
		CHECK ( Cert.IsValidAt(FutureStart + chrono::hours(1)) == true );

		// the cert should be valid near the end: FutureStart + ValidFor - 1 day
		CHECK ( Cert.IsValidAt(FutureStart + ValidFor - chrono::hours(24)) == true );

		// the cert should NOT be valid beyond FutureStart + ValidFor
		CHECK ( Cert.IsValidAt(FutureStart + ValidFor + chrono::hours(24)) == false );
	}

	SECTION("CheckOrCreateKeyAndCertErrorOnBadKey")
	{
		// CheckOrCreateKeyAndCert must return error on key load failure
		KString sKey  = "INVALID PEM KEY DATA";
		KString sCert;

		auto sError = KRSACert::CheckOrCreateKeyAndCert(false, sKey, sCert);
		CHECK ( sError.empty() == false );
	}

	SECTION("CertSerialUnique")
	{
		// serial numbers should not all be identical
		KRSAKey Key(1024);

		KRSACert Cert1(Key, "localhost", "US");
		KRSACert Cert2(Key, "localhost", "US");

		auto sPEM1 = Cert1.GetPEM();
		auto sPEM2 = Cert2.GetPEM();

		CHECK ( sPEM1.size() > 0 );
		CHECK ( sPEM2.size() > 0 );
		// with random serials, the PEMs should differ
		CHECK ( sPEM1 != sPEM2 );
	}

	SECTION("PEMRoundTrip")
	{
		KRSAKey Key(1024);
		KRSACert Cert(Key, "myhost.local", "DE", "MyOrg");
		CHECK ( Cert.empty() == false );

		auto sPEM = Cert.GetPEM();
		CHECK ( sPEM.size() > 0 );

		KRSACert Cert2(sPEM);
		CHECK ( Cert2.empty() == false );
		CHECK ( Cert2.CheckDomain("myhost.local") == true );
		CHECK ( Cert2.CheckCountryCode("DE") == true );
		CHECK ( Cert2.CheckOrganization("MyOrg") == true );
		CHECK ( Cert2.IsValidNow() == true );
	}
}
