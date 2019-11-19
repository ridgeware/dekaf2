#include "catch.hpp"

#include <dekaf2/kwebclient.h>
#include <dekaf2/kstring.h>
#include <iostream>

using namespace dekaf2;

// helper class to access protected method
class KMyWebClient : public KWebClient
{
public:

	bool ReusesConnection(KStringView sEndpoint) const
	{
		return AlreadyConnected(sEndpoint);
	}
};

TEST_CASE("KHTTPClient")
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

	SECTION("TLSinTLS")
	{
		KMyWebClient HTTP;
//		HTTP.SetProxy("https://192.168.1.1:3128");
// if above .SetProxy() call is commented out uses proxy set by env var "HTTPS_PROXY"

		CHECK ( HTTP.ReusesConnection("https://www.google.fr") == false );
		KString sHTML = HTTP.Get("https://www.google.fr");
		CHECK ( sHTML.empty() == false );
		CHECK ( HTTP.GetStatusCode() == 200 );

		CHECK ( HTTP.ReusesConnection("https://www.google.fr") == true );
		// second request, check reuse in KLog
		sHTML = HTTP.Get("https://www.google.fr/imghp");
		CHECK ( sHTML.empty() == false );
		CHECK ( HTTP.GetStatusCode() == 200 );
	}
}
