#include "catch.hpp"

#include <dekaf2/http/protocol/kchunkedtransfer.h>
#include <dekaf2/http/protocol/khttp_header.h>
#include <dekaf2/http/server/khttpoutputfilter.h>
#include <dekaf2/io/streams/kstringstream.h>
#include <dekaf2/core/format/kformat.h>

using namespace dekaf2;

namespace {

// read a whole chunked body through KChunkedSource
KString ReadChunked(KStringView sInput)
{
	KInStringStream iss(sInput);
	KChunkedSource  Source(iss, true);
	return Source.read();
}

} // end anonymous namespace

TEST_CASE("KChunkedSource")
{
	SECTION("valid chunked transfer")
	{
		// two chunks, "Wiki" (4) + "pedia" (5), terminated by a 0 chunk
		CHECK ( ReadChunked("4\r\nWiki\r\n5\r\npedia\r\n0\r\n\r\n") == "Wikipedia" );
	}

	SECTION("single chunk")
	{
		CHECK ( ReadChunked("5\r\nHello\r\n0\r\n\r\n") == "Hello" );
	}

	SECTION("empty body")
	{
		CHECK ( ReadChunked("0\r\n\r\n").empty() );
	}

	SECTION("chunk extensions are ignored")
	{
		// RFC 9112 7.1.1 - extensions and trailers do not end the transfer
		CHECK ( ReadChunked("5;ext=value\r\nHello\r\n6 ; a=\"b\"\r\n World\r\n0\r\nTrailer: x\r\n\r\n") == "Hello World" );
	}

	SECTION("invalid chunk size character aborts with an error")
	{
		// the remainder must not be read as an unchunked body - it could be the next request
		KInStringStream iss("zz\r\nGET /admin HTTP/1.1\r\n\r\n");
		KChunkedSource  Source(iss, true);
		auto sBody = Source.read();
		CHECK ( sBody.empty()       );
		CHECK ( Source.IsFinished() );
		CHECK ( Source.HadError()   );
	}

	SECTION("missing chunk size aborts with an error")
	{
		KInStringStream iss("\r\nHello\r\n0\r\n\r\n");
		KChunkedSource  Source(iss, true);
		CHECK ( Source.read().empty() );
		CHECK ( Source.HadError()     );
	}

	SECTION("chunk data longer than announced aborts with an error")
	{
		KInStringStream iss("5\r\nHello World\r\n0\r\n\r\n");
		KChunkedSource  Source(iss, true);
		CHECK ( Source.read() == "Hello" );
		CHECK ( Source.HadError()         );
	}

	SECTION("a clean transfer has no error")
	{
		KInStringStream iss("5\r\nHello\r\n0\r\n\r\n");
		KChunkedSource  Source(iss, true);
		CHECK ( Source.read() == "Hello" );
		CHECK ( Source.IsFinished()       );
		CHECK ( Source.HadError() == false );
	}

	SECTION("input that ends before the last chunk is an error")
	{
		for (KStringView sInput : { "5\r\nHello\r\n", "5\r\nHello\r\n0", "5\r\nHel", "" })
		{
			INFO ( sInput );
			KInStringStream iss(sInput);
			KChunkedSource  Source(iss, true);
			Source.read();
			CHECK ( Source.IsFinished() );
			CHECK ( Source.HadError()   );
		}
	}

	SECTION("input that ends within the trailer section is complete")
	{
		// the last chunk arrived, only the end of the trailer section is missing
		for (KStringView sInput : { "5\r\nHello\r\n0\r\n", "5\r\nHello\r\n0\r\nTrailer: x\r\n" })
		{
			INFO ( sInput );
			KInStringStream iss(sInput);
			KChunkedSource  Source(iss, true);
			CHECK ( Source.read() == "Hello" );
			CHECK ( Source.IsFinished()        );
			CHECK ( Source.HadError() == false );
		}
	}

	SECTION("input that ends before Content-Length is an error")
	{
		{
			KInStringStream iss("Hello");
			KChunkedSource  Source(iss, false, 10);
			CHECK ( Source.read() == "Hello" );
			CHECK ( Source.HadError() );
		}
		{
			KInStringStream iss("Hello");
			KChunkedSource  Source(iss, false, 5);
			CHECK ( Source.read() == "Hello" );
			CHECK ( Source.HadError() == false );
		}
		{
			// without a Content-Length, the end of the input is the end of the body
			KInStringStream iss("Hello");
			KChunkedSource  Source(iss, false);
			CHECK ( Source.read() == "Hello" );
			CHECK ( Source.HadError() == false );
		}
	}

	SECTION("oversized chunk size is rejected without overflow")
	{
		// A crafted chunk-size line whose value does not fit into the signed
		// std::streamsize accumulator. Before the guard was added the value
		// wrapped around: on C++20+ builds it merely truncated the body (the
		// std::min() clamp and the signed->unsigned round trip in the read path
		// masked the damage), but on the pre-C++20 compilers dekaf2 still targets
		// the signed overflow was undefined behavior. The reader must now reject
		// such a size and yield no body rather than relying on wraparound.

		const KString sBody(8192, 'A');

		// sign bit set (2^63)
		CHECK ( ReadChunked(kFormat("8000000000000000\r\n{}\r\n0\r\n\r\n", sBody)).empty() );

		// wraps to exactly zero (2^64) - would otherwise be mistaken for the
		// terminating 0-chunk
		CHECK ( ReadChunked(kFormat("10000000000000000\r\n{}\r\n0\r\n\r\n", sBody)).empty() );

		// far more hex digits than fit into 64 bits
		CHECK ( ReadChunked(kFormat("FFFFFFFFFFFFFFFFFFFFFFFF\r\n{}\r\n0\r\n\r\n", sBody)).empty() );
	}
}

TEST_CASE("KOutHTTPFilter")
{
	SECTION("Write() from a stream returns the count of written bytes")
	{
		// writes sContent through a filter without chunks, returns what Write() returned
		auto Write = [](KStringView sContent, std::size_t iLen, KString& sOut)
		{
			KOutStringStream oss(sOut);
			KOutHTTPFilter   Filter(oss);
			KInStringStream  iss(sContent);
			auto iWrote = Filter.Write(iss, iLen);
			CHECK ( Filter.FilteredStream().Good() );
			Filter.Flush();
			return iWrote;
		};

		KString sOut;
		CHECK ( Write("0123456789", npos, sOut) == 10 );
		CHECK ( sOut == "0123456789" );

		// an empty stream is no error
		sOut.clear();
		CHECK ( Write("", npos, sOut) == 0 );
		CHECK ( sOut.empty() );

		sOut.clear();
		CHECK ( Write("0123456789", 10, sOut) == 10 );
		CHECK ( sOut == "0123456789" );

		// the stream is longer than the requested count
		sOut.clear();
		CHECK ( Write("0123456789", 4, sOut) == 4 );
		CHECK ( sOut == "0123" );

		// the stream ends before the requested count
		sOut.clear();
		CHECK ( Write("0123456789", 20, sOut) == 10 );
		CHECK ( sOut == "0123456789" );

		// more than one copy buffer, with all byte values
		KString sLarge;
		for (std::size_t i = 0; i < 3 * 1024 * 1024 + 17; ++i)
		{
			sLarge += static_cast<char>(i * 7);
		}
		sOut.clear();
		CHECK ( Write(sLarge, npos, sOut) == sLarge.size() );
		CHECK ( sOut == sLarge );
	}

	SECTION("a chunked message from an empty stream is the last chunk")
	{
		KHTTPHeaders Headers;
		Headers.Headers.Set(KHTTPHeader::TRANSFER_ENCODING, "chunked");

		KString          sOut;
		KOutStringStream oss(sOut);
		KOutHTTPFilter   Filter(oss);
		Filter.Parse(Headers);

		KString          sEmpty;
		KInStringStream  iss(sEmpty);
		CHECK ( Filter.Write(iss, npos) == 0 );
		Filter.Flush();
		CHECK ( sOut == "0\r\n\r\n" );
	}

	SECTION("Abort() ends a message without its pending output and without the last chunk")
	{
		KHTTPHeaders Headers;
		Headers.Headers.Set(KHTTPHeader::TRANSFER_ENCODING, "chunked");

		KString          sOut;
		KOutStringStream oss(sOut);
		KOutHTTPFilter   Filter(oss);
		Filter.Parse(Headers);

		CHECK ( Filter.Write("this message is not completed") > 0 );
		Filter.Abort();
		oss.Flush();
		CHECK_FALSE ( sOut.contains("0\r\n\r\n") );
		auto iAborted = sOut.size();

		// Abort() without a message does nothing
		Filter.Abort();
		oss.Flush();
		CHECK ( sOut.size() == iAborted );

		// the next message is complete
		Filter.Parse(Headers);
		CHECK ( Filter.Write("hello") == 5 );
		Filter.Flush();
		CHECK ( ReadChunked(sOut.substr(iAborted)) == "hello" );
		CHECK ( sOut.ends_with("\r\n0\r\n\r\n") );
	}
}
