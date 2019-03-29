#include "catch.hpp"

#include <dekaf2/kcompression.h>

#include <iostream>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/copy.hpp>
#include <dekaf2/kstringstream.h>

using namespace dekaf2;

TEST_CASE("KCompression") {

	SECTION("KGZip")
	{

		KString s("This is the content of a string");
		KString o;
		KString o2;
		KGZip gzip(o);
		gzip.Write(s);
		gzip.Close();

		CHECK ( !o.empty() );
		CHECK ( s != o );

		KGUnZip gunzip(o);
		gunzip.Read(o2);
		gunzip.Close();

		CHECK ( s == o2 );
	}

	SECTION("KBZip2")
	{

		KString s("This is the content of a string");
		KString o;
		KString o2;
		KBZip2 bzip(o);
		bzip.Write(s);
		bzip.Close();

		CHECK ( !o.empty() );
		CHECK ( s != o );

		KBUnZip2 bunzip(o);
		bunzip.Read(o2);
		bunzip.Close();

		CHECK ( s == o2 );
	}

	SECTION("KZlib")
	{

		KString s("This is the content of a string");
		KString o;
		KString o2;
		KZlib zlib(o);
		zlib.Write(s);
		zlib.Close();

		CHECK ( !o.empty() );
		CHECK ( s != o );

		KUnZlib unzlib(o);
		unzlib.Read(o2);
		unzlib.Close();

		CHECK ( s == o2 );
	}

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
		if (!gzip.Write(s2))
		{
			CHECK ( false == true );
		}
		gzip.Close();

		CHECK ( !o.empty() );
		CHECK ( o3 != o );

		KGUnZip gunzip(o);
		gunzip.Read(o2);
		gunzip.Close();

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
		auto l1 = o.size();
		out.write(s2.data(), s2.size());
		out.reset();
		auto l2 = o.size();

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



}

