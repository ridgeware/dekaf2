#include "catch.hpp"

#include <dekaf2/kcompression.h>

using namespace dekaf2;

TEST_CASE("KCompression") {

	SECTION("KGZip")
	{

		KString s("This is the content of a string");
		KString o;
		KString o2;
		KGZip gzip(o);
		gzip.Write(s);

		CHECK ( !o.empty() );
		CHECK ( s != o );

		KGUnZip gunzip(o2);
		gunzip.Write(o);

		CHECK ( s == o2 );
	}

	SECTION("KBZip2")
	{

		KString s("This is the content of a string");
		KString o;
		KString o2;
		KBZip2 bzip(o);
		bzip.Write(s);

		CHECK ( !o.empty() );
		CHECK ( s != o );

		KBUnZip2 bunzip(o2);
		bunzip.Write(o);

		CHECK ( s == o2 );
	}

	SECTION("KZlib")
	{

		KString s("This is the content of a string");
		KString o;
		KString o2;
		KZlib zlib(o);
		zlib.Write(s);

		CHECK ( !o.empty() );
		CHECK ( s != o );

		KUnZlib unzlib(o2);
		unzlib.Write(o);

		CHECK ( s == o2 );
	}

}

