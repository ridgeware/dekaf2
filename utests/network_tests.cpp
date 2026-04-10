#include "catch.hpp"

#include <dekaf2/core/types/kdefinitions.h>
#include <dekaf2/rest/framework/krest.h>
#include <dekaf2/rest/framework/krestclient.h>
#include <dekaf2/http/server/khttperror.h>
#include <dekaf2/net/tcp/ktcpserver.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/system/os/ksystem.h>
#include <dekaf2/system/filesystem/kfilesystem.h>
#include <dekaf2/http/client/kwebclient.h>

#ifndef DEKAF2_IS_WINDOWS

using namespace dekaf2;

namespace {

// ============================================================================
// Test route handlers
// ============================================================================

void json_echo_handler(KRESTServer& REST)
{
	REST.json.tx = REST.json.rx;
	REST.json.tx["echo"] = true;
}

void not_found_handler(KRESTServer& REST)
{
	throw KHTTPError { KHTTPError::H4xx_NOTFOUND, "resource does not exist" };
}

void bad_request_handler(KRESTServer& REST)
{
	throw KHTTPError { KHTTPError::H4xx_BADREQUEST, "missing parameters" };
}

void server_error_handler(KRESTServer& REST)
{
	throw KHTTPError { KHTTPError::H5xx_ERROR, "internal failure" };
}

void plain_echo_handler(KRESTServer& REST)
{
	REST.SetRawOutput(REST.GetRequestBody());
}

// use high ports to avoid conflicts with other tests (30303, 6780, 7653, 7654)
uint16_t g_iHTTPPort  = 18710;
uint16_t g_iHTTPSPort = 18750;

} // end of anonymous namespace

// ============================================================================
// Network Stream and HTTP Tests
// ============================================================================

TEST_CASE("KNetwork")
{
	// ========================================================================
	// TCP Tests
	// ========================================================================

	SECTION("TCP basic JSON")
	{
		KRESTRoutes Routes;
		Routes.AddRoute({ KHTTPMethod::GET, false, "/ping", [](KRESTServer& http)
		{
			http.json.tx["pong"] = true;
		}});
		Routes.AddRoute({ KHTTPMethod::POST, false, "/echo", json_echo_handler });

		KREST::Options Options;
		Options.Type      = KREST::HTTP;
		Options.iPort     = g_iHTTPPort;
		Options.iTimeout  = 3;
		Options.bBlocking = false;
		Options.bCreateEphemeralCert = false;

		KREST Server;
		REQUIRE ( Server.Execute(Options, Routes) );

		// basic GET
		{
			KHTTPError ec;
			KJsonRestClient Client(kFormat("http://localhost:{}", g_iHTTPPort));
			Client.RequestCompression(false);
			Client.AllowConnectionRetry(false);

			auto jResult = Client.Get("ping").SetError(ec).Request();
			CHECK ( ec.value() == 0 );
			CHECK ( kjson::GetBool(jResult, "pong") == true );
		}

		// POST echo
		{
			KHTTPError ec;
			KJsonRestClient Client(kFormat("http://localhost:{}", g_iHTTPPort));
			Client.RequestCompression(false);
			Client.AllowConnectionRetry(false);

			auto jResult = Client.Post("echo")
			                     .SetError(ec)
			                     .Request(KJSON{{ "hello", "world" }});

			CHECK ( ec.value() == 0 );
			CHECK ( kjson::GetStringRef(jResult, "hello") == "world" );
			CHECK ( kjson::GetBool(jResult, "echo") == true );
		}
	}

	SECTION("TCP keepalive multiple requests")
	{
		KRESTRoutes Routes;
		uint16_t iCallCount { 0 };

		Routes.AddRoute({ KHTTPMethod::GET, false, "/count", [&](KRESTServer& http)
		{
			++iCallCount;
			http.json.tx["count"] = iCallCount;
		}});

		KREST::Options Options;
		Options.Type      = KREST::HTTP;
		Options.iPort     = g_iHTTPPort + 1;
		Options.iTimeout  = 3;
		Options.bBlocking = false;
		Options.bCreateEphemeralCert = false;

		KREST Server;
		REQUIRE ( Server.Execute(Options, Routes) );

		{
			KHTTPError ec;
			KJsonRestClient Client(kFormat("http://localhost:{}", g_iHTTPPort + 1));
			Client.RequestCompression(false);
			Client.AllowConnectionRetry(false);

			// send multiple requests on the same keepalive connection
			for (int i = 1; i <= 5; ++i)
			{
				auto jResult = Client.Get("count").SetError(ec).Request();
				CHECK ( ec.value() == 0 );
				CHECK ( kjson::GetUInt(jResult, "count") == static_cast<uint64_t>(i) );
			}

			CHECK ( iCallCount == 5 );
		}
	}

	SECTION("TCP large payload transfer")
	{
		KRESTRoutes Routes;
		Routes.AddRoute({ KHTTPMethod::GET, false, "/large", [](KRESTServer& http)
		{
			// generate a deterministic large JSON response (~100KB)
			KJSON jArray = KJSON::array();
			for (int i = 0; i < 1000; ++i)
			{
				jArray.push_back(kFormat("item_{:06d}_padding_to_make_it_larger", i));
			}
			http.json.tx["data"] = std::move(jArray);
			http.json.tx["count"] = 1000;
		}});

		KREST::Options Options;
		Options.Type      = KREST::HTTP;
		Options.iPort     = g_iHTTPPort + 2;
		Options.iTimeout  = 5;
		Options.bBlocking = false;
		Options.bCreateEphemeralCert = false;

		KREST Server;
		REQUIRE ( Server.Execute(Options, Routes) );

		{
			KHTTPError ec;
			KJsonRestClient Client(kFormat("http://localhost:{}", g_iHTTPPort + 2));
			Client.RequestCompression(false);
			Client.AllowConnectionRetry(false);

			auto jResult = Client.Get("large").SetError(ec).Request();
			CHECK ( ec.value() == 0 );
			CHECK ( kjson::GetUInt(jResult, "count") == 1000 );
			CHECK ( jResult["data"].is_array() );
			CHECK ( jResult["data"].size() == 1000 );
		}
	}

	SECTION("TCP 4xx errors with keepalive")
	{
		KRESTRoutes Routes;
		uint16_t iCallCount { 0 };

		Routes.AddRoute({ KHTTPMethod::GET, false, "/ok", [&](KRESTServer& http)
		{
			++iCallCount;
			http.json.tx["status"] = "ok";
		}});

		Routes.AddRoute({ KHTTPMethod::GET, false, "/notfound", not_found_handler });
		Routes.AddRoute({ KHTTPMethod::GET, false, "/badrequest", bad_request_handler });

		KREST::Options Options;
		Options.Type      = KREST::HTTP;
		Options.iPort     = g_iHTTPPort + 3;
		Options.iTimeout  = 3;
		Options.bBlocking = false;
		Options.bCreateEphemeralCert = false;

		KREST Server;
		REQUIRE ( Server.Execute(Options, Routes) );

		{
			KHTTPError ec;
			KJsonRestClient Client(kFormat("http://localhost:{}", g_iHTTPPort + 3));
			Client.RequestCompression(false);
			Client.AllowConnectionRetry(false);

			// first request succeeds
			auto jResult = Client.Get("ok").SetError(ec).Request();
			CHECK ( ec.value() == 0 );
			CHECK ( iCallCount == 1 );

			// 404 - should maintain keepalive (GET has no body, IsInputConsumed == true)
			jResult = Client.Get("notfound").SetError(ec).Request();
			CHECK ( ec.value() == 404 );

			// third request should still work on same connection
			jResult = Client.Get("ok").SetError(ec).Request();
			CHECK ( ec.value() == 0 );
			CHECK ( iCallCount == 2 );

			// 400 error should also maintain keepalive for bodyless GET
			jResult = Client.Get("badrequest").SetError(ec).Request();
			CHECK ( ec.value() == 400 );

			// still alive
			jResult = Client.Get("ok").SetError(ec).Request();
			CHECK ( ec.value() == 0 );
			CHECK ( iCallCount == 3 );
		}
	}

	SECTION("TCP 5xx errors break keepalive")
	{
		KRESTRoutes Routes;
		uint16_t iCallCount { 0 };

		Routes.AddRoute({ KHTTPMethod::GET, false, "/ok", [&](KRESTServer& http)
		{
			++iCallCount;
			http.json.tx["status"] = "ok";
		}});

		Routes.AddRoute({ KHTTPMethod::GET, false, "/error", server_error_handler });

		KREST::Options Options;
		Options.Type      = KREST::HTTP;
		Options.iPort     = g_iHTTPPort + 4;
		Options.iTimeout  = 3;
		Options.bBlocking = false;
		Options.bCreateEphemeralCert = false;

		KREST Server;
		REQUIRE ( Server.Execute(Options, Routes) );

		{
			KHTTPError ec;
			KJsonRestClient Client(kFormat("http://localhost:{}", g_iHTTPPort + 4));
			Client.RequestCompression(false);
			Client.AllowConnectionRetry(false);

			auto jResult = Client.Get("ok").SetError(ec).Request();
			CHECK ( ec.value() == 0 );
			CHECK ( iCallCount == 1 );

			// 500 error - connection should be closed
			jResult = Client.Get("error").SetError(ec).Request();
			CHECK ( ec.value() == 500 );
		}
	}

	// ========================================================================
	// TLS Tests
	// ========================================================================

	SECTION("TLS basic with ephemeral cert")
	{
		KRESTRoutes Routes;
		uint16_t iCallCount { 0 };

		Routes.AddRoute({ KHTTPMethod::GET, false, "/secure", [&](KRESTServer& http)
		{
			++iCallCount;
			http.json.tx["secure"] = true;
		}});

		Routes.AddRoute({ KHTTPMethod::POST, false, "/echo", json_echo_handler });

		KREST::Options Options;
		Options.Type      = KREST::HTTP;
		Options.iPort     = g_iHTTPSPort;
		Options.iTimeout  = 5;
		Options.bBlocking = false;
		Options.bCreateEphemeralCert = true;
		Options.bStoreEphemeralCert  = false;

		KREST Server;
		REQUIRE ( Server.Execute(Options, Routes) );

		{
			KHTTPError ec;
			KJsonRestClient Client(kFormat("https://localhost:{}", g_iHTTPSPort));
			Client.RequestCompression(false);
			Client.AllowConnectionRetry(false);
			Client.SetStreamOptions(KStreamOptions(false));

			// basic TLS GET
			auto jResult = Client.Get("secure").SetError(ec).Request();
			CHECK ( ec.value() == 0 );
			CHECK ( iCallCount == 1 );
			CHECK ( kjson::GetBool(jResult, "secure") == true );

			// TLS keepalive: second request on same connection
			jResult = Client.Get("secure").SetError(ec).Request();
			CHECK ( ec.value() == 0 );
			CHECK ( iCallCount == 2 );
		}

		// POST with JSON body over TLS
		{
			KHTTPError ec;
			KJsonRestClient Client(kFormat("https://localhost:{}", g_iHTTPSPort));
			Client.RequestCompression(false);
			Client.AllowConnectionRetry(false);
			Client.SetStreamOptions(KStreamOptions(false));

			auto jResult = Client.Post("echo")
			                     .SetError(ec)
			                     .Request(KJSON{{ "data", "encrypted" }});

			CHECK ( ec.value() == 0 );
			CHECK ( kjson::GetStringRef(jResult, "data") == "encrypted" );
			CHECK ( kjson::GetBool(jResult, "echo") == true );
		}
	}

	SECTION("TLS keepalive with 4xx errors")
	{
		KRESTRoutes Routes;
		uint16_t iCallCount { 0 };

		Routes.AddRoute({ KHTTPMethod::GET, false, "/ok", [&](KRESTServer& http)
		{
			++iCallCount;
			http.json.tx["status"] = "ok";
		}});

		Routes.AddRoute({ KHTTPMethod::GET, false, "/notfound", not_found_handler });

		KREST::Options Options;
		Options.Type      = KREST::HTTP;
		Options.iPort     = g_iHTTPSPort + 1;
		Options.iTimeout  = 5;
		Options.bBlocking = false;
		Options.bCreateEphemeralCert = true;
		Options.bStoreEphemeralCert  = false;

		KREST Server;
		REQUIRE ( Server.Execute(Options, Routes) );

		{
			KHTTPError ec;
			KJsonRestClient Client(kFormat("https://localhost:{}", g_iHTTPSPort + 1));
			Client.RequestCompression(false);
			Client.AllowConnectionRetry(false);
			Client.SetStreamOptions(KStreamOptions(false));

			auto jResult = Client.Get("ok").SetError(ec).Request();
			CHECK ( ec.value() == 0 );
			CHECK ( iCallCount == 1 );

			// 404 should maintain TLS keepalive
			jResult = Client.Get("notfound").SetError(ec).Request();
			CHECK ( ec.value() == 404 );

			// connection should still work
			jResult = Client.Get("ok").SetError(ec).Request();
			CHECK ( ec.value() == 0 );
			CHECK ( iCallCount == 2 );
		}
	}

	// ========================================================================
	// Unix Socket Tests
	// ========================================================================

#ifdef DEKAF2_HAS_UNIX_SOCKETS

	SECTION("Unix socket basic")
	{
		KRESTRoutes Routes;
		uint16_t iCallCount { 0 };

		Routes.AddRoute({ KHTTPMethod::GET,  false, "/ping", [&](KRESTServer& http)
		{
			++iCallCount;
			http.SetRawOutput("pong");
		}, KRESTRoute::PLAIN });

		Routes.AddRoute({ KHTTPMethod::POST, false, "/echo", plain_echo_handler, KRESTRoute::PLAIN });

		KTempDir SocketDir(true, 95);
		KString sSocketFile = kFormat("{}/test.sock", SocketDir.Name());

		KREST::Options Options;
		Options.Type        = KREST::UNIX;
		Options.sSocketFile = sSocketFile;
		Options.iTimeout    = 3;
		Options.bBlocking   = false;

		KREST Server;
		REQUIRE ( Server.Execute(Options, Routes) );

		{
			KWebClient HTTP;
			HTTP.SetTimeout(chrono::seconds(3));
			KURL ConnectURL = kFormat("unix://{}", sSocketFile);

			auto sResult = HTTP.HttpRequest2Host(ConnectURL, "localhost/ping", KHTTPMethod::GET);
			CHECK ( HTTP.GetStatusCode() == 200 );
			CHECK ( sResult == "pong" );
			CHECK ( iCallCount == 1 );
			HTTP.Disconnect();

			// second call
			sResult = HTTP.HttpRequest2Host(ConnectURL, "localhost/ping", KHTTPMethod::GET);
			CHECK ( HTTP.GetStatusCode() == 200 );
			CHECK ( sResult == "pong" );
			CHECK ( iCallCount == 2 );
		}

		// POST with body
		{
			KWebClient HTTP;
			HTTP.SetTimeout(chrono::seconds(3));
			KURL ConnectURL = kFormat("unix://{}", sSocketFile);

			auto sResult = HTTP.HttpRequest2Host(ConnectURL, "localhost/echo", KHTTPMethod::POST, "hello unix", KMIME::TEXT_PLAIN);
			CHECK ( HTTP.GetStatusCode() == 200 );
			CHECK ( sResult == "hello unix" );
		}
	}

	SECTION("Unix socket 4xx keepalive")
	{
		KRESTRoutes Routes;
		uint16_t iCallCount { 0 };

		Routes.AddRoute({ KHTTPMethod::GET, false, "/ok", [&](KRESTServer& http)
		{
			++iCallCount;
			http.SetRawOutput("ok");
		}, KRESTRoute::PLAIN });

		Routes.AddRoute({ KHTTPMethod::GET, false, "/missing", not_found_handler });

		KTempDir SocketDir(true, 95);
		KString sSocketFile = kFormat("{}/test2.sock", SocketDir.Name());

		KREST::Options Options;
		Options.Type        = KREST::UNIX;
		Options.sSocketFile = sSocketFile;
		Options.iTimeout    = 3;
		Options.bBlocking   = false;

		KREST Server;
		REQUIRE ( Server.Execute(Options, Routes) );

		{
			KWebClient HTTP;
			HTTP.SetTimeout(chrono::seconds(3));
			KURL ConnectURL = kFormat("unix://{}", sSocketFile);

			auto sResult = HTTP.HttpRequest2Host(ConnectURL, "localhost/ok", KHTTPMethod::GET);
			CHECK ( HTTP.GetStatusCode() == 200 );
			CHECK ( iCallCount == 1 );
			HTTP.Disconnect();

			// 404 should keep connection alive
			sResult = HTTP.HttpRequest2Host(ConnectURL, "localhost/missing", KHTTPMethod::GET);
			CHECK ( HTTP.GetStatusCode() == 404 );
			HTTP.Disconnect();

			// still reachable
			sResult = HTTP.HttpRequest2Host(ConnectURL, "localhost/ok", KHTTPMethod::GET);
			CHECK ( HTTP.GetStatusCode() == 200 );
			CHECK ( iCallCount == 2 );
		}
	}

#endif // DEKAF2_HAS_UNIX_SOCKETS

	// ========================================================================
	// HTTP Compression Tests (response compression)
	// ========================================================================

	SECTION("HTTP compression gzip")
	{
		KRESTRoutes Routes;
		Routes.AddRoute({ KHTTPMethod::GET, false, "/data", [](KRESTServer& http)
		{
			http.json.tx["message"] = "the quick brown fox jumps over the lazy dog";
			KJSON jArr = KJSON::array();
			for (int i = 0; i < 200; ++i)
			{
				jArr.push_back(kFormat("item_{:04d}", i));
			}
			http.json.tx["items"] = std::move(jArr);
		}});

		KREST::Options Options;
		Options.Type      = KREST::HTTP;
		Options.iPort     = g_iHTTPPort + 10;
		Options.iTimeout  = 5;
		Options.bBlocking = false;
		Options.bCreateEphemeralCert = false;

		KREST Server;
		REQUIRE ( Server.Execute(Options, Routes) );

		{
			KHTTPError ec;
			KJsonRestClient Client(kFormat("http://localhost:{}", g_iHTTPPort + 10));
			Client.RequestCompression(true, "gzip");
			Client.AllowConnectionRetry(false);

			auto jResult = Client.Get("data").SetError(ec).Request();
			CHECK ( ec.value() == 0 );
			CHECK ( jResult["items"].is_array() );
			CHECK ( jResult["items"].size() == 200 );

			// keepalive: second gzip request
			jResult = Client.Get("data").SetError(ec).Request();
			CHECK ( ec.value() == 0 );
			CHECK ( jResult["items"].size() == 200 );
		}
	}

	SECTION("HTTP compression deflate")
	{
		KRESTRoutes Routes;
		Routes.AddRoute({ KHTTPMethod::GET, false, "/data", [](KRESTServer& http)
		{
			KJSON jArr = KJSON::array();
			for (int i = 0; i < 200; ++i)
			{
				jArr.push_back(kFormat("deflate_{:04d}", i));
			}
			http.json.tx["items"] = std::move(jArr);
		}});

		KREST::Options Options;
		Options.Type      = KREST::HTTP;
		Options.iPort     = g_iHTTPPort + 11;
		Options.iTimeout  = 5;
		Options.bBlocking = false;
		Options.bCreateEphemeralCert = false;

		KREST Server;
		REQUIRE ( Server.Execute(Options, Routes) );

		{
			KHTTPError ec;
			KJsonRestClient Client(kFormat("http://localhost:{}", g_iHTTPPort + 11));
			Client.RequestCompression(true, "deflate");
			Client.AllowConnectionRetry(false);

			auto jResult = Client.Get("data").SetError(ec).Request();
			CHECK ( ec.value() == 0 );
			CHECK ( jResult["items"].size() == 200 );

			// keepalive after deflate response
			jResult = Client.Get("data").SetError(ec).Request();
			CHECK ( ec.value() == 0 );
			CHECK ( jResult["items"].size() == 200 );
		}
	}

#ifdef DEKAF2_HAS_LIBZSTD

	SECTION("HTTP compression zstd")
	{
		KRESTRoutes Routes;
		Routes.AddRoute({ KHTTPMethod::GET, false, "/data", [](KRESTServer& http)
		{
			KJSON jArr = KJSON::array();
			for (int i = 0; i < 200; ++i)
			{
				jArr.push_back(kFormat("zstd_{:04d}", i));
			}
			http.json.tx["items"] = std::move(jArr);
		}});

		KREST::Options Options;
		Options.Type      = KREST::HTTP;
		Options.iPort     = g_iHTTPPort + 12;
		Options.iTimeout  = 5;
		Options.bBlocking = false;
		Options.bCreateEphemeralCert = false;

		KREST Server;
		REQUIRE ( Server.Execute(Options, Routes) );

		{
			KHTTPError ec;
			KJsonRestClient Client(kFormat("http://localhost:{}", g_iHTTPPort + 12));
			Client.RequestCompression(true, "zstd");
			Client.AllowConnectionRetry(false);

			auto jResult = Client.Get("data").SetError(ec).Request();
			CHECK ( ec.value() == 0 );
			CHECK ( jResult["items"].size() == 200 );

			// keepalive after zstd response
			jResult = Client.Get("data").SetError(ec).Request();
			CHECK ( ec.value() == 0 );
			CHECK ( jResult["items"].size() == 200 );
		}
	}

#endif // DEKAF2_HAS_LIBZSTD

	SECTION("HTTP compression gzip over TLS")
	{
		KRESTRoutes Routes;
		Routes.AddRoute({ KHTTPMethod::GET, false, "/data", [](KRESTServer& http)
		{
			KJSON jArr = KJSON::array();
			for (int i = 0; i < 200; ++i)
			{
				jArr.push_back(kFormat("tls_gzip_{:04d}", i));
			}
			http.json.tx["items"] = std::move(jArr);
		}});

		KREST::Options Options;
		Options.Type      = KREST::HTTP;
		Options.iPort     = g_iHTTPSPort + 10;
		Options.iTimeout  = 5;
		Options.bBlocking = false;
		Options.bCreateEphemeralCert = true;
		Options.bStoreEphemeralCert  = false;

		KREST Server;
		REQUIRE ( Server.Execute(Options, Routes) );

		// gzip over TLS
		{
			KHTTPError ec;
			KJsonRestClient Client(kFormat("https://localhost:{}", g_iHTTPSPort + 10));
			Client.RequestCompression(true, "gzip");
			Client.AllowConnectionRetry(false);
			Client.SetStreamOptions(KStreamOptions(false));

			auto jResult = Client.Get("data").SetError(ec).Request();
			CHECK ( ec.value() == 0 );
			CHECK ( jResult["items"].size() == 200 );

			// keepalive with gzip over TLS
			jResult = Client.Get("data").SetError(ec).Request();
			CHECK ( ec.value() == 0 );
			CHECK ( jResult["items"].size() == 200 );
		}

		// deflate over TLS
		{
			KHTTPError ec;
			KJsonRestClient Client(kFormat("https://localhost:{}", g_iHTTPSPort + 10));
			Client.RequestCompression(true, "deflate");
			Client.AllowConnectionRetry(false);
			Client.SetStreamOptions(KStreamOptions(false));

			auto jResult = Client.Get("data").SetError(ec).Request();
			CHECK ( ec.value() == 0 );
			CHECK ( jResult["items"].size() == 200 );
		}
	}

	// ========================================================================
	// JSON echo with compression roundtrip
	// ========================================================================

	SECTION("HTTP JSON roundtrip with gzip compression")
	{
		KRESTRoutes Routes;
		Routes.AddRoute({ KHTTPMethod::POST, false, "/echo", json_echo_handler });

		KREST::Options Options;
		Options.Type      = KREST::HTTP;
		Options.iPort     = g_iHTTPPort + 20;
		Options.iTimeout  = 5;
		Options.bBlocking = false;
		Options.bCreateEphemeralCert = false;

		KREST Server;
		REQUIRE ( Server.Execute(Options, Routes) );

		{
			KHTTPError ec;
			KJsonRestClient Client(kFormat("http://localhost:{}", g_iHTTPPort + 20));
			Client.RequestCompression(true, "gzip");
			Client.AllowConnectionRetry(false);

			KJSON jRequest = {
				{ "name", "test" },
				{ "value", 42 },
				{ "nested", { { "a", 1 }, { "b", 2 } } }
			};

			auto jResult = Client.Post("echo")
			                     .SetError(ec)
			                     .Request(jRequest);

			CHECK ( ec.value() == 0 );
			CHECK ( kjson::GetStringRef(jResult, "name") == "test" );
			CHECK ( kjson::GetUInt(jResult, "value") == 42 );
			CHECK ( kjson::GetBool(jResult, "echo") == true );
		}
	}

	SECTION("HTTP JSON roundtrip with deflate compression")
	{
		KRESTRoutes Routes;
		Routes.AddRoute({ KHTTPMethod::POST, false, "/echo", json_echo_handler });

		KREST::Options Options;
		Options.Type      = KREST::HTTP;
		Options.iPort     = g_iHTTPPort + 21;
		Options.iTimeout  = 5;
		Options.bBlocking = false;
		Options.bCreateEphemeralCert = false;

		KREST Server;
		REQUIRE ( Server.Execute(Options, Routes) );

		{
			KHTTPError ec;
			KJsonRestClient Client(kFormat("http://localhost:{}", g_iHTTPPort + 21));
			Client.RequestCompression(true, "deflate");
			Client.AllowConnectionRetry(false);

			KJSON jRequest = {
				{ "name", "deflate_test" },
				{ "value", 99 }
			};

			auto jResult = Client.Post("echo")
			                     .SetError(ec)
			                     .Request(jRequest);

			CHECK ( ec.value() == 0 );
			CHECK ( kjson::GetStringRef(jResult, "name") == "deflate_test" );
			CHECK ( kjson::GetUInt(jResult, "value") == 99 );
			CHECK ( kjson::GetBool(jResult, "echo") == true );
		}
	}

#ifdef DEKAF2_HAS_LIBZSTD

	SECTION("HTTP JSON roundtrip with zstd compression")
	{
		KRESTRoutes Routes;
		Routes.AddRoute({ KHTTPMethod::POST, false, "/echo", json_echo_handler });

		KREST::Options Options;
		Options.Type      = KREST::HTTP;
		Options.iPort     = g_iHTTPPort + 22;
		Options.iTimeout  = 5;
		Options.bBlocking = false;
		Options.bCreateEphemeralCert = false;

		KREST Server;
		REQUIRE ( Server.Execute(Options, Routes) );

		{
			KHTTPError ec;
			KJsonRestClient Client(kFormat("http://localhost:{}", g_iHTTPPort + 22));
			Client.RequestCompression(true, "zstd");
			Client.AllowConnectionRetry(false);

			KJSON jRequest = {
				{ "name", "zstd_test" },
				{ "value", 77 }
			};

			auto jResult = Client.Post("echo")
			                     .SetError(ec)
			                     .Request(jRequest);

			CHECK ( ec.value() == 0 );
			CHECK ( kjson::GetStringRef(jResult, "name") == "zstd_test" );
			CHECK ( kjson::GetUInt(jResult, "value") == 77 );
			CHECK ( kjson::GetBool(jResult, "echo") == true );
		}
	}

#endif // DEKAF2_HAS_LIBZSTD

	// ========================================================================
	// HTTP Range Request Tests
	// ========================================================================

	SECTION("HTTP range request via web server")
	{
		constexpr KStringView sContent = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
		KTempDir WebRoot;
		{
			KOutFile OutFile(kFormat("{}/range.txt", WebRoot.Name()));
			REQUIRE ( OutFile.is_open() );
			OutFile.Write(sContent);
		}

		KRESTRoutes Routes;
		Routes.AddRoute({ KHTTPMethod::GET, false, "/files/*", WebRoot.Name(), Routes, &KRESTRoutes::WebServer });

		KREST::Options Options;
		Options.Type      = KREST::HTTP;
		Options.iPort     = g_iHTTPPort + 30;
		Options.iTimeout  = 5;
		Options.bBlocking = false;
		Options.bCreateEphemeralCert = false;

		KREST Server;
		REQUIRE ( Server.Execute(Options, Routes) );

		// full file GET
		{
			KWebClient HTTP;
			HTTP.AllowCompression(false);
			auto sResult = HTTP.Get(kFormat("http://localhost:{}/files/range.txt", g_iHTTPPort + 30));
			CHECK ( HTTP.GetStatusCode() == 200 );
			CHECK ( sResult == sContent );
		}

		// range request: first 10 bytes
		{
			KWebClient HTTP;
			HTTP.AllowCompression(false);
			HTTP.AddHeader(KHTTPHeader::RANGE, "bytes=0-9");
			auto sResult = HTTP.Get(kFormat("http://localhost:{}/files/range.txt", g_iHTTPPort + 30));
			CHECK ( HTTP.GetStatusCode() == 206 );
			CHECK ( sResult == "0123456789" );
		}

		// range request: from offset to end
		{
			KWebClient HTTP;
			HTTP.AllowCompression(false);
			HTTP.AddHeader(KHTTPHeader::RANGE, "bytes=26-");
			auto sResult = HTTP.Get(kFormat("http://localhost:{}/files/range.txt", g_iHTTPPort + 30));
			CHECK ( HTTP.GetStatusCode() == 206 );
			CHECK ( sResult == "QRSTUVWXYZ" );
		}
	}

	// ========================================================================
	// Combined: compression + keepalive + errors
	// ========================================================================

	SECTION("HTTP compression with keepalive after 4xx")
	{
		KRESTRoutes Routes;
		uint16_t iCallCount { 0 };

		Routes.AddRoute({ KHTTPMethod::GET, false, "/ok", [&](KRESTServer& http)
		{
			++iCallCount;
			KJSON jArr = KJSON::array();
			for (int i = 0; i < 50; ++i)
			{
				jArr.push_back(kFormat("data_{}", i));
			}
			http.json.tx["items"] = std::move(jArr);
		}});

		Routes.AddRoute({ KHTTPMethod::GET, false, "/missing", not_found_handler });

		KREST::Options Options;
		Options.Type      = KREST::HTTP;
		Options.iPort     = g_iHTTPPort + 40;
		Options.iTimeout  = 5;
		Options.bBlocking = false;
		Options.bCreateEphemeralCert = false;

		KREST Server;
		REQUIRE ( Server.Execute(Options, Routes) );

		{
			KHTTPError ec;
			KJsonRestClient Client(kFormat("http://localhost:{}", g_iHTTPPort + 40));
			Client.RequestCompression(true, "gzip");
			Client.AllowConnectionRetry(false);

			// compressed response
			auto jResult = Client.Get("ok").SetError(ec).Request();
			CHECK ( ec.value() == 0 );
			CHECK ( iCallCount == 1 );

			// 404 error - keepalive should survive
			jResult = Client.Get("missing").SetError(ec).Request();
			CHECK ( ec.value() == 404 );

			// still alive with compression
			jResult = Client.Get("ok").SetError(ec).Request();
			CHECK ( ec.value() == 0 );
			CHECK ( iCallCount == 2 );
		}
	}

}

#endif // of DEKAF2_IS_WINDOWS
