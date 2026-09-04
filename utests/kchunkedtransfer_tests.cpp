#include "catch.hpp"

#include <dekaf2/http/protocol/kchunkedtransfer.h>
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
