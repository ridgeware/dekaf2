#include "catch.hpp"

#include <dekaf2/core/init/kdefinitions.h>

#if DEKAF2_HAS_NGHTTP2

#include <dekaf2/http/client/kwebclient.h>
#include <dekaf2/core/strings/kstring.h>

#ifndef DEKAF2_IS_WINDOWS

using namespace dekaf2;

TEST_CASE("HTTP2")
{
	SECTION("HTTP/2 GET to nghttp2.org")
	{
		KWebClient HTTP;
		HTTP.SetStreamOptions(KStreamOptions(KStreamOptions::RequestHTTP2 | KStreamOptions::FallBackToHTTP1));
		HTTP.AllowCompression(false);
		HTTP.SetTimeout(10);

		auto sResult = HTTP.Get("https://nghttp2.org/");

		if (HTTP.GetStatusCode() == 0)
		{
			WARN("cannot reach nghttp2.org: " << HTTP.Error());
			return;
		}

		CHECK ( HTTP.GetStatusCode() == 200 );

		auto sVersion = HTTP.Response.GetHTTPVersion();

		if (sVersion == KHTTPVersion::http2)
		{
			CHECK ( sVersion == KHTTPVersion::http2 );
		}
		else
		{
			WARN("HTTP/2 not negotiated, got: " << sVersion.Serialize());
		}
	}

	SECTION("HTTP/2 GET with response body")
	{
		KWebClient HTTP;
		HTTP.SetStreamOptions(KStreamOptions(KStreamOptions::RequestHTTP2 | KStreamOptions::FallBackToHTTP1));
		HTTP.AllowCompression(false);
		HTTP.SetTimeout(10);

		auto sResult = HTTP.Get("https://www.google.com/");

		if (HTTP.GetStatusCode() == 0)
		{
			WARN("cannot reach www.google.com: " << HTTP.Error());
			return;
		}

		CHECK ( HTTP.GetStatusCode() == 200 );
		CHECK ( sResult.size() > 0 );

		auto sVersion = HTTP.Response.GetHTTPVersion();
		if (sVersion != KHTTPVersion::http2)
		{
			WARN("expected HTTP/2, got: " << sVersion.Serialize());
		}
	}

	SECTION("HTTP/2 multiple requests on same connection")
	{
		KWebClient HTTP;
		HTTP.SetStreamOptions(KStreamOptions(KStreamOptions::RequestHTTP2 | KStreamOptions::FallBackToHTTP1));
		HTTP.AllowCompression(false);
		HTTP.SetTimeout(10);

		auto sResult1 = HTTP.Get("https://nghttp2.org/");

		if (HTTP.GetStatusCode() == 0)
		{
			WARN("cannot reach nghttp2.org: " << HTTP.Error());
			return;
		}

		CHECK ( HTTP.GetStatusCode() == 200 );

		// second request on same connection
		auto sResult2 = HTTP.Get("https://nghttp2.org/");

		CHECK ( HTTP.GetStatusCode() == 200 );
		CHECK ( sResult2.size() > 0 );
	}

	SECTION("HTTP/2 strict mode (no fallback)")
	{
		KWebClient HTTP;
		HTTP.SetStreamOptions(KStreamOptions(KStreamOptions::RequestHTTP2));
		HTTP.AllowCompression(false);
		HTTP.SetTimeout(10);

		auto sResult = HTTP.Get("https://nghttp2.org/");

		if (HTTP.GetStatusCode() == 0)
		{
			WARN("HTTP/2 strict mode failed: " << HTTP.Error());
			return;
		}

		CHECK ( HTTP.Response.GetHTTPVersion() == KHTTPVersion::http2 );
		CHECK ( HTTP.GetStatusCode() == 200 );
	}
}

#endif // DEKAF2_IS_WINDOWS
#endif // DEKAF2_HAS_NGHTTP2
