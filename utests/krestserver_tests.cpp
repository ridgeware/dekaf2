#include "catch.hpp"

#include <dekaf2/rest/framework/krestserver.h>
#include <dekaf2/net/tcp/ktcpserver.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/data/json/kjson.h>
#include <dekaf2/rest/framework/krest.h>
#include <dekaf2/http/server/khttperror.h>
#include <dekaf2/io/compression/kcompression.h>

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
		KRESTServer::Options Options;
		Options.bPrettyPrint = true;
		Options.KLogHeader = "x-klog";
		KRESTRoutes Routes;
		Routes.AddRoute({ KHTTPMethod::GET, false, "/test", [&](KRESTServer& http)
		{
			http.json.tx["response"] = "hello world";
		}});
		KRESTServer Browser(stream, "192.168.178.1:234", url::KProtocol::HTTP, 80, Routes, Options);
		Browser.Execute();

		sResponse.ClipAtReverse("\r\n\r\n");
		sResponse.remove_prefix("\r\n\r\n");
		KJSON json;
		kjson::Parse(json, sResponse);
		CHECK ( json.is_object() );
		CHECK ( json["response"] == "hello world" );
		KJSON& xklog = json["x-klog"];
		CHECK ( xklog.is_array() );
		KJSON& object1 = xklog[1];
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
		KRESTServer::Options Options;
		Options.bPrettyPrint = true;
		Options.KLogHeader = "x-klog";
		KRESTRoutes Routes;
		Routes.AddRoute({ KHTTPMethod::GET, false, "/test", [&](KRESTServer& http)
		{
			http.json.tx["response"] = "hello world";
		}});
		KRESTServer Browser(stream, "192.168.178.1:234", url::KProtocol::HTTP, 80, Routes, Options);
		Browser.Execute();

		CHECK ( sResponse.contains("x-klog-00001: | LVL ") );
		CHECK ( sResponse.contains("KRESTServer::Output(): HTTP-200: OK\r\n") );
		sResponse.ClipAtReverse("\r\n\r\n");
		sResponse.remove_prefix("\r\n\r\n");
		KJSON json;
		kjson::Parse(json, sResponse);
		CHECK ( json.is_object() );
		CHECK ( json["response"] == "hello world" );
	}
#endif

	SECTION("Content-Length exceeds body size limit")
	{
		KString sRequest =
			"POST /api HTTP/1.1\r\n"
			"Host: localhost\r\n"
			"Content-Type: application/json\r\n"
			"Content-Length: 1000\r\n"
			"\r\n";

		KString sResponse;
		KInStringStream iss(sRequest);
		KOutStringStream oss(sResponse);
		KStream stream(iss, oss);
		KRESTServer::Options Options;
		Options.iMaxRequestBodySize = 100;
		KRESTRoutes Routes;
		Routes.AddRoute({ KHTTPMethod::POST, false, "/api", [&](KRESTServer& http)
		{
			http.json.tx["status"] = "ok";
		}});
		KRESTServer Server(stream, "127.0.0.1:1234", url::KProtocol::HTTP, 80, Routes, Options);
		Server.Execute();

		CHECK ( sResponse.contains("HTTP/1.1 413") );
		CHECK ( sResponse.contains("request body too large") );
		CHECK ( sResponse.contains("connection: close") );
	}

	SECTION("POST within body size limit")
	{
		KString sBody = R"({"input":"hello"})";
		KString sRequest = kFormat(
			"POST /api HTTP/1.1\r\n"
			"Host: localhost\r\n"
			"Content-Type: application/json\r\n"
			"Content-Length: {}\r\n"
			"\r\n"
			"{}",
			sBody.size(), sBody);

		KString sResponse;
		KInStringStream iss(sRequest);
		KOutStringStream oss(sResponse);
		KStream stream(iss, oss);
		KRESTServer::Options Options;
		Options.bPrettyPrint = true;
		Options.iMaxRequestBodySize = 1000;
		KRESTRoutes Routes;
		Routes.AddRoute({ KHTTPMethod::POST, false, "/api", [&](KRESTServer& http)
		{
			http.json.tx["status"] = "ok";
		}});
		KRESTServer Server(stream, "127.0.0.1:1234", url::KProtocol::HTTP, 80, Routes, Options);
		Server.Execute();

		CHECK ( sResponse.contains("HTTP/1.1 200") );
		CHECK ( sResponse.contains("\"status\": \"ok\"") );
	}

	SECTION("decompression bomb protection")
	{
		// create a large string that compresses well
		KString sLargeBody(500, 'A');

		// compress it with gzip
		KString sCompressed;
		KGZip gzip(sCompressed);
		gzip.Write(sLargeBody);
		gzip.close();

		// the compressed size should be much smaller than 500
		REQUIRE ( sCompressed.size() < 100 );

		// build an HTTP request with the compressed body
		KString sRequest = kFormat(
			"POST /api HTTP/1.1\r\n"
			"Host: localhost\r\n"
			"Content-Type: application/json\r\n"
			"Content-Encoding: gzip\r\n"
			"Content-Length: {}\r\n"
			"\r\n",
			sCompressed.size());
		sRequest += sCompressed;

		KString sResponse;
		KInStringStream iss(sRequest);
		KOutStringStream oss(sResponse);
		KStream stream(iss, oss);
		KRESTServer::Options Options;
		// set limit below the decompressed size but above the compressed size
		Options.iMaxRequestBodySize = 100;
		KRESTRoutes Routes;
		Routes.AddRoute({ KHTTPMethod::POST, false, "/api", [&](KRESTServer& http)
		{
			http.json.tx["status"] = "ok";
		}, KRESTRoute::PLAIN });
		KRESTServer Server(stream, "127.0.0.1:1234", url::KProtocol::HTTP, 80, Routes, Options);
		Server.Execute();

		CHECK ( sResponse.contains("HTTP/1.1 413") );
		CHECK ( sResponse.contains("decompressed request body exceeds size limit") );
		CHECK ( sResponse.contains("connection: close") );
	}

	SECTION("4xx keepalive error cleanup")
	{
		// test that a 4xx error from a route handler that partially built
		// json.tx gets cleaned up and produces a proper JSON error response
		KString sRequest =
			"GET /bad HTTP/1.1\r\n"
			"Host: localhost\r\n"
			"Connection: keep-alive\r\n"
			"\r\n";

		KString sResponse;
		KInStringStream iss(sRequest);
		KOutStringStream oss(sResponse);
		KStream stream(iss, oss);
		KRESTServer::Options Options;
		Options.bPrettyPrint = true;
		KRESTRoutes Routes;
		Routes.AddRoute({ KHTTPMethod::GET, false, "/bad", [&](KRESTServer& http)
		{
			// partially build output, then throw
			http.json.tx["partial"] = "data";
			http.json.tx["nested"]  = KJSON::object();
			throw KHTTPError { KHTTPError::H4xx_NOTFOUND, "resource not found" };
		}});
		KRESTServer Server(stream, "127.0.0.1:1234", url::KProtocol::HTTP, 80, Routes, Options);
		Server.Execute();

		CHECK ( sResponse.contains("HTTP/1.1 404") );
		CHECK ( sResponse.contains("\"message\": \"resource not found\"") );
		// partial handler output must not leak into the error response
		CHECK ( sResponse.contains("partial") == false );
		CHECK ( sResponse.contains("nested")  == false );
	}

	SECTION("duplicate Content-Length with conflicting values")
	{
		KString sRequest =
			"POST /api HTTP/1.1\r\n"
			"Host: localhost\r\n"
			"Content-Type: application/json\r\n"
			"Content-Length: 10\r\n"
			"Content-Length: 20\r\n"
			"\r\n"
			"{\"a\":\"b\"}";

		KString sResponse;
		KInStringStream iss(sRequest);
		KOutStringStream oss(sResponse);
		KStream stream(iss, oss);
		KRESTServer::Options Options;
		KRESTRoutes Routes;
		Routes.AddRoute({ KHTTPMethod::POST, false, "/api", [&](KRESTServer& http)
		{
			http.json.tx["status"] = "ok";
		}});
		KRESTServer Server(stream, "127.0.0.1:1234", url::KProtocol::HTTP, 80, Routes, Options);
		Server.Execute();

		// must reject - conflicting Content-Length is a request smuggling vector
		CHECK ( sResponse.contains("HTTP/1.1 400") );
		CHECK ( sResponse.contains("duplicate Content-Length") );
	}

	SECTION("duplicate Content-Length with same values accepted")
	{
		KString sBody = R"({"a":"b"})";
		KString sRequest = kFormat(
			"POST /api HTTP/1.1\r\n"
			"Host: localhost\r\n"
			"Content-Type: application/json\r\n"
			"Content-Length: {}\r\n"
			"Content-Length: {}\r\n"
			"\r\n"
			"{}",
			sBody.size(), sBody.size(), sBody);

		KString sResponse;
		KInStringStream iss(sRequest);
		KOutStringStream oss(sResponse);
		KStream stream(iss, oss);
		KRESTServer::Options Options;
		KRESTRoutes Routes;
		Routes.AddRoute({ KHTTPMethod::POST, false, "/api", [&](KRESTServer& http)
		{
			http.json.tx["status"] = "ok";
		}});
		KRESTServer Server(stream, "127.0.0.1:1234", url::KProtocol::HTTP, 80, Routes, Options);
		Server.Execute();

		// same values should be accepted (RFC 7230 §3.3.3)
		CHECK ( sResponse.contains("HTTP/1.1 200") );
	}

	SECTION("Content-Length + Transfer-Encoding conflict rejected")
	{
		KString sRequest =
			"POST /api HTTP/1.1\r\n"
			"Host: localhost\r\n"
			"Content-Type: application/json\r\n"
			"Content-Length: 10\r\n"
			"Transfer-Encoding: chunked\r\n"
			"\r\n"
			"a\r\n"
			"{\"a\":\"b\"}\r\n"
			"0\r\n"
			"\r\n";

		KString sResponse;
		KInStringStream iss(sRequest);
		KOutStringStream oss(sResponse);
		KStream stream(iss, oss);
		KRESTServer::Options Options;
		KRESTRoutes Routes;
		Routes.AddRoute({ KHTTPMethod::POST, false, "/api", [&](KRESTServer& http)
		{
			http.json.tx["status"] = "ok";
		}});
		KRESTServer Server(stream, "127.0.0.1:1234", url::KProtocol::HTTP, 80, Routes, Options);
		Server.Execute();

		// must reject on server side - CL+TE is a request smuggling vector (RFC 7230 §3.3.3)
		CHECK ( sResponse.contains("HTTP/1.1 400") );
	}

	SECTION("4xx error sets Content-Type to JSON")
	{
		// test that the Content-Type is reset to JSON when a handler
		// changes it (e.g. to XML) and then throws a 4xx error
		KString sRequest =
			"GET /xmlerror HTTP/1.1\r\n"
			"Host: localhost\r\n"
			"\r\n";

		KString sResponse;
		KInStringStream iss(sRequest);
		KOutStringStream oss(sResponse);
		KStream stream(iss, oss);
		KRESTServer::Options Options;
		Options.bPrettyPrint = true;
		KRESTRoutes Routes;
		Routes.AddRoute({ KHTTPMethod::GET, false, "/xmlerror", [&](KRESTServer& http)
		{
			http.Response.Headers.Set(KHTTPHeader::CONTENT_TYPE, KMIME::XML);
			throw KHTTPError { KHTTPError::H4xx_BADREQUEST, "bad request" };
		}});
		KRESTServer Server(stream, "127.0.0.1:1234", url::KProtocol::HTTP, 80, Routes, Options);
		Server.Execute();

		CHECK ( sResponse.contains("HTTP/1.1 400") );
		CHECK ( sResponse.contains("content-type: application/json") );
		// must not contain the XML content-type the handler set
		CHECK ( sResponse.contains("application/xml") == false );
	}

}
