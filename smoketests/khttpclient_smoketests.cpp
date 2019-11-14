#include "catch.hpp"

#include <dekaf2/kwebclient.h>
#include <dekaf2/kstring.h>
#include <iostream>

using namespace dekaf2;

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
