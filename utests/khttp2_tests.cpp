#include "catch.hpp"

#include <dekaf2/core/init/kdefinitions.h>

#if DEKAF2_HAS_NGHTTP2

#include <dekaf2/http/protocol/khttp2.h>
#include <dekaf2/http/protocol/khttp_version.h>
#include <dekaf2/net/util/kstreamoptions.h>

#ifndef DEKAF2_IS_WINDOWS

using namespace dekaf2;

// ============================================================================
// HTTP/2 offline unit tests
// ============================================================================

TEST_CASE("HTTP2")
{
	SECTION("KHTTPVersion http2 value")
	{
		CHECK ( KHTTPVersion(KHTTPVersion::http2).Serialize() == "HTTP/2" );
	}

	SECTION("KStreamOptions RequestHTTP2 flag")
	{
		KStreamOptions opts(KStreamOptions::RequestHTTP2 | KStreamOptions::FallBackToHTTP1);
		CHECK ( opts.IsSet(KStreamOptions::RequestHTTP2)    == true  );
		CHECK ( opts.IsSet(KStreamOptions::FallBackToHTTP1) == true  );
		CHECK ( opts.IsSet(KStreamOptions::RequestHTTP3)    == false );
	}

	SECTION("KStreamOptions RequestHTTP2 without fallback")
	{
		KStreamOptions opts(KStreamOptions::RequestHTTP2);
		CHECK ( opts.IsSet(KStreamOptions::RequestHTTP2)    == true  );
		CHECK ( opts.IsSet(KStreamOptions::FallBackToHTTP1) == false );
	}

	SECTION("KHTTPVersion http2 round trip")
	{
		KHTTPVersion v("HTTP/2");
		CHECK ( v == KHTTPVersion::http2 );
		CHECK ( v.Serialize() == "HTTP/2" );
	}

	SECTION("KHTTPVersion http2 bitmask")
	{
		KHTTPVersion v(KHTTPVersion::http11 | KHTTPVersion::http2);
		CHECK ( (v & KHTTPVersion::http2)  );
		CHECK ( (v & KHTTPVersion::http11) );
		CHECK ( !(v & KHTTPVersion::http3) );
	}
}

#endif // DEKAF2_IS_WINDOWS
#endif // DEKAF2_HAS_NGHTTP2
