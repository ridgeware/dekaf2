#include "catch.hpp"

#include <dekaf2/kcompression.h>

#include <iostream>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <dekaf2/kstringstream.h>

using namespace dekaf2;

TEST_CASE("KCompression") {

	SECTION("KGZip")
	{

		KString s("This is the content of a string");
		KString o;
		KString o2;
		KGZip comp(o, 50);
		comp.Write(s);
		comp.close();

		CHECK ( !o.empty() );
		CHECK ( s != o );

		KUnGZip uncomp(o);
		uncomp.ReadRemaining(o2);
		uncomp.close();

		CHECK ( s == o2 );
	}

	SECTION("KBZip2")
	{

		KString s("This is the content of a string");
		KString o;
		KString o2;
		KBZip2 comp(o);
		comp.Write(s);
		comp.close();

		CHECK ( !o.empty() );
		CHECK ( s != o );

		KUnBZip2 uncomp(o);
		uncomp.ReadRemaining(o2);
		uncomp.close();

		CHECK ( s == o2 );
	}

	SECTION("KZlib")
	{

		KString s("This is the content of a string");
		KString o;
		KString o2;
		KZlib comp(o);
		comp.Write(s);
		comp.close();

		CHECK ( !o.empty() );
		CHECK ( s != o );

		KUnZlib uncomp(o);
		uncomp.ReadRemaining(o2);
		uncomp.close();

		CHECK ( s == o2 );
	}

#ifdef DEKAF2_HAS_LIBZSTD
	SECTION("KZSTD")
	{

		KString s("This is the content of a string");
		KString o;
		KString o2;
		KZSTD comp(o);
		comp.Write(s);
		comp.close();

		CHECK ( !o.empty() );
		CHECK ( s != o );

		KUnZSTD uncomp(o);
		uncomp.ReadRemaining(o2);
		uncomp.close();

		CHECK ( s == o2 );
	}
#endif

#ifdef DEKAF2_HAS_LIBLZMA
	SECTION("KLZMA")
	{

		KString s("This is the content of a string");
		KString o;
		KString o2;
		KLZMA comp(o);
		comp.Write(s);
		comp.close();

		CHECK ( !o.empty() );
		CHECK ( s != o );

		KUnLZMA uncomp(o);
		uncomp.ReadRemaining(o2);
		uncomp.close();

		CHECK ( s == o2 );
	}
#endif

#ifdef DEKAF2_HAS_LIBBROTLI
	SECTION("KBrotli")
	{

		KString s("This is the content of a string");
		KString o;
		KString o2;
		KBrotli comp(o);
		comp.Write(s);
		comp.close();

		CHECK ( !o.empty() );
		CHECK ( s != o );

		KUnBrotli uncomp(o);
		uncomp.ReadRemaining(o2);
		uncomp.close();

		CHECK ( s == o2 );
	}
#endif


	SECTION("KGZip multiple chunks")
	{

		KString s("This is the content of a string.");
		KString s2(" And this is another content of a string.");
		KString o;
		KString o2;
		KString o3;
		o3 = s;
		o3 += s2;
		KGZip gzip(o);
		gzip.Write(s);
		if (!gzip.Write(s2).Good())
		{
			CHECK ( false == true );
		}
		gzip.close();

		CHECK ( !o.empty() );
		CHECK ( o3 != o );

		KUnGZip gunzip(o);
		gunzip.ReadRemaining(o2);
		gunzip.close();

		CHECK ( o3 == o2 );
	}

	SECTION("manual boost")
	{

		KString s("This is the content of a string.");
		KString s2(" And this is another content of a string.");
		KString o;
		KString o2;
		KString o3;
		o3 = s;
		o3 += s2;

		KOStringStream compressedstream(o);

		boost::iostreams::filtering_ostream out;
		out.push(boost::iostreams::gzip_compressor());
		out.push(compressedstream);

		KIStringStream inputstream(s);

		out << inputstream.rdbuf();
		out.write(s2.data(), s2.size());
		out.reset();

		CHECK ( !o.empty() );
		CHECK ( o3 != o );

		KOStringStream uncompressedstream(o2);

		boost::iostreams::filtering_ostream out2;
		out2.push(boost::iostreams::gzip_decompressor());
		out2.push(uncompressedstream);

		out2.write(o.data(), o.size());
		out2.reset();
		CHECK ( o3 == o2 );
	}

	SECTION("suffixes etc")
	{
		CHECK ( KCompress::GetCompressionMethodFromFilename("test.out") == KCompress::NONE   );
		CHECK ( KCompress::GetCompressionMethodFromFilename("test.gz" ) == KCompress::GZIP   );
		CHECK ( KCompress::GetCompressionMethodFromFilename("test.z"  ) == KCompress::ZLIB   );
		CHECK ( KCompress::GetCompressionMethodFromFilename("test.bz2") == KCompress::BZIP2  );
#ifdef DEKAF2_HAS_LIBZSTD
		CHECK ( KCompress::GetCompressionMethodFromFilename("test.zst") == KCompress::ZSTD   );
#endif
#ifdef DEKAF2_HAS_LIBLZMA
		CHECK ( KCompress::GetCompressionMethodFromFilename("test.xz" ) == KCompress::LZMA   );
#endif
#ifdef DEKAF2_HAS_LIBBROTLI
		CHECK ( KCompress::GetCompressionMethodFromFilename("test.br" ) == KCompress::BROTLI );
#endif

		CHECK ( KCompress::ToString(KCompress::NONE  ) == ""      );
		CHECK ( KCompress::ToString(KCompress::GZIP  ) == "gzip"  );
		CHECK ( KCompress::ToString(KCompress::ZLIB  ) == "zlib"  );
		CHECK ( KCompress::ToString(KCompress::BZIP2 ) == "bzip2" );
#ifdef DEKAF2_HAS_LIBZSTD
		CHECK ( KCompress::ToString(KCompress::ZSTD  ) == "zstd"  );
#endif
#ifdef DEKAF2_HAS_LIBLZMA
		CHECK ( KCompress::ToString(KCompress::LZMA  ) == "lzma"  );
#endif
#ifdef DEKAF2_HAS_LIBBROTLI
		CHECK ( KCompress::ToString(KCompress::BROTLI) == "br"    );
#endif

		CHECK ( KCompress::DefaultExtension(KCompress::NONE  ) == ""    );
		CHECK ( KCompress::DefaultExtension(KCompress::GZIP  ) == "gz"  );
		CHECK ( KCompress::DefaultExtension(KCompress::ZLIB  ) == "z"   );
		CHECK ( KCompress::DefaultExtension(KCompress::BZIP2 ) == "bz2" );
#ifdef DEKAF2_HAS_LIBZSTD
		CHECK ( KCompress::DefaultExtension(KCompress::ZSTD  ) == "zst" );
#endif
#ifdef DEKAF2_HAS_LIBLZMA
		CHECK ( KCompress::DefaultExtension(KCompress::LZMA  ) == "xz"  );
#endif
#ifdef DEKAF2_HAS_LIBBROTLI
		CHECK ( KCompress::DefaultExtension(KCompress::BROTLI) == "br"  );
#endif

		CHECK ( KCompress::ScaleLevel(  0,  9) ==  1 );
		CHECK ( KCompress::ScaleLevel(  1,  9) ==  1 );
		CHECK ( KCompress::ScaleLevel( 10,  9) ==  1 );
		CHECK ( KCompress::ScaleLevel( 56,  9) ==  5 );
		CHECK ( KCompress::ScaleLevel(100,  9) ==  9 );
		CHECK ( KCompress::ScaleLevel(200,  9) ==  9 );

		CHECK ( KCompress::ScaleLevel(  0, 19) ==  1 );
		CHECK ( KCompress::ScaleLevel(  1, 19) ==  1 );
		CHECK ( KCompress::ScaleLevel( 11, 19) ==  2 );
		CHECK ( KCompress::ScaleLevel( 56, 19) == 10 );
		CHECK ( KCompress::ScaleLevel(100, 19) == 19 );
		CHECK ( KCompress::ScaleLevel(200, 19) == 19 );
	}
}

