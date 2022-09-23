#include "catch.hpp"

#include <dekaf2/kwebclient.h>
#include <dekaf2/kstring.h>
#include <dekaf2/kjson.h>
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

	SECTION("TLSinTLS")
	{
		KMyWebClient HTTP;
		KJSON json;
		HTTP.SetServiceSummary(&json);
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
		kDebug(1, json.dump(1, '\t'));
	}

	SECTION("No DNS")
	{
		KWebClient HTTP;
		KJSON json;
		HTTP.SetServiceSummary(&json);
		// this tests a redirect to https://www.google.fr
		auto sResult = HTTP.Get("https://google.fr");
		CHECK ( HTTP.HttpSuccess() == true );
		CHECK ( HTTP.GetConnectedEndpoint().Domain == "www.google.fr" );
		CHECK ( HTTP.GetConnectedEndpoint().Serialize() == "www.google.fr:443" );
		CHECK ( sResult.empty() == false );
		sResult = HTTP.Get("https://wxy.judgvbdfasjh.skjhgds.org");
		CHECK ( HTTP.HttpSuccess() == false );
		CHECK ( sResult.empty() == true );
		kDebug(1, json.dump(1, '\t'));
	}

	SECTION("compressors")
	{
		KURL URL("https://www.facebook.com");
		KWebClient HTTP;
		// do not take cookies
		HTTP.AcceptCookies(false);
		// set a user agent that is accepted by facebook
		HTTP.AddHeader(KHTTPHeader::USER_AGENT, "curl/7.54");
		// request the reference
		HTTP.RequestCompression(true, "gzip, deflate");
		auto sReference = HTTP.Get(URL);
		CHECK ( HTTP.Response.Headers.Get(KHTTPHeader::CONTENT_ENCODING) == "gzip" );
		CHECK ( sReference.starts_with("<!DOCTYPE html>") );
		CHECK ( sReference.Right(7) == "</html>" );
		CHECK ( sReference.ends_with("</html>") );
#ifdef DEKAF2_HAS_LIBZSTD
		// request the zstd version
		HTTP.RequestCompression(true, "zstd");
		auto sZSTD = HTTP.Get(URL);
		CHECK ( HTTP.Response.Headers.Get(KHTTPHeader::CONTENT_ENCODING) == "zstd" );
		CHECK ( sZSTD.starts_with("<!DOCTYPE html>") );
		CHECK ( sZSTD.ends_with("</html>") );
#endif
#ifdef DEKAF2_HAS_LIBBROTLI
		// request the brotli version
		HTTP.RequestCompression(true, "br");
		auto sBrotli = HTTP.Get(URL);
		CHECK ( HTTP.Response.Headers.Get(KHTTPHeader::CONTENT_ENCODING) == "br" );
		CHECK ( sBrotli.starts_with("<!DOCTYPE html>") );
		CHECK ( sBrotli.ends_with("</html>") );
#endif
	}
}
