#include "catch.hpp"

#include <dekaf2/kstringstream.h>
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

}

