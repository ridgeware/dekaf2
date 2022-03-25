#include "catch.hpp"

#include <dekaf2/krestserver.h>
#include <dekaf2/ktcpserver.h>
#include <dekaf2/kstring.h>
#include <dekaf2/kjson.h>

using namespace dekaf2;


TEST_CASE("KRESTServer") {

#ifdef DEKAF2_KLOG_WITH_TCP
	SECTION("json logging")
	{
		KStringView sRequest =
(R"(GET /test HTTP/1.1
Host: www.test.com
x-klog: -out json -level 1

)");
		KString sResponse;
		KInStringStream iss(sRequest);
		KOutStringStream oss(sResponse);
		KStream stream(iss, oss);
		KRESTServer Browser(stream, "192.168.178.1:234", url::KProtocol::HTTP, 80);
		KRESTServer::Options Options;
		Options.KLogHeader = "x-klog";
		KRESTRoutes Routes;
		Routes.AddRoute({ KHTTPMethod::GET, false, "/test", [&](KRESTServer& http)
		{
			http.json.tx["response"] = "hello world";
		}});
		Browser.Execute(Options, Routes);

		sResponse.ClipAtReverse("\r\n\r\n");
		sResponse.remove_prefix("\r\n\r\n");
		KJSON json;
		kjson::Parse(json, sResponse);
		CHECK ( json.is_object() );
		CHECK ( json["response"] == "hello world" );
		KJSON& xklog = json["x-klog"];
		CHECK ( xklog.is_array() );
		KJSON& object1 = xklog[2];
		CHECK ( object1.is_object() );
		CHECK( kjson::GetStringRef(object1, "message") == "HTTP-200: OK" );
	}
#endif

#ifdef DEKAF2_KLOG_WITH_TCP
	SECTION("header logging")
	{
		KStringView sRequest =
(R"(GET /test HTTP/1.1
Host: www.test.com
x-klog: -level 1

)");
		KString sResponse;
		KInStringStream iss(sRequest);
		KOutStringStream oss(sResponse);
		KStream stream(iss, oss);
		KRESTServer Browser(stream, "192.168.178.1:234", url::KProtocol::HTTP, 80);
		KRESTServer::Options Options;
		Options.KLogHeader = "x-klog";
		KRESTRoutes Routes;
		Routes.AddRoute({ KHTTPMethod::GET, false, "/test", [&](KRESTServer& http)
		{
			http.json.tx["response"] = "hello world";
		}});
		Browser.Execute(Options, Routes);

		sResponse.contains("KRESTServer::Output(): HTTP-200: OK\r\nx-klog");
		sResponse.ClipAtReverse("\r\n\r\n");
		sResponse.remove_prefix("\r\n\r\n");
		KJSON json;
		kjson::Parse(json, sResponse);
		CHECK ( json.is_object() );
		CHECK ( json["response"] == "hello world" );
	}
#endif

}

