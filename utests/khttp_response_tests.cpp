#include "catch.hpp"

#include <dekaf2/http/protocol/khttp_response.h>
#include <dekaf2/io/streams/kstringstream.h>
#include <dekaf2/core/strings/kstring.h>
#include <vector>

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
		CHECK ( Response.GetParseError() == KHTTPHeaders::TooManyInterimResponses );
	}
}

TEST_CASE("KHTTPResponse limits")
{
	SECTION("default limits")
	{
		KInHTTPResponse Response;
		CHECK ( Response.GetHeaderLimits().iMaxFieldLength == KHTTPHeaders::DefaultResponseLimits.iMaxFieldLength );
		CHECK ( Response.GetHeaderLimits().iMaxFieldCount  == KHTTPHeaders::DefaultResponseLimits.iMaxFieldCount  );
		CHECK ( Response.GetHeaderLimits().iMaxSectionSize == KHTTPHeaders::DefaultResponseLimits.iMaxSectionSize );
	}

	SECTION("a field line of 64 KiB is accepted, one byte more is not")
	{
		// "X: " plus the value - KInStringStream reads from a view, so the input
		// must outlive the stream
		KString sRaw = "HTTP/1.1 200 OK\r\nX: " + KString(64 * 1024 - 3, 'a') + "\r\nY: 1\r\n\r\nbody";

		{
			KInStringStream iss(sRaw);
			KHTTPResponseHeaders Response;
			CHECK ( Response.Parse(iss) == true );
			CHECK ( Response.Headers.Get("X").size() == 64 * 1024 - 3 );
			// the line end of the long line is consumed, the next line and the body follow
			CHECK ( Response.Headers.Get("Y") == "1" );
			KString sBody;
			CHECK ( iss.ReadLine(sBody) );
			CHECK ( sBody == "body" );
		}

		sRaw = "HTTP/1.1 200 OK\r\nX: " + KString(64 * 1024 - 2, 'a') + "\r\n\r\n";

		{
			KInStringStream iss(sRaw);
			KHTTPResponseHeaders Response;
			CHECK ( Response.Parse(iss) == false );
			CHECK ( Response.GetParseError() == KHTTPHeaders::FieldTooLong );
		}
	}

	SECTION("the limits set on KInHTTPResponse apply")
	{
		KString sRaw = "HTTP/1.1 200 OK\r\nA: 1\r\nB: 2\r\nC: 3\r\n\r\n";

		{
			KInStringStream iss(sRaw);
			KInHTTPResponse Response(iss);
			Response.SetHeaderLimits({ 0, 2, 0 });
			CHECK ( Response.Parse() == false );
			CHECK ( Response.GetParseError() == KHTTPHeaders::TooManyFields );
		}
		{
			KInStringStream iss(sRaw);
			KInHTTPResponse Response(iss);
			Response.SetHeaderLimits({ 0, 3, 0 });
			CHECK ( Response.Parse() == true );
			CHECK ( Response.GetParseError() == KHTTPHeaders::NoError );
		}
	}

	SECTION("each header block is counted on its own")
	{
		// two fields in the interim and two in the final response - with a
		// common count, the final response would exceed the limit of 2
		KString sRaw =
			"HTTP/1.1 103 Early Hints\r\n"
			"Link: </style.css>; rel=preload; as=style\r\n"
			"Link: </app.js>; rel=preload; as=script\r\n"
			"\r\n"
			"HTTP/1.1 200 OK\r\n"
			"Content-Type: text/plain\r\n"
			"Content-Length: 0\r\n"
			"\r\n";

		KInStringStream iss(sRaw);
		KHTTPResponseHeaders Response;
		CHECK ( Response.Parse(iss, { 0, 2, 0 }) == true );
		CHECK ( Response.GetStatusCode() == 200 );
	}

	SECTION("the status line is limited by the field length")
	{
		// the status line has 25 bytes
		KString sRaw = "HTTP/1.1 200 OK 123456789\r\n\r\n";

		{
			KInStringStream iss(sRaw);
			KHTTPResponseHeaders Response;
			CHECK ( Response.Parse(iss, { 25, 0, 0 }) == true );
		}
		{
			KInStringStream iss(sRaw);
			KHTTPResponseHeaders Response;
			CHECK ( Response.Parse(iss, { 24, 0, 0 }) == false );
			CHECK ( Response.GetParseError() == KHTTPHeaders::StartLineTooLong );
			CHECK ( Response.Error() == "HTTP status line longer than 24 bytes" );
		}
	}

	SECTION("incomplete and malformed responses")
	{
		struct Case { KStringView sRaw; KHTTPHeaders::ParseError Error; };

		std::vector<Case> Cases
		{
			{ "",                                       KHTTPHeaders::Incomplete },
			{ "HTTP/1.1 200 OK\r\nA: 1\r\n",            KHTTPHeaders::Incomplete },
			{ "HTTP/1.1 103 Early Hints\r\n\r\n",       KHTTPHeaders::Incomplete },
			{ "\r\n\r\n",                               KHTTPHeaders::Malformed  },
			{ "garbage\r\n\r\n",                        KHTTPHeaders::Malformed  },
			{ "FOO/1.1 200 OK\r\n\r\n",                 KHTTPHeaders::Malformed  },
		};

		for (const auto& Case : Cases)
		{
			INFO ( Case.sRaw );
			KInStringStream iss(Case.sRaw);
			KHTTPResponseHeaders Response;
			CHECK ( Response.Parse(iss) == false );
			CHECK ( Response.GetParseError() == Case.Error );
		}
	}

	SECTION("a new parse resets the reason")
	{
		KString sBad  = "garbage\r\n\r\n";
		KString sGood = "HTTP/1.1 200 OK\r\n\r\n";
		KHTTPResponseHeaders Response;

		KInStringStream iss1(sBad);
		CHECK ( Response.Parse(iss1) == false );
		CHECK ( Response.GetParseError() == KHTTPHeaders::Malformed );

		KInStringStream iss2(sGood);
		CHECK ( Response.Parse(iss2) == true );
		CHECK ( Response.GetParseError() == KHTTPHeaders::NoError );
	}
}
