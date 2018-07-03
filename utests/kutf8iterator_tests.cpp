#include "catch.hpp"

#include <dekaf2/kutf8iterator.h>
#include <dekaf2/kstring.h>
#include <vector>

using namespace dekaf2;

TEST_CASE("UTF8Iterator") {

	SECTION("const iterator")
	{
		KString str("abcäöüabc");
		Unicode::UTF8ConstIterator it(str, false);
		Unicode::UTF8ConstIterator ie(str, true);
		CHECK (    it != ie  );
		CHECK ( *it++ == 'a' );
		CHECK (    it != ie  );
		CHECK (   *it == 'b' );
		CHECK ( *++it == 'c' );
		++it;
		CHECK (    it != ie  );
		CHECK ( *it++ == 0xe4);
		CHECK (    it != ie  );
		CHECK ( *it++ == 0xf6);
		CHECK (    it != ie  );
		CHECK ( *it++ == 0xfc);
		CHECK (    it != ie  );
		CHECK ( *it++ == 'a' );
		CHECK (    it != ie  );
		CHECK ( *it++ == 'b' );
		CHECK (    it != ie  );
		CHECK ( *it++ == 'c' );
		CHECK (    it == ie  );
	}

	SECTION("iterator")
	{
		KString str("abcäöüabc");
		Unicode::UTF8Iterator it(str, false);
		Unicode::UTF8Iterator ie(str, true);
		CHECK (    it != ie  );
		CHECK ( *it++ == 'a' );
		CHECK (    it != ie  );
		CHECK (   *it == 'b' );
		CHECK ( *++it == 'c' );
		++it;
		CHECK (    it != ie  );
		CHECK ( *it++ == 0xe4);
		CHECK (    it != ie  );
		CHECK ( *it++ == 0xf6);
		CHECK (    it != ie  );
		CHECK ( *it++ == 0xfc);
		CHECK (    it != ie  );
		CHECK ( *it++ == 'a' );
		CHECK (    it != ie  );
		CHECK ( *it++ == 'b' );
		CHECK (    it != ie  );
		CHECK ( *it++ == 'c' );
		CHECK (    it == ie  );
	}

	SECTION("modifying iterator")
	{
		KString str("abcäöüabc");
		Unicode::UTF8Iterator it(str, false);
		Unicode::UTF8Iterator ie(str, true);
		CHECK (    it != ie  );
		CHECK ( *it++ == 'a' );
		CHECK (    it != ie  );
		CHECK (   *it == 'b' );
		*it = 'a';
		CHECK (   *it == 'a' );
		CHECK ( *++it == 'c' );
		++it;
		CHECK (    it != ie  );
		CHECK ( *it++ == 0xe4);
		CHECK (    it != ie  );
		CHECK (   *it == 0xf6);
		*it++ = 'o';
		CHECK (    it != ie  );
		CHECK ( *it++ == 0xfc);
		CHECK (    it != ie  );
		CHECK ( *it++ == 'a' );
		CHECK (    it != ie  );
		*it = 0xe4;
		CHECK ( *it++ == 0xe4);
		CHECK (    it != ie  );
		CHECK ( *it++ == 'c' );
		CHECK (    it == ie  );
		CHECK ( str == "aacäoüaäc" );
	}

}

