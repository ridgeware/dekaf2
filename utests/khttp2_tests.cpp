#include "catch.hpp"

#include <dekaf2/core/init/kdefinitions.h>

#if DEKAF2_HAS_NGHTTP2

#include <dekaf2/http/protocol/khttp2.h>
#include <dekaf2/http/protocol/khttp_version.h>
#include <dekaf2/net/util/kstreamoptions.h>


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

TEST_CASE("HTTP2 response header limits")
{
	KURL URL("https://localhost/");

	SECTION("the pseudo header fields count toward the field count")
	{
		KHTTPResponseHeaders Response;
		khttp2::Stream Stream(URL, KHTTPMethod::GET, Response, { 0, 3, 0 });
		Stream.StartResponseHeaders();
		CHECK ( Stream.AddResponseHeader(1, ":status", "200") );
		CHECK ( Stream.AddResponseHeader(1, "a", "1") );
		CHECK ( Stream.AddResponseHeader(1, "b", "2") );
		CHECK ( Response.GetStatusCode() == 200 );
		CHECK ( Stream.IsRejected() == false );
		CHECK ( Stream.AddResponseHeader(1, "c", "3") == false );
		CHECK ( Stream.IsRejected() );
		CHECK ( Response.GetParseError() == KHTTPHeaders::TooManyFields );
	}

	SECTION("a field above the length limit")
	{
		KHTTPResponseHeaders Response;
		khttp2::Stream Stream(URL, KHTTPMethod::GET, Response, { 10, 0, 0 });
		Stream.StartResponseHeaders();
		CHECK ( Stream.AddResponseHeader(1, ":status", "200") );
		// name plus value: 4 + 6 = 10 bytes
		CHECK ( Stream.AddResponseHeader(1, "name", "123456") );
		CHECK ( Stream.AddResponseHeader(1, "name", "1234567") == false );
		CHECK ( Response.GetParseError() == KHTTPHeaders::FieldTooLong );
	}

	SECTION("the section size")
	{
		KHTTPResponseHeaders Response;
		// ":status" counts 7 + 3 + 32 = 42 bytes, "a" 1 + 1 + 32 = 34
		khttp2::Stream Stream(URL, KHTTPMethod::GET, Response, { 0, 0, 76 });
		Stream.StartResponseHeaders();
		CHECK ( Stream.AddResponseHeader(1, ":status", "200") );
		CHECK ( Stream.AddResponseHeader(1, "a", "1") );
		CHECK ( Stream.AddResponseHeader(1, "b", "") == false );
		CHECK ( Response.GetParseError() == KHTTPHeaders::SectionTooLarge );
	}

	SECTION("each header block is counted on its own")
	{
		KHTTPResponseHeaders Response;
		khttp2::Stream Stream(URL, KHTTPMethod::GET, Response, { 0, 2, 0 });

		Stream.StartResponseHeaders();
		CHECK ( Stream.AddResponseHeader(1, ":status", "103") );
		CHECK ( Stream.AddResponseHeader(1, "link", "</style.css>; rel=preload") );
		CHECK ( Stream.EndResponseHeaders(1) == false );

		Stream.StartResponseHeaders();
		CHECK ( Stream.AddResponseHeader(1, ":status", "200") );
		CHECK ( Stream.AddResponseHeader(1, "a", "1") );
		CHECK ( Stream.EndResponseHeaders(1) == true );
		CHECK ( Response.GetStatusCode() == 200 );
		CHECK ( Response.Headers.Get("link").empty() );
		CHECK ( Response.Headers.Get("a") == "1" );
	}

	SECTION("16 interim responses are accepted, 17 are not")
	{
		KHTTPResponseHeaders Response;
		khttp2::Stream Stream(URL, KHTTPMethod::GET, Response);

		for (uint16_t i = 0; i < detail::iMaxInterimResponses; ++i)
		{
			Stream.StartResponseHeaders();
			CHECK ( Stream.AddResponseHeader(1, ":status", "103") );
			CHECK ( Stream.EndResponseHeaders(1) == false );
		}

		Stream.StartResponseHeaders();
		CHECK ( Stream.AddResponseHeader(1, ":status", "103") == false );
		CHECK ( Response.GetParseError() == KHTTPHeaders::TooManyInterimResponses );
	}

	SECTION("trailers are not counted")
	{
		KHTTPResponseHeaders Response;
		khttp2::Stream Stream(URL, KHTTPMethod::GET, Response, { 0, 2, 0 });
		Stream.StartResponseHeaders();
		CHECK ( Stream.AddResponseHeader(1, ":status", "200") );
		CHECK ( Stream.AddResponseHeader(1, "a", "1") );
		CHECK ( Stream.EndResponseHeaders(1) == true );
		Stream.StartResponseHeaders();
		CHECK ( Stream.AddResponseHeader(1, "x", "1") );
		CHECK ( Stream.AddResponseHeader(1, "y", "2") );
		CHECK ( Stream.AddResponseHeader(1, "z", "3") );
		CHECK ( Stream.IsRejected() == false );
	}
}

#endif // DEKAF2_HAS_NGHTTP2
