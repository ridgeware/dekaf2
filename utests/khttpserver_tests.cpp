#include "catch.hpp"

#include <dekaf2/khttpserver.h>
#include <dekaf2/ktcpserver.h>
#include <dekaf2/kstring.h>

using namespace dekaf2;


TEST_CASE("KHTTPServer") {

/*
	 X-Forwarded-For: 203.0.113.195, 70.41.3.18, 150.172.238.178
	 X-Forwarded-For: 2001:db8:85a3:8d3:1319:8a2e:370:7348
	 X-Forwarded-For: 203.0.113.195
	 X-ProxyUser-Ip: 203.0.113.19
	 Forwarded: For="[2001:db8:cafe::17]:4711"
	 Forwarded: for=192.0.2.43, for=198.51.100.17
	 Forwarded: for=192.0.2.60; proto=http; by=203.0.113.43
*/

	SECTION("GetBrowserIP 1")
	{
		KStringView sRequest;
		sRequest =
(R"(GET / HTTP/1.1
Host: www.test.com
X-Forwarded-For: 203.0.113.195, 70.41.3.18, 150.172.238.178
Forwarded: for=192.0.2.60; proto=http; by=203.0.113.43
X-ProxyUser-Ip: 203.0.113.19

)");
		KString sResponse;
		KInStringStream iss(sRequest);
		KOutStringStream oss(sResponse);
		KStream stream(iss, oss);
		KHTTPServer Browser(stream, "192.168.178.1:234");
		Browser.Parse();
		CHECK ( Browser.GetBrowserIP() == "192.0.2.60" );
	}

	SECTION("GetBrowserIP 2")
	{
		KStringView sRequest;
		sRequest =
(R"(GET / HTTP/1.1
Host: www.test.com
X-Forwarded-For: 203.0.113.195, 70.41.3.18, 150.172.238.178

)");
		KString sResponse;
		KInStringStream iss(sRequest);
		KOutStringStream oss(sResponse);
		KStream stream(iss, oss);
		KHTTPServer Browser(stream, "192.168.178.1:234");
		Browser.Parse();
		CHECK ( Browser.GetBrowserIP() == "203.0.113.195" );
	}

	SECTION("GetBrowserIP 2")
	{
		KStringView sRequest;
		sRequest =
(R"(GET / HTTP/1.1
Host: www.test.com

)");
		KString sResponse;
		KInStringStream iss(sRequest);
		KOutStringStream oss(sResponse);
		KStream stream(iss, oss);
		KHTTPServer Browser(stream, "192.168.178.1:234");
		Browser.Parse();
		CHECK ( Browser.GetBrowserIP() == "192.168.178.1" );
	}

	SECTION("GetBrowserIP 3")
	{
		KStringView sRequest;
		sRequest =
(R"(GET / HTTP/1.1
Host: www.test.com
X-Forwarded-For: 2001:db8:85a3:8d3:1319:8a2e:370:7348

)");
		KString sResponse;
		KInStringStream iss(sRequest);
		KOutStringStream oss(sResponse);
		KStream stream(iss, oss);
		KHTTPServer Browser(stream, "192.168.178.1:234");
		Browser.Parse();
		CHECK ( Browser.GetBrowserIP() == "2001:db8:85a3:8d3:1319:8a2e:370" );
	}

	SECTION("GetBrowserIP 4")
	{
		KStringView sRequest;
		sRequest =
(R"(GET / HTTP/1.1
Host: www.test.com
X-Forwarded-For: 203.0.113.195

)");
		KString sResponse;
		KInStringStream iss(sRequest);
		KOutStringStream oss(sResponse);
		KStream stream(iss, oss);
		KHTTPServer Browser(stream, "192.168.178.1:234");
		Browser.Parse();
		CHECK ( Browser.GetBrowserIP() == "203.0.113.195" );
	}

	SECTION("GetBrowserIP 5")
	{
		KStringView sRequest;
		sRequest =
(R"(GET / HTTP/1.1
Host: www.test.com
X-ProxyUser-Ip: 203.0.113.19:93

)");
		KString sResponse;
		KInStringStream iss(sRequest);
		KOutStringStream oss(sResponse);
		KStream stream(iss, oss);
		KHTTPServer Browser(stream, "192.168.178.1:234");
		Browser.Parse();
		CHECK ( Browser.GetBrowserIP() == "203.0.113.19" );
	}

	SECTION("GetBrowserIP 6")
	{
		KStringView sRequest;
		sRequest =
(R"(GET / HTTP/1.1
Host: www.test.com
Forwarded: For="[2001:db8:cafe::17]:4711"

)");
		KString sResponse;
		KInStringStream iss(sRequest);
		KOutStringStream oss(sResponse);
		KStream stream(iss, oss);
		KHTTPServer Browser(stream, "192.168.178.1:234");
		Browser.Parse();
		CHECK ( Browser.GetBrowserIP() == "2001:db8:cafe::17" );
	}

	SECTION("GetBrowserIP 7")
	{
		KStringView sRequest;
		sRequest =
(R"(GET / HTTP/1.1
Host: www.test.com
Forwarded: for="[2001:db8:cafe::17]"

)");
		KString sResponse;
		KInStringStream iss(sRequest);
		KOutStringStream oss(sResponse);
		KStream stream(iss, oss);
		KHTTPServer Browser(stream, "192.168.178.1:234");
		Browser.Parse();
		CHECK ( Browser.GetBrowserIP() == "2001:db8:cafe::17" );
	}

	SECTION("GetBrowserIP 8")
	{
		KStringView sRequest;
sRequest =
(R"(GET / HTTP/1.1
Host: www.test.com
Forwarded: for=192.0.2.43, for=198.51.100.17

)");
		KString sResponse;
		KInStringStream iss(sRequest);
		KOutStringStream oss(sResponse);
		KStream stream(iss, oss);
		KHTTPServer Browser(stream, "192.168.178.1:234");
		Browser.Parse();
		CHECK ( Browser.GetBrowserIP() == "192.0.2.43" );
	}

}

