#include "catch.hpp"

#include <dekaf2/core/init/kdefinitions.h>

#if DEKAF2_HAS_NGHTTP3 && DEKAF2_HAS_OPENSSL_QUIC

#include <dekaf2/http/protocol/khttp3.h>
#include <dekaf2/http/protocol/khttp_version.h>
#include <dekaf2/net/quic/kquicstream.h>
#include <dekaf2/net/util/kstreamoptions.h>

#ifndef DEKAF2_IS_WINDOWS

using namespace dekaf2;

// ============================================================================
// HTTP/3 offline unit tests
// ============================================================================

TEST_CASE("HTTP3")
{
	SECTION("KHTTPVersion http3 value")
	{
		CHECK ( KHTTPVersion(KHTTPVersion::http3).Serialize() == "HTTP/3" );
	}

	SECTION("KStreamOptions RequestHTTP3 flag")
	{
		KStreamOptions opts(KStreamOptions::RequestHTTP3);
		CHECK ( opts.IsSet(KStreamOptions::RequestHTTP3)    == true  );
		CHECK ( opts.IsSet(KStreamOptions::RequestHTTP2)    == false );
		CHECK ( opts.IsSet(KStreamOptions::FallBackToHTTP1) == false );
	}

	SECTION("KHTTPVersion http3 round trip")
	{
		KHTTPVersion v("HTTP/3");
		CHECK ( v == KHTTPVersion::http3 );
		CHECK ( v.Serialize() == "HTTP/3" );
	}

	SECTION("KQuicStream construct default")
	{
		KQuicStream Stream;
		CHECK ( Stream.is_open() == false );
	}
}

#endif // DEKAF2_IS_WINDOWS
#endif // DEKAF2_HAS_NGHTTP3 && DEKAF2_HAS_OPENSSL_QUIC
