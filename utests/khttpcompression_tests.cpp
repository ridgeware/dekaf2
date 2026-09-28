#include "catch.hpp"

#include <dekaf2/http/protocol/khttpcompression.h>

using namespace dekaf2;

TEST_CASE("KHTTPCompression")
{
	SECTION("Construct")
	{
		{
			KHTTPCompression HTTPComp("x-gzip");
			CHECK ( HTTPComp.GetCompression() == KHTTPCompression::GZIP );
		}
		{
			KHTTPCompression HTTPComp("z-gzip");
			CHECK ( HTTPComp == KHTTPCompression::NONE );
		}
		{
			KHTTPCompression HTTPComp("bzip2,superflat,gzip, deflate");
			CHECK ( HTTPComp == KHTTPCompression::ZLIB );
		}
#ifdef DEKAF2_HAS_LIBZSTD
		{
			KHTTPCompression HTTPComp("xyz,myzip, bzip2, lzma, zstd, gzip, deflate, br");
			CHECK ( HTTPComp == KHTTPCompression::ZSTD );
		}
#endif
#ifdef DEKAF2_HAS_LIBLZMA
		{
			KHTTPCompression HTTPComp("lzma");
			CHECK ( HTTPComp == KHTTPCompression::LZMA );
		}
		{
			KHTTPCompression HTTPComp("xz");
			CHECK ( HTTPComp == KHTTPCompression::XZ );
		}
#endif
	}

	SECTION("Assign")
	{
		KHTTPCompression HTTPComp;
		HTTPComp = "x-gzip";
		CHECK ( HTTPComp == KHTTPCompression::GZIP );
		HTTPComp = "z-gzip";
		CHECK ( HTTPComp == KHTTPCompression::NONE );
		HTTPComp = "bzip2,superflat,gzip, deflate";
		CHECK ( HTTPComp == KHTTPCompression::ZLIB );
#ifdef DEKAF2_HAS_LIBZSTD
		HTTPComp = "xyz,myzip, bzip2, lzma, zstd, gzip, deflate, br";
		CHECK ( HTTPComp == KHTTPCompression::ZSTD );
#endif
#ifdef DEKAF2_HAS_LIBLZMA
		HTTPComp = "lzma";
		CHECK ( HTTPComp == KHTTPCompression::LZMA );
		HTTPComp = "lzma,xz";
		CHECK ( HTTPComp == KHTTPCompression::XZ );
#endif
	}

	SECTION("GetBestSupportedCompression")
	{
		CHECK ( KHTTPCompression::GetBestSupportedCompressor("bzip2,superflat,gzip, deflate") == KHTTPCompression::ZLIB );
#ifdef DEKAF2_HAS_LIBZSTD
		CHECK ( KHTTPCompression::GetBestSupportedCompressor("xyz,myzip, bzip2, lzma, zstd, gzip, deflate, br") == KHTTPCompression::ZSTD );
#endif
#ifdef DEKAF2_HAS_LIBLZMA
		CHECK ( KHTTPCompression::GetBestSupportedCompressor("bzip2, xz") == KHTTPCompression::XZ );
#endif
#ifdef DEKAF2_HAS_LIBBROTLI
		CHECK ( KHTTPCompression::GetBestSupportedCompressor("bzip2, gzip, deflate, br") == KHTTPCompression::BROTLI );
#endif
	}

	SECTION("quality values")
	{
		// q=0 means "not acceptable", other quality values do not change our ranking
		CHECK ( KHTTPCompression::GetBestSupportedCompressor("gzip;q=0, deflate") == KHTTPCompression::ZLIB );
		CHECK ( KHTTPCompression::GetBestSupportedCompressor("deflate;q=0, gzip") == KHTTPCompression::GZIP );
		CHECK ( KHTTPCompression::GetBestSupportedCompressor("deflate;q=0.0, gzip;q=0") == KHTTPCompression::NONE );
		CHECK ( KHTTPCompression::GetBestSupportedCompressor("gzip;q=1.0, deflate;q=0.1") == KHTTPCompression::ZLIB );
		CHECK ( KHTTPCompression::GetBestSupportedCompressor("gzip ;q=0.5") == KHTTPCompression::GZIP );
#ifdef DEKAF2_HAS_LIBZSTD
		CHECK ( KHTTPCompression::GetBestSupportedCompressor("zstd;q=0, gzip") == KHTTPCompression::GZIP );
#endif

		// the single value constructor uses the same rules
		KHTTPCompression HTTPComp;
		HTTPComp = "gzip;q=0.8";
		CHECK ( HTTPComp == KHTTPCompression::GZIP );
		HTTPComp = "gzip;q=0";
		CHECK ( HTTPComp == KHTTPCompression::NONE );
	}

	SECTION("GetAcceptedCompressors")
	{
		CHECK ( KHTTPCompression::GetAcceptedCompressors("") == 0 );
		CHECK ( KHTTPCompression::GetAcceptedCompressors("*") == 0 );
		CHECK ( KHTTPCompression::GetAcceptedCompressors("identity, superflat") == 0 );
		CHECK ( KHTTPCompression::GetAcceptedCompressors("gzip, deflate;q=0, bzip2") == (KHTTPCompression::GZIP | KHTTPCompression::BZIP2) );
		CHECK ( KHTTPCompression::GetAcceptedCompressors("x-gzip") == KHTTPCompression::GZIP );
#ifdef DEKAF2_HAS_LIBLZMA
		// lzma is never accepted from an accept-encoding list
		CHECK ( KHTTPCompression::GetAcceptedCompressors("lzma, xz") == KHTTPCompression::XZ );
#endif
#if defined(DEKAF2_HAS_LIBZSTD) && defined(DEKAF2_HAS_LIBBROTLI)
		CHECK ( KHTTPCompression::GetAcceptedCompressors("gzip, deflate, br, zstd") ==
		        (KHTTPCompression::ZSTD | KHTTPCompression::BROTLI | KHTTPCompression::ZLIB | KHTTPCompression::GZIP) );
#endif

		auto comp = KHTTPCompression::GetPermittedCompressors();
		KHTTPCompression::SetPermittedCompressors("gzip");
		CHECK ( KHTTPCompression::GetAcceptedCompressors("gzip, deflate, br, zstd") == KHTTPCompression::GZIP );
		KHTTPCompression::SetPermittedCompressors(comp);
	}

	SECTION("GetBestCompressor")
	{
		CHECK ( KHTTPCompression::GetBestCompressor(KHTTPCompression::COMP{}) == KHTTPCompression::NONE );
		CHECK ( KHTTPCompression::GetBestCompressor(KHTTPCompression::NONE) == KHTTPCompression::NONE );
		CHECK ( KHTTPCompression::GetBestCompressor(KHTTPCompression::GZIP | KHTTPCompression::BZIP2) == KHTTPCompression::GZIP );
		CHECK ( KHTTPCompression::GetBestCompressor(KHTTPCompression::GZIP | KHTTPCompression::ZLIB) == KHTTPCompression::ZLIB );
#if defined(DEKAF2_HAS_LIBZSTD) && defined(DEKAF2_HAS_LIBBROTLI)
		CHECK ( KHTTPCompression::GetBestCompressor(KHTTPCompression::BROTLI | KHTTPCompression::ZSTD | KHTTPCompression::GZIP) == KHTTPCompression::ZSTD );
		CHECK ( KHTTPCompression::GetBestCompressor(KHTTPCompression::BROTLI | KHTTPCompression::GZIP) == KHTTPCompression::BROTLI );
#endif
	}

	SECTION("GetBestSupportedCompression2")
	{
		auto comp = KHTTPCompression::GetPermittedCompressors();
		KHTTPCompression::SetPermittedCompressors("deflate, gzip,bzip2, br ");
#ifdef DEKAF2_HAS_LIBBROTLI
		CHECK ( KHTTPCompression::GetCompressors() == "br,deflate,gzip,bzip2" );
#else
		CHECK ( KHTTPCompression::GetCompressors() == "deflate,gzip,bzip2" );
#endif
		CHECK ( KHTTPCompression::GetBestSupportedCompressor("bzip2,superflat,gzip, deflate") == KHTTPCompression::ZLIB );
#ifdef DEKAF2_HAS_LIBBROTLI
		CHECK ( KHTTPCompression::GetBestSupportedCompressor("xyz,myzip, bzip2, lzma, zstd, gzip, deflate, br") == KHTTPCompression::BROTLI );
#else
		CHECK ( KHTTPCompression::GetBestSupportedCompressor("xyz,myzip, bzip2, lzma, zstd, gzip, deflate, br") == KHTTPCompression::ZLIB );
#endif
		CHECK ( KHTTPCompression::GetBestSupportedCompressor("bzip2, xz") == KHTTPCompression::BZIP2 );
#ifdef DEKAF2_HAS_LIBBROTLI
		CHECK ( KHTTPCompression::GetBestSupportedCompressor("bzip2, gzip, deflate, br") == KHTTPCompression::BROTLI );
#endif
		KHTTPCompression::SetPermittedCompressors(comp);
	}

	SECTION("ToString")
	{
		CHECK ( KHTTPCompression::ToString(KHTTPCompression::NONE  ) == ""        );
		CHECK ( KHTTPCompression::ToString(KHTTPCompression::GZIP  ) == "gzip"    );
		CHECK ( KHTTPCompression::ToString(KHTTPCompression::ZLIB  ) == "deflate" );
		CHECK ( KHTTPCompression::ToString(KHTTPCompression::BZIP2 ) == "bzip2"   );
#ifdef DEKAF2_HAS_LIBZSTD
		CHECK ( KHTTPCompression::ToString(KHTTPCompression::ZSTD  ) == "zstd"    );
#endif
#ifdef DEKAF2_HAS_LIBLZMA
		CHECK ( KHTTPCompression::ToString(KHTTPCompression::LZMA  ) == "lzma"    );
#endif
#ifdef DEKAF2_HAS_LIBBROTLI
		CHECK ( KHTTPCompression::ToString(KHTTPCompression::BROTLI) == "br"      );
#endif
	}
}

