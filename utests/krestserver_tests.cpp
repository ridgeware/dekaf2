#include "catch.hpp"
#include <random>

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
#include <dekaf2/http/protocol/khttp_response.h>
#include <dekaf2/io/streams/kinstringstream.h>
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

	SECTION("Expect: 100-continue")
	{
		// a client that sent "Expect: 100-continue" gets the interim response
		// before the final one, unless a final status is pending anyway
		// (e.g. no such route) or the request is HTTP/1.0
		auto Serve = [](KStringView sRequest, KRESTServer::OutputType Out = KRESTServer::HTTP) -> KString
		{
			KString sResponse;
			KInStringStream iss(sRequest);
			KOutStringStream oss(sResponse);
			KStream stream(iss, oss);
			KRESTServer::Options Options;
			Options.bPrettyPrint = true;
			Options.Out = Out;
			KRESTRoutes Routes;
			Routes.AddRoute({ KHTTPMethod::POST, false, "/api", [&](KRESTServer& http)
			{
				http.json.tx["status"] = "ok";
			}});
			KRESTServer Server(stream, "127.0.0.1:1234", url::KProtocol::HTTP, 80, Routes, Options);
			Server.Execute();
			return sResponse;
		};

		KString sBody = R"({"input":"hello"})";

		auto sResponse = Serve(kFormat(
			"POST /api HTTP/1.1\r\n"
			"Host: localhost\r\n"
			"Expect: 100-continue\r\n"
			"Content-Type: application/json\r\n"
			"Content-Length: {}\r\n"
			"\r\n"
			"{}",
			sBody.size(), sBody));

		CHECK ( sResponse.starts_with("HTTP/1.1 100 Continue\r\n\r\nHTTP/1.1 200") );
		CHECK ( sResponse.contains("\"status\": \"ok\"") );

		// no such route: the final status is pending, no interim response
		sResponse = Serve(kFormat(
			"POST /nowhere HTTP/1.1\r\n"
			"Host: localhost\r\n"
			"Expect: 100-continue\r\n"
			"Content-Type: application/json\r\n"
			"Content-Length: {}\r\n"
			"\r\n"
			"{}",
			sBody.size(), sBody));

		CHECK ( sResponse.starts_with("HTTP/1.1 404") );
		CHECK_FALSE ( sResponse.contains("100 Continue") );

		// HTTP/1.0 has no interim responses
		sResponse = Serve(kFormat(
			"POST /api HTTP/1.0\r\n"
			"Host: localhost\r\n"
			"Expect: 100-continue\r\n"
			"Content-Type: application/json\r\n"
			"Content-Length: {}\r\n"
			"\r\n"
			"{}",
			sBody.size(), sBody));

		CHECK ( sResponse.starts_with("HTTP/1.1 200") );
		CHECK_FALSE ( sResponse.contains("100 Continue") );

		// CGI, parsed or NPH: the web server owns the connection and has answered
		// the expectation itself
		for (auto Out : { KRESTServer::CGI, KRESTServer::NPH })
		{
			sResponse = Serve(kFormat(
				"POST /api HTTP/1.1\r\n"
				"Host: localhost\r\n"
				"Expect: 100-continue\r\n"
				"Content-Type: application/json\r\n"
				"Content-Length: {}\r\n"
				"\r\n"
				"{}",
				sBody.size(), sBody), Out);

			CHECK ( sResponse.contains(" 200") );
			CHECK_FALSE ( sResponse.contains("100 Continue") );
		}
	}

	SECTION("websocket upgrade behind a web server")
	{
		// in CGI mode the web server owns the connection - a protocol upgrade
		// cannot be honored, whereas the same request on a direct connection
		// switches protocols
		auto Serve = [](KRESTServer::OutputType Out) -> KString
		{
			KString sRequest =
				"GET /ws HTTP/1.1\r\n"
				"Host: localhost\r\n"
				"Upgrade: websocket\r\n"
				"Connection: Upgrade\r\n"
				"Sec-WebSocket-Version: 13\r\n"
				"Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
				"\r\n";

			KString sResponse;
			KInStringStream iss(sRequest);
			KOutStringStream oss(sResponse);
			KStream stream(iss, oss);
			KRESTServer::Options Options;
			Options.Out = Out;
			KRESTRoutes Routes;
			Routes.AddRoute({ KHTTPMethod::GET, { KRESTRoute::Options::WEBSOCKET }, "/ws", [&](KRESTServer& http)
			{
				http.SetWebSocketHandler([](KWebSocket&) {});
			}});
			KRESTServer Server(stream, "127.0.0.1:1234", url::KProtocol::HTTP, 80, Routes, Options);
			Server.Execute();
			return sResponse;
		};

		CHECK ( Serve(KRESTServer::HTTP).starts_with("HTTP/1.1 101") );

		for (auto Out : { KRESTServer::CGI, KRESTServer::NPH })
		{
			auto sResponse = Serve(Out);
			CHECK_FALSE ( sResponse.contains(" 101") );
			CHECK ( sResponse.contains("bad mode") );
		}
	}

	SECTION("compression: HTTP and NPH compress, parsed CGI does not")
	{
		// an NPH response goes to the client unparsed, so it is framed exactly like
		// on a connection we own - compression and chunking included. With parsed
		// headers the web server frames and compresses, a chunked body would break
		auto Serve = [](KRESTServer::OutputType Out) -> KString
		{
			KString sRequest =
				"GET /big HTTP/1.1\r\n"
				"Host: localhost\r\n"
				"Accept-Encoding: gzip, deflate, br\r\n"
				"\r\n";

			KString sResponse;
			KInStringStream iss(sRequest);
			KOutStringStream oss(sResponse);
			KStream stream(iss, oss);
			KRESTServer::Options Options;
			Options.Out = Out;
			KRESTRoutes Routes;
			Routes.AddRoute({ KHTTPMethod::GET, false, "/big", [&](KRESTServer& http)
			{
				http.json.tx["data"] = KString(4000, 'x');
			}});
			KRESTServer Server(stream, "127.0.0.1:1234", url::KProtocol::HTTP, 80, Routes, Options);
			Server.Execute();
			return sResponse.ToLowerASCII();
		};

		// on a connection we own the response is compressed and chunked
		auto sHTTP = Serve(KRESTServer::HTTP);
		CHECK ( sHTTP.contains("content-encoding:") );
		CHECK ( sHTTP.contains("transfer-encoding: chunked") );

		// and so is the NPH response
		auto sNPH = Serve(KRESTServer::NPH);
		CHECK ( sNPH.starts_with("http/1.1 200") );
		CHECK ( sNPH.contains("content-encoding:") );
		CHECK ( sNPH.contains("transfer-encoding: chunked") );

		// parsed headers: neither, the body has a length
		auto sCGI = Serve(KRESTServer::CGI);
		CHECK ( sCGI.starts_with("status: 200") );
		CHECK_FALSE ( sCGI.contains("content-encoding:") );
		CHECK_FALSE ( sCGI.contains("transfer-encoding:") );
		CHECK ( sCGI.contains("content-length:") );
	}

	SECTION("compression: Vary, quality values, and already encoded content")
	{
		auto Serve = [](KStringView sRequestHeaders, const KRESTRoute::RESTCallback& Handler) -> KString
		{
			KString sRequest = "GET /data HTTP/1.1\r\nHost: localhost\r\n";
			sRequest += sRequestHeaders;
			sRequest += "\r\n";

			KString sResponse;
			KInStringStream iss(sRequest);
			KOutStringStream oss(sResponse);
			KStream stream(iss, oss);
			KRESTServer::Options Options;
			KRESTRoutes Routes;
			Routes.AddRoute({ KHTTPMethod::GET, false, "/data", Handler });
			KRESTServer Server(stream, "127.0.0.1:1234", url::KProtocol::HTTP, 80, Routes, Options);
			Server.Execute();
			return sResponse;
		};

		auto JsonHandler = [](KRESTServer& http)
		{
			http.json.tx["data"] = KString(4000, 'x');
		};

		// q=0 excludes gzip, the response depends on Accept-Encoding
		auto sResponse = Serve("Accept-Encoding: gzip;q=0, deflate\r\n", JsonHandler).ToLowerASCII();
		CHECK ( sResponse.contains("content-encoding: deflate\r\n") );
		CHECK ( sResponse.contains("vary: accept-encoding\r\n") );

		// without Accept-Encoding the response is not compressed, and its headers
		// stay as they were without compression support
		sResponse = Serve("", JsonHandler).ToLowerASCII();
		CHECK_FALSE ( sResponse.contains("content-encoding:") );
		CHECK_FALSE ( sResponse.contains("vary:") );

		// a handler that sends already encoded content switches compression off -
		// the Content-Encoding, the Content-Length and the body stay as they are
		sResponse = Serve("Accept-Encoding: gzip, deflate\r\n", [](KRESTServer& http)
		{
			http.Response.Headers.Set(KHTTPHeader::CONTENT_TYPE, KMIME::TEXT_UTF8);
			http.Response.Headers.Set(KHTTPHeader::CONTENT_ENCODING, "gzip");
			http.AllowCompression(false);
			http.SetRawOutput("encoded bytes");
		});
		CHECK ( sResponse.ToLowerASCII().contains("content-encoding: gzip\r\n") );
		CHECK ( sResponse.ToLowerASCII().contains("content-length: 13\r\n") );
		CHECK_FALSE ( sResponse.ToLowerASCII().contains("transfer-encoding:") );
		CHECK ( sResponse.ends_with("\r\n\r\nencoded bytes") );

		// a Content-Encoding without AllowCompression(false), like one copied from a
		// proxied response whose body the client has already uncompressed, is replaced
		// by the negotiated compression
		sResponse = Serve("Accept-Encoding: deflate\r\n", [](KRESTServer& http)
		{
			http.Response.Headers.Set(KHTTPHeader::CONTENT_ENCODING, "gzip");
			http.json.tx["data"] = KString(4000, 'x');
		}).ToLowerASCII();
		CHECK ( sResponse.contains("content-encoding: deflate\r\n") );
		CHECK_FALSE ( sResponse.contains("content-encoding: gzip") );
		CHECK ( sResponse.contains("transfer-encoding: chunked\r\n") );
		CHECK ( sResponse.contains("vary: accept-encoding\r\n") );

		// AllowCompression(false) without a Content-Encoding sends the content as it is
		sResponse = Serve("Accept-Encoding: gzip, deflate\r\n", [](KRESTServer& http)
		{
			http.AllowCompression(false);
			http.json.tx["data"] = KString(4000, 'x');
		}).ToLowerASCII();
		CHECK_FALSE ( sResponse.contains("content-encoding:") );
		CHECK_FALSE ( sResponse.contains("transfer-encoding:") );
		CHECK_FALSE ( sResponse.contains("vary:") );
		CHECK ( sResponse.contains("content-length:") );
		CHECK ( sResponse.contains(KString(4000, 'x')) );
	}

	SECTION("LAMBDA multiValueHeaders")
	{
		// payload format 1.0 transports repeated response headers (Set-Cookie) only
		// through multiValueHeaders - headers keeps one value per name
		KString sRequest =
			"GET /login HTTP/1.1\r\n"
			"Host: localhost\r\n"
			"\r\n";

		KString sResponse;
		KInStringStream iss(sRequest);
		KOutStringStream oss(sResponse);
		KStream stream(iss, oss);
		KRESTServer::Options Options;
		Options.Out = KRESTServer::LAMBDA;
		KRESTRoutes Routes;
		Routes.AddRoute({ KHTTPMethod::GET, false, "/login", [&](KRESTServer& http)
		{
			http.Response.Headers.Add(KHTTPHeader::SET_COOKIE, "session=abc; Path=/");
			http.Response.Headers.Add(KHTTPHeader::SET_COOKIE, "theme=dark; Path=/");
			http.json.tx["ok"] = true;
		}});
		KRESTServer Server(stream, "127.0.0.1:1234", url::KProtocol::HTTP, 80, Routes, Options);
		Server.Execute();

		KJSON jResponse;
		kjson::Parse(jResponse, sResponse);
		REQUIRE ( jResponse.is_object() );
		CHECK ( jResponse["statusCode"] == 200 );
		CHECK ( jResponse["headers"].is_object() );
		// single valued headers stay in headers, and only there
		CHECK ( jResponse["headers"]["content-type"].is_string() );
		CHECK ( jResponse["multiValueHeaders"]["content-type"].is_null() );
		// the repeated header moves to multiValueHeaders, and only there
		CHECK ( jResponse["headers"]["set-cookie"].is_null() );

		const KJSON& jCookies = jResponse["multiValueHeaders"]["set-cookie"];
		REQUIRE ( jCookies.is_array() );
		CHECK ( jCookies.size() == 2 );
		CHECK ( jCookies[0] == "session=abc; Path=/" );
		CHECK ( jCookies[1] == "theme=dark; Path=/" );
	}

	SECTION("CGI response head: parsed vs NPH")
	{
		// NPH (xapis installs as nph-xapis.cgi): the web server passes the response
		// through unparsed, so it carries the HTTP status line and the Connection
		// header. Parsed headers (RFC 3875 6.3.3): a Status: field, no status line,
		// no connection fields. Both on the regular and on the error path
		auto Serve = [](KStringView sPath, KRESTServer::OutputType Out) -> KString
		{
			KString sRequest = kFormat(
				"GET {} HTTP/1.1\r\n"
				"Host: localhost\r\n"
				"\r\n", sPath);

			KString sResponse;
			KInStringStream iss(sRequest);
			KOutStringStream oss(sResponse);
			KStream stream(iss, oss);
			KRESTServer::Options Options;
			Options.Out = Out;
			KRESTRoutes Routes;
			Routes.AddRoute({ KHTTPMethod::GET, false, "/test", [&](KRESTServer& http)
			{
				http.json.tx["response"] = "hello world";
			}});
			KRESTServer Server(stream, "127.0.0.1:1234", url::KProtocol::HTTP, 80, Routes, Options);
			Server.Execute();
			return sResponse;
		};

		auto sResponse = Serve("/test", KRESTServer::NPH);
		CHECK ( sResponse.starts_with("HTTP/1.1 200 OK\r\n") );
		CHECK ( sResponse.ToLowerASCII().contains("connection: close") );
		CHECK ( sResponse.contains("\"response\"") );

		sResponse = Serve("/nowhere", KRESTServer::NPH);
		CHECK ( sResponse.starts_with("HTTP/1.1 404") );
		CHECK ( sResponse.ToLowerASCII().contains("connection: close") );

		sResponse = Serve("/test", KRESTServer::CGI);
		CHECK ( sResponse.starts_with("Status: 200 OK\r\n") );
		CHECK_FALSE ( sResponse.contains("HTTP/1.1") );
		CHECK_FALSE ( sResponse.ToLowerASCII().contains("connection:") );
		CHECK ( sResponse.contains("\"response\"") );

		sResponse = Serve("/nowhere", KRESTServer::CGI);
		CHECK ( sResponse.starts_with("Status: 404") );
		CHECK_FALSE ( sResponse.contains("HTTP/1.1") );
		CHECK_FALSE ( sResponse.ToLowerASCII().contains("connection:") );
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
		// a space and an ampersand need the percent encoding in the link, the
		// ampersand also the entity in the text. Quote and angle brackets are the
		// dangerous characters for the markup, but Windows forbids them in file
		// names, so only the POSIX systems test them
		struct Entry { KStringView sFile; KStringView sHref; KStringView sText; };

		std::vector<Entry> Entries
		{
			{ "a b.html",   "href=\"a%20b.html\"",       "a b.html"             },
			{ "x&y.html",   "href=\"x%26y.html\"",       "x&amp;y.html"         },
#if !DEKAF2_IS_WINDOWS
			{ "a\"b.html",  "href=\"a%22b.html\"",       {}                     },
			{ "<x>&y.html", "href=\"%3Cx%3E%26y.html\"", "&lt;x&gt;&amp;y.html" },
#endif
		};

		KTempDir WebRoot;

		for (const auto& Entry : Entries)
		{
			KOutFile OutFile(kFormat("{}/{}", WebRoot.Name(), Entry.sFile));
			CHECK ( OutFile.is_open() );
			OutFile.Write("x");
		}

		KRESTServer::Options Options;
		KRESTRoutes Routes;
		Routes.AddWebServer(WebRoot.Name(), "/web/*", KWebServerPermissions(KJSON{{ "permissions", "read|browse" }}), KJSON{});

		auto sResponse = RunRequest("GET /web/ HTTP/1.1\r\nHost: localhost\r\n\r\n", Routes, Options);
		CHECK ( sResponse.contains("HTTP/1.1 200") );

		for (const auto& Entry : Entries)
		{
			INFO ( "file: " << Entry.sFile );
			CHECK ( sResponse.contains(Entry.sHref) );

			if (!Entry.sText.empty())
			{
				CHECK ( sResponse.contains(Entry.sText) );
			}
		}

		// nothing reaches the page unencoded
		CHECK ( sResponse.contains("<x>")      == false );
		CHECK ( sResponse.contains("\"a\"b")   == false );
		CHECK ( sResponse.contains("x&y.html") == false );
	}

	SECTION("web server: HEAD, ranges and compression")
	{
		KTempDir WebRoot;
		{
			KOutFile OutFile(kFormat("{}/data.txt", WebRoot.Name()));
			CHECK ( OutFile.is_open() );
			OutFile.Write(KString(3000, 'x'));
		}

		KRESTServer::Options Options;
		KRESTRoutes Routes;
		Routes.AddWebServer(WebRoot.Name(), "/web/*", KWebServerPermissions(KJSON{{ "permissions", "read|browse" }}), KJSON{});

		auto Request = [&](KStringView sMethod, KStringView sHeaders)
		{
			return RunRequest(kFormat("{} /web/data.txt HTTP/1.1\r\nHost: localhost\r\n{}\r\n", sMethod, sHeaders), Routes, Options);
		};

		// HEAD sends the headers of a GET, without a body
		auto sResponse = Request("HEAD", "Accept-Encoding: gzip\r\n").ToLowerASCII();
		CHECK ( sResponse.starts_with("http/1.1 200") );
		CHECK ( sResponse.contains("content-length: 3000\r\n") );
		CHECK ( sResponse.contains("content-type: text/plain") );
		CHECK ( sResponse.contains("last-modified: ") );
		CHECK ( sResponse.contains("accept-ranges: bytes\r\n") );
		CHECK ( sResponse.ends_with("\r\n\r\n") );

		// HEAD ignores a Range header
		sResponse = Request("HEAD", "Range: bytes=0-9\r\n").ToLowerASCII();
		CHECK ( sResponse.starts_with("http/1.1 200") );
		CHECK ( sResponse.contains("content-length: 3000\r\n") );
		CHECK_FALSE ( sResponse.contains("content-range:") );

		// HEAD evaluates conditional requests like GET
		sResponse = Request("HEAD", "If-Modified-Since: Fri, 01 Jan 2100 00:00:00 GMT\r\n").ToLowerASCII();
		CHECK ( sResponse.starts_with("http/1.1 304") );

		// a partial response is not compressed
		sResponse = Request("GET", "Range: bytes=0-9\r\nAccept-Encoding: gzip, deflate\r\n");
		CHECK ( sResponse.starts_with("HTTP/1.1 206") );
		CHECK ( sResponse.ToLowerASCII().contains("content-range: bytes 0-9/3000\r\n") );
		CHECK ( sResponse.ToLowerASCII().contains("content-length: 10\r\n") );
		CHECK_FALSE ( sResponse.ToLowerASCII().contains("content-encoding:") );
		CHECK_FALSE ( sResponse.ToLowerASCII().contains("transfer-encoding:") );
		CHECK ( sResponse.ends_with("\r\n\r\nxxxxxxxxxx") );

		// the complete file is compressed
		sResponse = Request("GET", "Accept-Encoding: gzip\r\n").ToLowerASCII();
		CHECK ( sResponse.starts_with("http/1.1 200") );
		CHECK ( sResponse.contains("content-encoding: gzip\r\n") );
		CHECK ( sResponse.contains("vary: accept-encoding\r\n") );
	}

	SECTION("web server: compression cache")
	{
		KTempDir WebRoot;
		KTempDir CacheRoot;

		KString sText;

		for (int i = 0; i < 2000; ++i)
		{
			sText += kFormat("line {} of a file that compresses well\n", i);
		}

		KString sRandom;
		std::mt19937 Random(42);

		for (int i = 0; i < 16 * 1024; ++i)
		{
			sRandom += static_cast<char>(Random() & 0xff);
		}

		for (auto sFile : { "app.js", "other.js", "stream.js", "keepalive.js", "safari.js", "safari2.js" })
		{
			REQUIRE ( kWriteFile(kFormat("{}/{}", WebRoot.Name(), sFile), sText) );
		}

		REQUIRE ( kWriteFile(kFormat("{}/small.js",   WebRoot.Name()), "var x = 1;\n") );
		REQUIRE ( kWriteFile(kFormat("{}/random.txt", WebRoot.Name()), sRandom) );

		auto AddWebServer = [&](KRESTRoutes& Routes, uint64_t iDeadline)
		{
			Routes.AddWebServer(WebRoot.Name(), "/web/*", KWebServerPermissions(KJSON{{ "permissions", "read|browse" }}),
			                    KJSON{{ "compression_cache", CacheRoot.Name() }, { "compression_deadline", iDeadline }});
		};

		// a pass that ends before the deadline, and one that streams at once
		KRESTRoutes Routes;
		KRESTRoutes StreamRoutes;
		AddWebServer(Routes, 60);
		AddWebServer(StreamRoutes, 0);

		auto Request = [&](KRESTRoutes& Routes, KStringView sPath, KStringView sHeaders, KStringView sMethod = "GET", KStringView sVersion = "HTTP/1.1")
		{
			KRESTServer::Options Options;
			return RunRequest(kFormat("{} /web/{} {}\r\nHost: localhost\r\n{}\r\n", sMethod, sPath, sVersion, sHeaders), Routes, Options);
		};

		struct Parsed
		{
			uint16_t                 iStatus { 0 };
			KHTTPHeaders::KHeaderMap Headers;
			KString                  sBody;
		};

		// parses a response, and reads its body without the transfer and content encoding
		auto Parse = [](KStringView sResponse, bool bReadBody = true)
		{
			Parsed Result;
			KInStringStream iss(sResponse);
			KInHTTPResponse Response(iss);

			if (Response.Parse())
			{
				Result.iStatus = Response.GetStatusCode();
				Result.Headers = Response.Headers;

				if (bReadBody)
				{
					Response.Read(Result.sBody);
				}
			}

			return Result;
		};

		// the first request compresses into the cache, the entry is sent with a Content-Length
		auto sResponse = Request(Routes, "app.js", "Accept-Encoding: gzip\r\n");
		auto First     = Parse(sResponse);
		CHECK ( First.iStatus == 200 );
		CHECK ( First.Headers.Get(KHTTPHeader::CONTENT_ENCODING) == "gzip" );
		CHECK ( First.Headers.Get(KHTTPHeader::VARY) == "accept-encoding" );
		CHECK ( First.Headers.Get(KHTTPHeader::TRANSFER_ENCODING).empty() );
		CHECK ( First.Headers.Get(KHTTPHeader::CONTENT_LENGTH).UInt64() < sText.size() );
		CHECK ( First.sBody == sText );

		// the second request finds the entry
		auto Second = Parse(Request(Routes, "app.js", "Accept-Encoding: gzip\r\n"));
		CHECK ( Second.Headers.Get(KHTTPHeader::CONTENT_ENCODING) == "gzip" );
		CHECK ( Second.Headers.Get(KHTTPHeader::CONTENT_LENGTH) == First.Headers.Get(KHTTPHeader::CONTENT_LENGTH) );
		CHECK ( Second.sBody == sText );

		// HEAD sends the headers of the entry
		sResponse = Request(Routes, "app.js", "Accept-Encoding: gzip\r\n", "HEAD");
		auto Head = Parse(sResponse, false);
		CHECK ( Head.Headers.Get(KHTTPHeader::CONTENT_ENCODING) == "gzip" );
		CHECK ( Head.Headers.Get(KHTTPHeader::CONTENT_LENGTH) == First.Headers.Get(KHTTPHeader::CONTENT_LENGTH) );
		CHECK ( sResponse.ends_with("\r\n\r\n") );

		// HEAD without an entry sends the headers of the uncompressed file, and does not compress
		Head = Parse(Request(Routes, "other.js", "Accept-Encoding: gzip\r\n", "HEAD"), false);
		CHECK ( Head.Headers.Get(KHTTPHeader::CONTENT_ENCODING).empty() );
		CHECK ( Head.Headers.Get(KHTTPHeader::VARY).empty() );
		CHECK ( Head.Headers.Get(KHTTPHeader::CONTENT_LENGTH).UInt64() == sText.size() );

		// an HTTP/1.0 client cannot receive a streamed pass: no new entry, and no compression
		auto Old = Parse(Request(Routes, "other.js", "Accept-Encoding: gzip\r\n", "GET", "HTTP/1.0"));
		CHECK ( Old.Headers.Get(KHTTPHeader::CONTENT_ENCODING).empty() );
		CHECK ( Old.sBody == sText );

		// but it gets an existing entry, with a Content-Length
		Old = Parse(Request(Routes, "app.js", "Accept-Encoding: gzip\r\n", "GET", "HTTP/1.0"));
		CHECK ( Old.Headers.Get(KHTTPHeader::CONTENT_ENCODING) == "gzip" );
		CHECK ( Old.sBody == sText );

		// without Accept-Encoding the file is sent uncompressed, without Vary
		auto Plain = Parse(Request(Routes, "app.js", ""));
		CHECK ( Plain.Headers.Get(KHTTPHeader::CONTENT_ENCODING).empty() );
		CHECK ( Plain.Headers.Get(KHTTPHeader::VARY).empty() );
		CHECK ( Plain.sBody == sText );

		// a compression the cache does not have is not applied on the fly either
		Plain = Parse(Request(Routes, "app.js", "Accept-Encoding: deflate\r\n"));
		CHECK ( Plain.Headers.Get(KHTTPHeader::CONTENT_ENCODING).empty() );
		CHECK ( Plain.sBody == sText );

		// a range request gets the uncompressed file
		auto Range = Parse(Request(Routes, "app.js", "Range: bytes=0-9\r\nAccept-Encoding: gzip\r\n"));
		CHECK ( Range.iStatus == 206 );
		CHECK ( Range.Headers.Get(KHTTPHeader::CONTENT_ENCODING).empty() );
		CHECK ( Range.sBody == sText.ToView(0, 10) );

		// a small file, and a file that does not get smaller, are sent uncompressed
		auto Small = Parse(Request(Routes, "small.js", "Accept-Encoding: gzip\r\n"));
		CHECK ( Small.Headers.Get(KHTTPHeader::CONTENT_ENCODING).empty() );
		CHECK ( Small.sBody == "var x = 1;\n" );
		auto Incompressible = Parse(Request(Routes, "random.txt", "Accept-Encoding: gzip\r\n"));
		CHECK ( Incompressible.Headers.Get(KHTTPHeader::CONTENT_ENCODING).empty() );
		CHECK ( Incompressible.sBody == sRandom );

		// a 304 carries Vary when the 200 would be compressed
		sResponse = Request(Routes, "app.js", "Accept-Encoding: gzip\r\nIf-Modified-Since: Fri, 01 Jan 2100 00:00:00 GMT\r\n");
		CHECK ( sResponse.starts_with("HTTP/1.1 304") );
		CHECK ( sResponse.contains("vary: accept-encoding\r\n") );
		sResponse = Request(Routes, "app.js", "If-Modified-Since: Fri, 01 Jan 2100 00:00:00 GMT\r\n");
		CHECK ( sResponse.starts_with("HTTP/1.1 304") );
		CHECK_FALSE ( sResponse.contains("vary:") );

		// with the deadline passed the pass streams the compressed data, chunked
		auto Streamed = Parse(Request(StreamRoutes, "stream.js", "Accept-Encoding: gzip\r\n"));
		CHECK ( Streamed.iStatus == 200 );
		CHECK ( Streamed.Headers.Get(KHTTPHeader::CONTENT_ENCODING)  == "gzip" );
		CHECK ( Streamed.Headers.Get(KHTTPHeader::TRANSFER_ENCODING) == "chunked" );
		CHECK ( Streamed.Headers.Get(KHTTPHeader::CONTENT_LENGTH).empty() );
		CHECK ( Streamed.Headers.Get(KHTTPHeader::VARY) == "accept-encoding" );
		CHECK ( Streamed.sBody == sText );

		// and completed the entry at the same time
		auto Stored = Parse(Request(StreamRoutes, "stream.js", "Accept-Encoding: gzip\r\n"));
		CHECK ( Stored.Headers.Get(KHTTPHeader::TRANSFER_ENCODING).empty() );
		CHECK ( Stored.Headers.Get(KHTTPHeader::CONTENT_LENGTH).UInt64() > 0 );
		CHECK ( Stored.sBody == sText );

		// the streamed response ends properly, the next request on the connection gets its answer
		{
			KRESTServer::Options Options;
			sResponse = RunRequest("GET /web/keepalive.js HTTP/1.1\r\nHost: localhost\r\nAccept-Encoding: gzip\r\n\r\n"
			                       "GET /web/keepalive.js HTTP/1.1\r\nHost: localhost\r\nAccept-Encoding: gzip\r\nConnection: close\r\n\r\n",
			                       StreamRoutes, Options);

			auto iSecond = sResponse.find("\r\n0\r\n\r\nHTTP/1.1 200");
			REQUIRE ( iSecond != KString::npos );
			iSecond += 7;

			auto KeepAliveFirst  = Parse(sResponse.ToView(0, iSecond));
			auto KeepAliveSecond = Parse(sResponse.ToView(iSecond));
			CHECK ( KeepAliveFirst.Headers.Get(KHTTPHeader::TRANSFER_ENCODING) == "chunked" );
			CHECK ( KeepAliveFirst.sBody  == sText );
			CHECK ( KeepAliveSecond.Headers.Get(KHTTPHeader::CONTENT_ENCODING) == "gzip" );
			CHECK ( KeepAliveSecond.sBody == sText );
		}

#if defined(DEKAF2_HAS_LIBZSTD) && defined(DEKAF2_HAS_LIBBROTLI)
		constexpr KStringView sSafari26 = "User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/26.3.1 Safari/605.1.15\r\n";

		// a zstd entry, created for a client without the Safari bug
		auto Zstd = Parse(Request(Routes, "safari.js", "Accept-Encoding: zstd, br, gzip\r\n"));
		CHECK ( Zstd.Headers.Get(KHTTPHeader::CONTENT_ENCODING) == "zstd" );
		CHECK ( Zstd.sBody == sText );

		// Safari 26 gets the existing zstd entry, because it has a Content-Length
		auto Safari = Parse(Request(Routes, "safari.js", kFormat("Accept-Encoding: zstd, br, gzip\r\n{}", sSafari26)));
		CHECK ( Safari.Headers.Get(KHTTPHeader::CONTENT_ENCODING) == "zstd" );
		CHECK ( Safari.Headers.Get(KHTTPHeader::TRANSFER_ENCODING).empty() );
		CHECK ( Safari.sBody == sText );

		// but a streamed pass for Safari 26 uses brotli
		Safari = Parse(Request(StreamRoutes, "safari2.js", kFormat("Accept-Encoding: zstd, br, gzip\r\n{}", sSafari26)));
		CHECK ( Safari.Headers.Get(KHTTPHeader::CONTENT_ENCODING)  == "br" );
		CHECK ( Safari.Headers.Get(KHTTPHeader::TRANSFER_ENCODING) == "chunked" );
		CHECK ( Safari.sBody == sText );
#endif
	}

	SECTION("web server: uploads and deletions remove cache entries")
	{
		KTempDir WebRoot;
		KTempDir CacheRoot;

		KString sText;

		for (int i = 0; i < 2000; ++i)
		{
			sText += kFormat("line {} of a file that compresses well\n", i);
		}

		KString sChanged = sText + "one more line\n";

		REQUIRE ( kCreateDir(kFormat("{}/lib", WebRoot.Name())) );

		KJSON jConfig {{ "compression_cache", CacheRoot.Name() }, { "compression_deadline", 60 }};
		KWebServerPermissions Permissions(KJSON{{ "permissions", "read|browse|write|erase" }});

		KRESTRoutes Routes;
		Routes.AddWebServer(WebRoot.Name(), "/web/*", Permissions, jConfig);
		Routes.AddWebDAV   (WebRoot.Name(), "/dav/*", Permissions, jConfig);

		auto Run = [&](KStringView sRequest)
		{
			KRESTServer::Options Options;
			return RunRequest(sRequest, Routes, Options);
		};

		auto CountEntries = [&]()
		{
			return KDirectory(CacheRoot.Name(), KFileType::FILE, /*bRecursive=*/true).size();
		};

		// requests a file compressed, which creates its cache entry
		auto CreateEntry = [&](KStringView sPath)
		{
			return Run(kFormat("GET {} HTTP/1.1\r\nHost: localhost\r\nAccept-Encoding: gzip\r\n\r\n", sPath)).contains("content-encoding: gzip\r\n");
		};

		auto Form = [&](KStringView sBody)
		{
			return Run(kFormat("POST /web/ HTTP/1.1\r\nHost: localhost\r\n"
			                   "Content-Type: application/x-www-form-urlencoded\r\nContent-Length: {}\r\n\r\n{}", sBody.size(), sBody));
		};

		// PUT replaces the file
		REQUIRE ( kWriteFile(kFormat("{}/app.js", WebRoot.Name()), sText) );
		CHECK ( CreateEntry("/web/app.js") );
		CHECK ( CountEntries() == 1 );
		CHECK ( Run(kFormat("PUT /web/app.js HTTP/1.1\r\nHost: localhost\r\nContent-Length: {}\r\n\r\n{}", sChanged.size(), sChanged)).starts_with("HTTP/1.1 200") );
		CHECK ( CountEntries() == 0 );

		// DELETE removes the file
		CHECK ( CreateEntry("/web/app.js") );
		CHECK ( CountEntries() == 1 );
		CHECK ( Run("DELETE /web/app.js HTTP/1.1\r\nHost: localhost\r\n\r\n").starts_with("HTTP/1.1 200") );
		CHECK ( CountEntries() == 0 );

		// the delete forms of the ad hoc index
		REQUIRE ( kWriteFile(kFormat("{}/app.js", WebRoot.Name()), sText) );
		CHECK ( CreateEntry("/web/app.js") );
		CHECK ( Form("deleteFile=app.js").starts_with("HTTP/1.1 200") );
		CHECK ( CountEntries() == 0 );

		REQUIRE ( kWriteFile(kFormat("{}/lib/x.js", WebRoot.Name()), sText) );
		CHECK ( CreateEntry("/web/lib/x.js") );
		CHECK ( Form("deleteDir=lib").starts_with("HTTP/1.1 200") );
		CHECK ( CountEntries() == 0 );

		// a multipart upload replaces the file
		REQUIRE ( kWriteFile(kFormat("{}/app.js", WebRoot.Name()), sText) );
		CHECK ( CreateEntry("/web/app.js") );
		{
			auto sMultipart = kFormat("--XyZ\r\nContent-Disposition: form-data; name=\"upload1\"; filename=\"app.js\"\r\n"
			                          "Content-Type: application/javascript\r\n\r\n{}\r\n--XyZ--\r\n", sChanged);
			CHECK ( Run(kFormat("POST /web/ HTTP/1.1\r\nHost: localhost\r\nContent-Type: multipart/form-data; boundary=XyZ\r\n"
			                    "Content-Length: {}\r\n\r\n{}", sMultipart.size(), sMultipart)).starts_with("HTTP/1.1 200") );
		}
		CHECK ( kReadAll(kFormat("{}/app.js", WebRoot.Name())) == sChanged );
		CHECK ( CountEntries() == 0 );

		// WebDAV MOVE removes the entries of the source
		REQUIRE ( kWriteFile(kFormat("{}/a.js", WebRoot.Name()), sText) );
		CHECK ( CreateEntry("/dav/a.js") );
		CHECK ( CountEntries() == 1 );
		CHECK ( Run("MOVE /dav/a.js HTTP/1.1\r\nHost: localhost\r\nDestination: http://localhost/dav/b.js\r\n\r\n").starts_with("HTTP/1.1 201") );
		CHECK ( CountEntries() == 0 );

		// WebDAV COPY removes the entries of the overwritten destination only
		REQUIRE ( kWriteFile(kFormat("{}/c.js", WebRoot.Name()), sChanged) );
		CHECK ( CreateEntry("/dav/b.js") );
		CHECK ( CreateEntry("/dav/c.js") );
		CHECK ( CountEntries() == 2 );
		CHECK ( Run("COPY /dav/b.js HTTP/1.1\r\nHost: localhost\r\nDestination: http://localhost/dav/c.js\r\nOverwrite: T\r\n\r\n").starts_with("HTTP/1.1 204") );
		CHECK ( CountEntries() == 1 );

		// WebDAV DELETE
		CHECK ( Run("DELETE /dav/b.js HTTP/1.1\r\nHost: localhost\r\n\r\n").starts_with("HTTP/1.1 204") );
		CHECK ( CountEntries() == 0 );
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
