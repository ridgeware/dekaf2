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

	SECTION("GetRemoteIP 1")
	{
		KStringView sRequest;
		sRequest =
(R"(GET / HTTP/1.1
Host: www.test.com
X-Forwarded-For: 203.0.113.195, 70.41.3.18, 150.172.238.178
Forwarded: for=192.0.2.60; proto=https; by=203.0.113.43
X-ProxyUser-Ip: 203.0.113.19

)");
		KString sResponse;
		KInStringStream iss(sRequest);
		KOutStringStream oss(sResponse);
		KStream stream(iss, oss); 
		KHTTPServer Browser(stream, "192.168.178.1:234", url::KProtocol::HTTP, 80, 5);
		Browser.Parse();
		CHECK ( Browser.GetRemoteIP() == "192.0.2.60" );
		CHECK ( Browser.GetRemotePort() == 0 );
		CHECK ( Browser.GetRemoteProxy() == "203.0.113.43" );
		CHECK ( ( Browser.GetRemoteProto() == url::KProtocol::HTTPS ) ); // inner parens for clang 7
	}

	SECTION("GetRemoteIP 2")
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
		KHTTPServer Browser(stream, "192.168.178.1:234", url::KProtocol::HTTP, 80, 5);
		Browser.Parse();
		CHECK ( Browser.GetRemoteIP() == "203.0.113.195" );
		CHECK ( Browser.GetRemotePort() == 0 );
		CHECK ( ( Browser.GetRemoteProto() == url::KProtocol::HTTP ) );
	}

	SECTION("GetRemoteIP 2")
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
		KHTTPServer Browser(stream, "192.168.178.1:234", url::KProtocol::HTTPS, 80, 5);
		Browser.Parse();
		CHECK ( Browser.GetRemoteIP() == "192.168.178.1" );
		CHECK ( Browser.GetRemotePort() == 234 );
		CHECK ( ( Browser.GetRemoteProto() == url::KProtocol::HTTPS ) );
	}

	SECTION("GetRemoteIP 3")
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
		KHTTPServer Browser(stream, "192.168.178.1:234", url::KProtocol::HTTP, 80, 5);
		Browser.Parse();
		CHECK ( Browser.GetRemoteIP() == "2001:db8:85a3:8d3:1319:8a2e:370:7348" );
		CHECK ( Browser.GetRemotePort() == 0 );
		CHECK ( ( Browser.GetRemoteProto() == url::KProtocol::HTTP ) );
	}

	SECTION("GetRemoteIP 4")
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
		KHTTPServer Browser(stream, "192.168.178.1:234", url::KProtocol::HTTP, 80, 5);
		Browser.Parse();
		CHECK ( Browser.GetRemoteIP() == "203.0.113.195" );
		CHECK ( Browser.GetRemotePort() == 0 );
		CHECK ( ( Browser.GetRemoteProto() == url::KProtocol::HTTP ) );
	}

	SECTION("GetRemoteIP 5")
	{
		KStringView sRequest;
		sRequest =
(R"(GET / HTTP/1.1
Host: www.test.com
X-ProxyUser-Ip: 203.0.113.19

)");
		KString sResponse;
		KInStringStream iss(sRequest);
		KOutStringStream oss(sResponse);
		KStream stream(iss, oss);
		KHTTPServer Browser(stream, "192.168.178.1:234", url::KProtocol::HTTP, 80, 5);
		Browser.Parse();
		CHECK ( Browser.GetRemoteIP() == "203.0.113.19" );
		CHECK ( Browser.GetRemotePort() == 0 );
		CHECK ( ( Browser.GetRemoteProto() == url::KProtocol::HTTP ) );
	}

	SECTION("GetRemoteIP 6")
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
		KHTTPServer Browser(stream, "192.168.178.1:234", url::KProtocol::HTTP, 80, 5);
		Browser.Parse();
		CHECK ( Browser.GetRemoteIP() == "2001:db8:cafe::17" );
		CHECK ( Browser.GetRemotePort() == 4711 );
		CHECK ( ( Browser.GetRemoteProto() == url::KProtocol::HTTP ) );
	}

	SECTION("GetRemoteIP 7")
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
		KHTTPServer Browser(stream, "192.168.178.1:234", url::KProtocol::HTTP, 80, 5);
		Browser.Parse();
		CHECK ( Browser.GetRemoteIP() == "2001:db8:cafe::17" );
		CHECK ( Browser.GetRemotePort() == 0 );
		CHECK ( ( Browser.GetRemoteProto() == url::KProtocol::HTTP ) );
	}

	SECTION("GetRemoteIP 8")
	{
		KStringView sRequest;
sRequest =
(R"(GET / HTTP/1.1
Host: www.test.com
Forwarded: for=192.0.2.43:1234, for=198.51.100.17; proto=https

)");
		KString sResponse;
		KInStringStream iss(sRequest);
		KOutStringStream oss(sResponse);
		KStream stream(iss, oss);
		KHTTPServer Browser(stream, "192.168.178.1:234", url::KProtocol::HTTP, 80, 5);
		Browser.Parse();
		CHECK ( Browser.GetRemoteIP() == "192.0.2.43" );
		CHECK ( Browser.GetRemotePort() == 1234 );
		CHECK ( ( Browser.GetRemoteProto() == url::KProtocol::HTTP ) );
	}

	SECTION("GetRemoteIP 9")
	{
		KStringView sRequest;
sRequest =
(R"(GET / HTTP/1.1
Host: www.test.com
Forwarded: for=192.0.2.43:1234; proto=https, for=198.51.100.17

)");
		KString sResponse;
		KInStringStream iss(sRequest);
		KOutStringStream oss(sResponse);
		KStream stream(iss, oss);
		KHTTPServer Browser(stream, "192.168.178.1:234", url::KProtocol::HTTP, 80, 5);
		Browser.Parse();
		CHECK ( Browser.GetRemoteIP() == "192.0.2.43" );
		CHECK ( Browser.GetRemotePort() == 1234 );
		CHECK ( ( Browser.GetRemoteProto() == url::KProtocol::HTTPS ) );
	}

	SECTION("RequestLine")
	{
		KStringView sRequest;
sRequest =
(R"(GET  / HTTP/1.1
Host: www.test.com
Forwarded: for=192.0.2.43:1234, for=198.51.100.17; proto=https

)");
		KString sResponse;
		KInStringStream iss(sRequest);
		KOutStringStream oss(sResponse);
		KStream stream(iss, oss);
		KHTTPServer Browser(stream, "192.168.178.1:234", url::KProtocol::HTTP, 80, 5);
		Browser.Parse();
		CHECK ( Browser.Error() == "invalid request line" );
		CHECK ( Browser.GetRemoteIP() == "192.168.178.1" ); 
		CHECK ( Browser.GetRemotePort() == 234 );
		CHECK ( ( Browser.GetRemoteProto() == url::KProtocol::HTTP ) );
	}

	SECTION("RequestLine")
	{
		KStringView sRequest;
sRequest =
(R"(GET / FTP/1.1
Host: www.test.com
Forwarded: for=192.0.2.43:1234, for=198.51.100.17; proto=https

)");
		KString sResponse;
		KInStringStream iss(sRequest);
		KOutStringStream oss(sResponse);
		KStream stream(iss, oss);
		KHTTPServer Browser(stream, "192.168.178.1:234", url::KProtocol::HTTP, 80, 5);
		Browser.Parse();
		CHECK ( Browser.Error() == "invalid request line" );
		CHECK ( Browser.GetRemoteIP() == "192.168.178.1" );
		CHECK ( Browser.GetRemotePort() == 234 );
		CHECK ( ( Browser.GetRemoteProto() == url::KProtocol::HTTP ) );
	}

}

