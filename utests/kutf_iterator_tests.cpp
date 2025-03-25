#include "catch.hpp"

#include <dekaf2/kutf_iterator.h>
#include <dekaf2/kstring.h>
#include <vector>

using namespace dekaf2;

TEST_CASE("UTFIterator") {

	SECTION("const iterator")
	{
		KString str("abcäöüabc");
		// the KString template argument is only needed for C++ < 17
		kutf::ConstIterator<KString> it(str, false);
		kutf::ConstIterator<KString> ie(str, true);
		CHECK (    it != ie  );
		CHECK ( uint32_t(*it++) == 'a' );
		CHECK (    it != ie  );
		CHECK ( uint32_t(*it)   == 'b' );
		CHECK ( uint32_t(*++it) == 'c' );
		++it;
		CHECK (    it != ie  );
		CHECK ( uint32_t(*it++) == 0xe4);
		CHECK (    it != ie  );
		CHECK ( uint32_t(*it++) == 0xf6);
		CHECK (    it != ie  );
		CHECK ( uint32_t(*it++) == 0xfc);
		CHECK (    it != ie  );
		CHECK ( uint32_t(*it++) == 'a' );
		CHECK (    it != ie  );
		CHECK ( uint32_t(*it++) == 'b' );
		CHECK (    it != ie  );
		CHECK ( uint32_t(*it++) == 'c' );
		CHECK (    it == ie  );
		CHECK ( uint32_t(*--it) == 'c' );
		CHECK ( uint32_t(*--it) == 'b' );
		CHECK ( uint32_t(*--it) == 'a' );
		CHECK ( uint32_t(*--it) == 0xfc);
		CHECK ( uint32_t(*--it) == 0xf6);
		CHECK ( uint32_t(*--it) == 0xe4);
		CHECK ( uint32_t(*--it) == 'c' );
		CHECK ( uint32_t(*--it) == 'b' );
		CHECK ( uint32_t(*--it) == 'a' );
// TBD
//		CHECK ( kutf::Increment(it, ie, 4) );
//		CHECK ( uint32_t(*it) == 0xf6);
//
//		CHECK ( kutf::Decrement(it, ie, 3) );
//		CHECK ( uint32_t(*it) == 'b' );
	}

	SECTION("iterator")
	{
		KString str("abcäöüabc");
		kutf::Iterator<KString> it(str, false);
		kutf::Iterator<KString> ie(str, true);
		CHECK (    it != ie  );
		CHECK ( uint32_t(*it++) == 'a' );
		CHECK (    it != ie  );
		CHECK ( uint32_t(*it)   == 'b' );
		CHECK ( uint32_t(*++it) == 'c' );
		++it;
		CHECK (    it != ie  );
		CHECK ( uint32_t(*it++) == 0xe4);
		CHECK (    it != ie  );
		CHECK ( uint32_t(*it++) == 0xf6);
		CHECK (    it != ie  );
		CHECK ( uint32_t(*it++) == 0xfc);
		CHECK (    it != ie  );
		CHECK ( uint32_t(*it++) == 'a' );
		CHECK (    it != ie  );
		CHECK ( uint32_t(*it++) == 'b' );
		CHECK ( uint32_t(*--it) == 'b' );
		++it;
		CHECK (    it != ie  );
		CHECK ( uint32_t(*it++) == 'c' );
		CHECK (    it == ie  );
		CHECK ( uint32_t(*--it) == 'c' );
		CHECK ( uint32_t(*--it) == 'b' );
		CHECK ( uint32_t(*--it) == 'a' );
		CHECK ( uint32_t(*--it) == 0xfc);
		CHECK ( uint32_t(*--it) == 0xf6);
		CHECK ( uint32_t(*--it) == 0xe4);
		CHECK ( uint32_t(*--it) == 'c' );
		CHECK ( uint32_t(*--it) == 'b' );
		CHECK ( uint32_t(*--it) == 'a' );
// TBD
//		CHECK ( kutf::Increment(it, ie, 4) );
//		CHECK ( uint32_t(*it) == 0xf6);
//
//		CHECK ( kutf::Decrement(it, ie, 3) );
//		CHECK ( uint32_t(*it) == 'b' );
	}

	SECTION("modifying iterator")
	{
		KString str("abcäöüabc");
		kutf::Iterator<KString> it(str, false);
		kutf::Iterator<KString> ie(str, true);
		CHECK (    it != ie  );
		CHECK ( uint32_t(*it++) == 'a' );
		CHECK (    it != ie  );
		CHECK ( uint32_t(*it)   == 'b' );
		*it = 'a';
		CHECK ( uint32_t(*it)   == 'a' );
		CHECK ( uint32_t(*++it) == 'c' );
		++it;
		CHECK (    it != ie  );
		CHECK ( uint32_t(*it++) == 0xe4);
		CHECK (    it != ie  );
		CHECK ( uint32_t(*it)   == 0xf6);
		*it = 'o';
		++it;
		CHECK (    it != ie  );
		CHECK ( uint32_t(*it++) == 0xfc);
		CHECK (    it != ie  );
		CHECK ( uint32_t(*it)   == 'a' );
		CHECK (    it != ie  );
		*++it = 0xe4;
		CHECK ( uint32_t(*it++) == 0xe4);
		CHECK (    it != ie  );
		CHECK ( uint32_t(*it++) == 'c' );
		CHECK (    it == ie  );
		CHECK ( str == "aacäoüaäc" );
		CHECK ( uint32_t(*--it) == 'c' );
		CHECK ( uint32_t(*--it) == 0xe4);
		CHECK ( uint32_t(*--it) == 'a' );
		CHECK ( uint32_t(*--it) == 0xfc);
		CHECK ( uint32_t(*--it) == 'o' );
		CHECK ( uint32_t(*--it) == 0xe4);
		CHECK ( uint32_t(*--it) == 'c' );
		CHECK ( uint32_t(*--it) == 'a' );
		CHECK ( uint32_t(*--it) == 'a' );
	}
}

