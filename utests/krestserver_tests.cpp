#include "catch.hpp"
#include <random>

#include <dekaf2/rest/framework/krestserver.h>
#include <dekaf2/core/logging/ktracecontext.h>
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
#include <dekaf2/http/client/kwebclient.h>
#include <dekaf2/data/json/kndjson.h>
#include <dekaf2/io/readwrite/kreader.h>
#include <dekaf2/system/os/ksystem.h>
#include <dekaf2/net/tcp/ktcpstream.h>
#include <dekaf2/net/util/kstreamoptions.h>
#include <atomic>
#include <mutex>
#include <new>

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
		// NPH (a script installed as nph-abcxyz.cgi): the web server passes the response
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

	SECTION("a stream of unknown length without compression is sent whole and framed")
	{
		KString sContent;
		std::mt19937 Random(14);
		for (std::size_t i = 0; i < 121270; ++i)
		{
			sContent += static_cast<char>(Random() & 0xff);
		}

		KRESTServer::Options Options;
		KRESTRoutes Routes;
		std::size_t iContentLength { 0 };
		Routes.AddRoute({ KHTTPMethod::GET, false, "/stream", [&](KRESTServer& http)
		{
			http.Response.Headers.Set(KHTTPHeader::CONTENT_TYPE, KMIME::BINARY);
			http.Response.Headers.Set(KHTTPHeader::CONTENT_ENCODING, "gzip");
			http.AllowCompression(false);
			http.SetStreamToOutput(std::make_unique<KInStringStream>(sContent), npos, false);
			http.SetPostResponseCallback([&](const KRESTServer& http) { iContentLength = http.GetContentLength(); });
		}});

		auto Split = [](KStringView sResponse, KStringView& sHead, KStringView& sBody)
		{
			auto iPos = sResponse.find("\r\n\r\n");
			REQUIRE ( iPos != KStringView::npos );
			sHead = sResponse.substr(0, iPos + 2);
			sBody = sResponse.substr(iPos + 4);
		};

		auto Dechunk = [](KStringView sBody) -> KString
		{
			KString sDecoded;
			for (;;)
			{
				auto iEnd = sBody.find("\r\n");
				if (iEnd == KStringView::npos) return "<missing chunk size>";
				auto iSize = kToInt<std::size_t>(sBody.substr(0, iEnd), 16);
				sBody.remove_prefix(iEnd + 2);
				if (!iSize) return sBody == "\r\n" ? sDecoded : KString("<bad last chunk>");
				if (sBody.size() < iSize + 2) return "<short chunk>";
				sDecoded += sBody.substr(0, iSize);
				sBody.remove_prefix(iSize + 2);
			}
		};

		// HTTP/1.1 keeps the connection and sends the stream chunked
		auto sResponse = RunRequest("GET /stream HTTP/1.1\r\nHost: localhost\r\nAccept-Encoding: gzip\r\n\r\n", Routes, Options);
		KStringView sHead, sBody;
		Split(sResponse, sHead, sBody);
		auto sLowerHead = sHead.ToLowerASCII();
		CHECK ( sLowerHead.starts_with("http/1.1 200") );
		CHECK ( sLowerHead.contains("transfer-encoding: chunked\r\n") );
		CHECK_FALSE ( sLowerHead.contains("content-length:") );
		CHECK ( sLowerHead.contains("content-encoding: gzip\r\n") );
		CHECK ( sLowerHead.contains("connection: keep-alive\r\n") );
		auto sDecoded = Dechunk(sBody);
		CHECK ( sDecoded.size() == sContent.size() );
		CHECK ( sDecoded == sContent );
		// the server counted what it read from the stream
		CHECK ( iContentLength == sContent.size() );

		// HTTP/1.0 has no chunked framing, the end of the stream is the closed connection
		sResponse = RunRequest("GET /stream HTTP/1.0\r\nHost: localhost\r\nConnection: keep-alive\r\n\r\n", Routes, Options);
		Split(sResponse, sHead, sBody);
		sLowerHead = sHead.ToLowerASCII();
		CHECK ( sLowerHead.contains(" 200 ") );
		CHECK_FALSE ( sLowerHead.contains("transfer-encoding:") );
		CHECK_FALSE ( sLowerHead.contains("content-length:") );
		CHECK ( sLowerHead.contains("connection: close\r\n") );
		CHECK ( sBody.size() == sContent.size() );
		CHECK ( sBody == sContent );

		// HEAD sends no body, so it is not framed: neither a length nor chunks; and a GET after it on the same
		// connection gets its whole body
		sResponse = RunRequest("HEAD /stream HTTP/1.1\r\nHost: localhost\r\n\r\n"
		                       "GET /stream HTTP/1.1\r\nHost: localhost\r\n\r\n", Routes, Options);
		Split(sResponse, sHead, sBody);
		sLowerHead = sHead.ToLowerASCII();
		CHECK ( sLowerHead.starts_with("http/1.1 200") );
		CHECK_FALSE ( sLowerHead.contains("transfer-encoding:") );
		CHECK_FALSE ( sLowerHead.contains("content-length:") );
		CHECK ( sLowerHead.contains("connection: keep-alive\r\n") );
		REQUIRE ( sBody.starts_with("HTTP/1.1 200") );
		KStringView sGet = sBody;
		Split(sGet, sHead, sBody);
		CHECK ( sHead.ToLowerASCII().contains("transfer-encoding: chunked\r\n") );
		sDecoded = Dechunk(sBody);
		CHECK ( sDecoded.size() == sContent.size() );
		CHECK ( sDecoded == sContent );

		auto Count = [](KStringView sHay, KStringView sNeedle)
		{
			std::size_t iCount = 0;
			for (auto iPos = sHay.find(sNeedle); iPos != KStringView::npos; iPos = sHay.find(sNeedle, iPos + 1)) ++iCount;
			return iCount;
		};

		// a stream the output filter may compress
		KString sText(5000, 'x');
		Routes.AddRoute({ KHTTPMethod::GET, false, "/compressible", [&](KRESTServer& http)
		{
			http.Response.Headers.Set(KHTTPHeader::CONTENT_TYPE, KMIME::JSON);
			http.SetStreamToOutput(std::make_unique<KInStringStream>(sText), npos, true);
		}});

		// HTTP/1.1 compresses it, framed by exactly one Transfer-Encoding header
		sResponse = RunRequest("GET /compressible HTTP/1.1\r\nHost: localhost\r\nAccept-Encoding: gzip\r\n\r\n", Routes, Options);
		Split(sResponse, sHead, sBody);
		sLowerHead = sHead.ToLowerASCII();
		CHECK ( sLowerHead.contains("content-encoding: gzip\r\n") );
		CHECK ( Count(sLowerHead, "transfer-encoding:") == 1 );
		CHECK ( sLowerHead.contains("transfer-encoding: chunked\r\n") );

		// HTTP/1.0 does not compress, and has no chunks: the closed connection ends it
		sResponse = RunRequest("GET /compressible HTTP/1.0\r\nHost: localhost\r\nAccept-Encoding: gzip\r\n\r\n", Routes, Options);
		Split(sResponse, sHead, sBody);
		sLowerHead = sHead.ToLowerASCII();
		CHECK_FALSE ( sLowerHead.contains("content-encoding:") );
		CHECK_FALSE ( sLowerHead.contains("transfer-encoding:") );
		CHECK ( sLowerHead.contains("connection: close\r\n") );
		CHECK ( sBody == sText );

		// a length the handler set frames the stream: no chunks, and the connection stays open
		Routes.AddRoute({ KHTTPMethod::GET, false, "/length", [&](KRESTServer& http)
		{
			http.Response.Headers.Set(KHTTPHeader::CONTENT_TYPE, KMIME::BINARY);
			http.Response.Headers.Set(KHTTPHeader::CONTENT_LENGTH, kFormat("{}", sContent.size()));
			http.AllowCompression(false);
			http.SetStreamToOutput(std::make_unique<KInStringStream>(sContent), npos, false);
		}});

		for (KStringView sVersion : { "HTTP/1.1", "HTTP/1.0" })
		{
			INFO ( sVersion );
			sResponse = RunRequest(kFormat("GET /length {}\r\nHost: localhost\r\nConnection: keep-alive\r\n\r\n", sVersion), Routes, Options);
			Split(sResponse, sHead, sBody);
			sLowerHead = sHead.ToLowerASCII();
			CHECK ( Count(sLowerHead, "content-length:") == 1 );
			CHECK ( sLowerHead.contains(kFormat("content-length: {}\r\n", sContent.size())) );
			CHECK_FALSE ( sLowerHead.contains("transfer-encoding:") );
			CHECK ( sLowerHead.contains("connection: keep-alive\r\n") );
			CHECK ( sBody == sContent );
		}

		// a stream of known length is framed by its length: no chunks, and the connection stays open
		Routes.AddRoute({ KHTTPMethod::GET, false, "/known", [&](KRESTServer& http)
		{
			http.Response.Headers.Set(KHTTPHeader::CONTENT_TYPE, KMIME::BINARY);
			http.AllowCompression(false);
			http.SetStreamToOutput(std::make_unique<KInStringStream>(sContent), sContent.size(), false);
		}});

		for (KStringView sVersion : { "HTTP/1.1", "HTTP/1.0" })
		{
			INFO ( sVersion );
			sResponse = RunRequest(kFormat("GET /known {}\r\nHost: localhost\r\nConnection: keep-alive\r\n\r\n", sVersion), Routes, Options);
			Split(sResponse, sHead, sBody);
			sLowerHead = sHead.ToLowerASCII();
			CHECK ( sLowerHead.contains(kFormat("content-length: {}\r\n", sContent.size())) );
			CHECK_FALSE ( sLowerHead.contains("transfer-encoding:") );
			CHECK ( sLowerHead.contains("connection: keep-alive\r\n") );
			CHECK ( sBody == sContent );
		}

		// CGI output: the web server frames the answer, so neither header
		{
			KRESTServer::Options CGIOptions;
			CGIOptions.Out = KRESTServer::CGI;
			sResponse = RunRequest("GET /stream HTTP/1.1\r\nHost: localhost\r\n\r\n", Routes, CGIOptions);
			Split(sResponse, sHead, sBody);
			sLowerHead = sHead.ToLowerASCII();
			CHECK ( sLowerHead.starts_with("status: 200") );
			CHECK_FALSE ( sLowerHead.contains("transfer-encoding:") );
			CHECK_FALSE ( sLowerHead.contains("content-length:") );
			CHECK ( sBody == sContent );
		}

		// NPH output is a complete HTTP response: chunks for HTTP/1.1, none for HTTP/1.0
		{
			KRESTServer::Options NPHOptions;
			NPHOptions.Out = KRESTServer::NPH;
			sResponse = RunRequest("GET /stream HTTP/1.1\r\nHost: localhost\r\n\r\n", Routes, NPHOptions);
			Split(sResponse, sHead, sBody);
			sLowerHead = sHead.ToLowerASCII();
			CHECK ( sLowerHead.starts_with("http/1.1 200") );
			CHECK ( sLowerHead.contains("transfer-encoding: chunked\r\n") );
			CHECK ( Dechunk(sBody) == sContent );

			sResponse = RunRequest("GET /stream HTTP/1.0\r\nHost: localhost\r\n\r\n", Routes, NPHOptions);
			Split(sResponse, sHead, sBody);
			sLowerHead = sHead.ToLowerASCII();
			CHECK_FALSE ( sLowerHead.contains("transfer-encoding:") );
			CHECK ( sBody == sContent );
		}
	}

	SECTION("an empty stream of unknown length is framed, the next request gets its answer")
	{
		KString sEmpty;
		KRESTServer::Options Options;
		KRESTRoutes Routes;
		std::size_t iContentLength { npos };
		Routes.AddRoute({ KHTTPMethod::GET, false, "/empty", [&](KRESTServer& http)
		{
			http.Response.Headers.Set(KHTTPHeader::CONTENT_TYPE, KMIME::JSON);
			http.SetStreamToOutput(std::make_unique<KInStringStream>(sEmpty), npos, true);
			http.SetPostResponseCallback([&](const KRESTServer& http) { iContentLength = http.GetContentLength(); });
		}});
		Routes.AddRoute({ KHTTPMethod::GET, false, "/after", [&](KRESTServer& http)
		{
			http.SetRawOutput("the next answer");
		}});

		auto Split = [](KStringView sResponse, KStringView& sHead, KStringView& sBody)
		{
			auto iPos = sResponse.find("\r\n\r\n");
			REQUIRE ( iPos != KStringView::npos );
			sHead = sResponse.substr(0, iPos + 2);
			sBody = sResponse.substr(iPos + 4);
		};

		// HTTP/1.1 without compression: the last chunk is the whole body
		auto sResponse = RunRequest("GET /empty HTTP/1.1\r\nHost: localhost\r\n\r\n"
		                            "GET /after HTTP/1.1\r\nHost: localhost\r\n\r\n", Routes, Options);
		KStringView sHead, sBody;
		Split(sResponse, sHead, sBody);
		auto sLowerHead = sHead.ToLowerASCII();
		CHECK ( sLowerHead.starts_with("http/1.1 200") );
		CHECK ( sLowerHead.contains("transfer-encoding: chunked\r\n") );
		CHECK_FALSE ( sLowerHead.contains("content-length:") );
		CHECK ( sLowerHead.contains("connection: keep-alive\r\n") );
		REQUIRE ( sBody.starts_with("0\r\n\r\nHTTP/1.1 200") );
		CHECK ( sBody.ends_with("\r\n\r\nthe next answer") );
		CHECK ( iContentLength == 0 );

		// HTTP/1.1 with compression: one chunk with the empty gzip stream, then the last chunk
		sResponse = RunRequest("GET /empty HTTP/1.1\r\nHost: localhost\r\nAccept-Encoding: gzip\r\n\r\n"
		                       "GET /after HTTP/1.1\r\nHost: localhost\r\n\r\n", Routes, Options);
		Split(sResponse, sHead, sBody);
		sLowerHead = sHead.ToLowerASCII();
		CHECK ( sLowerHead.contains("transfer-encoding: chunked\r\n") );
		CHECK ( sLowerHead.contains("content-encoding: gzip\r\n") );
		auto iSizeEnd = sBody.find("\r\n");
		REQUIRE ( iSizeEnd != KStringView::npos );
		auto iSize = kToInt<std::size_t>(sBody.substr(0, iSizeEnd), 16);
		REQUIRE ( iSize > 0 );
		sBody.remove_prefix(iSizeEnd + 2);
		REQUIRE ( sBody.size() > iSize );
		KString sCompressed = sBody.substr(0, iSize);
		// the gzip magic number
		CHECK ( sCompressed.starts_with("\x1f\x8b") );
		KString sUncompressed;
		KUnGZip gunzip(sCompressed);
		gunzip.ReadRemaining(sUncompressed);
		gunzip.close();
		CHECK ( sUncompressed.empty() );
		sBody.remove_prefix(iSize);
		REQUIRE ( sBody.starts_with("\r\n0\r\n\r\nHTTP/1.1 200") );
		CHECK ( sBody.ends_with("\r\n\r\nthe next answer") );

		// HTTP/1.0: no chunks, the closed connection ends the empty body
		sResponse = RunRequest("GET /empty HTTP/1.0\r\nHost: localhost\r\nConnection: keep-alive\r\n\r\n", Routes, Options);
		Split(sResponse, sHead, sBody);
		sLowerHead = sHead.ToLowerASCII();
		CHECK ( sLowerHead.contains(" 200 ") );
		CHECK_FALSE ( sLowerHead.contains("transfer-encoding:") );
		CHECK_FALSE ( sLowerHead.contains("content-length:") );
		CHECK ( sLowerHead.contains("connection: close\r\n") );
		CHECK ( sBody.empty() );
	}

	SECTION("a streamed answer ends with the request, the next request gets its answer")
	{
		KRESTServer::Options Options;
		KRESTRoutes Routes;
		Routes.AddRoute({ KHTTPMethod::GET, false, "/stream", [](KRESTServer& http)
		{
			http.Response.Headers.Set(KHTTPHeader::CONTENT_TYPE, KMIME::TEXT_PLAIN);
			http.Response.Headers.Set(KHTTPHeader::TRANSFER_ENCODING, "chunked");
			http.Stream(/*bAllowCompressionIfPossible=*/false);
			http.Response.Write("hello");
		}});
		Routes.AddRoute({ KHTTPMethod::GET, false, "/plain", [](KRESTServer& http)
		{
			http.SetRawOutput("ok");
		}});

		auto sResponse = RunRequest("GET /stream HTTP/1.1\r\nHost: localhost\r\n\r\n"
		                            "GET /plain HTTP/1.1\r\nHost: localhost\r\n\r\n", Routes, Options);
		INFO ( sResponse );
		auto iSecond = sResponse.find("HTTP/1.1", 1);
		REQUIRE ( iSecond != KString::npos );
		KStringView sFirst = KStringView(sResponse).substr(0, iSecond);
		CHECK ( sFirst.contains("\r\n\r\n05\r\nhello\r\n") );
		CHECK ( sFirst.ends_with("\r\n0\r\n\r\n") );
		CHECK ( sResponse.ends_with("\r\n\r\nok") );
	}

	SECTION("a route added with RouteBuilder::Any answers every method")
	{
		KRESTServer::Options Options;
		KRESTRoutes Routes;
		Routes.AddRoute("/any").Any([](KRESTServer& http)
		{
			http.SetRawOutput(http.Request.Method.Serialize());
		});

		for (auto sMethod : { "GET", "POST", "PUT", "DELETE", "PATCH", "OPTIONS", "TRACE" })
		{
			auto sResponse = RunRequest(kFormat("{} /any HTTP/1.1\r\nHost: localhost\r\nContent-Length: 0\r\n\r\n", sMethod), Routes, Options);
			INFO ( sMethod );
			CHECK ( sResponse.starts_with("HTTP/1.1 200") );
			CHECK ( sResponse.ends_with(kFormat("\r\n\r\n{}", sMethod)) );
		}

		// HEAD gets the headers of the GET answer, without the body
		{
			auto sResponse = RunRequest("HEAD /any HTTP/1.1\r\nHost: localhost\r\n\r\n", Routes, Options);
			CHECK ( sResponse.starts_with("HTTP/1.1 200") );
			CHECK ( sResponse.ends_with("\r\n\r\n") );
		}

		// WebDAV methods match an Any route only with the WEBDAV option, as they match a route
		// with an empty method - without it they answer 404
		Routes.AddRoute("/dav").Any([](KRESTServer& http)
		{
			http.SetRawOutput(http.Request.Method.Serialize());
		}).Options(KRESTRoute::Options{KRESTRoute::Options::WEBDAV});
		Routes.AddRoute({ KHTTPMethod{KHTTPMethod::INVALID}, false, "/empty", [](KRESTServer& http)
		{
			http.SetRawOutput("empty");
		}});

		for (auto sPath : { "/any", "/empty" })
		{
			auto sResponse = RunRequest(kFormat("PROPFIND {} HTTP/1.1\r\nHost: localhost\r\nContent-Length: 0\r\n\r\n", sPath), Routes, Options);
			INFO ( sPath );
			CHECK ( sResponse.starts_with("HTTP/1.1 404") );
		}

		{
			auto sResponse = RunRequest("PROPFIND /dav HTTP/1.1\r\nHost: localhost\r\nContent-Length: 0\r\n\r\n", Routes, Options);
			CHECK ( sResponse.starts_with("HTTP/1.1 200") );
			CHECK ( sResponse.ends_with("\r\n\r\nPROPFIND") );
		}

		// options set after Any() apply to the same, single route
		KRESTRoutes OptionRoutes;
		OptionRoutes.AddRoute("/any").Any([](KRESTServer&) {}).Options(KRESTRoute::Options{KRESTRoute::Options::NO_REPEAT_LOG});
		OptionRoutes.AddRoute("/other").Get([](KRESTServer&) {});

		KRESTRoutes::Parameters Params;
		const auto& Route = OptionRoutes.FindRoute(KRESTPath(KHTTPMethod::POST, "/any"), Params, false, false);
		CHECK ( OptionRoutes.GetRouteIndex(Route) == 0 );
		CHECK ( Route.Method == KHTTPMethod::INVALID );
		CHECK ( Route.Option.Has(KRESTRoute::Options::NO_REPEAT_LOG) );
		CHECK ( OptionRoutes.GetRouteIndex(OptionRoutes.FindRoute(KRESTPath(KHTTPMethod::GET, "/other"), Params, false, false)) == 1 );
	}

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

	SECTION("request header limits")
	{
		KRESTServer::Options Options;
		KRESTRoutes Routes;
		Routes.AddRoute({ KHTTPMethod::GET, false, "/test", [&](KRESTServer& http)
		{
			http.json.tx["status"] = "ok";
		}});

		// a header line of 8 KiB is accepted, one byte more is answered with 431
		auto sResponse = RunRequest("GET /test HTTP/1.1\r\nHost: localhost\r\nX: " + KString(8 * 1024 - 3, 'a') + "\r\n\r\n", Routes, Options);
		CHECK ( sResponse.contains("HTTP/1.1 200") );

		KString sLongField = "GET /test HTTP/1.1\r\nHost: localhost\r\nX: " + KString(8 * 1024 - 2, 'a') + "\r\n\r\n";
		sResponse = RunRequest(sLongField, Routes, Options);
		CHECK ( sResponse.contains("HTTP/1.1 431") );
		CHECK ( sResponse.contains("connection: close") );

		// too many fields
		KString sManyFields = "GET /test HTTP/1.1\r\nHost: localhost\r\n";

		for (int i = 0; i < 500; ++i)
		{
			sManyFields += kFormat("X-{}: {}\r\n", i, i);
		}

		sManyFields += "\r\n";
		sResponse = RunRequest(sManyFields, Routes, Options);
		CHECK ( sResponse.contains("HTTP/1.1 431") );

		// ten fields of 7000 bytes each stay below the field limit, but exceed the
		// section limit of 64 KiB
		KString sLargeSection = "GET /test HTTP/1.1\r\nHost: localhost\r\n";

		for (int i = 0; i < 10; ++i)
		{
			sLargeSection += kFormat("X-{}: {}\r\n", i, KString(7000, 'a'));
		}

		sLargeSection += "\r\n";
		sResponse = RunRequest(sLargeSection, Routes, Options);
		CHECK ( sResponse.contains("HTTP/1.1 431") );

		// a request line that is too long is answered with 414
		sResponse = RunRequest("GET /test?" + KString(KInHTTPRequestLine::MAX_REQUESTLINELENGTH, 'a') + " HTTP/1.1\r\nHost: localhost\r\n\r\n", Routes, Options);
		CHECK ( sResponse.contains("HTTP/1.1 414") );

		// a malformed request is answered with 400
		sResponse = RunRequest("GET /test HTTP/1.1\r\nHost: localhost\r\nno colon\r\n\r\n", Routes, Options);
		CHECK ( sResponse.contains("HTTP/1.1 400") );

		// a request that ends within its headers is answered with 400 - read from a
		// file or a CGI input, the answer informs the sender
		sResponse = RunRequest("GET /test HTTP/1.1\r\nHost: localhost\r\n", Routes, Options);
		CHECK ( sResponse.contains("HTTP/1.1 400") );
		CHECK ( sResponse.contains("did not end with empty line") );

		// no request at all gets no answer
		sResponse = RunRequest("", Routes, Options);
		CHECK ( sResponse.empty() );

		// raised limits accept the same requests
		Options.RequestHeaderLimits = { 16 * 1024, 1000, 128 * 1024 };

		sResponse = RunRequest(sLongField, Routes, Options);
		CHECK ( sResponse.contains("HTTP/1.1 200") );

		sResponse = RunRequest(sManyFields, Routes, Options);
		CHECK ( sResponse.contains("HTTP/1.1 200") );

		sResponse = RunRequest(sLargeSection, Routes, Options);
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

	SECTION("web server: a compression cache inside of the document root is not used")
	{
		KTempDir WebRoot;

		KString sText;

		for (int i = 0; i < 2000; ++i)
		{
			sText += kFormat("line {} of a file that compresses well\n", i);
		}

		REQUIRE ( kWriteFile(kFormat("{}/app.js", WebRoot.Name()), sText) );

		auto sCacheDirectory = kFormat("{}/cache", WebRoot.Name());

		KRESTRoutes Routes;
		Routes.AddWebServer(WebRoot.Name(), "/web/*", KWebServerPermissions(KJSON{{ "permissions", "read|browse" }}),
		                    KJSON{{ "compression_cache", sCacheDirectory }});

		KRESTServer::Options Options;
		auto sResponse = RunRequest("GET /web/app.js HTTP/1.1\r\nHost: localhost\r\nAccept-Encoding: gzip\r\n\r\n", Routes, Options);

		// the route has no cache: the file is compressed on the fly, and no entry is written
		CHECK ( sResponse.starts_with("HTTP/1.1 200") );
		CHECK ( sResponse.contains("transfer-encoding: chunked\r\n") );
		CHECK_FALSE ( kDirExists(sCacheDirectory) );
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
			// the cache entries, without the CACHEDIR.TAG of the cache directory
			KDirectory Files(CacheRoot.Name(), KFileType::FILE, /*bRecursive=*/true);
			return Files.size() - (Files.Contains("CACHEDIR.TAG") ? 1 : 0);
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

TEST_CASE("KRESTServer: a request without a route on a keep-alive connection")
{
	// A route that logs only its first request of a connection, and after it a request
	// that matches no route: that one has to appear in the access log.
	KTempDir TempDir;

	auto sLogFile = kFormat("{}/access.log", TempDir.Name());

	KRESTRoutes Routes;

	Routes.AddRoute("/quiet").Get([](KRESTServer& HTTP)
	{
		HTTP.json.tx["quiet"] = true;
	}).Options(KRESTRoute::Options{KRESTRoute::Options::NO_REPEAT_LOG});

	{
		KREST::Options Options;

		Options.Type                 = KREST::HTTP;
		Options.iPort                = 44372;
		Options.Timeout              = chrono::seconds(2);
		Options.bBlocking            = false;
		Options.bCreateEphemeralCert = false;

		REQUIRE ( Options.Logger.Open(KHTTPLog::LOG_FORMAT::COMMON, sLogFile) );

		KREST Server;
		REQUIRE ( Server.Execute(Options, Routes) );

		KWebClient Client;

		Client.Get("http://localhost:44372/quiet");
		CHECK ( Client.GetStatusCode() == 200 );
		Client.Get("http://localhost:44372/quiet");
		CHECK ( Client.GetStatusCode() == 200 );
		Client.Get("http://localhost:44372/nowhere");
		CHECK ( Client.GetStatusCode() == 404 );

		// the server writes the log after it sent the response
		for (int i = 0; i < 2000 && !kReadAll(sLogFile).contains("/nowhere"); ++i)
		{
			kSleep(chrono::milliseconds(1));
		}
	}

	auto        sLog = kReadAll(sLogFile);
	std::size_t iQuiet   { 0 };
	std::size_t iNowhere { 0 };

	for (auto sLine : sLog.Split("\n"))
	{
		if (sLine.contains("/quiet"))
		{
			++iQuiet;
		}

		if (sLine.contains("/nowhere"))
		{
			++iNowhere;
			CHECK ( sLine.contains(" 404 ") );
		}
	}

	INFO  ( sLog );
	CHECK ( iQuiet   == 1 );
	CHECK ( iNowhere == 1 );
}

TEST_CASE("KRESTServer: byte accounting")
{
	// The access log reports for each request the received and the sent bytes and the
	// sizes of both bodies. A client on a raw connection knows what went over the wire,
	// and the test compares both.
	KTempDir TempDir;

	auto sLogFile  = kFormat("{}/access.log", TempDir.Name());
	auto sDataFile = kFormat("{}/data.txt"  , TempDir.Name());

	constexpr std::size_t iFileSize = 3000;
	constexpr std::size_t iBigSize  = 20000;
	// the response body size as the client received it
	constexpr std::size_t AsReceived = npos;
	// a size that the test does not know
	constexpr std::size_t Unchecked  = npos - 1;

	REQUIRE ( kWriteFile(sDataFile, KString(iFileSize, 'x')) );

	std::atomic<bool> bWaitEntered     { false };
	std::atomic<bool> bWaitFileEntered { false };

	KRESTRoutes Routes;

	Routes.AddRoute("/json").Get([](KRESTServer& HTTP)
	{
		HTTP.json.tx["hello"] = "world";
	});

	Routes.AddRoute("/json").Post([](KRESTServer& HTTP)
	{
		HTTP.json.tx["hello"] = "world";
	});

	Routes.AddRoute("/big").Get([](KRESTServer& HTTP)
	{
		HTTP.Response.Headers.Set(KHTTPHeader::CONTENT_TYPE, KMIME::TEXT_UTF8);
		HTTP.SetRawOutput(KString(iBigSize, 'a'));
	});

	Routes.AddRoute("/file").Get([&](KRESTServer& HTTP)
	{
		HTTP.SetFileToOutput(sDataFile);
	});

	// a stream of unknown length
	Routes.AddRoute("/unknown").Get([&](KRESTServer& HTTP)
	{
		HTTP.Response.Headers.Set(KHTTPHeader::CONTENT_TYPE, KMIME::TEXT_UTF8);
		HTTP.SetStreamToOutput(std::make_unique<KInFile>(sDataFile));
	});

	// routes that write the body themselves
	Routes.AddRoute("/streaming").Get([](KRESTServer& HTTP)
	{
		HTTP.Response.Headers.Set(KHTTPHeader::CONTENT_TYPE, KMIME::TEXT_UTF8);
		HTTP.Response.Headers.Set(KHTTPHeader::TRANSFER_ENCODING, "chunked");
		HTTP.Stream(false);
		HTTP.Response.Write(KString(1000, 's'));
	});

	Routes.AddRoute("/streamcompressed").Get([](KRESTServer& HTTP)
	{
		HTTP.Response.Headers.Set(KHTTPHeader::CONTENT_TYPE, KMIME::TEXT_UTF8);
		HTTP.Stream(true);
		HTTP.Response.Write(KString(1000, 's'));
	});

	Routes.AddRoute("/streamthrow").Get([](KRESTServer& HTTP)
	{
		HTTP.Response.Headers.Set(KHTTPHeader::CONTENT_TYPE, KMIME::TEXT_UTF8);
		HTTP.Response.Headers.Set(KHTTPHeader::TRANSFER_ENCODING, "chunked");
		HTTP.Stream(false);
		HTTP.Response.Write(KString(1000, 's'));
		throw KHTTPError { KHTTPError::H5xx_ERROR, "failed while streaming" };
	});

	// routes that read the request body themselves
	Routes.AddRoute("/readself").Post([](KRESTServer& HTTP)
	{
		KString sBody;
		HTTP.Request.Read(sBody);
		HTTP.json.tx["size"] = sBody.size();
	}).Parse(KRESTRoute::NOREAD);

	Routes.AddRoute("/readthrow").Post([](KRESTServer& HTTP)
	{
		KString sBody;
		HTTP.Request.Read(sBody);
		throw KHTTPError { KHTTPError::H5xx_ERROR, "failed after reading" };
	}).Parse(KRESTRoute::NOREAD);

	Routes.AddRoute("/throw").Get([](KRESTServer& HTTP)
	{
		throw KHTTPError { KHTTPError::H5xx_ERROR, "failed" };
	});

	Routes.AddRoute("/nocontent").Get([](KRESTServer& HTTP)
	{
		HTTP.Response.SetStatus(KHTTPError::H2xx_NO_CONTENT);
	});

	// routes that wait until the client left
	Routes.AddRoute("/wait").Get([&](KRESTServer& HTTP)
	{
		bWaitEntered = true;

		for (int i = 0; i < 500 && !HTTP.IsDisconnected(); ++i)
		{
			kSleep(chrono::milliseconds(10));
		}

		HTTP.json.tx["waited"] = true;
	});

	Routes.AddRoute("/waitfile").Get([&](KRESTServer& HTTP)
	{
		bWaitFileEntered = true;

		for (int i = 0; i < 500 && !HTTP.IsDisconnected(); ++i)
		{
			kSleep(chrono::milliseconds(10));
		}

		HTTP.SetFileToOutput(sDataFile);
	});

	struct Exchange
	{
		KString     sName;
		std::size_t iRequests     { 1 };         // the count of requests on the connection
		std::size_t iSent         { 0 };         // the bytes that the client sent
		std::size_t iReceived     { 0 };         // the bytes that the client received
		std::size_t iReceivedBody { 0 };         // of these the bytes after the response header
		std::size_t iBody         { 0 };         // the bytes of the request bodies as sent
		std::size_t iContent      { 0 };         // the expected size of the response bodies
		bool        bAllRead      { true };      // false if the server does not read the whole request
		bool        bAllSent      { true };      // false if the client does not get what the server wrote
	};

	std::vector<Exchange> Exchanges;

	{
		KREST::Options Options;

		Options.Type                 = KREST::HTTP;
		Options.iPort                = 0;
		Options.sBindAddress         = "127.0.0.1";
		Options.Timeout              = chrono::seconds(2);
		Options.bBlocking            = false;
		Options.bCreateEphemeralCert = false;
		Options.bPollForDisconnect   = true;

		REQUIRE ( Options.Logger.Open(KHTTPLog::LOG_FORMAT::JSON, sLogFile) );

		KREST Server;
		REQUIRE ( Server.Execute(Options, Routes) );

		auto sEndPoint = kFormat("127.0.0.1:{}", Server.GetPort());

		// sends the requests, and reads until the server closes the connection - the
		// server writes the log before that
		auto Run = [&](KStringView sName, KStringView sRequest, std::size_t iRequests, std::size_t iBody, std::size_t iContent) -> Exchange&
		{
			KTCPStream Stream(KTCPEndPoint(sEndPoint), KStreamOptions(chrono::seconds(5)));
			REQUIRE ( Stream.is_open() );

			Stream.Write(sRequest).Flush();

			KString sResponse;
			Stream.ReadRemaining(sResponse);

			Exchange Ex;
			Ex.sName     = sName;
			Ex.iRequests = iRequests;
			Ex.iSent     = sRequest.size();
			Ex.iReceived = sResponse.size();
			Ex.iBody     = iBody;
			Ex.iContent  = iContent;

			auto iHeaderEnd = sResponse.find("\r\n\r\n");

			if (iHeaderEnd != npos)
			{
				Ex.iReceivedBody = sResponse.size() - (iHeaderEnd + 4);
			}

			Exchanges.push_back(std::move(Ex));

			return Exchanges.back();
		};

		// sends the request, and leaves when the route runs
		auto RunAndLeave = [&](KStringView sName, KStringView sPath, std::atomic<bool>& bEntered)
		{
			auto sRequest = kFormat("GET {} HTTP/1.1\r\nHost: a\r\n\r\n", sPath);

			{
				KTCPStream Stream(KTCPEndPoint(sEndPoint), KStreamOptions(chrono::seconds(5)));
				REQUIRE ( Stream.is_open() );

				Stream.Write(sRequest).Flush();

				for (int i = 0; i < 500 && !bEntered; ++i)
				{
					kSleep(chrono::milliseconds(10));
				}

				REQUIRE ( bEntered );
			}

			Exchange Ex;
			Ex.sName    = sName;
			Ex.iSent    = sRequest.size();
			Ex.iContent = 0;
			Exchanges.push_back(std::move(Ex));

			// the server writes the log when the route noticed the disconnect
			auto sLogged = kFormat("\"{}\"", sPath);

			for (int i = 0; i < 6000 && !kReadAll(sLogFile).contains(sLogged); ++i)
			{
				kSleep(chrono::milliseconds(1));
			}
		};

		KStringView sClose    = "Host: a\r\nConnection: close\r\n\r\n";
		KStringView sGzip     = "Host: a\r\nAccept-Encoding: gzip\r\nConnection: close\r\n\r\n";
		KStringView sJSON     = "{\"a\":\"bcdef\"}";
		KStringView sJSONHead = "Host: a\r\nContent-Type: application/json\r\nContent-Length: 13\r\n";
		KString     sBody(500, 'b');

		REQUIRE ( sJSON.size() == 13 );

		// plain, compressed and HEAD answers
		Run("GET json"           , kFormat("GET /json HTTP/1.1\r\n{}"     , sClose), 1, 0, AsReceived);
		Run("GET json gzip"      , kFormat("GET /json HTTP/1.1\r\n{}"     , sGzip ), 1, 0, Unchecked );
		Run("HEAD json"          , kFormat("HEAD /json HTTP/1.1\r\n{}"    , sClose), 1, 0, Unchecked );
		Run("GET big"            , kFormat("GET /big HTTP/1.1\r\n{}"      , sClose), 1, 0, iBigSize  );
		Run("GET big gzip"       , kFormat("GET /big HTTP/1.1\r\n{}"      , sGzip ), 1, 0, iBigSize  );
		Run("GET file"           , kFormat("GET /file HTTP/1.1\r\n{}"     , sClose), 1, 0, iFileSize );
		Run("GET file gzip"      , kFormat("GET /file HTTP/1.1\r\n{}"     , sGzip ), 1, 0, iFileSize );
		Run("HEAD file"          , kFormat("HEAD /file HTTP/1.1\r\n{}"    , sClose), 1, 0, iFileSize );
		Run("GET nocontent"      , kFormat("GET /nocontent HTTP/1.1\r\n{}", sClose), 1, 0, 0         );

		// a stream of unknown length: chunked, compressed, framed by the closed connection,
		// and not read at all for HEAD
		Run("GET unknown"        , kFormat("GET /unknown HTTP/1.1\r\n{}"  , sClose), 1, 0, iFileSize );
		Run("GET unknown gzip"   , kFormat("GET /unknown HTTP/1.1\r\n{}"  , sGzip ), 1, 0, iFileSize );
		Run("GET unknown 1.0"    , "GET /unknown HTTP/1.0\r\nHost: a\r\n\r\n"      , 1, 0, iFileSize );
		Run("HEAD unknown"       , kFormat("HEAD /unknown HTTP/1.1\r\n{}" , sClose), 1, 0, 0         );

		// the route writes the body itself: the body size is the size as sent
		Run("GET streaming"      , kFormat("GET /streaming HTTP/1.1\r\n{}", sClose), 1, 0, AsReceived);
		Run("GET streamcompressed", kFormat("GET /streamcompressed HTTP/1.1\r\n{}", sGzip), 1, 0, AsReceived);
		// the connection ends without the buffered output
		Run("GET streamthrow"    , kFormat("GET /streamthrow HTTP/1.1\r\n{}", sClose), 1, 0, 0       ).bAllSent = false;

		// errors
		Run("GET throw"          , kFormat("GET /throw HTTP/1.1\r\n{}"    , sClose), 1, 0, AsReceived);
		Run("GET nowhere"        , kFormat("GET /nowhere HTTP/1.1\r\n{}"  , sClose), 1, 0, AsReceived);
		// only the first line gets read
		Run("invalid request"    , "INVALID\r\n\r\n"                               , 1, 0, AsReceived).bAllRead = false;

		// request bodies
		Run("POST json"          , kFormat("POST /json HTTP/1.1\r\n{}Connection: close\r\n\r\n{}", sJSONHead, sJSON), 1, sJSON.size(), AsReceived);
		Run("POST json chunked"  , kFormat("POST /json HTTP/1.1\r\nHost: a\r\nContent-Type: application/json\r\nTransfer-Encoding: chunked\r\nConnection: close\r\n\r\nd\r\n{}\r\n0\r\n\r\n", sJSON),
		                           1, sJSON.size() + 10, AsReceived);
		Run("POST expect"        , kFormat("POST /json HTTP/1.1\r\n{}Expect: 100-continue\r\nConnection: close\r\n\r\n{}", sJSONHead, sJSON), 1, sJSON.size(), Unchecked);
		Run("POST readself"      , kFormat("POST /readself HTTP/1.1\r\nHost: a\r\nContent-Length: {}\r\nConnection: close\r\n\r\n{}" , sBody.size(), sBody), 1, sBody.size(), AsReceived);
		Run("POST readthrow"     , kFormat("POST /readthrow HTTP/1.1\r\nHost: a\r\nContent-Length: {}\r\nConnection: close\r\n\r\n{}", sBody.size(), sBody), 1, sBody.size(), AsReceived);
		// the body of a request without a route is not read
		Run("POST nowhere"       , kFormat("POST /nowhere HTTP/1.1\r\nHost: a\r\nContent-Length: {}\r\nConnection: close\r\n\r\n{}"  , sBody.size(), sBody), 1, 0, AsReceived).bAllRead = false;

		// two requests on one connection: the counts of the first are not part of the second
		Run("POST, GET"          , kFormat("POST /json HTTP/1.1\r\n{}\r\n{}GET /json HTTP/1.1\r\n{}", sJSONHead, sJSON, sClose), 2, sJSON.size(), Unchecked);
		Run("big, nocontent"     , kFormat("GET /big HTTP/1.1\r\nHost: a\r\n\r\nGET /nocontent HTTP/1.1\r\n{}", sClose), 2, 0, iBigSize );
		Run("big, HEAD"          , kFormat("GET /big HTTP/1.1\r\nHost: a\r\n\r\nHEAD /file HTTP/1.1\r\n{}"    , sClose), 2, 0, iBigSize + iFileSize);
		Run("streaming, json"    , kFormat("GET /streaming HTTP/1.1\r\nHost: a\r\n\r\nGET /json HTTP/1.1\r\n{}", sClose), 2, 0, Unchecked);
		Run("json, big gzip"     , kFormat("GET /json HTTP/1.1\r\nHost: a\r\n\r\nGET /big HTTP/1.1\r\n{}"     , sGzip ), 2, 0, Unchecked);
		Run("POST, invalid"      , kFormat("POST /json HTTP/1.1\r\n{}\r\n{}INVALID\r\n\r\n", sJSONHead, sJSON), 2, sJSON.size(), Unchecked).bAllRead = false;

		// a client that leaves before the response: nothing is sent, also not a prepared file
		RunAndLeave("GET wait"    , "/wait"    , bWaitEntered    );
		RunAndLeave("GET waitfile", "/waitfile", bWaitFileEntered);
	}

	std::vector<KJSON> Log;

	for (auto sLine : kReadAll(sLogFile).Split("\n"))
	{
		if (!sLine.empty())
		{
			Log.push_back(kjson::Parse(sLine));
		}
	}

	std::size_t iLine { 0 };

	for (const auto& Ex : Exchanges)
	{
		INFO ( Ex.sName );
		REQUIRE ( iLine + Ex.iRequests <= Log.size() );

		std::size_t iRXSize { 0 };
		std::size_t iRXRecv { 0 };
		std::size_t iTXSize { 0 };
		std::size_t iTXSent { 0 };

		for (std::size_t i = 0; i < Ex.iRequests; ++i, ++iLine)
		{
			const auto& j = Log[iLine];

			// no value is the unsigned form of a negative number
			CHECK ( j("rx-size").UInt64() <= Ex.iSent );
			CHECK ( j("rx-recv").UInt64() <= Ex.iSent );
			CHECK ( j("tx-size").UInt64() <= iBigSize + iFileSize );
			CHECK ( j("tx-sent").UInt64() <= iBigSize + iFileSize );

			iRXSize += j("rx-size").UInt64();
			iRXRecv += j("rx-recv").UInt64();
			iTXSize += j("tx-size").UInt64();
			iTXSent += j("tx-sent").UInt64();
		}

		CHECK ( iRXSize == Ex.iBody );

		if (Ex.bAllRead)
		{
			CHECK ( iRXRecv == Ex.iSent );
		}

		if (Ex.bAllSent)
		{
			CHECK ( iTXSent == Ex.iReceived );
		}

		if (Ex.iContent == AsReceived)
		{
			CHECK ( iTXSize == Ex.iReceivedBody );
		}
		else if (Ex.iContent != Unchecked)
		{
			CHECK ( iTXSize == Ex.iContent );
		}
	}

	CHECK ( iLine == Log.size() );
}

TEST_CASE("KRESTServer: flush of a streamed response")
{
	// A route that writes its response itself sends a first piece with Flush(), and
	// continues only when the client has read it.
	std::atomic<bool> bClientHasFirst { false };
	std::atomic<bool> bFlushed        { false };
	std::atomic<bool> bWaitedInVain   { false };

	auto Body = [&](KRESTServer& HTTP)
	{
		HTTP.Write("first\n");
		bFlushed = HTTP.Flush();

		for (std::size_t i = 0; i < 500 && !bClientHasFirst; ++i)
		{
			kSleep(chrono::milliseconds(10));
		}

		bWaitedInVain = !bClientHasFirst;

		HTTP.Write("second\n");
	};

	KRESTRoutes Routes;

	Routes.AddRoute("/chunked").Get([&](KRESTServer& HTTP)
	{
		HTTP.Response.Headers.Set(KHTTPHeader::CONTENT_TYPE, KMIME::TEXT_UTF8);
		HTTP.Response.Headers.Set(KHTTPHeader::TRANSFER_ENCODING, "chunked");
		HTTP.Stream(false);
		Body(HTTP);
	});

	Routes.AddRoute("/plain").Get([&](KRESTServer& HTTP)
	{
		HTTP.Response.Headers.Set(KHTTPHeader::CONTENT_TYPE, KMIME::TEXT_UTF8);
		HTTP.Stream(false);
		Body(HTTP);
	});

	KREST::Options Options;

	Options.Type                 = KREST::HTTP;
	Options.iPort                = 0;
	Options.sBindAddress         = "127.0.0.1";
	Options.Timeout              = chrono::seconds(2);
	Options.bBlocking            = false;
	Options.bCreateEphemeralCert = false;

	KREST Server;
	REQUIRE ( Server.Execute(Options, Routes) );

	auto sEndPoint = kFormat("127.0.0.1:{}", Server.GetPort());

	for (KStringView sPath : { "/chunked", "/plain" })
	{
		INFO ( sPath );

		bClientHasFirst = false;
		bFlushed        = false;
		bWaitedInVain   = false;

		KTCPStream Stream(KTCPEndPoint(sEndPoint), KStreamOptions(chrono::seconds(10)));
		REQUIRE ( Stream.is_open() );

		Stream.Write(kFormat("GET {} HTTP/1.1\r\nHost: a\r\nConnection: close\r\n\r\n", sPath)).Flush();

		// the header ends with an empty line, then follows the first piece
		KString sLine;
		bool    bInBody   { false };
		bool    bHasFirst { false };

		while (!bHasFirst && Stream.ReadLine(sLine))
		{
			if (!bInBody)
			{
				bInBody = sLine.empty();
			}
			else
			{
				bHasFirst = sLine == "first";
			}
		}

		CHECK ( bHasFirst );
		bClientHasFirst = true;

		KString sRest;
		Stream.ReadRemaining(sRest);

		CHECK ( bFlushed );
		CHECK_FALSE ( bWaitedInVain );
		CHECK ( sRest.contains("second\n") );

		if (sPath == "/chunked")
		{
			// the body has one end, after the last piece
			CHECK ( sRest.ends_with("second\n\r\n0\r\n\r\n") );
			CHECK ( sRest.find("0\r\n\r\n") == sRest.size() - 5 );
		}
	}
}

TEST_CASE("KRESTServer: framing of a streamed response")
{
	// A route that writes its response itself, without a content length. With bFrameBody
	// the client sees the end of the body, and the connection can be used for a next request.
	auto Body = [](KRESTServer& HTTP)
	{
		HTTP.Write("first\n");
		HTTP.Flush();
		HTTP.Write("second\n");
	};

	KRESTRoutes Routes;

	Routes.AddRoute("/framed").Get([&](KRESTServer& HTTP)
	{
		HTTP.Response.Headers.Set(KHTTPHeader::CONTENT_TYPE, KMIME::TEXT_UTF8);
		HTTP.Stream(false, true, true);
		Body(HTTP);
	});

	Routes.AddRoute("/unframed").Get([&](KRESTServer& HTTP)
	{
		HTTP.Response.Headers.Set(KHTTPHeader::CONTENT_TYPE, KMIME::TEXT_UTF8);
		HTTP.Stream(false);
		Body(HTTP);
	});

	Routes.AddRoute("/framedlength").Get([&](KRESTServer& HTTP)
	{
		HTTP.Response.Headers.Set(KHTTPHeader::CONTENT_TYPE, KMIME::TEXT_UTF8);
		HTTP.SetContentLengthToOutput(13);
		HTTP.Stream(false, true, true);
		Body(HTTP);
	});

	Routes.AddRoute("/framedthrow").Get([](KRESTServer& HTTP)
	{
		HTTP.Response.Headers.Set(KHTTPHeader::CONTENT_TYPE, KMIME::TEXT_UTF8);
		HTTP.Stream(false, true, true);
		HTTP.Write("first\n");
		HTTP.Flush();
		HTTP.Write("not sent\n");
		throw KHTTPError { KHTTPError::H5xx_ERROR, "failed while streaming" };
	});

	// an error below 500 does not end the connection of a response that is not streamed
	Routes.AddRoute("/framedthrow4xx").Get([](KRESTServer& HTTP)
	{
		HTTP.Response.Headers.Set(KHTTPHeader::CONTENT_TYPE, KMIME::TEXT_UTF8);
		HTTP.Stream(false, true, true);
		HTTP.Write("first\n");
		HTTP.Flush();
		HTTP.Write("not sent\n");
		throw KHTTPError { KHTTPError::H4xx_BADREQUEST, "failed while streaming" };
	});

	Routes.AddRoute("/json").Get([](KRESTServer& HTTP)
	{
		HTTP.json.tx["hello"] = "world";
	});

	KREST::Options Options;

	Options.Type                 = KREST::HTTP;
	Options.iPort                = 0;
	Options.sBindAddress         = "127.0.0.1";
	Options.Timeout              = chrono::seconds(2);
	Options.bBlocking            = false;
	Options.bCreateEphemeralCert = false;

	KREST Server;
	REQUIRE ( Server.Execute(Options, Routes) );

	auto sEndPoint = kFormat("127.0.0.1:{}", Server.GetPort());

	// sends the requests, and reads until the server closes the connection
	auto Run = [&](KStringView sRequest) -> KString
	{
		KTCPStream Stream(KTCPEndPoint(sEndPoint), KStreamOptions(chrono::seconds(10)));
		REQUIRE ( Stream.is_open() );

		Stream.Write(sRequest).Flush();

		KString sResponse;
		Stream.ReadRemaining(sResponse);

		return sResponse;
	};

	// counts the responses in what the client received
	auto CountResponses = [](KStringView sResponse) -> std::size_t
	{
		std::size_t iCount { 0 };

		for (auto iPos = sResponse.find("HTTP/1.1 "); iPos != npos; iPos = sResponse.find("HTTP/1.1 ", iPos + 1))
		{
			++iCount;
		}

		return iCount;
	};

	KStringView sLastRequest { "GET /json HTTP/1.1\r\nHost: a\r\nConnection: close\r\n\r\n" };

	SECTION("HTTP/1.1: the body is chunked, and the next request gets its response")
	{
		auto sResponse = Run(kFormat("GET /framed HTTP/1.1\r\nHost: a\r\n\r\n{}", sLastRequest));

		INFO  ( sResponse );
		CHECK ( sResponse.contains("transfer-encoding: chunked\r\n") );
		CHECK ( sResponse.contains("connection: keep-alive\r\n") );
		CHECK ( sResponse.contains("\r\n06\r\nfirst\n\r\n07\r\nsecond\n\r\n0\r\n\r\nHTTP/1.1 200 ") );
		CHECK ( CountResponses(sResponse) == 2 );
		CHECK ( sResponse.contains("\"hello\"") );
	}

	SECTION("HTTP/1.0: the body ends with the connection")
	{
		auto sResponse = Run("GET /framed HTTP/1.0\r\nHost: a\r\nConnection: keep-alive\r\n\r\n");

		INFO  ( sResponse );
		CHECK_FALSE ( sResponse.contains("transfer-encoding") );
		CHECK ( sResponse.contains("connection: close\r\n") );
		CHECK ( sResponse.ends_with("\r\n\r\nfirst\nsecond\n") );
	}

	SECTION("a content length frames the body already")
	{
		auto sResponse = Run(kFormat("GET /framedlength HTTP/1.1\r\nHost: a\r\n\r\n{}", sLastRequest));

		INFO  ( sResponse );
		CHECK_FALSE ( sResponse.contains("transfer-encoding") );
		CHECK ( sResponse.contains("content-length: 13\r\n") );
		CHECK ( sResponse.contains("\r\n\r\nfirst\nsecond\nHTTP/1.1 200 ") );
		CHECK ( CountResponses(sResponse) == 2 );
	}

	SECTION("without bFrameBody the response is not framed")
	{
		auto sResponse = Run("GET /unframed HTTP/1.1\r\nHost: a\r\nConnection: close\r\n\r\n");

		INFO  ( sResponse );
		CHECK_FALSE ( sResponse.contains("transfer-encoding") );
		CHECK_FALSE ( sResponse.contains("content-length") );
		CHECK ( sResponse.ends_with("\r\n\r\nfirst\nsecond\n") );
	}

	SECTION("a route that fails ends the body without the last chunk, and the connection")
	{
		for (KStringView sPath : { "/framedthrow", "/framedthrow4xx" })
		{
			auto sResponse = Run(kFormat("GET {} HTTP/1.1\r\nHost: a\r\n\r\n{}", sPath, sLastRequest));

			INFO  ( sPath );
			INFO  ( sResponse );
			CHECK ( sResponse.contains("transfer-encoding: chunked\r\n") );
			CHECK ( sResponse.ends_with("\r\n06\r\nfirst\n\r\n") );
			CHECK_FALSE ( sResponse.contains("not sent") );
			CHECK_FALSE ( sResponse.contains("0\r\n\r\n") );
			// the next request on this connection is not answered
			CHECK ( CountResponses(sResponse) == 1 );
		}
	}
}

TEST_CASE("KRESTServer: the state of a new server")
{
	// what a new object tells does not depend on what its memory held before
	KRESTRoutes          Routes;
	KRESTServer::Options Options;

	std::vector<unsigned char> Memory(sizeof(KRESTServer), 0xff);

	auto& Server = *new (Memory.data()) KRESTServer(Routes, Options);

	CHECK_FALSE ( Server.GetLostConnection() );
	CHECK_FALSE ( Server.IsDisconnected()    );

	Server.~KRESTServer();
}

TEST_CASE("KRESTServer: event stream")
{
	// A route sends events with the writer of EventStream(). The first event has to reach
	// the client while the route runs: the route continues only when the client has it.
	std::atomic<bool> bClientHasFirst { false };
	std::atomic<bool> bWaitedInVain   { false };
	std::atomic<bool> bAllSent        { false };
	std::atomic<bool> bGoodAtStart    { false };
	std::atomic<bool> bWriterEnded    { false };
	std::atomic<bool> bRouteEnded     { false };

	auto SendEvents = [&](KRESTServer::EventWriter Events, bool bWithRetry)
	{
		bool bSent { true };

		if (bWithRetry)
		{
			bSent = Events.Retry(chrono::seconds(3));
		}

		bSent = Events.Send({ kFormat("last id <{}>", Events.GetLastEventID()), "start", "1" }) && bSent;

		for (std::size_t i = 0; i < 500 && !bClientHasFirst; ++i)
		{
			kSleep(chrono::milliseconds(10));
		}

		bWaitedInVain = !bClientHasFirst;

		// an event without content sends nothing, and does not end the body
		bSent = Events.Send(KServerSentEvent{}) && bSent;
		bSent = Events.KeepAlive() && bSent;
		bSent = Events.Send({ "two\nlines", "", "2" }) && bSent;

		bAllSent = bSent;
	};

	KRESTRoutes Routes;

	Routes.AddRoute("/events").Get([&](KRESTServer& HTTP)
	{
		SendEvents(HTTP.EventStream(), true);
	});

	Routes.AddRoute("/compressed").Get([&](KRESTServer& HTTP)
	{
		SendEvents(HTTP.EventStream(true), false);
	});

	// sends until the writer tells to end, for ten seconds at most
	Routes.AddRoute("/endless").Get([&](KRESTServer& HTTP)
	{
		auto Events = HTTP.EventStream();

		bGoodAtStart = Events.Good();

		for (std::size_t i = 0; i < 1000 && Events.Good(); ++i)
		{
			Events.KeepAlive();
			kSleep(chrono::milliseconds(10));
		}

		bWriterEnded = !Events.Good();
		bRouteEnded  = true;
	});

	Routes.AddRoute("/json").Get([](KRESTServer& HTTP)
	{
		HTTP.json.tx["hello"] = "world";
	});

	KREST::Options Options;

	Options.Type                 = KREST::HTTP;
	Options.iPort                = 0;
	Options.sBindAddress         = "127.0.0.1";
	Options.Timeout              = chrono::seconds(2);
	Options.bBlocking            = false;
	Options.bCreateEphemeralCert = false;

	auto Server = std::make_unique<KREST>();
	REQUIRE ( Server->Execute(Options, Routes) );

	auto sEndPoint = kFormat("127.0.0.1:{}", Server->GetPort());

	// reads the header of a response, with one LF after each line
	auto ReadHeader = [](KTCPStream& Stream) -> KString
	{
		KString sHeader;
		KString sLine;

		while (Stream.ReadLine(sLine) && !sLine.empty())
		{
			sHeader += sLine;
			sHeader += '\n';
		}

		return sHeader;
	};

	// appends the next chunk of a chunked body to sBody, returns false after the last chunk
	auto ReadChunk = [](KTCPStream& Stream, KString& sBody) -> bool
	{
		KString sLine;

		if (!Stream.ReadLine(sLine))
		{
			return false;
		}

		auto iSize = sLine.UInt64(true);

		if (!iSize || Stream.Read(sBody, iSize) != iSize)
		{
			// the empty line after the last chunk
			Stream.ReadLine(sLine);
			return false;
		}

		// the line break after the chunk
		Stream.ReadLine(sLine);

		return true;
	};

	auto WaitForRouteEnd = [&]()
	{
		for (std::size_t i = 0; i < 1500 && !bRouteEnded; ++i)
		{
			kSleep(chrono::milliseconds(10));
		}
	};

	auto CheckEvents = [](const std::vector<KServerSentEvent>& Events, KStringView sLastEventID)
	{
		REQUIRE ( Events.size() == 2 );
		CHECK ( Events[0].sData  == kFormat("last id <{}>", sLastEventID) );
		CHECK ( Events[0].sEvent == "start" );
		CHECK ( Events[0].sID    == "1" );
		CHECK ( Events[1].sData  == "two\nlines" );
		CHECK ( Events[1].sEvent == "" );
		CHECK ( Events[1].sID    == "2" );
	};

	KStringView sLastRequest { "GET /json HTTP/1.1\r\nHost: a\r\nConnection: close\r\n\r\n" };

	SECTION("the events arrive while the route runs, and the next request gets its response")
	{
		KTCPStream Stream(KTCPEndPoint(sEndPoint), KStreamOptions(chrono::seconds(10)));
		REQUIRE ( Stream.is_open() );

		Stream.Write(kFormat("GET /events HTTP/1.1\r\nHost: a\r\nLast-Event-ID: 41\r\n\r\n{}", sLastRequest)).Flush();

		auto sHeader = ReadHeader(Stream);

		INFO  ( sHeader );
		CHECK ( sHeader.starts_with("HTTP/1.1 200 ") );
		CHECK ( sHeader.contains("content-type: text/event-stream\n") );
		CHECK ( sHeader.contains("cache-control: no-cache\n") );
		CHECK ( sHeader.contains("transfer-encoding: chunked\n") );
		CHECK ( sHeader.contains("connection: keep-alive\n") );
		CHECK_FALSE ( sHeader.contains("content-encoding") );

		KServerSentEventParser        Parser;
		KServerSentEvent              Event;
		std::vector<KServerSentEvent> Events;

		for (bool bMore = true; bMore;)
		{
			KString sBody;
			bMore = ReadChunk(Stream, sBody);
			Parser.Add(sBody);

			while (Parser.Next(Event))
			{
				Events.push_back(Event);
				bClientHasFirst = true;
			}
		}

		CHECK_FALSE ( bWaitedInVain );
		CHECK ( bAllSent );
		CheckEvents(Events, "41");
		CHECK ( Parser.HasRetry() );
		CHECK ( Parser.GetRetry() == chrono::seconds(3) );
		CHECK ( Parser.GetLastEventID() == "2" );

		KString sRest;
		Stream.ReadRemaining(sRest);

		CHECK ( sRest.starts_with("HTTP/1.1 200 ") );
		CHECK ( sRest.contains("\"hello\"") );
	}

	SECTION("a compressed stream is flushed after each event")
	{
		KTCPStream Stream(KTCPEndPoint(sEndPoint), KStreamOptions(chrono::seconds(10)));
		REQUIRE ( Stream.is_open() );

		Stream.Write("GET /compressed HTTP/1.1\r\nHost: a\r\nAccept-Encoding: gzip\r\nConnection: close\r\n\r\n").Flush();

		auto sHeader = ReadHeader(Stream);

		INFO  ( sHeader );
		CHECK ( sHeader.contains("content-type: text/event-stream\n") );
		CHECK ( sHeader.contains("content-encoding: gzip\n") );
		CHECK ( sHeader.contains("transfer-encoding: chunked\n") );

		KString sCompressed;

		while (ReadChunk(Stream, sCompressed))
		{
			// the header of the gzip format has 10 bytes, what follows is the first event
			if (sCompressed.size() > 10)
			{
				bClientHasFirst = true;
			}
		}

		CHECK_FALSE ( bWaitedInVain );
		CHECK ( bAllSent );

		KString     sBody;
		KUnCompress UnCompress(sCompressed, KUnCompressIStream::GZIP);
		UnCompress.ReadRemaining(sBody);

		KServerSentEventParser        Parser;
		KServerSentEvent              Event;
		std::vector<KServerSentEvent> Events;

		Parser.Add(sBody);

		while (Parser.Next(Event))
		{
			Events.push_back(Event);
		}

		CheckEvents(Events, "");
	}

	SECTION("a compression that cannot be flushed is not used")
	{
		KTCPStream Stream(KTCPEndPoint(sEndPoint), KStreamOptions(chrono::seconds(10)));
		REQUIRE ( Stream.is_open() );

		Stream.Write("GET /compressed HTTP/1.1\r\nHost: a\r\nAccept-Encoding: bzip2\r\nConnection: close\r\n\r\n").Flush();

		auto sHeader = ReadHeader(Stream);

		INFO  ( sHeader );
		CHECK ( sHeader.contains("content-type: text/event-stream\n") );
		CHECK_FALSE ( sHeader.contains("content-encoding") );

		KServerSentEventParser        Parser;
		KServerSentEvent              Event;
		std::vector<KServerSentEvent> Events;

		for (bool bMore = true; bMore;)
		{
			KString sBody;
			bMore = ReadChunk(Stream, sBody);
			Parser.Add(sBody);

			while (Parser.Next(Event))
			{
				Events.push_back(Event);
				bClientHasFirst = true;
			}
		}

		CHECK_FALSE ( bWaitedInVain );
		CHECK ( bAllSent );
		CheckEvents(Events, "");
	}

	SECTION("the writer tells when the client left")
	{
		{
			KTCPStream Stream(KTCPEndPoint(sEndPoint), KStreamOptions(chrono::seconds(10)));
			REQUIRE ( Stream.is_open() );

			Stream.Write("GET /endless HTTP/1.1\r\nHost: a\r\n\r\n").Flush();

			auto sHeader = ReadHeader(Stream);
			CHECK ( sHeader.contains("content-type: text/event-stream\n") );

			KString sBody;
			CHECK ( ReadChunk(Stream, sBody) );
			CHECK ( sBody.starts_with(":\n") );
		}

		WaitForRouteEnd();

		CHECK ( bRouteEnded  );
		CHECK ( bGoodAtStart );
		CHECK ( bWriterEnded );
	}

	SECTION("the writer tells when the server stops")
	{
		KTCPStream Stream(KTCPEndPoint(sEndPoint), KStreamOptions(chrono::seconds(10)));
		REQUIRE ( Stream.is_open() );

		Stream.Write("GET /endless HTTP/1.1\r\nHost: a\r\n\r\n").Flush();

		auto sHeader = ReadHeader(Stream);
		CHECK ( sHeader.contains("content-type: text/event-stream\n") );

		KString sBody;
		CHECK ( ReadChunk(Stream, sBody) );
		CHECK_FALSE ( bRouteEnded );

		// the destructor stops the server
		Server.reset();
		WaitForRouteEnd();

		CHECK ( bRouteEnded  );
		CHECK ( bGoodAtStart );
		CHECK ( bWriterEnded );
	}

	SECTION("a HEAD request gets the headers, and the writer is not good")
	{
		KTCPStream Stream(KTCPEndPoint(sEndPoint), KStreamOptions(chrono::seconds(10)));
		REQUIRE ( Stream.is_open() );

		Stream.Write(kFormat("HEAD /endless HTTP/1.1\r\nHost: a\r\n\r\n{}", sLastRequest)).Flush();

		KString sResponse;
		Stream.ReadRemaining(sResponse);

		INFO  ( sResponse );
		CHECK ( bRouteEnded );
		CHECK_FALSE ( bGoodAtStart );
		CHECK ( sResponse.contains("content-type: text/event-stream\r\n") );
		// no body, the second response follows the headers of the first
		CHECK ( sResponse.contains("\r\n\r\nHTTP/1.1 200 ") );
		CHECK ( sResponse.contains("\"hello\"") );
	}
}

TEST_CASE("KRESTServer: NDJSON stream")
{
	// A route sends lines with the writer of NDJSONStream(). Where the lines are sent at
	// once the first line has to reach the client while the route runs: the route continues
	// only when the client has it.
	std::atomic<bool>        bClientHasFirst { false };
	std::atomic<bool>        bWaitedInVain   { false };
	std::atomic<bool>        bAllSent        { false };
	std::atomic<bool>        bGoodAtStart    { false };
	std::atomic<bool>        bWriterEnded    { false };
	std::atomic<bool>        bRouteEnded     { false };
	std::atomic<std::size_t> iLines          { 0 };

	auto WaitForClient = [&]()
	{
		for (std::size_t i = 0; i < 500 && !bClientHasFirst; ++i)
		{
			kSleep(chrono::milliseconds(10));
		}

		bWaitedInVain = !bClientHasFirst;
	};

	// the lines that follow the first one
	auto MakeRest = []() -> KJSON
	{
		KJSON Broken;
		Broken["text"] = KString("bad \xff byte");

		KJSON Rest = KJSON::array();
		Rest.push_back(2);
		Rest.push_back("two\nlines");
		Rest.push_back(Broken);

		return Rest;
	};

	KStringView sExpected { "{\"line\":1}\n2\n\"two\\nlines\"\n{\"text\":\"bad \xef\xbf\xbd byte\"}\n" };

	auto SendLines = [&](KRESTServer::NDJSONWriter Lines)
	{
		KJSON First;
		First["line"] = 1;

		bool bSent = Lines.Write(First);

		WaitForClient();

		bSent = Lines.WriteAll(MakeRest()) && bSent;
		// what is no array sends nothing
		bSent = !Lines.WriteAll(KJSON()) && bSent;

		iLines   = Lines.GetLines();
		bAllSent = bSent;
	};

	KRESTRoutes Routes;

	Routes.AddRoute("/lines").Get([&](KRESTServer& HTTP)
	{
		SendLines(HTTP.NDJSONStream(true));
	});

	Routes.AddRoute("/compressed").Get([&](KRESTServer& HTTP)
	{
		SendLines(HTTP.NDJSONStream(true, true));
	});

	// does not send every line at once
	Routes.AddRoute("/buffered").Get([&](KRESTServer& HTTP)
	{
		auto Lines = HTTP.NDJSONStream(false, true);

		KJSON First;
		First["line"] = 1;

		bool bSent = Lines.Write(First);
		bSent      = Lines.WriteAll(MakeRest()) && bSent;

		iLines   = Lines.GetLines();
		bAllSent = bSent;
	});

	// sends the first line with Flush()
	Routes.AddRoute("/flushed").Get([&](KRESTServer& HTTP)
	{
		auto Lines = HTTP.NDJSONStream();

		KJSON First;
		First["line"] = 1;

		bool bSent = Lines.Write(First);
		bSent      = Lines.Flush() && bSent;

		WaitForClient();

		bSent = Lines.WriteAll(MakeRest()) && bSent;

		iLines   = Lines.GetLines();
		bAllSent = bSent;
	});

	// sends until the writer tells to end, for ten seconds at most
	Routes.AddRoute("/endless").Get([&](KRESTServer& HTTP)
	{
		auto Lines = HTTP.NDJSONStream(true);

		bGoodAtStart = Lines.Good();

		for (std::size_t i = 0; i < 1000 && Lines.Good(); ++i)
		{
			Lines.Write(KJSON(i));
			kSleep(chrono::milliseconds(10));
		}

		bWriterEnded = !Lines.Good();
		bRouteEnded  = true;
	});

	// fails after the first line
	Routes.AddRoute("/throws").Get([&](KRESTServer& HTTP)
	{
		auto Lines = HTTP.NDJSONStream(true);

		Lines.Write(KJSON(1));

		throw KHTTPError { KHTTPError::H5xx_ERROR, "failed after the first line" };
	});

	Routes.AddRoute("/json").Get([](KRESTServer& HTTP)
	{
		HTTP.json.tx["hello"] = "world";
	});

	KREST::Options Options;

	Options.Type                 = KREST::HTTP;
	Options.iPort                = 0;
	Options.sBindAddress         = "127.0.0.1";
	Options.Timeout              = chrono::seconds(2);
	Options.bBlocking            = false;
	Options.bCreateEphemeralCert = false;

	auto Server = std::make_unique<KREST>();
	REQUIRE ( Server->Execute(Options, Routes) );

	auto sEndPoint = kFormat("127.0.0.1:{}", Server->GetPort());

	// reads the header of a response, with one LF after each line
	auto ReadHeader = [](KTCPStream& Stream) -> KString
	{
		KString sHeader;
		KString sLine;

		while (Stream.ReadLine(sLine) && !sLine.empty())
		{
			sHeader += sLine;
			sHeader += '\n';
		}

		return sHeader;
	};

	// appends the next chunk of a chunked body to sBody, returns false after the last chunk
	auto ReadChunk = [](KTCPStream& Stream, KString& sBody) -> bool
	{
		KString sLine;

		if (!Stream.ReadLine(sLine))
		{
			return false;
		}

		auto iSize = sLine.UInt64(true);

		if (!iSize || Stream.Read(sBody, iSize) != iSize)
		{
			// the empty line after the last chunk
			Stream.ReadLine(sLine);
			return false;
		}

		// the line break after the chunk
		Stream.ReadLine(sLine);

		return true;
	};

	// reads a chunked body to its end, and tells when the first line is there
	auto ReadLines = [&](KTCPStream& Stream) -> KString
	{
		KString sLines;

		for (bool bMore = true; bMore;)
		{
			bMore = ReadChunk(Stream, sLines);

			if (sLines.contains('\n'))
			{
				bClientHasFirst = true;
			}
		}

		return sLines;
	};

	auto WaitForRouteEnd = [&]()
	{
		for (std::size_t i = 0; i < 1500 && !bRouteEnded; ++i)
		{
			kSleep(chrono::milliseconds(10));
		}
	};

	KStringView sLastRequest { "GET /json HTTP/1.1\r\nHost: a\r\nConnection: close\r\n\r\n" };

	SECTION("the lines arrive while the route runs, and the next request gets its response")
	{
		KTCPStream Stream(KTCPEndPoint(sEndPoint), KStreamOptions(chrono::seconds(10)));
		REQUIRE ( Stream.is_open() );

		Stream.Write(kFormat("GET /lines HTTP/1.1\r\nHost: a\r\n\r\n{}", sLastRequest)).Flush();

		auto sHeader = ReadHeader(Stream);

		INFO  ( sHeader );
		CHECK ( sHeader.starts_with("HTTP/1.1 200 ") );
		CHECK ( sHeader.contains("content-type: application/x-ndjson\n") );
		CHECK ( sHeader.contains("transfer-encoding: chunked\n") );
		CHECK ( sHeader.contains("connection: keep-alive\n") );
		CHECK_FALSE ( sHeader.contains("content-encoding") );

		auto sLines = ReadLines(Stream);

		CHECK_FALSE ( bWaitedInVain );
		CHECK ( bAllSent );
		CHECK ( iLines == 4 );
		CHECK ( sLines == sExpected );

		KString sRest;
		Stream.ReadRemaining(sRest);

		CHECK ( sRest.starts_with("HTTP/1.1 200 ") );
		CHECK ( sRest.contains("\"hello\"") );
	}

	SECTION("a client reads the lines with KHTTPClient and KNDJSONReader")
	{
		KURL        URL(kFormat("http://{}/lines", sEndPoint));
		KHTTPClient HTTP;

		HTTP.SetTimeout(chrono::seconds(5));
		HTTP.RequestCompression(false);

		REQUIRE ( HTTP.Connect(URL)  );
		REQUIRE ( HTTP.Resource(URL) );
		REQUIRE ( HTTP.SendRequest() );

		CHECK ( HTTP.Response.GetStatusCode() == 200 );
		CHECK ( HTTP.Response.Headers.Get(KHTTPHeader::CONTENT_TYPE) == KMIME::NDJSON );

		KNDJSONReader Reader(HTTP.Response.FilteredStream());
		KJSON         Value;

		REQUIRE ( Reader.Next(Value) );
		CHECK   ( Value.dump() == R"({"line":1})" );

		bClientHasFirst = true;

		auto Rest = Reader.GetAll();

		CHECK ( Reader.HasError() == false );
		CHECK ( Reader.GetLine()  == 4     );
		REQUIRE ( Rest.size() == 3 );
		CHECK ( Rest[0] == 2 );
		CHECK ( Rest[1] == "two\nlines" );
		CHECK_FALSE ( bWaitedInVain );
	}

	SECTION("a compressed stream is flushed after each line")
	{
		KTCPStream Stream(KTCPEndPoint(sEndPoint), KStreamOptions(chrono::seconds(10)));
		REQUIRE ( Stream.is_open() );

		Stream.Write("GET /compressed HTTP/1.1\r\nHost: a\r\nAccept-Encoding: gzip\r\nConnection: close\r\n\r\n").Flush();

		auto sHeader = ReadHeader(Stream);

		INFO  ( sHeader );
		CHECK ( sHeader.contains("content-type: application/x-ndjson\n") );
		CHECK ( sHeader.contains("content-encoding: gzip\n") );
		CHECK ( sHeader.contains("transfer-encoding: chunked\n") );

		KString sCompressed;

		while (ReadChunk(Stream, sCompressed))
		{
			// the header of the gzip format has 10 bytes, what follows is the first line
			if (sCompressed.size() > 10)
			{
				bClientHasFirst = true;
			}
		}

		CHECK_FALSE ( bWaitedInVain );
		CHECK ( bAllSent );

		KString     sLines;
		KUnCompress UnCompress(sCompressed, KUnCompressIStream::GZIP);
		UnCompress.ReadRemaining(sLines);

		CHECK ( sLines == sExpected );
	}

	SECTION("a compression that cannot be flushed is not used for lines that are sent at once")
	{
		KTCPStream Stream(KTCPEndPoint(sEndPoint), KStreamOptions(chrono::seconds(10)));
		REQUIRE ( Stream.is_open() );

		Stream.Write("GET /compressed HTTP/1.1\r\nHost: a\r\nAccept-Encoding: bzip2\r\nConnection: close\r\n\r\n").Flush();

		auto sHeader = ReadHeader(Stream);

		INFO  ( sHeader );
		CHECK ( sHeader.contains("content-type: application/x-ndjson\n") );
		CHECK_FALSE ( sHeader.contains("content-encoding") );

		auto sLines = ReadLines(Stream);

		CHECK_FALSE ( bWaitedInVain );
		CHECK ( bAllSent );
		CHECK ( sLines == sExpected );
	}

	SECTION("lines that are not sent at once arrive with the end of the route")
	{
		KTCPStream Stream(KTCPEndPoint(sEndPoint), KStreamOptions(chrono::seconds(10)));
		REQUIRE ( Stream.is_open() );

		Stream.Write(kFormat("GET /buffered HTTP/1.1\r\nHost: a\r\nAccept-Encoding: gzip\r\n\r\n{}", sLastRequest)).Flush();

		auto sHeader = ReadHeader(Stream);

		INFO  ( sHeader );
		CHECK ( sHeader.contains("content-type: application/x-ndjson\n") );
		CHECK ( sHeader.contains("content-encoding: gzip\n") );
		CHECK ( sHeader.contains("transfer-encoding: chunked\n") );

		KString sCompressed;

		while (ReadChunk(Stream, sCompressed)) {}

		KString     sLines;
		KUnCompress UnCompress(sCompressed, KUnCompressIStream::GZIP);
		UnCompress.ReadRemaining(sLines);

		CHECK ( bAllSent );
		CHECK ( iLines == 4 );
		CHECK ( sLines == sExpected );

		KString sRest;
		Stream.ReadRemaining(sRest);

		CHECK ( sRest.starts_with("HTTP/1.1 200 ") );
		CHECK ( sRest.contains("\"hello\"") );
	}

	SECTION("Flush() sends the lines that are buffered")
	{
		KTCPStream Stream(KTCPEndPoint(sEndPoint), KStreamOptions(chrono::seconds(10)));
		REQUIRE ( Stream.is_open() );

		Stream.Write("GET /flushed HTTP/1.1\r\nHost: a\r\nConnection: close\r\n\r\n").Flush();

		auto sHeader = ReadHeader(Stream);

		INFO  ( sHeader );
		CHECK ( sHeader.contains("content-type: application/x-ndjson\n") );

		auto sLines = ReadLines(Stream);

		CHECK_FALSE ( bWaitedInVain );
		CHECK ( bAllSent );
		CHECK ( iLines == 4 );
		CHECK ( sLines == sExpected );
	}

	SECTION("the writer tells when the client left")
	{
		{
			KTCPStream Stream(KTCPEndPoint(sEndPoint), KStreamOptions(chrono::seconds(10)));
			REQUIRE ( Stream.is_open() );

			Stream.Write("GET /endless HTTP/1.1\r\nHost: a\r\n\r\n").Flush();

			auto sHeader = ReadHeader(Stream);
			CHECK ( sHeader.contains("content-type: application/x-ndjson\n") );

			KString sBody;
			CHECK ( ReadChunk(Stream, sBody) );
			CHECK ( sBody.starts_with("0\n") );
		}

		WaitForRouteEnd();

		CHECK ( bRouteEnded  );
		CHECK ( bGoodAtStart );
		CHECK ( bWriterEnded );
	}

	SECTION("a HEAD request gets the headers, and the writer is not good")
	{
		KTCPStream Stream(KTCPEndPoint(sEndPoint), KStreamOptions(chrono::seconds(10)));
		REQUIRE ( Stream.is_open() );

		Stream.Write(kFormat("HEAD /endless HTTP/1.1\r\nHost: a\r\n\r\n{}", sLastRequest)).Flush();

		KString sResponse;
		Stream.ReadRemaining(sResponse);

		INFO  ( sResponse );
		CHECK ( bRouteEnded );
		CHECK_FALSE ( bGoodAtStart );
		CHECK ( sResponse.contains("content-type: application/x-ndjson\r\n") );
		// no body, the second response follows the headers of the first
		CHECK ( sResponse.contains("\r\n\r\nHTTP/1.1 200 ") );
		CHECK ( sResponse.contains("\"hello\"") );
	}

	SECTION("a route that throws ends the response without its last chunk")
	{
		KTCPStream Stream(KTCPEndPoint(sEndPoint), KStreamOptions(chrono::seconds(10)));
		REQUIRE ( Stream.is_open() );

		Stream.Write(kFormat("GET /throws HTTP/1.1\r\nHost: a\r\n\r\n{}", sLastRequest)).Flush();

		auto sHeader = ReadHeader(Stream);

		INFO  ( sHeader );
		CHECK ( sHeader.starts_with("HTTP/1.1 200 ") );
		CHECK ( sHeader.contains("transfer-encoding: chunked\n") );

		KString sRest;
		Stream.ReadRemaining(sRest);

		// the chunk with the first line, and nothing after it: neither the last chunk
		// nor the response to the second request
		CHECK ( sRest.ends_with("\r\n1\n\r\n") );
		CHECK ( sRest.size() < 12 );
		CHECK_FALSE ( sRest.contains("HTTP/1.1") );
	}
}

TEST_CASE("KRESTServer: trace context")
{
	// the example of the W3C Trace Context
	KStringView sTraceParent { "00-4bf92f3577b34da6a3ce929d0e0e4736-00f067aa0ba902b7-01" };
	KStringView sTraceID     { "4bf92f3577b34da6a3ce929d0e0e4736" };

	// what the route and the post response callback of the error route saw
	std::mutex        Mutex;
	KString           sSeenInRoute;
	KString           sSeenAfterError;

	KRESTRoutes Routes;

	// returns the context of the request and of the thread
	Routes.AddRoute("/trace").Get([](KRESTServer& HTTP)
	{
		const auto& Trace = HTTP.GetTraceContext();

		HTTP.json.tx["trace_id"]    = Trace.GetTraceID();
		HTTP.json.tx["span_id"]     = Trace.GetSpanID();
		HTTP.json.tx["parent_id"]   = Trace.GetParentID();
		HTTP.json.tx["tracestate"]  = Trace.GetTraceState();
		HTTP.json.tx["thread"]      = KTraceContext::Current().GetTraceID();
	});

	// fails after it set a callback, which runs at the end of the request
	Routes.AddRoute("/fail").Get([&](KRESTServer& HTTP)
	{
		{
			std::lock_guard<std::mutex> Lock(Mutex);
			sSeenInRoute = KTraceContext::Current().GetTraceID();
		}

		HTTP.SetPostResponseCallback([&](const KRESTServer&)
		{
			std::lock_guard<std::mutex> Lock(Mutex);
			sSeenAfterError = KTraceContext::Current().GetTraceID();
		});

		throw KHTTPError { KHTTPError::H4xx_BADREQUEST, "failing on purpose" };
	});

	KTempDir TempDir;

	auto sJSONLog   = kFormat("{}/access.json", TempDir.Name());
	auto sParsedLog = kFormat("{}/access.log" , TempDir.Name());

	// runs a server with the trace options, and requests /trace with the headers
	auto Request = [&](KRESTServer::Options::TraceMode Mode, KStringView sSources, KStringView sHeaders,
	                   KStringView sPath = "/trace", KStringView sTrustedProxies = KStringView{}) -> KJSON
	{
		KREST::Options Options;

		Options.Type                 = KREST::HTTP;
		Options.iPort                = 0;
		Options.sBindAddress         = "127.0.0.1";
		Options.Timeout              = chrono::seconds(2);
		Options.bBlocking            = false;
		Options.bCreateEphemeralCert = false;
		Options.Trace                = Mode;

		Options.AddTraceSources(sSources);
		Options.AddTrustedProxies(sTrustedProxies);

		KREST Server;
		REQUIRE ( Server.Execute(Options, Routes) );

		KTCPStream Stream(KTCPEndPoint(kFormat("127.0.0.1:{}", Server.GetPort())), KStreamOptions(chrono::seconds(5)));
		REQUIRE ( Stream.is_open() );

		Stream.Write(kFormat("GET {} HTTP/1.1\r\nHost: a\r\nConnection: close\r\n{}\r\n", sPath, sHeaders)).Flush();

		KString sResponse;
		Stream.ReadRemaining(sResponse);

		auto iBody = sResponse.find("\r\n\r\n");
		REQUIRE ( iBody != npos );

		INFO ( sResponse );

		return kjson::Parse(sResponse.substr(iBody + 4));
	};

	auto IsNewTrace = [&](const KJSON& j)
	{
		CHECK ( j("trace_id").String().size() == 32 );
		CHECK ( j("trace_id").String() != sTraceID );
		CHECK ( j("span_id").String().size()  == 16 );
		CHECK ( j("parent_id").String().empty() );
		CHECK ( j("tracestate").String().empty() );
		CHECK ( j("thread") == j("trace_id") );
	};

	auto sHeaders = kFormat("traceparent: {}\r\ntracestate: a=1\r\n", sTraceParent);

	SECTION("off: no trace context")
	{
		auto j = Request(KRESTServer::Options::TraceMode::Off, "127.0.0.1", sHeaders);

		CHECK ( j("trace_id").String().empty() );
		CHECK ( j("thread").String().empty() );
	}

	SECTION("start: every request starts a new trace")
	{
		IsNewTrace(Request(KRESTServer::Options::TraceMode::Start, "127.0.0.1", sHeaders));
		IsNewTrace(Request(KRESTServer::Options::TraceMode::Start, "127.0.0.1", ""));
	}

	SECTION("continue: the trace of a trace source")
	{
		auto j = Request(KRESTServer::Options::TraceMode::Continue, "127.0.0.1/32", sHeaders);

		CHECK ( j("trace_id").String()   == sTraceID );
		CHECK ( j("parent_id").String()  == "00f067aa0ba902b7" );
		CHECK ( j("span_id").String().size() == 16 );
		CHECK ( j("span_id").String()    != "00f067aa0ba902b7" );
		CHECK ( j("tracestate").String() == "a=1" );
		CHECK ( j("thread").String()     == sTraceID );

		// any case of the header name
		j = Request(KRESTServer::Options::TraceMode::Continue, "0.0.0.0/0,::/0", kFormat("TraceParent: {}\r\n", sTraceParent));
		CHECK ( j("trace_id").String() == sTraceID );
	}

	SECTION("continue: a new trace for other sources, without header, or with an invalid one")
	{
		IsNewTrace(Request(KRESTServer::Options::TraceMode::Continue, "10.0.0.0/8", sHeaders));
		IsNewTrace(Request(KRESTServer::Options::TraceMode::Continue, "", sHeaders));
		IsNewTrace(Request(KRESTServer::Options::TraceMode::Continue, "127.0.0.1", ""));
		IsNewTrace(Request(KRESTServer::Options::TraceMode::Continue, "127.0.0.1", "traceparent: 00-4BF92F3577B34DA6A3CE929D0E0E4736-00f067aa0ba902b7-01\r\n"));
	}

	SECTION("continue: behind a trusted proxy the client that the proxy confirms counts")
	{
		auto sForwarded = kFormat("X-Forwarded-For: 10.1.2.3\r\n{}", sHeaders);

		// the client is a trace source
		auto j = Request(KRESTServer::Options::TraceMode::Continue, "10.0.0.0/8", sForwarded, "/trace", "127.0.0.1");
		CHECK ( j("trace_id").String() == sTraceID );

		// the proxy is one, but not its client
		IsNewTrace(Request(KRESTServer::Options::TraceMode::Continue, "127.0.0.1", sForwarded, "/trace", "127.0.0.1"));

		// without trusted proxies the forwarded address does not count, the direct peer does
		IsNewTrace(Request(KRESTServer::Options::TraceMode::Continue, "10.0.0.0/8", sForwarded));
		j = Request(KRESTServer::Options::TraceMode::Continue, "127.0.0.1", sForwarded);
		CHECK ( j("trace_id").String() == sTraceID );
	}

	SECTION("a failing request keeps its trace to the end")
	{
		Request(KRESTServer::Options::TraceMode::Continue, "127.0.0.1", sHeaders, "/fail");

		std::lock_guard<std::mutex> Lock(Mutex);
		CHECK ( sSeenInRoute    == sTraceID );
		CHECK ( sSeenAfterError == sTraceID );
	}

	SECTION("the access log has the trace id")
	{
		for (auto Mode : { KRESTServer::Options::TraceMode::Continue, KRESTServer::Options::TraceMode::Off })
		{
			bool bOn = Mode != KRESTServer::Options::TraceMode::Off;

			KREST::Options Options;

			Options.Type                 = KREST::HTTP;
			Options.iPort                = 0;
			Options.sBindAddress         = "127.0.0.1";
			Options.Timeout              = chrono::seconds(2);
			Options.bBlocking            = false;
			Options.bCreateEphemeralCert = false;
			Options.Trace                = Mode;

			Options.AddTraceSources("127.0.0.1");

			KString sLog = bOn ? sJSONLog : sParsedLog;

			if (bOn)
			{
				REQUIRE ( Options.Logger.Open(KHTTPLog::LOG_FORMAT::JSON, sJSONLog) );
			}
			else
			{
				REQUIRE ( Options.Logger.Open(KHTTPLog::LOG_FORMAT::PARSED, sParsedLog, "%s [%L]") );
			}

			{
				KREST Server;
				REQUIRE ( Server.Execute(Options, Routes) );

				KTCPStream Stream(KTCPEndPoint(kFormat("127.0.0.1:{}", Server.GetPort())), KStreamOptions(chrono::seconds(5)));
				REQUIRE ( Stream.is_open() );

				Stream.Write(kFormat("GET /trace HTTP/1.1\r\nHost: a\r\nConnection: close\r\n{}\r\n", sHeaders)).Flush();

				KString sResponse;
				Stream.ReadRemaining(sResponse);

				for (int i = 0; i < 500 && !kReadAll(sLog).contains("200"); ++i)
				{
					kSleep(chrono::milliseconds(10));
				}
			}

			auto sLine = kReadAll(sLog);

			INFO  ( sLine );

			if (bOn)
			{
				auto j = kjson::Parse(sLine);
				CHECK ( j("trace_id").String() == sTraceID );
				CHECK ( j("span_id").String().size() == 16 );
			}
			else
			{
				// without a trace the field is empty
				CHECK ( sLine.starts_with("200 [") );
				CHECK_FALSE ( sLine.contains(sTraceID) );
			}
		}

		// %L with a trace
		KREST::Options Options;

		Options.Type                 = KREST::HTTP;
		Options.iPort                = 0;
		Options.sBindAddress         = "127.0.0.1";
		Options.Timeout              = chrono::seconds(2);
		Options.bBlocking            = false;
		Options.bCreateEphemeralCert = false;
		Options.Trace                = KRESTServer::Options::TraceMode::Continue;

		Options.AddTraceSources("127.0.0.1");

		auto sTracedLog = kFormat("{}/traced.log", TempDir.Name());
		REQUIRE ( Options.Logger.Open(KHTTPLog::LOG_FORMAT::PARSED, sTracedLog, "%s [%L]") );

		{
			KREST Server;
			REQUIRE ( Server.Execute(Options, Routes) );

			KTCPStream Stream(KTCPEndPoint(kFormat("127.0.0.1:{}", Server.GetPort())), KStreamOptions(chrono::seconds(5)));
			REQUIRE ( Stream.is_open() );

			Stream.Write(kFormat("GET /trace HTTP/1.1\r\nHost: a\r\nConnection: close\r\n{}\r\n", sHeaders)).Flush();

			KString sResponse;
			Stream.ReadRemaining(sResponse);

			for (int i = 0; i < 500 && !kReadAll(sTracedLog).contains("200"); ++i)
			{
				kSleep(chrono::milliseconds(10));
			}
		}

		auto sLine = kReadAll(sTracedLog);

		INFO  ( sLine );
		CHECK ( sLine.starts_with(kFormat("200 [{}]", sTraceID)) );
	}
}

TEST_CASE("KHTTPClient: trace context")
{
	// service B continues the traces of local callers, and returns what it got
	KRESTRoutes RoutesB;

	RoutesB.AddRoute("/echo").Get([](KRESTServer& HTTP)
	{
		HTTP.json.tx["traceparent"] = HTTP.Request.Headers.Get(KHTTPHeader::TRACEPARENT);
		HTTP.json.tx["tracestate"]  = HTTP.Request.Headers.Get(KHTTPHeader::TRACESTATE);
		HTTP.json.tx["trace_id"]    = HTTP.GetTraceContext().GetTraceID();
		HTTP.json.tx["parent_id"]   = HTTP.GetTraceContext().GetParentID();
	});

	KREST::Options OptionsB;

	OptionsB.Type                 = KREST::HTTP;
	OptionsB.iPort                = 0;
	OptionsB.sBindAddress         = "127.0.0.1";
	OptionsB.Timeout              = chrono::seconds(2);
	OptionsB.bBlocking            = false;
	OptionsB.bCreateEphemeralCert = false;
	OptionsB.Trace                = KRESTServer::Options::TraceMode::Continue;

	OptionsB.AddTraceSources("127.0.0.1");

	KREST ServiceB;
	REQUIRE ( ServiceB.Execute(OptionsB, RoutesB) );

	KURL EchoURL(kFormat("http://127.0.0.1:{}/echo", ServiceB.GetPort()));

	auto Echo = [&](KWebClient& Client) -> KJSON
	{
		auto sBody = Client.Get(EchoURL);
		INFO ( sBody );
		return kjson::Parse(sBody);
	};

	SECTION("off unless switched on")
	{
		KTraceContext::Scope Trace(KTraceContext::Start());

		KWebClient Client;
		CHECK_FALSE ( Client.GetPropagateTraceContext() );

		auto j = Echo(Client);
		CHECK ( j("traceparent").String().empty() );
	}

	SECTION("nothing without a context of the thread")
	{
		KWebClient Client;
		Client.PropagateTraceContext(true);

		auto j = Echo(Client);
		CHECK ( j("traceparent").String().empty() );
	}

	SECTION("the context of the thread goes with the request")
	{
		KTraceContext Context;
		REQUIRE ( Context.Parse("00-4bf92f3577b34da6a3ce929d0e0e4736-00f067aa0ba902b7-01", "a=1,b=2") );

		KTraceContext::Scope Trace(Context);

		KWebClient Client;
		Client.PropagateTraceContext(true);

		auto j = Echo(Client);

		CHECK ( j("traceparent").String() == Context.Serialize() );
		CHECK ( j("tracestate").String()  == "a=1,b=2" );
		// the receiver continues the trace, with our span as its parent
		CHECK ( j("trace_id").String()    == "4bf92f3577b34da6a3ce929d0e0e4736" );
		CHECK ( j("parent_id").String()   == Context.GetSpanID() );
	}

	SECTION("a traceparent of the caller stays")
	{
		KTraceContext::Scope Trace(KTraceContext::Start());

		KWebClient Client;
		Client.PropagateTraceContext(true);
		Client.AddHeader(KHTTPHeader::TRACEPARENT, "00-0af7651916cd43dd8448eb211c80319c-b7ad6b7169203331-01");

		auto j = Echo(Client);
		CHECK ( j("traceparent").String() == "00-0af7651916cd43dd8448eb211c80319c-b7ad6b7169203331-01" );
	}

	SECTION("each request of a client takes the context that the thread has then")
	{
		KWebClient Client;
		Client.PropagateTraceContext(true);

		auto First = KTraceContext::Start();

		{
			KTraceContext::Scope Trace(First);
			CHECK ( Echo(Client)("traceparent").String() == First.Serialize() );
		}

		// no stale header without a context
		CHECK ( Echo(Client)("traceparent").String().empty() );

		auto Second = KTraceContext::Start();

		{
			KTraceContext::Scope Trace(Second);
			CHECK ( Echo(Client)("traceparent").String() == Second.Serialize() );
		}
	}

	SECTION("the default for new clients")
	{
		KTraceContext::Scope Trace(KTraceContext::Start());

		KHTTPClient::SetDefaultTracePropagation(true);
		KWebClient On;
		KHTTPClient::SetDefaultTracePropagation(false);
		KWebClient Off;

		CHECK ( On.GetPropagateTraceContext() );
		CHECK_FALSE ( Off.GetPropagateTraceContext() );
		CHECK_FALSE ( Echo(On)("traceparent").String().empty() );
		CHECK ( Echo(Off)("traceparent").String().empty() );
	}

	SECTION("a request through two services keeps its trace")
	{
		// service A starts a trace for each request, and asks service B
		KRESTRoutes RoutesA;

		RoutesA.AddRoute("/call").Get([&](KRESTServer& HTTP)
		{
			KWebClient Client;
			Client.PropagateTraceContext(true);

			HTTP.json.tx["trace_id"] = HTTP.GetTraceContext().GetTraceID();
			HTTP.json.tx["span_id"]  = HTTP.GetTraceContext().GetSpanID();
			HTTP.json.tx["b"]        = kjson::Parse(Client.Get(EchoURL));
		});

		KREST::Options OptionsA;

		OptionsA.Type                 = KREST::HTTP;
		OptionsA.iPort                = 0;
		OptionsA.sBindAddress         = "127.0.0.1";
		OptionsA.Timeout              = chrono::seconds(2);
		OptionsA.bBlocking            = false;
		OptionsA.bCreateEphemeralCert = false;
		OptionsA.Trace                = KRESTServer::Options::TraceMode::Start;

		KREST ServiceA;
		REQUIRE ( ServiceA.Execute(OptionsA, RoutesA) );

		KWebClient Client;
		auto sBody = Client.Get(KURL(kFormat("http://127.0.0.1:{}/call", ServiceA.GetPort())));

		INFO ( sBody );

		auto j = kjson::Parse(sBody);

		CHECK ( j("trace_id").String().size() == 32 );
		CHECK ( j("b")("trace_id").String()  == j("trace_id").String() );
		CHECK ( j("b")("parent_id").String() == j("span_id").String() );
	}
}
