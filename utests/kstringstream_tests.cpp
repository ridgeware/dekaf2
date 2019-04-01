#include "catch.hpp"

#include <dekaf2/kstringstream.h>
#include <dekaf2/kinstringstream.h>
#include <dekaf2/koutstringstream.h>
#include <vector>

using namespace dekaf2;

TEST_CASE("KStringStream") {

	SECTION("Read on KIStringStream")
	{

		KString s("This is the content of a string\n");
		KString o;
		KIStringStream istream(s);
		bool ok = std::getline(istream, o).good();
		// readline skips the LF, therefore we append it for the comparison
		o += '\n';
		CHECK( ok == true );
		CHECK( o == s );
		ok = std::getline(istream, o).good();
		CHECK( ok == false );
	}

	SECTION("Write on KOStringStream")
	{

		KString s("This is the content of a string\n");
		KString o;
		KOStringStream ostream(o);
		ostream.write(s.data(), s.size());
		CHECK( o == s );
	}

	SECTION("Move a KIStringStream")
	{
		KString s("This is the content of a string\n");
		KString o;
		KIStringStream istream(s);

		KIStringStream istream2("hello world");
		istream2 = std::move(istream);

		bool ok = std::getline(istream2, o).good();
		// readline skips the LF, therefore we append it for the comparison
		o += '\n';
		CHECK( ok == true );
		CHECK( o == s );
	}

	SECTION("Read on KStringStream")
	{

		KString s("This is the content of a string\n");
		KString o;
		KStringStream stream(s);
		bool ok = std::getline(stream, o).good();
		// readline skips the LF, therefore we append it for the comparison
		o += '\n';
		CHECK( ok == true );
		CHECK( o == s );
		ok = std::getline(stream, o).good();
		CHECK( ok == false );
	}

	SECTION("Write on KStringStream")
	{

		KString s("This is the content of a string\n");
		KString o;
		KStringStream stream(o);
		stream.write(s.data(), s.size());
		CHECK( o == s );
	}

	SECTION("Read and write on KStringStream")
	{
		KString sBuffer;
		KStringStream kss(sBuffer);
		CHECK ( kss.is_open() );
		kss.Write("hello world\n");
		kss.WriteLine("one more line");
		CHECK ( sBuffer == "hello world\none more line\n");
		KString sOut;
		kss.ReadLine(sOut);
		CHECK ( sOut == "hello world");
		kss.ReadRemaining(sOut);
		CHECK ( sOut == "one more line\n");
	}

	SECTION("Read and write on preloaded KStringStream")
	{
		KString sBuffer;
		KStringStream kss(sBuffer);
		CHECK ( kss.is_open() );
		kss.str("initial data\n");
		kss.Write("hello world\n");
		kss.WriteLine("one more line");
		CHECK ( sBuffer == "initial data\nhello world\none more line\n");
		KString sOut;
		kss.ReadLine(sOut);
		CHECK ( sOut == "initial data");
		kss.ReadLine(sOut);
		CHECK ( sOut == "hello world");
		kss.ReadRemaining(sOut);
		CHECK ( sOut == "one more line\n");
	}

	SECTION("Move a KStringStream")
	{
		KString s("This is the content of a string\n");
		KString o;
		KStringStream stream(s);

		KString empty;
		KStringStream stream2(empty);
		stream2 = std::move(stream);

		bool ok = std::getline(stream2, o).good();
		// readline skips the LF, therefore we append it for the comparison
		o += '\n';
		CHECK( ok == true );
		CHECK( o == s );
	}

}

