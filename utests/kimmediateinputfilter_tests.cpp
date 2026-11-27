#include "catch.hpp"

#include <dekaf2/io/compression/bits/kimmediateinputfilter.h>
#include <dekaf2/io/compression/kcompression.h>
#include <dekaf2/http/server/khttpoutputfilter.h>
#include <dekaf2/http/protocol/khttp_header.h>
#include <dekaf2/io/streams/kstringstream.h>
#include <dekaf2/io/readwrite/kread.h>
#include <dekaf2/io/readwrite/kreader.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/strings/kstringview.h>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/concepts.hpp>
#include <algorithm>
#include <cstring>
#include <vector>

using namespace dekaf2;

namespace {

using Compression = dekaf2::detail::KCompressionBase;

//-----------------------------------------------------------------------------
/// a source that hands out a string in pieces of the given sizes, one piece per
/// read, and counts the reads
class PieceSource : public boost::iostreams::source
//-----------------------------------------------------------------------------
{

public:

	PieceSource(KStringView sData, std::vector<std::size_t> Pieces, std::size_t* piReads = nullptr)
	: m_sData(sData)
	, m_Pieces(std::move(Pieces))
	, m_piReads(piReads)
	{
	}

	std::streamsize read(char* s, std::streamsize n)
	{
		if (m_piReads)
		{
			++*m_piReads;
		}

		if (m_sData.empty())
		{
			return -1;
		}

		auto iPiece = std::min({ m_sData.size(), static_cast<std::size_t>(n), m_Pieces[m_iNext % m_Pieces.size()] });

		++m_iNext;

		std::memcpy(s, m_sData.data(), iPiece);
		m_sData.remove_prefix(iPiece);

		return static_cast<std::streamsize>(iPiece);
	}

private:

	KStringView              m_sData;
	std::vector<std::size_t> m_Pieces;
	std::size_t*             m_piReads;
	std::size_t              m_iNext { 0 };

}; // PieceSource

//-----------------------------------------------------------------------------
/// text with a pattern and with bytes that do not repeat
KString MakeData(std::size_t iSize)
//-----------------------------------------------------------------------------
{
	KString  sData;
	uint32_t iValue { 12345 };

	sData.reserve(iSize);

	for (std::size_t i = 0; i < iSize; ++i)
	{
		if (i % 7 == 0)
		{
			iValue = iValue * 1103515245 + 12345;
			sData += static_cast<char>(iValue >> 16);
		}
		else
		{
			sData += static_cast<char>('a' + (i % 26));
		}
	}

	return sData;

} // MakeData

//-----------------------------------------------------------------------------
KString Compress(KStringView sPlain, Compression::COMPRESSION Method)
//-----------------------------------------------------------------------------
{
	KString sCompressed;

	{
		KCompressOStream Out(sCompressed, Method);
		Out.write(sPlain.data(), static_cast<std::streamsize>(sPlain.size()));
	}

	return sCompressed;

} // Compress

//-----------------------------------------------------------------------------
/// decompresses with the filter alone, or with the filter inside of the immediate filter
/// @param pbFailed set to true if the filter threw, which a reader of the stream buffer sees
template<class Filter>
KString Decompress(KStringView sCompressed, std::vector<std::size_t> Pieces, bool bImmediate, bool bReadSome, bool* pbFailed = nullptr)
//-----------------------------------------------------------------------------
{
	boost::iostreams::filtering_istream Chain;

	if (bImmediate)
	{
		Chain.push(dekaf2::iostreams::immediate_input_filter<Filter>());
	}
	else
	{
		Chain.push(Filter());
	}

	Chain.push(PieceSource(sCompressed, std::move(Pieces)));

	KString sResult;

	if (bReadSome)
	{
		char Buffer[777];

		try
		{
			for (;;)
			{
				auto iRead = kReadSome(Chain, Buffer, sizeof(Buffer));

				if (!iRead)
				{
					break;
				}

				sResult.append(Buffer, iRead);
			}
		}
		catch (const std::exception&)
		{
			if (pbFailed)
			{
				*pbFailed = true;
			}
		}
	}
	else
	{
		KInStream In(Chain);
		In.ReadRemaining(sResult);
	}

	return sResult;

} // Decompress

//-----------------------------------------------------------------------------
template<class Filter>
void CheckSameOutput(Compression::COMPRESSION Method)
//-----------------------------------------------------------------------------
{
	for (std::size_t iSize : { 0, 1, 100, 4095, 4096, 4097, 20000, 150000 })
	{
		auto sPlain      = MakeData(iSize);
		auto sCompressed = Compress(sPlain, Method);

		for (const auto& Pieces : std::vector<std::vector<std::size_t>>{ { 1 }, { 3, 7, 1 }, { 300 }, { 4096 }, { 100000 } })
		{
			INFO ( iSize );
			INFO ( Pieces.front() );

			CHECK ( Decompress<Filter>(sCompressed, Pieces, true, false) == sPlain );
			CHECK ( Decompress<Filter>(sCompressed, Pieces, true, true ) == sPlain );
		}
	}

} // CheckSameOutput

//-----------------------------------------------------------------------------
/// Input that ends too early, or that is damaged, has to end as without the
/// immediate filter: with a failure if the wrapped filter reports one (not all of
/// them do for input that ends too early), and with no less of the data before it.
template<class Filter>
void CheckBrokenInput(Compression::COMPRESSION Method)
//-----------------------------------------------------------------------------
{
	auto sPlain      = MakeData(20000);
	auto sCompressed = Compress(sPlain, Method);

	REQUIRE ( sCompressed.size() > 100 );

	for (std::size_t iCut : { std::size_t(0), std::size_t(1), std::size_t(10), sCompressed.size() / 2, sCompressed.size() - 1 })
	{
		for (const auto& Pieces : std::vector<std::vector<std::size_t>>{ { 1 }, { 300 }, { 100000 } })
		{
			KStringView sCut = KStringView(sCompressed).substr(0, iCut);

			bool bFailedImmediate { false };
			bool bFailedAlone     { false };

			auto sImmediate = Decompress<Filter>(sCut, Pieces, true,  true, &bFailedImmediate);
			auto sAlone     = Decompress<Filter>(sCut, Pieces, false, true, &bFailedAlone);

			INFO ( iCut );
			INFO ( Pieces.front() );

			CHECK ( bFailedImmediate == bFailedAlone );
			CHECK ( sPlain.starts_with(sImmediate) );
			CHECK ( sPlain.starts_with(sAlone)     );
			CHECK ( sImmediate.size() >= sAlone.size() );
		}
	}

	for (std::size_t iAt : { sCompressed.size() / 3, sCompressed.size() / 2, sCompressed.size() - 5 })
	{
		KString sDamaged = sCompressed;
		sDamaged[iAt] = static_cast<char>(sDamaged[iAt] ^ 0x55);

		for (const auto& Pieces : std::vector<std::vector<std::size_t>>{ { 1 }, { 300 }, { 100000 } })
		{
			bool bFailedImmediate { false };
			bool bFailedAlone     { false };

			auto sImmediate = Decompress<Filter>(sDamaged, Pieces, true,  true, &bFailedImmediate);
			auto sAlone     = Decompress<Filter>(sDamaged, Pieces, false, true, &bFailedAlone);

			INFO ( iAt );
			INFO ( Pieces.front() );

			CHECK ( bFailedImmediate == bFailedAlone );

			// The same data up to the failure. How much of it was handed on before
			// differs: the output of the call that fails is lost in both.
			INFO  ( sImmediate.size() );
			INFO  ( sAlone.size()     );
			CHECK ( (sImmediate.starts_with(sAlone) || sAlone.starts_with(sImmediate)) );
		}
	}

} // CheckBrokenInput

//-----------------------------------------------------------------------------
/// A source that hands out what a sender has sent so far: the string up to a limit
/// that the test raises, in pieces as a connection delivers them. Asked for more it
/// notes that, where a connection would make its reader wait, and ends.
class SentSoFarSource : public boost::iostreams::source
//-----------------------------------------------------------------------------
{

public:

	SentSoFarSource(KStringView sData, const std::size_t* piLimit, bool* pbAskedForMore)
	: m_sData(sData)
	, m_piLimit(piLimit)
	, m_pbAskedForMore(pbAskedForMore)
	{
	}

	std::streamsize read(char* s, std::streamsize n)
	{
		auto iLimit = std::min(*m_piLimit, m_sData.size());

		if (m_iPos >= iLimit)
		{
			if (m_iPos < m_sData.size())
			{
				*m_pbAskedForMore = true;
			}

			return -1;
		}

		auto iPiece = std::min({ iLimit - m_iPos, static_cast<std::size_t>(n), std::size_t(1400) });

		std::memcpy(s, m_sData.data() + m_iPos, iPiece);
		m_iPos += iPiece;

		return static_cast<std::streamsize>(iPiece);
	}

private:

	KStringView        m_sData;
	const std::size_t* m_piLimit;
	bool*              m_pbAskedForMore;
	std::size_t        m_iPos { 0 };

}; // SentSoFarSource

//-----------------------------------------------------------------------------
/// A sender writes messages and flushes after each. The reader has to get every
/// message in full from what was sent up to its flush, without asking for more.
template<class Filter>
void CheckMessages(KStringView sEncoding)
//-----------------------------------------------------------------------------
{
	// from a few bytes to more than the buffers of the filters and of the decompressors hold
	std::vector<KString> Messages { "data: first\n\n", MakeData(300 * 1024), "x", MakeData(5000), MakeData(70000), "data: last\n\n" };

	KHTTPHeaders Headers;
	Headers.Headers.Set(KHTTPHeader::CONTENT_ENCODING, sEncoding);

	KString                  sCompressed;
	std::vector<std::size_t> Sent;
	KString                  sPlain;

	{
		KOutStringStream Out(sCompressed);
		KOutHTTPFilter   Sender(Out);
		Sender.Parse(Headers);

		for (const auto& sMessage : Messages)
		{
			REQUIRE ( Sender.Write(sMessage) == sMessage.size() );
			REQUIRE ( Sender.Flush() );
			Sent.push_back(sCompressed.size());
		}

		Sender.Close();
	}

	std::size_t iLimit         { 0     };
	bool        bAskedForMore  { false };

	boost::iostreams::filtering_istream Chain;
	Chain.push(dekaf2::iostreams::immediate_input_filter<Filter>());
	Chain.push(SentSoFarSource(sCompressed, &iLimit, &bAskedForMore));

	KString sRead;
	char    Buffer[777];

	INFO ( sEncoding );

	for (std::size_t iMessage = 0; iMessage < Messages.size(); ++iMessage)
	{
		sPlain += Messages[iMessage];
		iLimit  = Sent[iMessage];

		while (sRead.size() < sPlain.size())
		{
			auto iRead = kReadSome(Chain, Buffer, sizeof(Buffer));

			if (!iRead)
			{
				break;
			}

			sRead.append(Buffer, iRead);
		}

		INFO  ( iMessage );
		CHECK ( sRead.size() == sPlain.size() );
		CHECK ( (sRead == sPlain) );
		CHECK_FALSE ( bAskedForMore );

		if (bAskedForMore)
		{
			// the stream has ended for the reader
			return;
		}
	}

	// the end of the stream
	iLimit = sCompressed.size();

	CHECK ( kReadSome(Chain, Buffer, sizeof(Buffer)) == 0 );
	CHECK_FALSE ( bAskedForMore );

} // CheckMessages

} // end of anonymous namespace

TEST_CASE("immediate_input_filter")
{
	SECTION("the output is the same as that of the wrapped filter")
	{
		CheckSameOutput<dekaf2::iostreams::gzip_members_decompressor>  (Compression::GZIP);
		CheckSameOutput<boost::iostreams::zlib_decompressor>  (Compression::ZLIB);
		CheckSameOutput<boost::iostreams::bzip2_decompressor> (Compression::BZIP2);
#ifdef DEKAF2_HAS_LIBLZMA
		CheckSameOutput<lzmaiostreams::lzma_decompressor>     (Compression::LZMA);
#endif
#ifdef DEKAF2_HAS_LIBZSTD
		CheckSameOutput<dekaf2::iostreams::zstd_decompressor> (Compression::ZSTD);
#endif
#ifdef DEKAF2_HAS_LIBBROTLI
		CheckSameOutput<dekaf2::iostreams::brotli_decompressor>(Compression::BROTLI);
#endif
	}

	SECTION("output is handed on before the next piece of the input is read")
	{
		// two gzip members, each with one line, and each in a piece of its own
		auto sFirst      = Compress("the first line\n",  Compression::GZIP);
		auto sSecond     = Compress("the second line\n", Compression::GZIP);
		auto sCompressed = sFirst + sSecond;

		std::size_t iReads { 0 };

		boost::iostreams::filtering_istream Chain;
		Chain.push(dekaf2::iostreams::immediate_input_filter<dekaf2::iostreams::gzip_members_decompressor>());
		Chain.push(PieceSource(sCompressed, { sFirst.size(), sSecond.size() }, &iReads));

		char Buffer[100];

		auto iRead = kReadSome(Chain, Buffer, sizeof(Buffer));

		CHECK ( KStringView(Buffer, iRead) == "the first line\n" );
		// the second piece was not asked for
		CHECK ( iReads == 1 );

		iRead = kReadSome(Chain, Buffer, sizeof(Buffer));

		CHECK ( KStringView(Buffer, iRead) == "the second line\n" );
		CHECK ( iReads >= 2 );

		CHECK ( kReadSome(Chain, Buffer, sizeof(Buffer)) == 0 );
	}

	SECTION("every flushed message of a stream is handed on in full without waiting for more")
	{
		CheckMessages<dekaf2::iostreams::gzip_members_decompressor>("gzip");
		CheckMessages<boost::iostreams::zlib_decompressor>         ("deflate");
#ifdef DEKAF2_HAS_LIBLZMA
		CheckMessages<lzmaiostreams::lzma_decompressor>            ("xz");
#endif
#ifdef DEKAF2_HAS_LIBZSTD
		CheckMessages<dekaf2::iostreams::zstd_decompressor>        ("zstd");
#endif
#ifdef DEKAF2_HAS_LIBBROTLI
		CheckMessages<dekaf2::iostreams::brotli_decompressor>      ("br");
#endif
	}

	SECTION("several gzip members")
	{
		auto sFirst  = MakeData(5000);
		auto sSecond = MakeData(1);
		auto sThird  = MakeData(70000);

		auto sCompressed = Compress(sFirst,  Compression::GZIP)
		                 + Compress("",      Compression::GZIP)
		                 + Compress(sSecond, Compression::GZIP)
		                 + Compress(sThird,  Compression::GZIP);

		auto sPlain = sFirst + sSecond + sThird;

		for (const auto& Pieces : std::vector<std::vector<std::size_t>>{ { 1 }, { 3, 7, 1 }, { 300 }, { 4096 }, { 100000 } })
		{
			INFO ( Pieces.front() );

			using Filter = dekaf2::iostreams::gzip_members_decompressor;

			CHECK ( Decompress<Filter>(sCompressed, Pieces, true,  false) == sPlain );
			CHECK ( Decompress<Filter>(sCompressed, Pieces, true,  true ) == sPlain );
			// as without the immediate filter
			CHECK ( Decompress<Filter>(sCompressed, Pieces, false, true ) == sPlain );
		}
	}

	SECTION("the gzip decompressor fails where the one of boost::iostreams fails")
	{
		auto sPlain      = MakeData(20000);
		auto sCompressed = Compress(sPlain, Compression::GZIP);

		using Filter = dekaf2::iostreams::gzip_members_decompressor;
		using Boost  = boost::iostreams::gzip_decompressor;

		// cut off, a damaged byte in the header, in the data and in the checksum, and
		// bytes that are no gzip member after the end
		std::vector<KString> Broken;

		for (std::size_t iCut : { std::size_t(0), std::size_t(1), std::size_t(10), sCompressed.size() / 2, sCompressed.size() - 1 })
		{
			Broken.push_back(sCompressed.substr(0, iCut));
		}

		for (std::size_t iAt : { std::size_t(0), std::size_t(2), sCompressed.size() / 2, sCompressed.size() - 6 })
		{
			Broken.push_back(sCompressed);
			Broken.back()[iAt] = static_cast<char>(Broken.back()[iAt] ^ 0x55);
		}

		Broken.push_back(sCompressed + "trailing bytes");

		for (const auto& sBroken : Broken)
		{
			bool bFailedImmediate { false };
			bool bFailedBoost     { false };

			auto sImmediate = Decompress<Filter>(sBroken, { 300 },    true,  true, &bFailedImmediate);
			auto sBoost     = Decompress<Boost> (sBroken, { 100000 }, false, true, &bFailedBoost);

			INFO ( &sBroken - Broken.data() );
			INFO ( sBroken.size() );

			CHECK ( bFailedImmediate );
			CHECK ( bFailedBoost     );
			CHECK ( sImmediate.size() <= sPlain.size() );
		}

		// and both take the whole input
		bool bFailed { false };

		CHECK ( Decompress<Filter>(sCompressed, { 300 }, true, true, &bFailed) == sPlain );
		CHECK ( bFailed == false );

		// A wrong size in the footer is the one difference: zlib checks it, the gzip
		// decompressor of boost::iostreams does not.
		KString sWrongSize = sCompressed;
		sWrongSize[sWrongSize.size() - 2] = static_cast<char>(sWrongSize[sWrongSize.size() - 2] ^ 0x55);

		bool bFailedBoost { false };

		Decompress<Filter>(sWrongSize, { 300 },    true,  true, &bFailed);
		Decompress<Boost> (sWrongSize, { 100000 }, false, true, &bFailedBoost);

		CHECK ( bFailed );
		CHECK ( bFailedBoost == false );
	}

	SECTION("input that ends too early or is damaged ends as with the wrapped filter")
	{
		CheckBrokenInput<dekaf2::iostreams::gzip_members_decompressor>  (Compression::GZIP);
		CheckBrokenInput<boost::iostreams::zlib_decompressor>  (Compression::ZLIB);
		CheckBrokenInput<boost::iostreams::bzip2_decompressor> (Compression::BZIP2);
#ifdef DEKAF2_HAS_LIBLZMA
		CheckBrokenInput<lzmaiostreams::lzma_decompressor>     (Compression::LZMA);
#endif
#ifdef DEKAF2_HAS_LIBZSTD
		CheckBrokenInput<dekaf2::iostreams::zstd_decompressor> (Compression::ZSTD);
#endif
#ifdef DEKAF2_HAS_LIBBROTLI
		CheckBrokenInput<dekaf2::iostreams::brotli_decompressor>(Compression::BROTLI);
#endif
	}
}
