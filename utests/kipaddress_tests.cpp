#include "catch.hpp"
#include <dekaf2/kipaddress.h>
#include <vector>


using namespace dekaf2;


TEST_CASE("KIPAddress")
{
	SECTION("kIsIPv6Address without braces")
	{
		std::vector<std::pair<KStringView, bool>> tests {{
			{ "", false },
			{ "01:23:45:67:89:ae:bf", false },
			{ "01:23:45:67:89:ae:bf:cd", true },
			{ "01:23:45:67:89:AE:BF:CD", true },
			{ "0123:2345:4567:6789:890a:a0b1:b1c2:c2d3", true },
			{ "0123:2345:4567:6789:890a:a0b1:1.2.3.4", true },
			{ "12345:2345:4567:6789:890a:a0b1:b1c2:c2d3", false },
			{ "0123:2345:4567:6789:890a:a0b1:b1c2:c2d3:fe32", false },
			{ "01:23:4g:67:89:a0:b1:c2", false },
			{ "a1", false },
			{ ":a1", false },
			{ "a1:", false },
			{ "::", true },
			{ "::a1", true },
			{ "a1::", true },
			{ "::a1::", false },
			{ "1::2:3:4:5:6:7:8", false },
			{ "1::2:3:4:5:6:7", true },
			{ "1:::2:3:4:5:6:7", false },
			{ "1::2:3:4::5:6", false },
		}};

		for (auto& pair : tests)
		{
			INFO  ( pair.first );
			CHECK ( kIsIPv6Address(pair.first, false) == pair.second );
		}
	}

	SECTION("kIsIPv6Address with braces")
	{
		std::vector<std::pair<KStringView, bool>> tests {{
			{ "[]", false },
			{ "[01:23:45:67:89:ae:bf]", false },
			{ "[01:23:45:67:89:ae:bf:cd]", true },
			{ "[01:23:45:67:89:AE:BF:CD]", true },
			{ "[0123:2345:4567:6789:890a:a0b1:b1c2:c2d3]", true },
			{ "[0123:2345:4567:6789:890a:a0b1:1.2.3.4]", true },
			{ "[12345:2345:4567:6789:890a:a0b1:b1c2:c2d3]", false },
			{ "[0123:2345:4567:6789:890a:a0b1:b1c2:c2d3:fe32]", false },
			{ "[01:23:4g:67:89:a0:b1:c2]", false },
			{ "[a1]", false },
			{ "[:a1]", false },
			{ "[a1:]", false },
			{ "[::]", true },
			{ "[::a1]", true },
			{ "[a1::]", true },
			{ "[::a1::]", false },
			{ "[1::2:3:4:5:6:7:8]", false },
			{ "[1::2:3:4:5:6:7]", true },
			{ "[1::2:3:4:5:6:7]:443", false },
			{ "[1:::2:3:4:5:6:7]", false },
			{ "[1::2:3:4::5:6]", false },
		}};

		for (auto& pair : tests)
		{
			INFO  ( pair.first );
			CHECK ( kIsIPv6Address(pair.first, true) == pair.second );
		}
	}

	SECTION("kIsIPv4Address")
	{
		std::vector<std::pair<KStringView, bool>> tests {{
			{ "", false },
			{ "01:23:45:67:89:ae:bf", false },
			{ "1.2.3.4", true  },
			{ "1.2.3.4.", false },
			{ ".1.2.3.4", false },
			{ "123.254.143.40", true },
			{ "123.254.1e.40", false },
			{ "123.254.143.400", false },
			{ "1.2.3.4.5", false },
			{ "1.2.3", false },
		}};

		for (auto& pair : tests)
		{
			INFO  ( pair.first );
			CHECK ( kIsIPv4Address(pair.first) == pair.second );
		}
	}

	SECTION("kIsPrivateIP")
	{
		struct tvals
		{
			KStringView sInput;
			bool bResult;
		};

		std::vector<tvals> tests
		{{
			{ ""                       , false },
			{ "1.2.3.4"                , false },
			{ "168.100.14.4"           , true  },
			{ "10.142.55.2"            , true  },
			{ "19.142.55.2"            , false },
			{ "127.13.1.1"             , true  },
			{ "::1"                    , true  },
			{ "::ffff:10.142.55.2"     , true  },
			{ "0:1:2:3:4:5:6:7:8"      , false },
			{ "[0:1:2:3:4:5:6:7:8]"    , false },
			{ "fd1a:cbed:34de::17"     , true  },
			{ "fe80:cbed:34de::17"     , true  },
			{ "fc80:cbed:34de::17"     , false },
		}};

	}

	SECTION("More")
	{
		KString sTestIP ("66.249.66.1"); // this is supposed to be crawl-66-249-66-1.googlebot.com
		CHECK (kIsIPv4Address (sTestIP));
		CHECK (!kIsIPv4Address ("257.1.1.1"));
		CHECK (kIsIPv6Address ("2001:0db8:85a3:0000:0000:8a2e:0370:7334", false));
		CHECK (!kIsIPv6Address (sTestIP, false));
	}

}
