#include "catch.hpp"
#include <dekaf2/kuuid.h>

#include <dekaf2/kstringview.h>
#include <dekaf2/kstringstream.h>
#include <dekaf2/kencode.h>

using namespace dekaf2;


TEST_CASE("KUUID")
{
	SECTION("Creation")
	{
		KUUID uuid;
		CHECK ( uuid.empty() == false );
		CHECK ( uuid.ToString().size() == 36 );
		CHECK ( uuid.ToString().MatchRegex("[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}") );
		CHECK ( uuid.GetVersion() == KUUID::Random );
		CHECK ( uuid.GetVariant() == 1 );
//		CHECK ( uuid.ToString() == "" );

		if (KUUID::HasVersion(KUUID::MACTime))
		{
			uuid = KUUID::Create(KUUID::MACTime);
			CHECK ( uuid.empty() == false );
			CHECK ( uuid.ToString().size() == 36 );
			CHECK ( uuid.ToString().MatchRegex("[0-9a-f]{8}-[0-9a-f]{4}-1[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}") );
			CHECK ( uuid.GetVersion() == KUUID::MACTime );
			CHECK ( uuid.GetVariant() == 1 );
			CHECK ( uuid != KStringView("") );
		}

		if (KUUID::HasVersion(KUUID::NamedMD5))
		{
			uuid = KUUID::Create(KUUID::ns::URL, "http://www.test.com/index.html", true);
			CHECK ( uuid.empty() == false );
			CHECK ( uuid.ToString().size() == 36 );
			CHECK ( uuid.ToString().MatchRegex("[0-9a-f]{8}-[0-9a-f]{4}-3[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}") );
			CHECK ( uuid.GetVersion() == KUUID::NamedMD5 );
			CHECK ( uuid != KStringView("") );

			uuid = KUUID::Create(KUUID::ns::DNS, "www.example.com", true);
			CHECK ( uuid == KStringView("5df41881-3aed-3515-88a7-2f4a814cf09e") );
			uuid = KUUID::Create(KUUID::ns::DNS, "google.com", true);
			CHECK ( uuid == KStringView("9a74c83e-2c09-3513-a74b-91d679be82b8") );
			uuid = KUUID::Create(KUUID::ns::URL, "https://www.php.net", true);
			CHECK ( uuid.GetVersion() == KUUID::NamedMD5 );
			CHECK ( uuid == KStringView("3f703955-aaba-3e70-a3cb-baff6aa3b28f") );
			uuid = KUUID::Create(KUUID::ns::X500, "TMP112", true);
			CHECK ( uuid.GetVersion() == KUUID::NamedMD5 );
			CHECK ( uuid == KStringView("85b5aa25-9de4-3a17-af74-2effc92e9a7a") );
		}

		if (KUUID::HasVersion(KUUID::NamedSHA1))
		{
			uuid = KUUID::Create(KUUID::ns::URL, "http://www.test.com/index.html");
			CHECK ( uuid.empty() == false );
			CHECK ( uuid.ToString().size() == 36 );
			CHECK ( uuid.ToString().MatchRegex("[0-9a-f]{8}-[0-9a-f]{4}-5[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}") );
			CHECK ( uuid.GetVersion() == KUUID::NamedSHA1 );
			CHECK ( uuid != KStringView("") );

			uuid = KUUID::Create(KUUID::ns::DNS, "www.example.com");
			CHECK ( uuid == KStringView("2ed6657d-e927-568b-95e1-2665a8aea6a2") );
			uuid = KUUID::Create(KUUID::ns::URL, "https://www.php.net");
			CHECK ( uuid == KStringView("a8f6ae40-d8a7-58f0-be05-a22f94eca9ec") );
		}

		uuid = "017f22e2-79b0-7cc3-98c4-dc0c0c07398f";
		CHECK ( uuid.GetVersion() == KUUID::Version::TimeRandom );
		auto tTime = uuid.GetTime();
		CHECK ( tTime.to_string() == "2022-02-22 19:22:22" );
		CHECK ( tTime.subseconds().milliseconds().count() == 0 );

		uuid = KUUID(KUUID::Version::TimeRandom);
		auto uuid2 = KUUID(KUUID::Version::TimeRandom);
		CHECK ( uuid.GetVersion() == KUUID::Version::TimeRandom );
		tTime = uuid.GetTime();
		auto tNow = KUnixTime::now();
		CHECK ( (tNow - tTime) < chrono::seconds(2) );
		CHECK ( uuid != uuid2 );

#if DEKAF2_HAS_CPP_20
		constexpr auto uuid1 = KUUID("e22e1622-5c14-11ea-b2f3-1db91a60758f");
		static_assert(uuid1.GetVersion() == KUUID::Version::MACTime, "wrong version");
#else
		auto uuid1 = KUUID("e22e1622-5c14-11ea-b2f3-1db91a60758f");
#endif
		CHECK ( uuid1.GetVersion() == KUUID::Version::MACTime );
		tTime = uuid1.GetTime();
		CHECK ( tTime.to_string() == "2020-03-01 23:32:15" );
		CHECK ( tTime.subseconds().microseconds().count() == 199389 );
		auto MAC = uuid1.GetMAC();
		CHECK ( MAC.ToHex('.') == "1d.b9.1a.60.75.8f" );
		CHECK ( MAC.ToHex('\0') == "1db91a60758f" );

		uuid = KUUID("3960c5d8-60f8-11ea-bc55-0242ac130003");
		CHECK ( uuid.GetVersion() == KUUID::Version::MACTime );
		CHECK ( uuid.GetTime().to_string() == "2020-03-08 04:49:41" );

		uuid = KUUID("1ea60f83-960c-65d8-bc55-15b913560758");
		CHECK ( uuid.ToString('.') == "1ea60f83.960c.65d8.bc55.15b913560758" );
		CHECK ( uuid.ToString('\0') == "1ea60f83960c65d8bc5515b913560758" );
		CHECK ( uuid.GetVersion() == KUUID::Version::MACTimeSort );
		tTime = uuid.GetTime();
		CHECK ( tTime.to_string() == "2020-03-08 04:49:41" );
		CHECK ( tTime.subseconds().milliseconds().count() == 902 );
		MAC = uuid.GetMAC();
		CHECK ( MAC.ToHex() == "15:b9:13:56:07:58" );

		uuid = KUUID::Create(KUUID::Null);
		CHECK ( uuid.empty() == true );
		CHECK ( uuid.ToString().size() == 36 );
		CHECK ( uuid == "00000000-0000-0000-0000-000000000000" );
		CHECK ( uuid.GetVersion() == KUUID::Null );
		CHECK ( uuid.GetVariant() == 0 );

		uuid = KUUID::Parse("697a9b88-a9ce-4383-b9d2-1db91a60758f");
		CHECK ( uuid.empty() == false );
		CHECK ( uuid == "697a9b88-a9ce-4383-b9d2-1db91a60758f");
		CHECK ( uuid.GetVersion() == KUUID::Random );
		CHECK ( uuid.GetVariant() == 1 );
		// this is actually an invalid variant 4 UUID - but the linux
		// implementation parses it anyways
		uuid2 = KUUID::Parse("697a9b88-a9ce-4383-19d2-1db91a60758f");
		CHECK ( uuid2.empty() == false );
		CHECK ( uuid2 == "697a9b88-a9ce-4383-19d2-1db91a60758f" );
		CHECK ( uuid.GetVersion() == KUUID::Random );
		CHECK ( uuid.GetVariant() == 1 );

		uuid = "85841df6-12d2-43a1-9866-8287eba94deb";
		CHECK ( uuid.empty() == false );
		CHECK ( uuid == "85841df6-12d2-43a1-9866-8287eba94deb" );
		CHECK ( uuid != uuid2 );
		CHECK ( uuid.GetVersion() == KUUID::Random );
		CHECK ( uuid.GetVariant() == 1 );

		uuid.clear();
		CHECK ( uuid.empty() == true );
		CHECK ( uuid.ToString().size() == 36 );
		CHECK ( uuid == "00000000-0000-0000-0000-000000000000" );
		CHECK ( uuid.GetVersion() == KUUID::Null );
		CHECK ( uuid.GetVariant() == 0 );

		KUUID abc("6ba7b810-9dad-11d1-80b4-00c04fd430c8");
		CHECK ( abc == KUUID::ns::DNS );
	}

	SECTION("formatting (fmt)")
	{
		KUUID uuid1;
		auto uuid2 = uuid1;
		auto s = kFormat("{}", uuid1);
		CHECK ( s == uuid2.ToString() );
		CHECK ( s.MatchRegex("[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}") );
	}

	SECTION("formatting (std::ostream)")
	{
		KUUID uuid1;
		auto uuid2 = uuid1;
		KString s;
		KOutStringStream oss(s);
		oss << uuid1;
		CHECK ( s == uuid2.ToString() );
		CHECK ( s.MatchRegex("[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}") );
	}

}
