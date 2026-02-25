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
			{ "192.168.52.0/a3", "192.168.52.0/0" },
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

	SECTION("KIPNetwork6 bad")
	{
		std::vector<std::pair<KStringView, KStringView>> Tests {
			{ "200a:e344:5302:0989:ff7e:ffff:192.168.178/56"   , "::/0" },
			{ ""                                               , "::/0" },
			{ "::"                                             , "::/0" },
			{ "nothing/11"                                     , "::/0" },
			{ "test"                                           , "::/0" },
			{ "test/1"                                         , "::/0" },
			{ ".1.2.3.4/12"                                    , "::/0" },
			{ "200a:ea44:5302:0989:ff7e:c24e:9a3b:aaea"        , "::/0" },
			{ "200a:er44:5302:0989:ff7e:c24e:9a3b:aaea/56"     , "::/0" },
			{ "200a::e344:5302:0989:ff7e:c24e:9a3b:aaea/56"    , "::/0" },
			{ "200a::e344:5302:0989:ff7e:c24e:9a3b:aaea/5e6"   , "::/0" },
			{ "200a::5302:0989::9a3b:aaea/56"                  , "::/0" },
			{ ":200a:e344:5302:0989:ff7e:c24e:9a3b/56"         , "::/0" },
			{ "200a:e344:5302:0989:ff7e:c24e:9a3b:/56"         , "::/0" },
			{ "200a:e344:5302:0989:ff7e:c24e:9a3b:4444:/56"    , "::/0" },
			{ ":200a:e344:5302:0989:ff7e:c24e:9a3b:4444/56"    , "::/0" },
			{ "[200a:e344:5302:0989:ff7e:c24e:9a3b:4444/56"    , "::/0" },
			{ "200a:e344:5302:0989:ff7e:c24e:9a3b:4444]/56"    , "::/0" },
			{ "200a:e344:::ff7e:c24e:9a3b:4444/56"             , "::/0" },
			{ "200a:e344:5302:0989:ff7e:ffff:192.168.178/56"   , "::/0" },
			{ "::ffff:192.321.23.22/56"                        , "::/0" },
			{ "e344:5302:0989:ff7e:c24e:9a3b:4444/56"          , "::/0" },
			{ "e356:200a:e344:5302:0989:ff7e:c24e:9a3b:4444/56", "::/0" },
			{ "e356:200a:e344:5302:0989:ff7e:c24e:9a3b/156"    , "e356:200a:e344:5302:989:ff7e:c24e:9a3b/128" },
		};

		for (auto& t : Tests)
		{
			KIPError ec;
			KIPNetwork6 Net(t.first, ec);
			INFO  ( t.first );
			CHECK ( ec.value() > 0 );
			CHECK ( Net.ToString() == t.second );
		}

		for (auto& t : Tests)
		{
			INFO  ( t.first );
			CHECK_THROWS( KIPNetwork6(t.first) );
		}
	}

	SECTION("KIPNetwork6 good")
	{
		std::vector<std::pair<KStringView, KStringView>> Tests {
			{ "::/0"                                        , "::/0" },
			{ "0000:0000:0000:0000:0000:0000:0000:0000/0"   , "::/0" },
			{ "1234:5678:90ab:cdef:1234:5678:90ab:cdef/0"   , "1234:5678:90ab:cdef:1234:5678:90ab:cdef/0" },
			{ "1234:5678::1234:5678:90ab:cdef/0"            , "1234:5678::1234:5678:90ab:cdef/0" },
			{ "::1234:5678:90ab:cdef:1234/0"                , "::1234:5678:90ab:cdef:1234/0" },
			{ "1234:5678:90ab:cdef:1234::/0"                , "1234:5678:90ab:cdef:1234::/0" },
			{ "::ffff:192.168.123.16/0"                     , "::ffff:192.168.123.16/0" },
			{ "::/64"                                       , "::/64" },
			{ "0000:0000:0000:0000:0000:0000:0000:0000/64"  , "::/64" },
			{ "1234:5678:90ab:cdef:1234:5678:90ab:cdef/64"  , "1234:5678:90ab:cdef:1234:5678:90ab:cdef/64" },
			{ "1234:5678::1234:5678:90ab:cdef/64"           , "1234:5678::1234:5678:90ab:cdef/64" },
			{ "::1234:5678:90ab:cdef:1234/64"               , "::1234:5678:90ab:cdef:1234/64" },
			{ "1234:5678:90ab:cdef:1234::/64"               , "1234:5678:90ab:cdef:1234::/64" },
			{ "::ffff:192.168.123.16/120"                   , "::ffff:192.168.123.16/120" },
		};

		for (auto& t : Tests)
		{
			KIPError ec;
			KIPNetwork6 Net(t.first, ec);
			INFO  ( t.first );
			CHECK ( ec.value() == 0 );
			CHECK ( Net.ToString() == t.second );
		}

		for (auto& t : Tests)
		{
			INFO  ( t.first );
			CHECK_NOTHROW( KIPNetwork6(t.first) );
		}
	}

	SECTION("KIPNetwork bad")
	{
		std::vector<std::pair<KStringView, KStringView>> Tests {
			{ "e356:200a:e344:5302:0989:ff7e:c24e:9a3b/156"    , "e356:200a:e344:5302:989:ff7e:c24e:9a3b/128" },
			{ ""               , "::/0" },
			{ "test/1"         , "::/0" },
			{ ".1.2.3.4/12"    , "::/0" },
			{ "1.2/8"          , "::/0" },
			{ "1.2.t.4/4"      , "::/0" },
			{ "1.2.3.4./6"     , "::/0" },
			{ "100.123.321.12" , "::/0" },
			{ "1a.12.44.2/1"   , "::/0" },
			{ "1:2:2:3"        , "::/0" },
			{ "[1.2.3.4]"      , "::/0" },
			{ "192.168.52.0/33", "192.168.52.0/32" },
			{ "192.168.52.0/a3", "192.168.52.0/0" },

			{ "200a:e344:5302:0989:ff7e:ffff:192.168.178/56"   , "::/0" },
			{ ""                                               , "::/0" },
			{ "::"                                             , "::/0" },
			{ "nothing/11"                                     , "::/0" },
			{ "test"                                           , "::/0" },
			{ "test/1"                                         , "::/0" },
			{ ".1.2.3.4/12"                                    , "::/0" },
			{ "200a:ea44:5302:0989:ff7e:c24e:9a3b:aaea"        , "::/0" },
			{ "200a:er44:5302:0989:ff7e:c24e:9a3b:aaea/56"     , "::/0" },
			{ "200a::e344:5302:0989:ff7e:c24e:9a3b:aaea/56"    , "::/0" },
			{ "200a::e344:5302:0989:ff7e:c24e:9a3b:aaea/5e6"   , "::/0" },
			{ "200a::5302:0989::9a3b:aaea/56"                  , "::/0" },
			{ ":200a:e344:5302:0989:ff7e:c24e:9a3b/56"         , "::/0" },
			{ "200a:e344:5302:0989:ff7e:c24e:9a3b:/56"         , "::/0" },
			{ "200a:e344:5302:0989:ff7e:c24e:9a3b:4444:/56"    , "::/0" },
			{ ":200a:e344:5302:0989:ff7e:c24e:9a3b:4444/56"    , "::/0" },
			{ "[200a:e344:5302:0989:ff7e:c24e:9a3b:4444/56"    , "::/0" },
			{ "200a:e344:5302:0989:ff7e:c24e:9a3b:4444]/56"    , "::/0" },
			{ "200a:e344:::ff7e:c24e:9a3b:4444/56"             , "::/0" },
			{ "200a:e344:5302:0989:ff7e:ffff:192.168.178/56"   , "::/0" },
			{ "::ffff:192.321.23.22/56"                        , "::/0" },
			{ "e344:5302:0989:ff7e:c24e:9a3b:4444/56"          , "::/0" },
			{ "e356:200a:e344:5302:0989:ff7e:c24e:9a3b/156"    , "e356:200a:e344:5302:989:ff7e:c24e:9a3b/128" },
		};

		for (auto& t : Tests)
		{
			KIPError ec;
			KIPNetwork Net(t.first, ec);
			INFO  ( t.first );
			CHECK ( ec.value() > 0 );
			CHECK ( Net.ToString() == t.second );
		}

		for (auto& t : Tests)
		{
			INFO  ( t.first );
			CHECK_THROWS( KIPNetwork(t.first) );
		}
	}

	SECTION("KIPNetwork good")
	{
		std::vector<std::pair<KStringView, KStringView>> Tests {
			{ "192.168.52.0/24" , "192.168.52.0/24" },
			{ "192.168.52.0/16" , "192.168.52.0/16" },
			{ "192.168.52.0/08" , "192.168.52.0/8"  },
			{ "192.168.52.12/24", "192.168.52.12/24"},
			{ "192.168.52.0/24" , "192.168.52.0/24" },

			{ "::/0"                                        , "::/0" },
			{ "0000:0000:0000:0000:0000:0000:0000:0000/0"   , "::/0" },
			{ "1234:5678:90ab:cdef:1234:5678:90ab:cdef/0"   , "1234:5678:90ab:cdef:1234:5678:90ab:cdef/0" },
			{ "1234:5678::1234:5678:90ab:cdef/0"            , "1234:5678::1234:5678:90ab:cdef/0" },
			{ "::1234:5678:90ab:cdef:1234/0"                , "::1234:5678:90ab:cdef:1234/0" },
			{ "1234:5678:90ab:cdef:1234::/0"                , "1234:5678:90ab:cdef:1234::/0" },
			{ "::ffff:192.168.123.16/0"                     , "::ffff:192.168.123.16/0" },
			{ "::/64"                                       , "::/64" },
			{ "0000:0000:0000:0000:0000:0000:0000:0000/64"  , "::/64" },
			{ "1234:5678:90ab:cdef:1234:5678:90ab:cdef/64"  , "1234:5678:90ab:cdef:1234:5678:90ab:cdef/64" },
			{ "1234:5678::1234:5678:90ab:cdef/64"           , "1234:5678::1234:5678:90ab:cdef/64" },
			{ "::1234:5678:90ab:cdef:1234/64"               , "::1234:5678:90ab:cdef:1234/64" },
			{ "1234:5678:90ab:cdef:1234::/64"               , "1234:5678:90ab:cdef:1234::/64" },
			{ "::ffff:192.168.123.16/120"                   , "::ffff:192.168.123.16/120" },
		};

		for (auto& t : Tests)
		{
			KIPError ec;
			KIPNetwork Net(t.first, ec);
			INFO  ( t.first );
			CHECK ( ec.value() == 0 );
			CHECK ( Net.ToString() == t.second );
		}

		for (auto& t : Tests)
		{
			INFO  ( t.first );
			CHECK_NOTHROW( KIPNetwork(t.first) );
		}
	}

	SECTION("KIPNetwork good no prefix")
	{
		std::vector<std::pair<KStringView, KStringView>> Tests {
			{ "192.168.52.5"                            , "192.168.52.5/32" },
			{ "1234:5678:90ab:cdef:1234::4321"          , "1234:5678:90ab:cdef:1234::4321/128" },
			{ "::ffff:192.168.123.16"                   , "::ffff:192.168.123.16/128" },
		};

		for (auto& t : Tests)
		{
			KIPError ec;
			KIPNetwork Net(t.first, true, ec);
			INFO  ( t.first );
			CHECK ( ec.value() == 0 );
			CHECK ( Net.ToString() == t.second );
		}

		for (auto& t : Tests)
		{
			INFO  ( t.first );
			CHECK_NOTHROW( KIPNetwork(t.first, true) );
		}
	}

	SECTION("IsSubnet4 bad")
	{
		std::vector<std::pair<KStringView, KStringView>> Tests {
			{ "192.168.52.0/16" , "192.168.52.0/24" },
			{ "100.100.52.0/24" , "192.168.52.0/16" },
		};

		for (auto& t : Tests)
		{
			KIPError ec;
			KIPNetwork4 Net(t.first, ec);
			INFO ( t.first );
			CHECK ( ec.value() == 0 );
			CHECK ( Net.IsSubnetOf(KIPNetwork4(t.second)) == false );
		}
	}

	SECTION("IsSubnet4 good")
	{
		std::vector<std::pair<KStringView, KStringView>> Tests {
			{ "192.168.52.0/24" , "192.168.52.0/16" },
			{ "100.100.52.0/24" , "100.168.52.0/8" },
		};

		for (auto& t : Tests)
		{
			KIPError ec;
			KIPNetwork4 Net(t.first, ec);
			INFO ( t.first );
			CHECK ( ec.value() == 0 );
			CHECK ( Net.IsSubnetOf(KIPNetwork4(t.second)) == true );
		}
	}

	SECTION("IsSubnet6 bad")
	{
		std::vector<std::pair<KStringView, KStringView>> Tests {
			{ "::eeee:1234:1234/100" , "::eeee:1234:1234/112" },
			{ "::eeee:0000/100"      , "::eeee:eee2/112" },
		};

		for (auto& t : Tests)
		{
			KIPError ec;
			KIPNetwork6 Net(t.first, ec);
			INFO ( t.first );
			CHECK ( ec.value() == 0 );
			CHECK ( Net.IsSubnetOf(KIPNetwork6(t.second)) == false );
		}
	}

	SECTION("IsSubnet6 good")
	{
		std::vector<std::pair<KStringView, KStringView>> Tests {
			{ "::eeee:1234:1234/112" , "::eeee:1234:1234/100" },
			{ "::eeee:eee2/112"      , "::eeee:0000/100" },
		};

		for (auto& t : Tests)
		{
			KIPError ec;
			KIPNetwork6 Net(t.first, ec);
			INFO ( t.first );
			CHECK ( ec.value() == 0 );
			CHECK ( Net.IsSubnetOf(KIPNetwork6(t.second)) == true );
		}
	}

	SECTION("Hosts4 good")
	{
		std::vector<std::pair<KStringView, KStringView>> Tests {
			{ "192.168.52.0/16" , "192.168.0.1 - 192.168.255.255" },
			{ "100.100.52.0/24" , "100.100.52.1 - 100.100.52.255" },
		};

		for (auto& t : Tests)
		{
			KIPError ec;
			KIPNetwork4 Net(t.first);
			INFO ( t.first );
			auto p = Net.Hosts();
			CHECK ( kFormat("{} - {}", p.first, p.second) == t.second );
		}
	}

	SECTION("Hosts6 good")
	{
		std::vector<std::pair<KStringView, KStringView>> Tests {
			{ "::1234:5678:0000:0000/96" , "::1234:5678:0:0 - ::1234:5679:0:0" },
		};

		for (auto& t : Tests)
		{
			KIPError ec;
			KIPNetwork6 Net(t.first);
			INFO ( t.first );
			auto p = Net.Hosts();
			CHECK ( kFormat("{} - {}", p.first, p.second) == t.second );
		}
	}

	SECTION("Contains4 bad")
	{
		std::vector<std::pair<KStringView, KStringView>> Tests {
			{ "192.168.52.0/16" , "192.168.255.255" },
			{ "100.100.52.0/24" , "101.100.52.34" },
		};

		for (auto& t : Tests)
		{
			KIPError ec;
			KIPNetwork4 Net(t.first);
			INFO ( t.first );
			CHECK ( Net.Contains(KIPAddress4(t.second)) == false );
		}
	}

	SECTION("Contains4 good")
	{
		std::vector<std::pair<KStringView, KStringView>> Tests {
			{ "192.168.52.0/16" , "192.168.0.1" },
			{ "100.100.52.0/24" , "100.100.52.34" },
		};

		for (auto& t : Tests)
		{
			KIPError ec;
			KIPNetwork4 Net(t.first);
			INFO ( t.first );
			CHECK ( Net.Contains(KIPAddress4(t.second)) == true );
		}
	}

	SECTION("Contains6 bad")
	{
		std::vector<std::pair<KStringView, KStringView>> Tests {
			{ "::1234:5678:0000:0000/96" , "::1234:5688:0e30:0025" },
			{ "::1234:5678:0000:0000/96" , "::5678:0000:0000" },
		};

		for (auto& t : Tests)
		{
			KIPError ec;
			KIPNetwork6 Net(t.first);
			INFO ( t.first );
			CHECK ( Net.Contains(KIPAddress6(t.second)) == false );
		}
	}

	SECTION("Contains6 good")
	{
		std::vector<std::pair<KStringView, KStringView>> Tests {
			{ "::1234:5678:0000:0000/96" , "::1234:5678:0e30:0025" },
			{ "::1234:5678:0000:0000/96" , "::1234:5678:0000:0000" },
		};

		for (auto& t : Tests)
		{
			KIPError ec;
			KIPNetwork6 Net(t.first);
			INFO ( t.first );
			CHECK ( Net.Contains(KIPAddress6(t.second)) == true );
		}
	}

	SECTION("Contains bad")
	{
		std::vector<std::pair<KStringView, KStringView>> Tests {
			{ "192.168.52.0/16" , "192.168.255.255" },
			{ "100.100.52.0/24" , "101.100.52.34" },
			{ "::1234:5678:0000:0000/96" , "::1234:5688:0e30:0025" },
			{ "::1234:5678:0000:0000/96" , "::5678:0000:0000" },
			{ "2e01:17a5:092f:56a1:7e2f:ac76:245b:c3af/128", "211.158.173.219" },
		};

		for (auto& t : Tests)
		{
			KIPError ec;
			KIPNetwork Net(t.first);
			INFO ( t.first );
			CHECK ( Net.Contains(KIPAddress(t.second)) == false );
		}
	}

	SECTION("Contains good")
	{
		std::vector<std::pair<KStringView, KStringView>> Tests {
			{ "192.168.52.0/16" , "192.168.0.1" },
			{ "100.100.52.0/24" , "100.100.52.34" },
			{ "::1234:5678:0000:0000/96" , "::1234:5678:0e30:0025" },
			{ "::1234:5678:0000:0000/96" , "::1234:5678:0000:0000" },
			{ "::ffff:0000:0000/96"      , "192.168.12.128"        },
			{ "172.17.0.0/12"            , "::ffff:172.26.71.7"    },
		};

		for (auto& t : Tests)
		{
			KIPError ec;
			KIPNetwork Net(t.first);
			INFO ( t.first );
			CHECK ( Net.Contains(KIPAddress(t.second)) == true );
		}
	}

#if DEKAF2_IS_64_BITS
	SECTION("sizes")
	{
		KIPNetwork4 ip4;
		CHECK ( sizeof(ip4) == 6 );
		KIPNetwork6 ip6;
		CHECK ( sizeof(ip6) == 24 );
		KIPNetwork  ip;
		CHECK ( sizeof(ip)  == 32 );
		CHECK ( alignof(KIPNetwork4) == 1 );
		CHECK ( alignof(KIPNetwork6) == 4 );
		CHECK ( alignof(KIPNetwork ) == 4 );
	}
#endif
}
