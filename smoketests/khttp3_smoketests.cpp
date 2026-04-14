#include "catch.hpp"

#include <dekaf2/core/init/kdefinitions.h>

#if DEKAF2_HAS_NGHTTP3 && DEKAF2_HAS_OPENSSL_QUIC

#include <dekaf2/http/client/kwebclient.h>
#include <dekaf2/net/quic/kquicstream.h>
#include <dekaf2/core/strings/kstring.h>

#ifndef DEKAF2_IS_WINDOWS

using namespace dekaf2;

TEST_CASE("HTTP3")
{
	SECTION("QUIC connect to cloudflare")
	{
		KQuicStream Stream;
		bool bConnected = Stream.Connect(
			KTCPEndPoint("cloudflare.com:443"),
			KStreamOptions(KStreamOptions::RequestHTTP3)
		);

		if (!bConnected)
		{
			WARN("QUIC connect to cloudflare.com failed: " << Stream.Error());
			return;
		}

		CHECK ( Stream.is_open() == true );

		auto sALPN = Stream.GetALPN();
		CHECK ( sALPN == "h3" );

		Stream.Disconnect();
	}

	SECTION("HTTP/3 GET to cloudflare")
	{
		KWebClient HTTP;
		HTTP.SetStreamOptions(KStreamOptions(KStreamOptions::RequestHTTP3));
		HTTP.AllowCompression(false);
		HTTP.SetTimeout(10);

		auto sResult = HTTP.Get("https://cloudflare.com/");

		if (HTTP.GetStatusCode() == 0)
		{
			WARN("cannot reach cloudflare.com via HTTP/3: " << HTTP.Error());
			return;
		}

		auto sVersion = HTTP.Response.GetHTTPVersion();

		if (sVersion == KHTTPVersion::http3)
		{
			CHECK ( sVersion == KHTTPVersion::http3 );
		}
		else
		{
			WARN("HTTP/3 not negotiated, got: " << sVersion.Serialize());
		}

		CHECK ( HTTP.GetStatusCode() >= 200 );
		CHECK ( HTTP.GetStatusCode() <  400 );
	}

	SECTION("HTTP/3 GET with body")
	{
		KWebClient HTTP;
		HTTP.SetStreamOptions(KStreamOptions(KStreamOptions::RequestHTTP3));
		HTTP.AllowCompression(false);
		HTTP.SetTimeout(10);

		auto sResult = HTTP.Get("https://cloudflare-quic.com/");

		if (HTTP.GetStatusCode() == 0)
		{
			WARN("cannot reach cloudflare-quic.com via HTTP/3: " << HTTP.Error());
			return;
		}

		CHECK ( HTTP.GetStatusCode() >= 200 );
		CHECK ( HTTP.GetStatusCode() <  400 );
		CHECK ( sResult.size() > 0 );
	}

	SECTION("HTTP/3 multiple requests")
	{
		KWebClient HTTP;
		HTTP.SetStreamOptions(KStreamOptions(KStreamOptions::RequestHTTP3));
		HTTP.AllowCompression(false);
		HTTP.SetTimeout(10);

		auto sResult1 = HTTP.Get("https://cloudflare.com/");

		if (HTTP.GetStatusCode() == 0)
		{
			WARN("cannot reach cloudflare.com via HTTP/3: " << HTTP.Error());
			return;
		}

		CHECK ( HTTP.GetStatusCode() >= 200 );
		CHECK ( HTTP.GetStatusCode() <  400 );

		auto sResult2 = HTTP.Get("https://cloudflare.com/cdn-cgi/trace");

		CHECK ( HTTP.GetStatusCode() >= 200 );
		CHECK ( HTTP.GetStatusCode() <  400 );
		CHECK ( sResult2.size() > 0 );
	}
}

#endif // DEKAF2_IS_WINDOWS
#endif // DEKAF2_HAS_NGHTTP3 && DEKAF2_HAS_OPENSSL_QUIC
