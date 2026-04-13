#include "catch.hpp"

#include <dekaf2/core/strings/kutf_iterator.h>
#include <dekaf2/core/strings/kstring.h>
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

	SECTION("forward iterator")
	{
		KString str("abcäöüabc");
		kutf::ConstForwardIterator<KString::const_iterator> it(str.cbegin(), str.cend());
		kutf::ConstForwardIterator<KString::const_iterator> ie(str.cend(),  str.cend());
		CHECK (    it != ie  );
		CHECK ( uint32_t(*it++) == 'a' );
		CHECK (    it != ie  );
		CHECK ( uint32_t(*it++) == 'b' );
		CHECK ( uint32_t(*it++) == 'c' );
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
	}

	SECTION("forward iterator empty string")
	{
		KStringView str;
		kutf::ConstForwardIterator<KStringView::const_iterator> it(str.cbegin(), str.cend());
		kutf::ConstForwardIterator<KStringView::const_iterator> ie(str.cend(),  str.cend());
		CHECK ( it == ie );
	}

	SECTION("forward iterator single char")
	{
		KStringView str("X");
		kutf::ConstForwardIterator<KStringView::const_iterator> it(str.cbegin(), str.cend());
		kutf::ConstForwardIterator<KStringView::const_iterator> ie(str.cend(),  str.cend());
		CHECK (    it != ie  );
		CHECK ( uint32_t(*it++) == 'X' );
		CHECK (    it == ie  );
	}

	SECTION("CodepointRange with KString")
	{
		KString str("héllo");
		std::vector<uint32_t> codepoints;

		for (auto cp : str.Codepoints())
		{
			codepoints.push_back(cp);
		}

		REQUIRE ( codepoints.size() == 5 );
		CHECK ( codepoints[0] == 'h'  );
		CHECK ( codepoints[1] == 0xe9 );
		CHECK ( codepoints[2] == 'l'  );
		CHECK ( codepoints[3] == 'l'  );
		CHECK ( codepoints[4] == 'o'  );
	}

	SECTION("CodepointRange with KStringView")
	{
		KStringView str("äöü");
		std::vector<uint32_t> codepoints;

		for (auto cp : str.Codepoints())
		{
			codepoints.push_back(cp);
		}

		REQUIRE ( codepoints.size() == 3 );
		CHECK ( codepoints[0] == 0xe4 );
		CHECK ( codepoints[1] == 0xf6 );
		CHECK ( codepoints[2] == 0xfc );
	}

	SECTION("CodepointRange with KStringViewZ")
	{
		KStringViewZ str("a€z");
		std::vector<uint32_t> codepoints;

		for (auto cp : str.Codepoints())
		{
			codepoints.push_back(cp);
		}

		REQUIRE ( codepoints.size() == 3 );
		CHECK ( codepoints[0] == 'a'    );
		CHECK ( codepoints[1] == 0x20ac );
		CHECK ( codepoints[2] == 'z'    );
	}

	SECTION("CodepointRange empty string")
	{
		KStringView str;
		std::vector<uint32_t> codepoints;

		for (auto cp : str.Codepoints())
		{
			codepoints.push_back(cp);
		}

		CHECK ( codepoints.empty() );
	}

	SECTION("CodepointRange 4-byte codepoints")
	{
		KString str("a\xF0\x9F\x98\x80z");
		std::vector<uint32_t> codepoints;

		for (auto cp : str.Codepoints())
		{
			codepoints.push_back(cp);
		}

		REQUIRE ( codepoints.size() == 3 );
		CHECK ( codepoints[0] == 'a'     );
		CHECK ( codepoints[1] == 0x1f600 );
		CHECK ( codepoints[2] == 'z'     );
	}
}

