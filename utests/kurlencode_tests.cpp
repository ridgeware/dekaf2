#include "catch.hpp"
#include <dekaf2/kurlencode.h>
#include <dekaf2/ksystem.h>
#include <vector>


using namespace dekaf2;


TEST_CASE("KURLEncode")
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

	SECTION("kIsValidIPv6 without braces")
	{
		std::vector<std::pair<KStringViewZ, bool>> tests {{
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
			CHECK ( kIsValidIPv6(pair.first) == pair.second );
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

	SECTION("kIsValidIPv4")
	{
		std::vector<std::pair<KStringViewZ, bool>> tests {{
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
			CHECK ( kIsValidIPv4(pair.first) == pair.second );
		}
	}


}
