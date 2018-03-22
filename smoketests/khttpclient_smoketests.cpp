#include "catch.hpp"

#include <dekaf2/khttpclient.h>
#include <dekaf2/kstring.h>
#include <iostream>

using namespace dekaf2;

// TODO move these tests into smoketests once kcurl has been merged

TEST_CASE("KHTTPCLient")
{
	KString sHTML = kHTTPGet("https://www.google.fr/");
	CHECK ( sHTML.empty() == false );

}
