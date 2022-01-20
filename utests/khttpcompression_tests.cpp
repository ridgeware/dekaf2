#include "catch.hpp"

#include <dekaf2/khttpcompression.h>

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
		CHECK ( KHTTPCompression::GetBestSupportedCompression("bzip2,superflat,gzip, deflate") == KHTTPCompression::ZLIB );
#ifdef DEKAF2_HAS_LIBZSTD
		CHECK ( KHTTPCompression::GetBestSupportedCompression("xyz,myzip, bzip2, lzma, zstd, gzip, deflate, br") == KHTTPCompression::ZSTD );
#endif
#ifdef DEKAF2_HAS_LIBLZMA
		CHECK ( KHTTPCompression::GetBestSupportedCompression("bzip2, xz") == KHTTPCompression::XZ );
#endif
#ifdef DEKAF2_HAS_LIBBROTLI
		CHECK ( KHTTPCompression::GetBestSupportedCompression("bzip2, gzip, deflate, br") == KHTTPCompression::BROTLI );
#endif
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

