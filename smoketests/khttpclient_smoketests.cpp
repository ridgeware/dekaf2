#include "catch.hpp"

#include <dekaf2/khttpclient.h>
#include <dekaf2/kstring.h>
#include <iostream>

using namespace dekaf2;

// TODO move these tests into smoketests once kcurl has been merged

TEST_CASE("KHTTPCLient")
{
	// KString sHTML = kHTTPGet("http://www.google.fr/");
	// CHECK ( sHTML.empty() == false );

	KURL URL = "http://www.google.fr/";
	KString sBuffer;
	KHTTPClient client;

	if (client.Connect(URL))
	{
		if (client.Resource(URL, KHTTPMethod::GET))
		{
			if (client.Request())
			{
				while (client.ReadLine(sBuffer))
				{
					std::cerr << sBuffer << std::endl;
				}
			}
		}
	}


}
