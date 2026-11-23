#include "catch.hpp"

#include <dekaf2/http/protocol/kchunkedtransfer.h>
#include <dekaf2/http/protocol/khttp_header.h>
#include <dekaf2/http/server/khttpoutputfilter.h>
#include <dekaf2/io/streams/kstringstream.h>
#include <dekaf2/io/streams/kfdstream.h>
#include <dekaf2/io/compression/kcompression.h>
#include <dekaf2/core/format/kformat.h>
#include <zlib.h>
#ifdef DEKAF2_HAS_LIBLZMA
	#include <lzma.h>
#endif
#include <array>
#ifndef DEKAF2_IS_WINDOWS
	#include <unistd.h>
#endif
#include <vector>

using namespace dekaf2;

namespace {

#ifndef DEKAF2_IS_WINDOWS
// write all of sData to a file descriptor
bool WriteTo(int fd, KStringView sData)
{
	return ::write(fd, sData.data(), sData.size()) == static_cast<ssize_t>(sData.size());
}
#endif

// read a whole chunked body through KChunkedSource
KString ReadChunked(KStringView sInput)
{
	KInStringStream iss(sInput);
	KChunkedSource  Source(iss, true);
	return Source.read();
}

// decompress a complete compressed stream
KString Uncompress(KStringView sInput, KUnCompressIStream::COMPRESSION Compression)
{
	KString sOutput;
	KUnCompress UnCompress(sInput, Compression);
	UnCompress.ReadRemaining(sOutput);
	return sOutput;
}

// decompress what a stream in the gzip or zlib format holds so far - the decompressors
// of boost::iostreams for zlib and lzma refuse a stream as long as it is not complete
KString InflateOpenStream(KStringView sInput)
{
	KString sOutput;

	z_stream Stream {};

	// 32 added to the window bits: detect the gzip or the zlib format
	if (::inflateInit2(&Stream, 15 + 32) != Z_OK)
	{
		return sOutput;
	}

	std::array<char, 16384> Buffer;

	Stream.next_in  = reinterpret_cast<Bytef*>(const_cast<char*>(sInput.data()));
	Stream.avail_in = static_cast<uInt>(sInput.size());

	for (;;)
	{
		Stream.next_out  = reinterpret_cast<Bytef*>(Buffer.data());
		Stream.avail_out = static_cast<uInt>(Buffer.size());

		auto iResult = ::inflate(&Stream, Z_SYNC_FLUSH);
		auto iHave   = Buffer.size() - Stream.avail_out;

		sOutput.append(Buffer.data(), iHave);

		if (iResult != Z_OK || iHave == 0)
		{
			break;
		}
	}

	::inflateEnd(&Stream);

	return sOutput;
}

#ifdef DEKAF2_HAS_LIBLZMA
// decompress what a stream in the xz format holds so far
KString UnXZOpenStream(KStringView sInput)
{
	KString sOutput;

	lzma_stream Stream = LZMA_STREAM_INIT;

	if (::lzma_stream_decoder(&Stream, UINT64_MAX, 0) != LZMA_OK)
	{
		return sOutput;
	}

	std::array<char, 16384> Buffer;

	Stream.next_in  = reinterpret_cast<const uint8_t*>(sInput.data());
	Stream.avail_in = sInput.size();

	for (;;)
	{
		Stream.next_out  = reinterpret_cast<uint8_t*>(Buffer.data());
		Stream.avail_out = Buffer.size();

		auto iResult = ::lzma_code(&Stream, LZMA_RUN);
		auto iHave   = Buffer.size() - Stream.avail_out;

		sOutput.append(Buffer.data(), iHave);

		if (iResult != LZMA_OK || iHave == 0)
		{
			break;
		}
	}

	::lzma_end(&Stream);

	return sOutput;
}
#endif

// decompress what a compressed stream holds so far - the stream may still be open
KString UncompressOpenStream(KStringView sInput, KUnCompressIStream::COMPRESSION Compression)
{
	switch (Compression)
	{
		case KUnCompressIStream::GZIP:
		case KUnCompressIStream::ZLIB:
			return InflateOpenStream(sInput);

#ifdef DEKAF2_HAS_LIBLZMA
		case KUnCompressIStream::LZMA:
			return UnXZOpenStream(sInput);
#endif

		default:
			// the decompressors of zstd and brotli read an open stream
			return Uncompress(sInput, Compression);
	}
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

	SECTION("a read returns a chunk, and does not wait for the next one")
	{
		KInStringStream iss("4\r\nWiki\r\n5\r\npedia\r\n0\r\n\r\n");
		KChunkedSource  Source(iss, true);

		std::array<char, 100> Buffer;

		auto iRead = Source.read(Buffer.data(), Buffer.size());
		REQUIRE ( iRead == 4 );
		CHECK   ( KStringView(Buffer.data(), 4) == "Wiki" );

		iRead = Source.read(Buffer.data(), Buffer.size());
		REQUIRE ( iRead == 5 );
		CHECK   ( KStringView(Buffer.data(), 5) == "pedia" );

		CHECK ( Source.read(Buffer.data(), Buffer.size()) <= 0 );
		CHECK ( Source.read(Buffer.data(), Buffer.size()) == -1 );
		CHECK_FALSE ( Source.HadError() );
	}

#ifndef DEKAF2_IS_WINDOWS
	SECTION("a chunk is returned while the next one has not yet arrived")
	{
		std::array<int, 2> Pipe;
		REQUIRE ( ::pipe(Pipe.data()) == 0 );

		KFDReader      Reader(Pipe[0]);
		KChunkedSource Source(Reader, true);

		std::array<char, 100> Buffer;

		REQUIRE ( WriteTo(Pipe[1], "6\r\nevent1\r\n") );

		// the write end stays open: a read that waited for more would not return
		auto iRead = Source.read(Buffer.data(), Buffer.size());
		REQUIRE ( iRead == 6 );
		CHECK   ( KStringView(Buffer.data(), 6) == "event1" );

		REQUIRE ( WriteTo(Pipe[1], "5\r\nabc") );

		// a part of a chunk is also returned
		iRead = Source.read(Buffer.data(), Buffer.size());
		REQUIRE ( iRead == 3 );
		CHECK   ( KStringView(Buffer.data(), 3) == "abc" );

		REQUIRE ( WriteTo(Pipe[1], "de\r\n0\r\n\r\n") );
		::close(Pipe[1]);

		iRead = Source.read(Buffer.data(), Buffer.size());
		REQUIRE ( iRead == 2 );
		CHECK   ( KStringView(Buffer.data(), 2) == "de" );

		CHECK ( Source.read(Buffer.data(), Buffer.size()) <= 0 );
		CHECK_FALSE ( Source.HadError() );

		Reader.close();
	}

	SECTION("a body without chunks is returned as it arrives")
	{
		std::array<int, 2> Pipe;
		REQUIRE ( ::pipe(Pipe.data()) == 0 );

		KFDReader      Reader(Pipe[0]);
		KChunkedSource Source(Reader, false);

		std::array<char, 100> Buffer;

		REQUIRE ( WriteTo(Pipe[1], "data: x\n\n") );

		auto iRead = Source.read(Buffer.data(), Buffer.size());
		REQUIRE ( iRead == 9 );
		CHECK   ( KStringView(Buffer.data(), 9) == "data: x\n\n" );

		::close(Pipe[1]);

		CHECK ( Source.read(Buffer.data(), Buffer.size()) <= 0 );

		Reader.close();
	}
#endif

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
			Filter.Close();
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
		Filter.Close();
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
		Filter.Close();
		CHECK ( ReadChunked(sOut.substr(iAborted)) == "hello" );
		CHECK ( sOut.ends_with("\r\n0\r\n\r\n") );
	}

	SECTION("Flush() sends the content and keeps a chunked message open")
	{
		KHTTPHeaders Headers;
		Headers.Headers.Set(KHTTPHeader::TRANSFER_ENCODING, "chunked");

		KString          sOut;
		KOutStringStream oss(sOut);
		KOutHTTPFilter   Filter(oss);
		Filter.Parse(Headers);

		CHECK ( Filter.Write("one") == 3 );
		CHECK ( Filter.Flush() );
		CHECK ( sOut == "03\r\none\r\n" );

		// a flush without new content writes nothing
		CHECK ( Filter.Flush() );
		CHECK ( sOut == "03\r\none\r\n" );

		CHECK ( Filter.Write("two") == 3 );
		CHECK ( Filter.Flush() );
		CHECK ( sOut == "03\r\none\r\n03\r\ntwo\r\n" );

		CHECK ( Filter.Write("three") == 5 );
		Filter.Close();
		CHECK ( sOut == "03\r\none\r\n03\r\ntwo\r\n05\r\nthree\r\n0\r\n\r\n" );
		CHECK ( static_cast<std::size_t>(Filter.Count()) == sOut.size() );
	}

	SECTION("Flush() sends the content of a message without chunks")
	{
		KString          sOut;
		KOutStringStream oss(sOut);
		KOutHTTPFilter   Filter(oss);

		CHECK ( Filter.Write("one") == 3 );
		CHECK ( Filter.Flush() );
		CHECK ( sOut == "one" );

		CHECK ( Filter.Write("two") == 3 );
		CHECK ( Filter.Flush() );
		CHECK ( sOut == "onetwo" );

		// a flush without a message does nothing
		Filter.Close();
		CHECK ( Filter.Flush() );
		CHECK ( sOut == "onetwo" );
	}

	SECTION("Flush() of a compressed message")
	{
		struct Encoding
		{
			KStringView                     sName;
			KUnCompressIStream::COMPRESSION Compression;
			bool                            bCanFlush;
		};

		std::vector<Encoding> Encodings
		{
			{ "gzip"   , KUnCompressIStream::GZIP  , true  },
			{ "deflate", KUnCompressIStream::ZLIB  , true  },
			// libbz2 keeps the last bits of a block at a flush
			{ "bzip2"  , KUnCompressIStream::BZIP2 , false },
#ifdef DEKAF2_HAS_LIBLZMA
			{ "xz"     , KUnCompressIStream::LZMA  , true  },
#endif
#ifdef DEKAF2_HAS_LIBZSTD
			{ "zstd"   , KUnCompressIStream::ZSTD  , true  },
#endif
#ifdef DEKAF2_HAS_LIBBROTLI
			{ "br"     , KUnCompressIStream::BROTLI, true  },
#endif
		};

		// a piece that does not fit into the buffers of the filters, and that
		// does not compress to nothing
		KString sLarge;

		for (std::size_t i = 0; i < 300 * 1024; ++i)
		{
			sLarge += static_cast<char>('a' + (i * i / 7) % 26);
		}

		std::vector<KStringView> Pieces { "data: first\n\n", "data: second\n\n", sLarge, "data: last\n\n" };

		for (const auto& Enc : Encodings)
		{
			INFO ( Enc.sName );

			KHTTPHeaders Headers;
			Headers.Headers.Set(KHTTPHeader::CONTENT_ENCODING, Enc.sName);

			KString          sOut;
			KOutStringStream oss(sOut);
			KOutHTTPFilter   Filter(oss);
			Filter.Parse(Headers);

			KString sWritten;

			for (auto sPiece : Pieces)
			{
				CHECK ( Filter.Write(sPiece) == sPiece.size() );
				sWritten += sPiece;

				CHECK ( Filter.Flush() == Enc.bCanFlush );

				if (Enc.bCanFlush)
				{
					// the receiver can read all that was written before the flush
					CHECK ( UncompressOpenStream(sOut, Enc.Compression) == sWritten );

					// a flush without new content writes nothing
					auto iSize = sOut.size();
					CHECK ( Filter.Flush() );
					CHECK ( sOut.size() == iSize );
				}
			}

			Filter.Close();
			CHECK ( Uncompress(sOut, Enc.Compression) == sWritten );
			CHECK ( static_cast<std::size_t>(Filter.Count()) == sOut.size() );
		}
	}

	SECTION("a message that is never flushed has the size of one that a symmetric filter writes")
	{
		// without a flush the compressor must not write more than it did before it could be flushed
		KString sContent;

		for (std::size_t i = 0; i < 20000; ++i)
		{
			sContent += static_cast<char>('a' + (i * i / 7) % 26);
		}

		KHTTPHeaders Headers;
		Headers.Headers.Set(KHTTPHeader::CONTENT_ENCODING, "gzip");

		KString sFiltered;
		{
			KOutStringStream oss(sFiltered);
			KOutHTTPFilter   Filter(oss);
			Filter.Parse(Headers);
			CHECK ( Filter.Write(sContent) == sContent.size() );
			Filter.Close();
		}

		KString sReference;
		{
			KGZip GZip(sReference);
			GZip.Write(sContent);
		}

		CHECK ( sFiltered.size() == sReference.size() );
		CHECK ( Uncompress(sFiltered, KUnCompressIStream::GZIP) == sContent );
	}
}
