#include "catch.hpp"
#include <dekaf2/kipnetwork.h>
#include <vector>


using namespace dekaf2;


TEST_CASE("KIPNetwork")
{
	SECTION("KIPNetwork4 bad")
	{
		std::vector<std::pair<KStringView, KStringView>> Tests {
			{ ""               , "0.0.0.0/0" },
			{ "test/1"         , "0.0.0.0/0" },
			{ ".1.2.3.4/12"    , "0.0.0.0/0" },
			{ "1.2/8"          , "0.0.0.0/0" },
			{ "1.2.t.4/4"      , "0.0.0.0/0" },
			{ "1.2.3.4./6"     , "0.0.0.0/0" },
			{ "100.123.321.12" , "0.0.0.0/0" },
			{ "1a.12.44.2/1"   , "0.0.0.0/0" },
			{ "1:2:2:3"        , "0.0.0.0/0" },
			{ "[1.2.3.4]"      , "0.0.0.0/0" },
			{ "192.168.52.0/33", "192.168.52.0/32" },
		};

		for (auto& t : Tests)
		{
			KIPError ec;
			KIPNetwork4 Net(t.first, ec);
			INFO  ( t.first );
			CHECK ( ec.value() > 0 );
			CHECK ( Net.ToString() == t.second );
		}

		for (auto& t : Tests)
		{
			INFO  ( t.first );
			CHECK_THROWS( KIPNetwork4(t.first) );
		}
	}

	SECTION("KIPNetwork4 good")
	{
		std::vector<std::pair<KStringView, KStringView>> Tests {
			{ "192.168.52.0/24" , "192.168.52.0/24" },
			{ "192.168.52.0/16" , "192.168.52.0/16" },
			{ "192.168.52.0/08" , "192.168.52.0/8"  },
			{ "192.168.52.12/24", "192.168.52.12/24"},
			{ "192.168.52.0/24" , "192.168.52.0/24" },
		};

		for (auto& t : Tests)
		{
			KIPError ec;
			KIPNetwork4 Net(t.first, ec);
			INFO  ( t.first );
			CHECK ( ec.value() == 0 );
			CHECK ( Net.ToString() == t.second );
		}

		for (auto& t : Tests)
		{
			INFO  ( t.first );
			CHECK_NOTHROW( KIPNetwork4(t.first) );
		}
	}
}
