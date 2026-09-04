#include "catch.hpp"

#include <dekaf2/rest/framework/krestserver.h>
#include <dekaf2/net/tcp/ktcpserver.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/data/json/kjson.h>
#include <dekaf2/rest/framework/krest.h>
#include <dekaf2/rest/framework/krestsession.h>
#include <dekaf2/rest/serving/kwebserverpermissions.h>
#include <dekaf2/system/filesystem/kfilesystem.h>
#include <dekaf2/io/readwrite/kwriter.h>
#include <dekaf2/http/server/khttperror.h>
#include <dekaf2/io/compression/kcompression.h>
#include <dekaf2/crypto/auth/ksession.h>
#include <dekaf2/crypto/auth/bits/ksessionmemorystore.h>

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

	SECTION("out-of-range Content-Length rejected")
	{
		// a Content-Length above INT64_MAX wraps to a negative std::streamsize.
		// It must be rejected: a negative size would otherwise bypass the
		// Content-Length/Transfer-Encoding conflict check (which is gated on
		// a non-negative size), re-opening a request smuggling desync.
		KString sRequest =
			"POST /api HTTP/1.1\r\n"
			"Host: localhost\r\n"
			"Content-Type: application/json\r\n"
			"Content-Length: 18446744073709551615\r\n"
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

		CHECK ( sResponse.contains("HTTP/1.1 400") );
	}

	SECTION("malformed Content-Length rejected")
	{
		// Content-Length must be 1*DIGIT (RFC 7230 3.3.2). A lenient parse would
		// read "9abc" as 9 while a front-end might read it differently -> desync.
		KString sRequest =
			"POST /api HTTP/1.1\r\n"
			"Host: localhost\r\n"
			"Content-Type: application/json\r\n"
			"Content-Length: 9abc\r\n"
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

	SECTION("empty JSON containers produce a body")
	{
		// a handler that sets json.tx to an empty array or object must produce
		// [] or {} - strict clients (e.g. a browser's Response.json()) treat an
		// empty body as a parse error, not as an empty result. Only a never
		// touched json.tx produces no body.
		auto Serve = [](std::function<void(KRESTServer&)> Handler, bool bEmitEmptyJsonContainers = true) -> KString
		{
			KString sRequest =
				"GET /test HTTP/1.1\r\n"
				"Host: localhost\r\n"
				"\r\n";

			KString sResponse;
			KInStringStream iss(sRequest);
			KOutStringStream oss(sResponse);
			KStream stream(iss, oss);
			KRESTServer::Options Options;
			// same output in release and debug builds
			Options.bPrettyPrint = true;
			Options.bEmitEmptyJsonContainers = bEmitEmptyJsonContainers;
			KRESTRoutes Routes;
			Routes.AddRoute({ KHTTPMethod::GET, false, "/test", [&](KRESTServer& http)
			{
				Handler(http);
			}});
			KRESTServer Server(stream, "127.0.0.1:1234", url::KProtocol::HTTP, 80, Routes, Options);
			Server.Execute();
			return sResponse;
		};

		auto Body = [](KStringView sResponse) -> KStringView
		{
			auto iPos = sResponse.find("\r\n\r\n");
			return (iPos == KStringView::npos) ? sResponse : sResponse.substr(iPos + 4);
		};

		auto sResponse = Serve([](KRESTServer& http) { http.json.tx = KJSON::array(); });
		CHECK ( sResponse.contains("HTTP/1.1 200") );
		CHECK ( Body(sResponse) == "[]\n" );

		sResponse = Serve([](KRESTServer& http) { http.json.tx = KJSON::object(); });
		CHECK ( sResponse.contains("HTTP/1.1 200") );
		CHECK ( Body(sResponse) == "{}\n" );

		// an untouched json.tx still produces no body at all
		sResponse = Serve([](KRESTServer& http) { });
		CHECK ( sResponse.contains("HTTP/1.1 200") );
		CHECK ( sResponse.contains("content-length: 0") );
		CHECK ( Body(sResponse).empty() );

		// with bEmitEmptyJsonContainers = false, empty containers produce no
		// body either (the behavior for consumers that rely on it)
		sResponse = Serve([](KRESTServer& http) { http.json.tx = KJSON::array(); }, false);
		CHECK ( sResponse.contains("HTTP/1.1 200") );
		CHECK ( sResponse.contains("content-length: 0") );
		CHECK ( Body(sResponse).empty() );

		sResponse = Serve([](KRESTServer& http) { http.json.tx = KJSON::object(); }, false);
		CHECK ( sResponse.contains("HTTP/1.1 200") );
		CHECK ( sResponse.contains("content-length: 0") );
		CHECK ( Body(sResponse).empty() );

		// non-empty json is unaffected by the option
		sResponse = Serve([](KRESTServer& http) { http.json.tx = KJSON { {"key", "value"} }; }, false);
		CHECK ( sResponse.contains("HTTP/1.1 200") );
		CHECK ( Body(sResponse) == "{\n\t\"key\": \"value\"\n}\n" );
	}

	// runs one request stream through a KRESTServer and returns the raw response(s)
	auto RunRequest = [](KStringView sRequest, KRESTRoutes& Routes, KRESTServer::Options& Options) -> KString
	{
		KString sResponse;
		KInStringStream iss(sRequest);
		KOutStringStream oss(sResponse);
		KStream stream(iss, oss);
		KRESTServer Server(stream, "127.0.0.1:1234", url::KProtocol::HTTP, 80, Routes, Options);
		Server.Execute();
		return sResponse;
	};

	SECTION("Transfer-Encoding other than a single chunked is rejected")
	{
		KRESTServer::Options Options;
		KRESTRoutes Routes;
		KString sBody;
		Routes.AddRoute({ KHTTPMethod::POST, false, "/plain", [&](KRESTServer& http)
		{
			sBody = http.GetRequestBody();
			http.json.tx["status"] = "ok";
		}, KRESTRoute::PLAIN });

		// "gzip, chunked" - chunked is not the only coding
		auto sResponse = RunRequest(
			"POST /plain HTTP/1.1\r\n"
			"Host: localhost\r\n"
			"Transfer-Encoding: gzip, chunked\r\n"
			"\r\n"
			"5\r\nhello\r\n0\r\n\r\n", Routes, Options);
		CHECK ( sResponse.contains("HTTP/1.1 400") );

		// two Transfer-Encoding headers
		sResponse = RunRequest(
			"POST /plain HTTP/1.1\r\n"
			"Host: localhost\r\n"
			"Transfer-Encoding: identity\r\n"
			"Transfer-Encoding: chunked\r\n"
			"\r\n"
			"5\r\nhello\r\n0\r\n\r\n", Routes, Options);
		CHECK ( sResponse.contains("HTTP/1.1 400") );

		// "xchunked"
		sResponse = RunRequest(
			"POST /plain HTTP/1.1\r\n"
			"Host: localhost\r\n"
			"Transfer-Encoding: xchunked\r\n"
			"\r\n"
			"5\r\nhello\r\n0\r\n\r\n", Routes, Options);
		CHECK ( sResponse.contains("HTTP/1.1 400") );

		// case insensitive single chunked is fine
		sBody.clear();
		sResponse = RunRequest(
			"POST /plain HTTP/1.1\r\n"
			"Host: localhost\r\n"
			"Transfer-Encoding: Chunked\r\n"
			"\r\n"
			"5\r\nhello\r\n0\r\n\r\n", Routes, Options);
		CHECK ( sResponse.contains("HTTP/1.1 200") );
		CHECK ( sBody == "hello" );
	}

	SECTION("chunk extensions are accepted, framing errors close the connection")
	{
		KRESTServer::Options Options;
		KRESTRoutes Routes;
		KString sBody;
		Routes.AddRoute({ KHTTPMethod::POST, false, "/plain", [&](KRESTServer& http)
		{
			sBody = http.GetRequestBody();
			http.json.tx["status"] = "ok";
		}, KRESTRoute::PLAIN });

		auto sResponse = RunRequest(
			"POST /plain HTTP/1.1\r\n"
			"Host: localhost\r\n"
			"Transfer-Encoding: chunked\r\n"
			"\r\n"
			"5;name=value\r\nhello\r\n0\r\n\r\n", Routes, Options);
		CHECK ( sResponse.contains("HTTP/1.1 200") );
		CHECK ( sResponse.contains("connection: keep-alive") );
		CHECK ( sBody == "hello" );

		// an invalid chunk size - whatever follows must not be taken as
		// the body, and the connection must not be reused
		sBody.clear();
		sResponse = RunRequest(
			"POST /plain HTTP/1.1\r\n"
			"Host: localhost\r\n"
			"Transfer-Encoding: chunked\r\n"
			"\r\n"
			"zz\r\nGET /plain HTTP/1.1\r\nHost: localhost\r\n\r\n", Routes, Options);
		CHECK ( sBody.empty() );
		CHECK ( sResponse.contains("connection: close") );
		// the smuggled GET was not answered
		CHECK ( sResponse.find("HTTP/1.1") == sResponse.rfind("HTTP/1.1") );
	}

	SECTION("multiple Host headers are rejected")
	{
		KRESTServer::Options Options;
		KRESTRoutes Routes;
		Routes.AddRoute({ KHTTPMethod::GET, false, "/test", [&](KRESTServer& http)
		{
			http.json.tx["status"] = "ok";
		}});

		auto sResponse = RunRequest(
			"GET /test HTTP/1.1\r\n"
			"Host: localhost\r\n"
			"Host: evil\r\n"
			"\r\n", Routes, Options);
		CHECK ( sResponse.contains("HTTP/1.1 400") );
		CHECK ( sResponse.contains("multiple Host headers") );
	}

	SECTION("HTTP/2 and HTTP/3 text request lines are rejected")
	{
		KRESTServer::Options Options;
		KRESTRoutes Routes;
		Routes.AddRoute({ KHTTPMethod::GET, false, "/test", [&](KRESTServer& http)
		{
			http.json.tx["status"] = "ok";
		}});

		auto sResponse = RunRequest("GET /test HTTP/2\r\nHost: localhost\r\n\r\n", Routes, Options);
		CHECK ( sResponse.contains("HTTP/1.1 400") );

		sResponse = RunRequest("GET /test HTTP/3\r\nHost: localhost\r\n\r\n", Routes, Options);
		CHECK ( sResponse.contains("HTTP/1.1 400") );

		sResponse = RunRequest("GET /test HTTP/1.1\r\nHost: localhost\r\n\r\n", Routes, Options);
		CHECK ( sResponse.contains("HTTP/1.1 200") );
	}

	SECTION("whitespace between header name and colon is rejected")
	{
		KRESTServer::Options Options;
		KRESTRoutes Routes;
		Routes.AddRoute({ KHTTPMethod::POST, false, "/api", [&](KRESTServer& http)
		{
			http.json.tx["status"] = "ok";
		}});

		auto sResponse = RunRequest(
			"POST /api HTTP/1.1\r\n"
			"Host: localhost\r\n"
			"Content-Length : 2\r\n"
			"\r\n"
			"{}", Routes, Options);
		CHECK ( sResponse.contains("HTTP/1.1 400") );
	}

	SECTION("unread request body is discarded before the next keepalive request")
	{
		KRESTServer::Options Options;
		KRESTRoutes Routes;
		uint16_t iPosts { 0 };
		uint16_t iGets  { 0 };
		Routes.AddRoute({ KHTTPMethod::POST, false, "/noread", [&](KRESTServer& http)
		{
			++iPosts;
			http.json.tx["status"] = "ok";
		}, KRESTRoute::NOREAD });
		Routes.AddRoute({ KHTTPMethod::GET, false, "/test", [&](KRESTServer& http)
		{
			++iGets;
			http.json.tx["status"] = "ok";
		}});

		// the body is not read by the NOREAD route - without discarding it,
		// "hello" would be parsed as the next request line
		auto sResponse = RunRequest(
			"POST /noread HTTP/1.1\r\n"
			"Host: localhost\r\n"
			"Content-Length: 5\r\n"
			"\r\n"
			"hello"
			"GET /test HTTP/1.1\r\n"
			"Host: localhost\r\n"
			"\r\n", Routes, Options);
		CHECK ( iPosts == 1 );
		CHECK ( iGets  == 1 );
		CHECK ( sResponse.find("HTTP/1.1 200") != sResponse.rfind("HTTP/1.1 200") );
		CHECK ( sResponse.contains("HTTP/1.1 400") == false );

		// a large unread body closes the connection after the response instead
		iPosts = iGets = 0;
		KString sLarge(100 * 1024, 'x');
		sResponse = RunRequest(kFormat(
			"POST /noread HTTP/1.1\r\n"
			"Host: localhost\r\n"
			"Content-Length: {}\r\n"
			"\r\n"
			"{}"
			"GET /test HTTP/1.1\r\n"
			"Host: localhost\r\n"
			"\r\n", sLarge.size(), sLarge), Routes, Options);
		CHECK ( iPosts == 1 );
		CHECK ( iGets  == 0 );
		CHECK ( sResponse.contains("HTTP/1.1 200") );
		CHECK ( sResponse.contains("connection: close") );
	}

	SECTION("OPTIONS is exempt from authentication only on a dedicated OPTIONS route")
	{
		KRESTServer::Options Options;
		Options.AuthLevel = KRESTServer::Options::VERIFY_AUTH_HEADER;

		{
			// a route for any method (the INVALID method matches all) that requires SSO -
			// OPTIONS must not slip through
			KRESTRoutes Routes;
			bool bCalled { false };
			Routes.AddRoute({ KHTTPMethod{KHTTPMethod::INVALID}, true, "/secured", [&](KRESTServer& http)
			{
				bCalled = true;
				http.json.tx["status"] = "ok";
			}});

			auto sResponse = RunRequest("OPTIONS /secured HTTP/1.1\r\nHost: localhost\r\n\r\n", Routes, Options);
			CHECK ( sResponse.contains("HTTP/1.1 401") );
			CHECK ( bCalled == false );
		}
		{
			// a dedicated OPTIONS route (CORS preflight) stays open
			KRESTRoutes Routes;
			bool bCalled { false };
			Routes.AddRoute({ KHTTPMethod::OPTIONS, true, "/secured", [&](KRESTServer& http)
			{
				bCalled = true;
				http.json.tx["status"] = "ok";
			}});

			auto sResponse = RunRequest("OPTIONS /secured HTTP/1.1\r\nHost: localhost\r\n\r\n", Routes, Options);
			CHECK ( sResponse.contains("HTTP/1.1 200") );
			CHECK ( bCalled == true );
		}
	}

	SECTION("invalid UTF-8 in the request is not a 500")
	{
		KRESTServer::Options Options;
		KRESTRoutes Routes;
		Routes.AddRoute({ KHTTPMethod::GET, false, "/test", [&](KRESTServer& http)
		{
			http.json.tx["status"] = "ok";
		}});

		// the path lands in the 404 message - a strict JSON serializer would
		// throw on the invalid byte and turn this into a 500
		auto sResponse = RunRequest("GET /%FF%FE HTTP/1.1\r\nHost: localhost\r\n\r\n", Routes, Options);
		CHECK ( sResponse.contains("HTTP/1.1 404") );
		CHECK ( sResponse.contains("HTTP/1.1 500") == false );
	}

	SECTION("status text with CR or LF is replaced")
	{
		KRESTServer::Options Options;
		KRESTRoutes Routes;
		Routes.AddRoute({ KHTTPMethod::GET, false, "/status", [&](KRESTServer& http)
		{
			http.SetStatus(400, "bad\r\nInjected: header");
			http.json.tx["status"] = "bad";
		}});

		auto sResponse = RunRequest("GET /status HTTP/1.1\r\nHost: localhost\r\n\r\n", Routes, Options);
		CHECK ( sResponse.starts_with("HTTP/1.1 400 BAD REQUEST\r\n") );
		CHECK ( sResponse.contains("Injected") == false );
	}

	SECTION("base route is removed only as a whole path segment")
	{
		KRESTServer::Options Options;
		Options.sBaseRoute = "/api";
		KRESTRoutes Routes;
		Routes.AddRoute({ KHTTPMethod::GET, false, "/test", [&](KRESTServer& http)
		{
			http.json.tx["status"] = "ok";
		}});

		auto sResponse = RunRequest("GET /api/test HTTP/1.1\r\nHost: localhost\r\n\r\n", Routes, Options);
		CHECK ( sResponse.contains("HTTP/1.1 200") );

		sResponse = RunRequest("GET /apix/test HTTP/1.1\r\nHost: localhost\r\n\r\n", Routes, Options);
		CHECK ( sResponse.contains("HTTP/1.1 404") );
	}

	SECTION("ad hoc index links are percent and entity encoded")
	{
		KTempDir WebRoot;
		{
			KOutFile OutFile(kFormat("{}/a\"b.html", WebRoot.Name()));
			CHECK ( OutFile.is_open() );
			OutFile.Write("x");
		}
		{
			KOutFile OutFile(kFormat("{}/<x>&y.html", WebRoot.Name()));
			CHECK ( OutFile.is_open() );
			OutFile.Write("x");
		}

		KRESTServer::Options Options;
		KRESTRoutes Routes;
		Routes.AddWebServer(WebRoot.Name(), "/web/*", KWebServerPermissions(KJSON{{ "permissions", "read|browse" }}), KJSON{});

		auto sResponse = RunRequest("GET /web/ HTTP/1.1\r\nHost: localhost\r\n\r\n", Routes, Options);
		CHECK ( sResponse.contains("HTTP/1.1 200") );
		CHECK ( sResponse.contains("href=\"a%22b.html\"") );
		CHECK ( sResponse.contains("href=\"%3Cx%3E%26y.html\"") );
		CHECK ( sResponse.contains("&lt;x&gt;&amp;y.html") );
		CHECK ( sResponse.contains("<x>") == false );
		CHECK ( sResponse.contains("\"a\"b") == false );
	}

	SECTION("KRESTSession LoginTrusted")
	{
		KSession::Config Config;
		Config.sCookieName   = "test_session";
		Config.sCookiePath   = "/";
		Config.bSecure       = false;                 // no "__Host-" prefix rules in the test
		Config.PurgeInterval = KDuration::zero();     // no background timer in the test
		KSession Session(std::make_unique<KSessionMemoryStore>(), Config);
		REQUIRE_FALSE ( Session.HasError() );

		KString sRequest =
			"GET /login HTTP/1.1\r\n"
			"Host: localhost\r\n"
			"User-Agent: utest\r\n"
			"\r\n";

		KString sResponse;
		KInStringStream iss(sRequest);
		KOutStringStream oss(sResponse);
		KStream stream(iss, oss);
		KRESTServer::Options Options;
		KRESTRoutes Routes;

		bool    bLoggedIn { false };
		KString sUser;

		Routes.AddRoute({ KHTTPMethod::GET, false, "/login", [&](KRESTServer& http)
		{
			// no password involved - the identity is vouched for by the caller
			KRESTSession Sess(Session, http);
			bLoggedIn = Sess.LoginTrusted("alice@example.com", R"({"role":"guest"})");
			sUser     = Sess.GetUser();
			http.json.tx["ok"] = bLoggedIn;
		}});

		KRESTServer Server(stream, "127.0.0.1:1234", url::KProtocol::HTTP, 80, Routes, Options);
		Server.Execute();

		CHECK ( bLoggedIn );
		CHECK ( sUser == "alice@example.com" );
		CHECK ( sResponse.contains("HTTP/1.1 200") );
		CHECK ( sResponse.ToLowerASCII().contains("set-cookie: test_session=") );
	}

}
