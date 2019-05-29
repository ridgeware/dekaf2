#include "catch.hpp"

#include <dekaf2/khttpclient.h>
#include <dekaf2/kstring.h>
#include <iostream>

using namespace dekaf2;

// TODO move these tests into smoketests once kcurl has been merged

TEST_CASE("KHTTPCLient")
{
	SECTION("TLS Get")
	{
		KString sHTML = kHTTPGet("https://www.google.fr/");
		CHECK ( sHTML.empty() == false );

		KFile File;
		KConnection Connection(File);
		CHECK ( Connection.Good() == true );
	}

//	SECTION("Redirect")
//	{
//		KString sHTML = kHTTPGet("http://mobil.spiegel.de/test.html");
//		CHECK ( sHTML.empty() == false );
//	}
}
