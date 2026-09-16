#include "catch.hpp"

#include <dekaf2/http/protocol/khttp_response.h>
#include <dekaf2/io/streams/kstringstream.h>
#include <dekaf2/core/strings/kstring.h>

using namespace dekaf2;

TEST_CASE("KHTTPResponse")
{
	SECTION("interim 1xx responses are skipped")
	{
		// a server may send any number of 1xx interim responses ahead of the
		// final response (RFC 9110 15.2) - the final response is the result,
		// the interim headers are dropped, and the body stays in the stream
		KString sRaw =
			"HTTP/1.1 103 Early Hints\r\n"
			"Link: </style.css>; rel=preload; as=style\r\n"
			"\r\n"
			"HTTP/1.1 103 Early Hints\r\n"
			"Link: </app.js>; rel=preload; as=script\r\n"
			"\r\n"
			"HTTP/1.1 200 OK\r\n"
			"Content-Type: text/plain\r\n"
			"Content-Length: 5\r\n"
			"\r\n"
			"hello";

		KInStringStream iss(sRaw);
		KHTTPResponseHeaders Response;

		CHECK ( Response.Parse(iss) );
		CHECK ( Response.GetStatusCode() == 200 );
		CHECK ( Response.Headers.Get(KHTTPHeader::CONTENT_TYPE) == "text/plain" );
		CHECK ( Response.Headers.Get(KHTTPHeader::CONTENT_LENGTH) == "5" );
		CHECK ( Response.Headers.Get(KHTTPHeader::LINK).empty() );

		KString sBody;
		CHECK ( iss.ReadLine(sBody) );
		CHECK ( sBody == "hello" );
	}

	SECTION("101 is a final response")
	{
		// the response to an upgrade request is not interim
		KString sRaw =
			"HTTP/1.1 101 Switching Protocols\r\n"
			"Upgrade: websocket\r\n"
			"Connection: Upgrade\r\n"
			"\r\n";

		KInStringStream iss(sRaw);
		KHTTPResponseHeaders Response;

		CHECK ( Response.Parse(iss) );
		CHECK ( Response.GetStatusCode() == 101 );
		CHECK ( Response.Headers.Get(KHTTPHeader::UPGRADE) == "websocket" );
	}

	SECTION("endless interim responses fail")
	{
		KString sRaw;

		for (int i = 0; i < 20; ++i)
		{
			sRaw += "HTTP/1.1 103 Early Hints\r\n\r\n";
		}

		sRaw += "HTTP/1.1 200 OK\r\n\r\n";

		KInStringStream iss(sRaw);
		KHTTPResponseHeaders Response;

		CHECK_FALSE ( Response.Parse(iss) );
	}
}
