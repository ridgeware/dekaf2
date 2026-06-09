#include "catch.hpp"

#include <dekaf2/net/geo/kgeoip.h>
#include <dekaf2/net/address/kipaddress.h>
#include <dekaf2/system/os/ksystem.h>
#include <cstdlib>

using namespace dekaf2;

// These expectations are the ground truth from MaxMind's freely distributed
// GeoIP2-City-Test.mmdb (which is an ip_version 6 database, so the IPv4 case
// below also exercises the IPv4 ::/96 start node path). A DB-IP Lite City
// database resolves the same addresses to the same countries.

TEST_CASE("KGeoIP")
{
	// This test needs a MaxMind DB (.mmdb) file. Point DEKAF2_GEOIP_TESTDB at one,
	// e.g. MaxMind's GeoIP2-City-Test.mmdb or a DB-IP Lite City database. Without
	// it the data tests are skipped, so a checkout without the data file still builds
	// and runs green.
	KStringViewZ sDB = kGetEnv("DEKAF2_GEOIP_TESTDB");

	if (sDB.empty())
	{
		// we skip silently
		// WARN("DEKAF2_GEOIP_TESTDB not set - skipping KGeoIP data tests");
		return;
	}

	KGeoIP GeoIP;
	REQUIRE  ( GeoIP.Open (sDB) );
	REQUIRE  ( GeoIP.is_open()  );

	SECTION("metadata")
	{
		CHECK( GeoIP.GetIPVersion() == 6 );
		CHECK( GeoIP.GetDatabaseType().find("City") != KStringView::npos );
		CHECK( GeoIP.GetBuildTime() > KUnixTime(0) );
	}

	SECTION("IPv4 lookup resolves through the ::/96 start node")
	{
		auto Loc = GeoIP.Lookup ("214.78.120.5");
		CHECK_FALSE( Loc.empty() );
		CHECK( Loc.sCountryISO     == "US" );
		CHECK( Loc.sCountryName    == "United States" );
		CHECK( Loc.sContinent      == "NA" );
		CHECK( Loc.sCity           == "San Diego" );
		CHECK( Loc.nLatitude       == Approx(  32.7405 ).margin(1e-3) );
		CHECK( Loc.nLongitude      == Approx(-117.0935 ).margin(1e-3) );
		CHECK( Loc.iAccuracyRadius == 100 );
	}

	SECTION("IPv6 lookup walks the full 128 bit tree")
	{
		auto Loc = GeoIP.Lookup ("2001:218::1");
		CHECK_FALSE( Loc.empty() );
		CHECK( Loc.sCountryISO     == "JP" );
		CHECK( Loc.sCountryName    == "Japan" );
		CHECK( Loc.sContinent      == "AS" );
		CHECK( Loc.sCity.empty() ); // this record carries no city
		CHECK( Loc.nLatitude       == Approx( 35.68536).margin(1e-3) );
		CHECK( Loc.nLongitude      == Approx(139.75309).margin(1e-3) );
		CHECK( Loc.iAccuracyRadius == 100 );
	}

	SECTION("the KIPAddress overload matches the string overload")
	{
		KIPError   ec;
		KIPAddress IP ("214.78.120.5", ec);
		REQUIRE_FALSE( bool(ec) );

		auto Loc = GeoIP.Lookup (IP);
		CHECK( Loc.sCountryISO == "US" );
		CHECK( Loc.sCity       == "San Diego" );
	}

	SECTION("an address not in the database returns an empty result")
	{
		CHECK( GeoIP.Lookup ("8.8.8.8").empty() );
		CHECK( GeoIP.Lookup ("10.0.0.1").empty() );
		CHECK( GeoIP.Lookup ("2606:4700::1").empty() );
	}

	SECTION("an unparseable address yields an empty result")
	{
		CHECK( GeoIP.Lookup ("not-an-ip-address").empty() );
	}

	SECTION("subdivision, postal code, time zone and metro code")
	{
		auto Loc = GeoIP.Lookup ("214.78.120.5");
		REQUIRE_FALSE( Loc.empty() );
		CHECK( Loc.sPostalCode == "92105" );
		CHECK( Loc.sTimeZone   == "America/Los_Angeles" );
		REQUIRE( Loc.Subdivisions.size() >= 1 );
		CHECK( Loc.Subdivisions[0].sISO  == "CA" );
		CHECK( Loc.Subdivisions[0].sName == "California" );
	}

	SECTION("geoname ids, the IDs/View lookups and language-independent equality")
	{
		auto Loc = GeoIP.Lookup ("214.78.120.5");
		CHECK( Loc.iContinentGeoNameID == 6255149u ); // North America
		CHECK( Loc.iCountryGeoNameID   == 6252001u ); // United States
		CHECK( Loc.iCityGeoNameID      == 5391811u ); // San Diego
		REQUIRE( Loc.SubdivisionGeoNameIDs.size() >= 1 );
		CHECK( Loc.SubdivisionGeoNameIDs[0] == 5332921u ); // California

		// zero-copy view lookup: same content and ids
		auto View = GeoIP.LookupView ("214.78.120.5");
		CHECK( View.sCity          == "San Diego" );
		CHECK( View.iCityGeoNameID == 5391811u );

		// owned, built from a view
		KGeoIP::Location Owned (View);
		CHECK( Owned.sCity          == "San Diego" );
		CHECK( Owned.iCityGeoNameID == 5391811u );

		// id-only lookup
		auto IDs = GeoIP.LookupIDs ("214.78.120.5");
		CHECK( IDs.iCountryGeoNameID == 6252001u );
		CHECK_FALSE( IDs.empty() );
		CHECK( GeoIP.LookupIDs ("8.8.8.8").empty() );

		// identity is language independent, and IDs/Location agree on it
		CHECK( GeoIP.Lookup    ("214.78.120.5", "ja") == GeoIP.Lookup ("214.78.120.5", "en") );
		CHECK( GeoIP.LookupIDs ("214.78.120.5")       == GeoIP.Lookup ("214.78.120.5", "ja") );

		// is_in_european_union lives in the LocationIDs base (both lookups carry it)
		CHECK_FALSE( GeoIP.Lookup ("214.78.120.5").bIsInEU ); // United States
		auto Sweden = GeoIP.Lookup ("89.160.20.112");
		CHECK( Sweden.sCountryISO == "SE" );
		CHECK( Sweden.bIsInEU );                              // Sweden is in the EU
		CHECK( GeoIP.LookupIDs ("89.160.20.112").bIsInEU );
	}

	SECTION("localized names and language fallback")
	{
		// Languages() reflects the metadata's advertised set (here "en", "zh").
		// Note records may actually carry more languages than the metadata lists -
		// the ja/ru/de lookups below resolve even though they are not advertised.
		CHECK_FALSE( GeoIP.Languages().empty() );
		bool bHasEn = false;
		bool bHasZh = false;
		for (const auto& sLang : GeoIP.Languages())
		{
			if (sLang == "en") { bHasEn = true; }
			if (sLang == "zh") { bHasZh = true; }
		}
		CHECK( bHasEn );
		CHECK( bHasZh );

		CHECK( GeoIP.GetDefaultLanguage() == "en" );

		// a single requested language per lookup
		CHECK( GeoIP.Lookup ("2001:218::1",  "ja").sCountryName == "日本" );
		CHECK( GeoIP.Lookup ("214.78.120.5", "ja").sCity        == "サンディエゴ" );
		CHECK( GeoIP.Lookup ("214.78.120.5", "ru").sCountryName == "США" );

		// requested language missing in the record -> fall back to the default ("en");
		// the San Diego city record carries no zh-CN name
		CHECK( GeoIP.Lookup ("214.78.120.5", "zh-CN").sCity == "San Diego" );

		// a settable default language, used when a lookup does not request one
		KGeoIP German;
		REQUIRE( German.SetDefaultLanguage("de").Open (sDB) );
		CHECK  ( German.GetDefaultLanguage() == "de" );
		CHECK  ( German.Lookup ("214.78.120.5").sCountryName == "Vereinigte Staaten" );

		// neither requested nor default present in the record -> first available language.
		// the San Diego city record carries no zh-CN name, so a zh-CN request with a
		// zh-CN default still yields a non-empty city name from the first available language
		KGeoIP Chinese;
		REQUIRE     ( Chinese.SetDefaultLanguage("zh-CN").Open (sDB) );
		CHECK_FALSE ( Chinese.Lookup ("214.78.120.5", "zh-CN").sCity.empty() );
	}
}
