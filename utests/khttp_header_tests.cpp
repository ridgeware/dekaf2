#include "catch.hpp"
#include <dekaf2/http/protocol/khttp_header.h>
#include <dekaf2/http/client/khttpclient.h>
#include <dekaf2/io/streams/kstringstream.h>
#include <dekaf2/core/strings/kjoin.h>
#include <algorithm>
#include <cerrno>
#include <vector>

using namespace dekaf2;

namespace {

void testHeader(const KHTTPHeader& header)
{
}

} // end of anonymous namespace

TEST_CASE("KHTTPHeader")
{
	SECTION("Construction and Compare")
	{
		KHTTPHeader Header1(KHTTPHeader::AUTHORIZATION);
		KHTTPHeader Header2("auTHoriZation");
		KHTTPHeader Header3(KHTTPHeader::CONTENT_TYPE);
		KHTTPHeader Header4(Header3);
		KHTTPHeader Header5(std::move(Header4));
		KHTTPHeader Header6;
		Header6 = Header5;
		KHTTPHeader Header7;
		Header7 = std::move(Header6);

		KHTTPHeader Header;
		Header = "abc";
		Header = "abc"_ks;
		Header = "abc"_ksv;
		Header = "abc"_ksz;
		Header = std::string("abc");

		testHeader(KHTTPHeader::CONTENT_TYPE);
		testHeader("abc");
		testHeader("abc"_ks);
		testHeader("abc"_ksv);
		testHeader("abc"_ksz);
		testHeader(std::string("abc"));

		CHECK ( Header1 == Header2 );
		CHECK ( Header1 != Header3 );
		CHECK ( Header1.Hash() == Header2.Hash() );
		CHECK ( Header1.Hash() != Header3.Hash() );
		if (Header1 != "aUthoriZation") { CHECK ( false ); }
		if (Header1 != "aUthoriZation"_ksv) { CHECK ( false ); }
		CHECK ( (Header1 == "authorization")     );
		CHECK ( (Header1 == "authorization"_ks)  );
		CHECK ( (Header1 == "authorization"_ksv) );
		CHECK ( (Header2 == "authorization"_ksz) );
		CHECK ( (Header1 == "aUthoRization")     );
		CHECK ( (Header1 == "aUthoRization"_ks)  );
		CHECK ( (Header1 == "aUthoRization"_ksv) );
		CHECK ( (Header2 == "aUthoRization"_ksz) );
		CHECK ( (Header1 == "something") == false );
		CHECK ( ("authorization"     == Header1) );
		CHECK ( ("authorization"_ks  == Header1) );
		CHECK ( ("authorization"_ksv == Header1) );
		CHECK ( ("authorization"_ksz == Header2) );
		CHECK ( ("aUthoRization"     == Header1) );
		CHECK ( ("aUthoRization"_ks  == Header1) );
		CHECK ( ("aUthoRization"_ksv == Header1) );
		CHECK ( ("aUthoRization"_ksz == Header2) );
		CHECK ( ("something" == Header1) == false );
		CHECK ( Header1 == KHTTPHeader::AUTHORIZATION );
		CHECK ( Header2 == KHTTPHeader::AUTHORIZATION );
		CHECK ( Header1 != KHTTPHeader::CONTENT_TYPE );
		CHECK ( Header2 != KHTTPHeader::CONTENT_TYPE );
		CHECK ( KHTTPHeader::CONTENT_TYPE != Header2 );
		CHECK ( Header1.Serialize() == "authorization" );
		CHECK ( Header2.Serialize() == "authorization" );

		Header1 = KHTTPHeader("non-standard-Header");
		CHECK ( Header1 == KHTTPHeader("non-stAnDard-header") );
		CHECK ( Header1.Serialize() == "non-standard-header"_ks);
		CHECK ( Header1 == "non-standard-header" );
		CHECK ( Header1 != KHTTPHeader::X_FORWARDED_FOR );

		KHTTPClient Client;
		Client.AddHeader("key", "value");
		Client.AddHeader("key"_ks, "value"_ks);
		Client.AddHeader("key"_ksv, "value"_ksv);
		Client.AddHeader("key"_ksz, "value"_ksz);
	}

	SECTION("QualityValue")
	{
		KStringView sContent = "text/css,  text/html  ;q=0.9  ,text/*;q=0.81s,x-none/*;q=0.8 , zzz/xx ;q=8, zzz/yy ;q=0*12";
		auto ContentTypes    = sContent.Split();

		CHECK ( ContentTypes.size() == 6);

		if (ContentTypes.size() == 6)
		{
			auto iQuality = KHTTPHeader::GetQualityValue(ContentTypes[0], true);
			CHECK ( iQuality == 100 );
			CHECK ( ContentTypes[0] == "text/css" );
			iQuality = KHTTPHeader::GetQualityValue(ContentTypes[1], true);
			CHECK ( iQuality == 90 );
			CHECK ( ContentTypes[1] == "text/html" );
			iQuality = KHTTPHeader::GetQualityValue(ContentTypes[2], true);
			CHECK ( iQuality == 81 );
			CHECK ( ContentTypes[2] == "text/*" );
			iQuality = KHTTPHeader::GetQualityValue(ContentTypes[3], true);
			CHECK ( iQuality == 80 );
			CHECK ( ContentTypes[3] == "x-none/*" );
			iQuality = KHTTPHeader::GetQualityValue(ContentTypes[4], true);
			CHECK ( iQuality == 100 );
			CHECK ( ContentTypes[4] == "zzz/xx" );
			iQuality = KHTTPHeader::GetQualityValue(ContentTypes[5], true);
			CHECK ( iQuality == 100 );
			CHECK ( ContentTypes[5] == "zzz/yy" );
		}
	}

	SECTION("SortedQualityValue")
	{
		KStringView sContent = "text/css,  text/html  ;q=0.5  ,text/*;q=0.81s,x-none/*;q=0.84 , zzz/xx ;q=8, zzz/yy ;q=0*12";
		auto ContentTypes    = KHTTPHeader::Split(sContent);

		CHECK ( ContentTypes.size() == 6);

		if (ContentTypes.size() == 6)
		{
			CHECK ( ContentTypes[0].iQuality == 100 );
			CHECK ( ContentTypes[0].sValue == "text/css" );
			CHECK ( ContentTypes[1].iQuality == 100 );
			CHECK ( ContentTypes[1].sValue == "zzz/xx" );
			CHECK ( ContentTypes[2].iQuality == 100 );
			CHECK ( ContentTypes[2].sValue == "zzz/yy" );
			CHECK ( ContentTypes[3].iQuality == 84 );
			CHECK ( ContentTypes[3].sValue == "x-none/*" );
			CHECK ( ContentTypes[4].iQuality == 81 );
			CHECK ( ContentTypes[4].sValue == "text/*" );
			CHECK ( ContentTypes[5].iQuality == 50 );
			CHECK ( ContentTypes[5].sValue == "text/html" );
		}
	}

	SECTION("Serialize drops headers with CR or LF")
	{
		KHTTPHeaders Headers;
		Headers.Headers.Add(KHTTPHeader::CONTENT_TYPE, "text/plain");
		Headers.Headers.Add(KHTTPHeader::LOCATION, "/elsewhere\r\nSet-Cookie: injected=1");
		Headers.Headers.Add(KHTTPHeader::CONNECTION, "close");

		KString sOut;
		KOutStringStream oss(sOut);
		CHECK ( Headers.Serialize(oss) );
		CHECK ( sOut.starts_with("content-type: text/plain") );
		CHECK ( sOut.contains("connection: close") );
		CHECK_FALSE ( sOut.contains("location") );
		CHECK_FALSE ( sOut.contains("injected") );
	}

	SECTION("Serialize fails on headers with CR or LF when asked to")
	{
		KHTTPHeaders Headers;
		Headers.Headers.Add(KHTTPHeader::CONTENT_TYPE, "text/plain");
		Headers.Headers.Add(KHTTPHeader::AUTHORIZATION, "Bearer token\n");

		KString sOut;
		KOutStringStream oss(sOut);
		CHECK_FALSE ( Headers.Serialize(oss, KHTTPHeaders::ReturnError) );
		CHECK ( Headers.GetLastErrorCode() == EINVAL );
		CHECK ( Headers.Error().contains("authorization") );
		CHECK_FALSE ( sOut.contains("Bearer") );
	}

	SECTION("HasLineBreak")
	{
		CHECK_FALSE ( KHTTPHeaders::HasLineBreak("") );
		CHECK_FALSE ( KHTTPHeaders::HasLineBreak("text/plain; charset=utf-8") );
		CHECK       ( KHTTPHeaders::HasLineBreak("value\n") );
		CHECK       ( KHTTPHeaders::HasLineBreak("value\r") );
		CHECK       ( KHTTPHeaders::HasLineBreak("a\r\nb: c") );
	}

	SECTION("Format")
	{
		KHTTPHeader H = KHTTPHeader::CONTENT_TYPE;
		auto sHeader = kFormat("{}: none", H);
		CHECK ( sHeader == "content-type: none");
		sHeader = kFormat("{}: none", KHTTPHeader::CONTENT_TYPE);
		CHECK ( sHeader == "content-type: none");
	}

	SECTION("GetRanges")
	{
		static constexpr uint64_t iSize = 1000; // valid byte offsets 0..999

		// closed range
		{
			auto Ranges = KHTTPHeader::GetRanges("bytes=0-499", iSize);
			REQUIRE ( Ranges.size() == 1 );
			CHECK ( Ranges.front().GetStart() == 0   );
			CHECK ( Ranges.front().GetEnd()   == 499 );
			CHECK ( Ranges.front().GetSize()  == 500 );
		}

		// open ended range "start-"
		{
			auto Ranges = KHTTPHeader::GetRanges("bytes=500-", iSize);
			REQUIRE ( Ranges.size() == 1 );
			CHECK ( Ranges.front().GetStart() == 500 );
			CHECK ( Ranges.front().GetEnd()   == 999 );
			CHECK ( Ranges.front().GetSize()  == 500 );
		}

		// closed range with end past EOF is clamped to the last byte
		{
			auto Ranges = KHTTPHeader::GetRanges("bytes=900-5000", iSize);
			REQUIRE ( Ranges.size() == 1 );
			CHECK ( Ranges.front().GetStart() == 900 );
			CHECK ( Ranges.front().GetEnd()   == 999 );
			CHECK ( Ranges.front().GetSize()  == 100 );
		}

		// suffix range "-N" = the last N bytes (must be exactly N, not N+1)
		{
			auto Ranges = KHTTPHeader::GetRanges("bytes=-100", iSize);
			REQUIRE ( Ranges.size() == 1 );
			CHECK ( Ranges.front().GetStart() == 900 );
			CHECK ( Ranges.front().GetEnd()   == 999 );
			CHECK ( Ranges.front().GetSize()  == 100 );
		}

		// suffix equal to the resource size = the whole resource
		{
			auto Ranges = KHTTPHeader::GetRanges("bytes=-1000", iSize);
			REQUIRE ( Ranges.size() == 1 );
			CHECK ( Ranges.front().GetStart() == 0   );
			CHECK ( Ranges.front().GetEnd()   == 999 );
			CHECK ( Ranges.front().GetSize()  == 1000 );
		}

		// suffix larger than the resource = the whole resource (not a negative start)
		{
			auto Ranges = KHTTPHeader::GetRanges("bytes=-2000", iSize);
			REQUIRE ( Ranges.size() == 1 );
			CHECK ( Ranges.front().GetStart() == 0   );
			CHECK ( Ranges.front().GetEnd()   == 999 );
			CHECK ( Ranges.front().GetSize()  == 1000 );
		}

		// no Range header at all -> no ranges
		{
			auto Ranges = KHTTPHeader::GetRanges("", iSize);
			CHECK ( Ranges.empty() );
		}
	}
}


TEST_CASE("KHTTPHeaders::ParseContentLength")
{
	using H = KHTTPHeaders;
	// valid: pure digits, full non-negative std::streamsize range
	CHECK ( H::ParseContentLength("0")                   == 0 );
	CHECK ( H::ParseContentLength("10")                  == 10 );
	CHECK ( H::ParseContentLength("007")                 == 7 );
	CHECK ( H::ParseContentLength("9223372036854775807") == 9223372036854775807LL ); // INT64_MAX

	// invalid -> -1 (empty, sign, whitespace, trailing garbage, out of range)
	CHECK ( H::ParseContentLength("")                    == -1 );
	CHECK ( H::ParseContentLength("+10")                 == -1 );
	CHECK ( H::ParseContentLength("-1")                  == -1 );
	CHECK ( H::ParseContentLength(" 10")                 == -1 );
	CHECK ( H::ParseContentLength("10 ")                 == -1 );
	CHECK ( H::ParseContentLength("10abc")               == -1 );
	CHECK ( H::ParseContentLength("0x10")                == -1 );
	CHECK ( H::ParseContentLength("9223372036854775808")  == -1 ); // INT64_MAX + 1
	CHECK ( H::ParseContentLength("18446744073709551616") == -1 ); // 2^64 (would wrap to 0)
	CHECK ( H::ParseContentLength("99999999999999999999999999") == -1 );
}

TEST_CASE("KHTTPHeaders::Parse")
{
	SECTION("whitespace before the colon is trimmed in lenient mode")
	{
		KInStringStream iss("Content-Length : 5\r\nX-Test\t: value\r\n\r\n");
		KHTTPHeaders Headers;
		CHECK ( Headers.Parse(iss) == true );
		CHECK ( Headers.Headers.Get(KHTTPHeader::CONTENT_LENGTH) == "5" );
		CHECK ( Headers.Headers.Get("X-Test") == "value" );
	}

	SECTION("whitespace before the colon is rejected in strict mode")
	{
		KInStringStream iss("Content-Length : 5\r\n\r\n");
		KHTTPHeaders Headers;
		CHECK ( Headers.Parse(iss, KHTTPHeaders::DefaultRequestLimits, true) == false );
		CHECK ( Headers.Error().contains("whitespace") );
	}

	SECTION("strict mode accepts well formed headers")
	{
		KInStringStream iss("Content-Length: 5\r\nHost: localhost\r\n\r\n");
		KHTTPHeaders Headers;
		CHECK ( Headers.Parse(iss, KHTTPHeaders::DefaultRequestLimits, true) == true );
		CHECK ( Headers.Headers.Get(KHTTPHeader::CONTENT_LENGTH) == "5" );
		CHECK ( Headers.Headers.Get(KHTTPHeader::HOST) == "localhost" );
		CHECK ( Headers.GetParseError() == KHTTPHeaders::NoError );
	}

	SECTION("strict mode rejects every line that is no valid field")
	{
		struct Case { KStringView sBlock; KStringView sError; };

		std::vector<Case> Cases
		{
			{ "Host: localhost\r\nno colon here\r\n\r\n",      "header line without colon"                },
			{ " Host: localhost\r\n\r\n",                       "whitespace before the first header field" },
			{ "X-A: 1\r\n 2\r\n\r\n",                           "obsolete line folding in header"          },
			{ "X-A: 1\r\n\t2\r\n\r\n",                          "obsolete line folding in header"          },
			{ "X Y: 1\r\n\r\n",                                 "invalid header field name"                },
			{ "X@Y: 1\r\n\r\n",                                 "invalid header field name"                },
			{ ": 1\r\n\r\n",                                    "invalid header field name"                },
		};

		for (const auto& Case : Cases)
		{
			INFO ( Case.sBlock );
			KInStringStream iss(Case.sBlock);
			KHTTPHeaders Headers;
			CHECK ( Headers.Parse(iss, KHTTPHeaders::DefaultRequestLimits, true) == false );
			CHECK ( Headers.GetParseError() == KHTTPHeaders::Malformed );
			CHECK ( Headers.Error() == Case.sError );
		}
	}

	SECTION("field names are tokens")
	{
		// a token may start with a digit or a symbol
		KStringView sBlock = "1x: a\r\n#x: b\r\nx.y~z: c\r\n!#$%&'*+-.^_`|~: d\r\n\r\n";

		for (bool bStrict : { true, false })
		{
			KInStringStream iss(sBlock);
			KHTTPHeaders Headers;
			CHECK ( Headers.Parse(iss, KHTTPHeaders::DefaultRequestLimits, bStrict) == true );
			CHECK ( Headers.Headers.size() == 4 );
			CHECK ( Headers.Headers.Get("1x")    == "a" );
			CHECK ( Headers.Headers.Get("#x")    == "b" );
			CHECK ( Headers.Headers.Get("x.y~z") == "c" );
			CHECK ( Headers.Headers.Get("!#$%&'*+-.^_`|~") == "d" );
		}
	}

	SECTION("lenient mode accepts the field names of mail headers")
	{
		// RFC 5322 allows all visible ASCII characters except the colon
		KInStringStream iss("X@Y: 1\r\n(x): 2\r\n\r\n");
		KHTTPHeaders Headers;
		CHECK ( Headers.Parse(iss) == true );
		CHECK ( Headers.Headers.Get("X@Y") == "1" );
		CHECK ( Headers.Headers.Get("(x)") == "2" );
	}

	SECTION("lenient mode drops lines that are no valid field")
	{
		KInStringStream iss(" lead\r\nA: 1\r\nno colon\r\nX Y: 2\r\n: 3\r\nB: 4\r\n\r\n");
		KHTTPHeaders Headers;
		CHECK ( Headers.Parse(iss) == true );
		CHECK ( Headers.Headers.size() == 2 );
		CHECK ( Headers.Headers.Get("A") == "1" );
		CHECK ( Headers.Headers.Get("B") == "4" );
		CHECK ( Headers.GetParseError() == KHTTPHeaders::NoError );
	}

	SECTION("malformed and incomplete blocks")
	{
		{
			KInStringStream iss("Content-Length : 5\r\n\r\n");
			KHTTPHeaders Headers;
			CHECK ( Headers.Parse(iss, KHTTPHeaders::DefaultRequestLimits, true) == false );
			CHECK ( Headers.GetParseError() == KHTTPHeaders::Malformed );
		}
		{
			KInStringStream iss("Content-Length: 5\r\nContent-Length: 6\r\n\r\n");
			KHTTPHeaders Headers;
			CHECK ( Headers.Parse(iss) == false );
			CHECK ( Headers.GetParseError() == KHTTPHeaders::Malformed );
		}
		{
			// no empty line at the end
			KInStringStream iss("Host: localhost\r\n");
			KHTTPHeaders Headers;
			CHECK ( Headers.Parse(iss) == false );
			CHECK ( Headers.GetParseError() == KHTTPHeaders::Incomplete );
		}
	}

	SECTION("field length")
	{
		// the first line has 20 bytes, name plus value 18 - KInStringStream reads
		// from a view, so the input must outlive the stream
		KString sBlock = "Abcd: 0123456789abcd\r\nNext: 1\r\n\r\n";

		{
			// a line of exactly the limit must be read with its line end, or the
			// following line would be lost
			KInStringStream iss(sBlock);
			KHTTPHeaders Headers;
			CHECK ( Headers.Parse(iss, { 20, 0, 0 }) == true );
			CHECK ( Headers.Headers.Get("Abcd") == "0123456789abcd" );
			CHECK ( Headers.Headers.Get("Next") == "1" );
		}
		{
			// the line is one byte too long, although name plus value would fit
			KInStringStream iss(sBlock);
			KHTTPHeaders Headers;
			CHECK ( Headers.Parse(iss, { 19, 0, 0 }) == false );
			CHECK ( Headers.GetParseError() == KHTTPHeaders::FieldTooLong );
			CHECK ( Headers.Error() == "HTTP header line longer than 19 bytes" );
		}
		{
			// the default request limit
			auto sDefault = kFormat("X: {}\r\n\r\n", KString(8 * 1024 - 3, 'a'));
			KInStringStream iss(sDefault);
			KHTTPHeaders Headers;
			CHECK ( Headers.Parse(iss) == true );
		}
		{
			auto sTooLong = kFormat("X: {}\r\n\r\n", KString(8 * 1024 - 2, 'a'));
			KInStringStream iss(sTooLong);
			KHTTPHeaders Headers;
			CHECK ( Headers.Parse(iss) == false );
			CHECK ( Headers.GetParseError() == KHTTPHeaders::FieldTooLong );
		}
		{
			// 0 is unlimited
			auto sUnlimited = kFormat("X: {}\r\n\r\n", KString(100000, 'a'));
			KInStringStream iss(sUnlimited);
			KHTTPHeaders Headers;
			CHECK ( Headers.Parse(iss, { 0, 0, 0 }) == true );
			CHECK ( Headers.Headers.Get("X").size() == 100000 );
		}
	}

	SECTION("continuation lines")
	{
		// a continuation line adds a space and its trimmed content to the value:
		// name plus value are 3 + 5 + 1 + 4 = 13 bytes
		KStringView sBlock = "X-A: 12345\r\n   6789\r\n\r\n";

		{
			KInStringStream iss(sBlock);
			KHTTPHeaders Headers;
			CHECK ( Headers.Parse(iss, { 13, 0, 0 }) == true );
			CHECK ( Headers.Headers.Get("X-A") == "12345 6789" );
		}
		{
			KInStringStream iss(sBlock);
			KHTTPHeaders Headers;
			CHECK ( Headers.Parse(iss, { 12, 0, 0 }) == false );
			CHECK ( Headers.GetParseError() == KHTTPHeaders::FieldTooLong );
			CHECK ( Headers.Error() == "HTTP header field too long: 13 bytes, the limit is 12" );
		}
		{
			// the continuation counts toward the section size: 13 + 32 = 45 bytes
			KInStringStream iss(sBlock);
			KHTTPHeaders Headers;
			CHECK ( Headers.Parse(iss, { 0, 0, 44 }) == false );
			CHECK ( Headers.GetParseError() == KHTTPHeaders::SectionTooLarge );
		}
	}

	SECTION("field count")
	{
		KStringView sBlock = "A: 1\r\nB: 2\r\nC: 3\r\n\r\n";

		{
			KInStringStream iss(sBlock);
			KHTTPHeaders Headers;
			CHECK ( Headers.Parse(iss, { 0, 3, 0 }) == true );
			CHECK ( Headers.Headers.size() == 3 );
		}
		{
			KInStringStream iss(sBlock);
			KHTTPHeaders Headers;
			CHECK ( Headers.Parse(iss, { 0, 2, 0 }) == false );
			CHECK ( Headers.GetParseError() == KHTTPHeaders::TooManyFields );
			CHECK ( Headers.Error() == "too many HTTP header fields: 3, the limit is 2" );
		}
	}

	SECTION("section size")
	{
		// each field counts 1 + 1 + 32 = 34 bytes
		KStringView sBlock = "A: 1\r\nB: 2\r\n\r\n";

		{
			KInStringStream iss(sBlock);
			KHTTPHeaders Headers;
			CHECK ( Headers.Parse(iss, { 0, 0, 68 }) == true );
		}
		{
			KInStringStream iss(sBlock);
			KHTTPHeaders Headers;
			CHECK ( Headers.Parse(iss, { 0, 0, 67 }) == false );
			CHECK ( Headers.GetParseError() == KHTTPHeaders::SectionTooLarge );
			CHECK ( Headers.Error() == "HTTP header section too large: 68 bytes, the limit is 67" );
		}
	}

	SECTION("dropped lines count toward the section size")
	{
		// "A: 1" counts 1 + 1 + 32 = 34 bytes, the dropped lines their length plus 32:
		// "garbage" (no colon) 39, "x y: 1" (no valid name) 38, "   " (only
		// whitespace after a field) 35
		KStringView sBlock = "A: 1\r\ngarbage\r\nx y: 1\r\n   \r\n\r\n";

		{
			KInStringStream iss(sBlock);
			KHTTPHeaders Headers;
			// the dropped lines are no fields
			CHECK ( Headers.Parse(iss, { 0, 1, 146 }) == true );
			CHECK ( Headers.Headers.size() == 1 );
			CHECK ( Headers.Headers.Get("A") == "1" );
		}
		{
			KInStringStream iss(sBlock);
			KHTTPHeaders Headers;
			CHECK ( Headers.Parse(iss, { 0, 1, 145 }) == false );
			CHECK ( Headers.GetParseError() == KHTTPHeaders::SectionTooLarge );
			CHECK ( Headers.Error() == "HTTP header section too large: 146 bytes, the limit is 145" );
		}
		{
			// garbage before the first field
			KInStringStream iss(" x\r\nA: 1\r\n\r\n");
			KHTTPHeaders Headers;
			CHECK ( Headers.Parse(iss, { 0, 0, 34 + 34 }) == true );
		}
		{
			KInStringStream iss(" x\r\nA: 1\r\n\r\n");
			KHTTPHeaders Headers;
			CHECK ( Headers.Parse(iss, { 0, 0, 34 + 33 }) == false );
			CHECK ( Headers.GetParseError() == KHTTPHeaders::SectionTooLarge );
		}
	}

	SECTION("a new parse resets the reason")
	{
		KHTTPHeaders Headers;
		KInStringStream iss1("A: 1\r\nB: 2\r\n\r\n");
		CHECK ( Headers.Parse(iss1, { 0, 1, 0 }) == false );
		CHECK ( Headers.GetParseError() == KHTTPHeaders::TooManyFields );
		KInStringStream iss2("A: 1\r\n\r\n");
		CHECK ( Headers.Parse(iss2, { 0, 1, 0 }) == true );
		CHECK ( Headers.GetParseError() == KHTTPHeaders::NoError );
	}
}

TEST_CASE("KHTTPHeaders::Counter")
{
	SECTION("default limits")
	{
		CHECK ( KHTTPHeaders::DefaultRequestLimits.iMaxFieldLength  ==   8 * 1024 );
		CHECK ( KHTTPHeaders::DefaultRequestLimits.iMaxFieldCount   ==        500 );
		CHECK ( KHTTPHeaders::DefaultRequestLimits.iMaxSectionSize  ==  64 * 1024 );
		CHECK ( KHTTPHeaders::DefaultResponseLimits.iMaxFieldLength ==  64 * 1024 );
		CHECK ( KHTTPHeaders::DefaultResponseLimits.iMaxFieldCount  ==        500 );
		CHECK ( KHTTPHeaders::DefaultResponseLimits.iMaxSectionSize == 256 * 1024 );
	}

	SECTION("accounting")
	{
		KHTTPHeaders::Counter Count({ 0, 0, 0 });
		CHECK ( Count.Add("abc", "defg") == true );
		CHECK ( Count.GetFieldCount()  == 1 );
		CHECK ( Count.GetSectionSize() == 3 + 4 + 32 );
		CHECK ( Count.Extend(5) == true );
		CHECK ( Count.GetFieldCount()  == 1 );
		CHECK ( Count.GetSectionSize() == 3 + 4 + 32 + 5 );
		CHECK ( Count.Add(":status", "200") == true );
		CHECK ( Count.GetFieldCount()  == 2 );
		CHECK ( Count.GetSectionSize() == 3 + 4 + 32 + 5 + 7 + 3 + 32 );
		CHECK ( Count.GetParseError()  == KHTTPHeaders::NoError );
		CHECK ( Count.GetErrorText().empty() );
	}

	SECTION("field length")
	{
		KHTTPHeaders::Counter Count({ 10, 0, 0 });
		CHECK ( Count.Add("name", "value1") == true );
		CHECK ( Count.Add("name", "value12") == false );
		CHECK ( Count.GetParseError() == KHTTPHeaders::FieldTooLong );
		CHECK ( Count.GetErrorText() == "HTTP header field too long: 11 bytes, the limit is 10" );
	}

	SECTION("field length through Extend")
	{
		KHTTPHeaders::Counter Count({ 10, 0, 0 });
		CHECK ( Count.Add("name", "value") == true );
		CHECK ( Count.Extend(1) == true );
		CHECK ( Count.Extend(1) == false );
		CHECK ( Count.GetParseError() == KHTTPHeaders::FieldTooLong );
	}

	SECTION("field count")
	{
		KHTTPHeaders::Counter Count({ 0, 2, 0 });
		CHECK ( Count.Add("a", "1") == true );
		CHECK ( Count.Add("b", "2") == true );
		CHECK ( Count.Add("c", "3") == false );
		CHECK ( Count.GetParseError() == KHTTPHeaders::TooManyFields );
		CHECK ( Count.GetErrorText() == "too many HTTP header fields: 3, the limit is 2" );
	}

	SECTION("section size")
	{
		KHTTPHeaders::Counter Count({ 0, 0, 100 });
		// 1 + 1 + 32 = 34 bytes
		CHECK ( Count.Add("a", "1") == true );
		// 34 + 1 + 33 + 32 = 100 bytes
		CHECK ( Count.Add("b", KString(33, 'x')) == true );
		CHECK ( Count.GetSectionSize() == 100 );
		// a field with an empty value still counts 1 + 32 bytes
		CHECK ( Count.Add("c", "") == false );
		CHECK ( Count.GetParseError() == KHTTPHeaders::SectionTooLarge );
		CHECK ( Count.GetErrorText() == "HTTP header section too large: 133 bytes, the limit is 100" );
	}

	SECTION("dropped lines")
	{
		KHTTPHeaders::Counter Count({ 0, 0, 100 });
		CHECK ( Count.Add("a", "1") == true );
		// 34 + 10 + 32 = 76 bytes
		CHECK ( Count.AddDropped(10) == true );
		CHECK ( Count.GetFieldCount()  == 1 );
		CHECK ( Count.GetSectionSize() == 76 );
		// 76 + 32 = 108 bytes
		CHECK ( Count.AddDropped(0) == false );
		CHECK ( Count.GetParseError() == KHTTPHeaders::SectionTooLarge );
	}

	SECTION("0 is unlimited")
	{
		KHTTPHeaders::Counter Count({ 0, 0, 0 });

		for (int i = 0; i < 100; ++i)
		{
			CHECK ( Count.Add("name", KString(100000, 'x')) == true );
		}

		CHECK ( Count.GetParseError() == KHTTPHeaders::NoError );
	}

	SECTION("the first violation is kept")
	{
		KHTTPHeaders::Counter Count({ 5, 1, 0 });
		CHECK ( Count.Add("name", "value") == false );
		CHECK ( Count.GetParseError() == KHTTPHeaders::FieldTooLong );
		CHECK ( Count.Add("a", "b") == false );
		CHECK ( Count.GetParseError() == KHTTPHeaders::FieldTooLong );
	}

	SECTION("clear")
	{
		KHTTPHeaders::Counter Count({ 0, 1, 0 });
		CHECK ( Count.Add("a", "1") == true );
		CHECK ( Count.Add("b", "2") == false );
		Count.clear();
		CHECK ( Count.GetFieldCount()  == 0 );
		CHECK ( Count.GetSectionSize() == 0 );
		CHECK ( Count.GetParseError()  == KHTTPHeaders::NoError );
		CHECK ( Count.Add("c", "3") == true );
		CHECK ( Count.GetLimits().iMaxFieldCount == 1 );
	}
}

TEST_CASE("KHTTPHeaders::AddVary")
{
	// returns all Vary header lines, sorted and joined by '|' - the order of
	// multiple header lines with the same name is not defined
	auto VaryValues = [](const KHTTPHeaders& Headers)
	{
		std::vector<KString> Values;
		auto Range = Headers.Headers.equal_range(KHTTPHeader::VARY);

		for (auto it = Range.first; it != Range.second; ++it)
		{
			Values.push_back(it->second);
		}

		std::sort(Values.begin(), Values.end());

		return kJoined(Values, "|");
	};

	SECTION("adds the header name once")
	{
		KHTTPHeaders Headers;
		Headers.AddVary(KHTTPHeader::ACCEPT_ENCODING);
		CHECK ( VaryValues(Headers) == "accept-encoding" );
		Headers.AddVary(KHTTPHeader::ACCEPT_ENCODING);
		CHECK ( VaryValues(Headers) == "accept-encoding" );
		Headers.AddVary(KHTTPHeader("Origin"));
		CHECK ( VaryValues(Headers) == "accept-encoding|origin" );
	}

	SECTION("keeps an existing Vary header")
	{
		KHTTPHeaders Headers;
		Headers.Headers.Set(KHTTPHeader::VARY, "Origin, Accept-Language");
		Headers.AddVary(KHTTPHeader::ACCEPT_ENCODING);
		CHECK ( VaryValues(Headers) == "Origin, Accept-Language|accept-encoding" );
	}

	SECTION("finds the name case insensitively in any Vary line")
	{
		KHTTPHeaders Headers;
		Headers.Headers.Add(KHTTPHeader::VARY, "Origin");
		Headers.Headers.Add(KHTTPHeader::VARY, "Cookie, Accept-Encoding");
		Headers.AddVary(KHTTPHeader::ACCEPT_ENCODING);
		CHECK ( VaryValues(Headers) == "Cookie, Accept-Encoding|Origin" );
	}

	SECTION("a Vary of * covers all header names")
	{
		KHTTPHeaders Headers;
		Headers.Headers.Set(KHTTPHeader::VARY, "*");
		Headers.AddVary(KHTTPHeader::ACCEPT_ENCODING);
		CHECK ( VaryValues(Headers) == "*" );
	}
}
