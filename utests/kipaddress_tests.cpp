#include "catch.hpp"
#include <dekaf2/net/address/kipaddress.h>
#include <vector>
#include <array>
#include <cstring>


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

	SECTION("KIPAddress4 bad")
	{
		std::vector<std::pair<KStringView, KStringView>> Tests {
			{ ""               , "0.0.0.0" },
			{ "test"           , "0.0.0.0" },
			{ ".1.2.3.4"       , "0.0.0.0" },
			{ "1.2"            , "0.0.0.0" },
			{ "1.2.t.4"        , "0.0.0.0" },
			{ "1.2.3.4."       , "0.0.0.0" },
			{ "100.123.321.12" , "0.0.0.0" },
			{ "1a.12.44.2"     , "0.0.0.0" },
			{ "1:2:2:3"        , "0.0.0.0" },
			{ "[1.2.3.4]"      , "0.0.0.0" },
			// text behind a complete address
			{ "1.2.3.4x"       , "0.0.0.0" },
			{ "1.2.3.123x"     , "0.0.0.0" },
			{ "1.2.3.4:80"     , "0.0.0.0" },
			{ "1.2.3.4/24"     , "0.0.0.0" },
			{ "1.2.3.4 "       , "0.0.0.0" },
			{ " 1.2.3.4"       , "0.0.0.0" },
		};

		for (auto& t : Tests)
		{
			KIPError ec;
			KIPAddress4 IP(t.first, ec);
			INFO  ( t.first );
			CHECK ( ec.value() > 0 );
			CHECK ( IP.ToString() == t.second );
		}

		for (auto& t : Tests)
		{
			INFO  ( t.first );
			CHECK_THROWS ( KIPAddress4(t.first) );
		}
	}

	SECTION("KIPAddress4 good")
	{
		std::vector<std::pair<KStringView, KStringView>> Tests {
			{ "192.168.52.126" , "192.168.52.126" },
		};

		for (auto& t : Tests)
		{
			KIPError ec;
			KIPAddress4 IP(t.first, ec);
			INFO  ( t.first );
			CHECK ( ec.value() == 0 );
			CHECK ( IP.ToString() == t.second );
		}

		for (auto& t : Tests)
		{
			INFO  ( t.first );
			CHECK_NOTHROW( KIPAddress4(t.first) );
		}
	}

	SECTION("KIPAddress6 unabridged bad")
	{
		std::vector<std::pair<KStringView, KStringView>> Tests {
			{ ""                                         , "0000:0000:0000:0000:0000:0000:0000:0000" },
			{ "[]"                                       , "0000:0000:0000:0000:0000:0000:0000:0000" },
			{ ":"                                        , "0000:0000:0000:0000:0000:0000:0000:0000" },
			{ "1.2.3.4"                                  , "0000:0000:0000:0000:0000:0000:0000:0000" },
			{ "test"                                     , "0000:0000:0000:0000:0000:0000:0000:0000" },
			{ "ffee:ed924:1123:4e6:22::"                 , "0000:0000:0000:0000:0000:0000:0000:0000" },
			{ "1:2:3:4:5:6:7:8:9"                        , "0000:0000:0000:0000:0000:0000:0000:0000" },
			{ "1122:3344:5566:7788:99aa:bbcc:ddee:"      , "0000:0000:0000:0000:0000:0000:0000:0000" },
			{ "1122:3344:5566:7788:99aa:bbcc:ddee:ff00:" , "0000:0000:0000:0000:0000:0000:0000:0000" },
			{ "1122:3344:5566:7788:99aa:bbcc:ddee:ff00::", "0000:0000:0000:0000:0000:0000:0000:0000" },
			{ "1122:3344:5566:7788:99aa::bbcc:ddee:ff00" , "0000:0000:0000:0000:0000:0000:0000:0000" },
			{ "[1122:3344:5566:7788:99aa:bbcc::"         , "0000:0000:0000:0000:0000:0000:0000:0000" },
			{ "1122:3344:5566:7788:99aa:bbcc::]"         , "0000:0000:0000:0000:0000:0000:0000:0000" },
			{ "11::ddee::ff00"                           , "0000:0000:0000:0000:0000:0000:0000:0000" },
			{ "11:::ddee:ff00"                           , "0000:0000:0000:0000:0000:0000:0000:0000" },
		};

		for (auto& t : Tests)
		{
			KIPError ec;
			KIPAddress6 IP(t.first, ec);
			INFO  ( t.first );
			CHECK ( ec.value() > 0 );
			CHECK ( IP.ToString(false, true) == t.second );
		}

		for (auto& t : Tests)
		{
			INFO  ( t.first );
			CHECK_THROWS ( KIPAddress6(t.first) );
		}
	}

	SECTION("KIPAddress6 unabridged good")
	{
		std::vector<std::pair<KStringView, KStringView>> Tests {
			{ "::"                                       , "0000:0000:0000:0000:0000:0000:0000:0000" },
			{ "1122:3344:5566:7788:99aa:bbcc::"          , "1122:3344:5566:7788:99aa:bbcc:0000:0000" },
			{ "1122:3344:5566:7788:99aa:bbcc::0"         , "1122:3344:5566:7788:99aa:bbcc:0000:0000" },
			{ "1122:3344:5566:7788:99aa:bbcc:ddee:ff00"  , "1122:3344:5566:7788:99aa:bbcc:ddee:ff00" },
			{ "[1122:3344:5566:7788:99aa:bbcc:ddee:ff00]", "1122:3344:5566:7788:99aa:bbcc:ddee:ff00" },
			{ "::0"                                      , "0000:0000:0000:0000:0000:0000:0000:0000" },
			{ "::1"                                      , "0000:0000:0000:0000:0000:0000:0000:0001" },
			{ "::1:2"                                    , "0000:0000:0000:0000:0000:0000:0001:0002" },
			{ "::ddee:ff00"                              , "0000:0000:0000:0000:0000:0000:ddee:ff00" },
			{ "11::ddee:ff00"                            , "0011:0000:0000:0000:0000:0000:ddee:ff00" },
			{ "1122:3344::bbcc:ddee:ff00"                , "1122:3344:0000:0000:0000:bbcc:ddee:ff00" },
			{ "1122:3344:55::bbcc:ddee:ff00"             , "1122:3344:0055:0000:0000:bbcc:ddee:ff00" },
			{ "1122:3344:155::bbcc:ddee:ff00"            , "1122:3344:0155:0000:0000:bbcc:ddee:ff00" },
			{ "1122:3344:5566::bbcc:ddee:ff00"           , "1122:3344:5566:0000:0000:bbcc:ddee:ff00" },
			{ "1122:3344::"                              , "1122:3344:0000:0000:0000:0000:0000:0000" },
			{ "::ffff:192.168.123.242"                   , "0000:0000:0000:0000:0000:ffff:c0a8:7bf2" },
			{ "0000:0000:0000:0000:0000:ffff:192.168.123.242", "0000:0000:0000:0000:0000:ffff:c0a8:7bf2" },
		};

		for (auto& t : Tests)
		{
			KIPError ec;
			KIPAddress6 IP(t.first, ec);
			INFO  ( t.first );
			CHECK ( ec.value() == 0 );
			CHECK ( IP.ToString(false, true) == t.second );
		}

		for (auto& t : Tests)
		{
			KIPAddress6 IP(t.first);
			INFO  ( t.first );
			CHECK_NOTHROW( KIPAddress6(t.first) );
		}
	}

	SECTION("KIPAddress6 bad")
	{
		std::vector<std::pair<KStringView, KStringView>> Tests {
			{ ""                                         , "::" },
			{ "[]"                                       , "::" },
			{ ":"                                        , "::" },
			{ "1.2.3.4"                                  , "::" },
			{ "test"                                     , "::" },
			{ "ffee:ed924:1123:4e6:22::"                 , "::" },
			{ "1:2:3:4:5:6:7:8:9"                        , "::" },
			{ "1122:3344:5566:7788:99aa:bbcc:ddee:"      , "::" },
			{ "1122:3344:5566:7788:99aa:bbcc:ddee:ff00:" , "::" },
			{ "1122:3344:5566:7788:99aa:bbcc:ddee:ff00::", "::" },
			{ "1122:3344:5566:7788:99aa::bbcc:ddee:ff00" , "::" },
			{ "[1122:3344:5566:7788:99aa:bbcc::"         , "::" },
			{ "1122:3344:5566:7788:99aa:bbcc::]"         , "::" },
			{ "11::ddee::ff00"                           , "::" },
			{ "11:::ddee:ff00"                           , "::" },
		};

		for (auto& t : Tests)
		{
			KIPError ec;
			KIPAddress6 IP(t.first, ec);
			INFO  ( t.first );
			CHECK ( ec.value() > 0 );
			CHECK ( IP.ToString(false, false) == t.second );
		}

		for (auto& t : Tests)
		{
			INFO  ( t.first );
			CHECK_THROWS ( KIPAddress6(t.first) );
		}
	}

	SECTION("KIPAddress6 good")
	{
		std::vector<std::pair<KStringView, KStringView>> Tests {
			{ "::"                                       , "::" },
			{ "1122:3344:5566:7788:99aa:bbcc:dd00:0"     , "1122:3344:5566:7788:99aa:bbcc:dd00:0" },
			{ "1122:3344:5566:7788:99aa:bbcc::"          , "1122:3344:5566:7788:99aa:bbcc::" },
			{ "1122:3344:5566:7788:99aa:bbcc::0"         , "1122:3344:5566:7788:99aa:bbcc::" },
			{ "1122:3344:5566:7788:99aa:bbcc:ddee:0"     , "1122:3344:5566:7788:99aa:bbcc:ddee:0" },
			{ "1122:3344:5566:7788:99aa:bbcc:ddee:ff00"  , "1122:3344:5566:7788:99aa:bbcc:ddee:ff00" },
			{ "[1122:3344:5566:7788:99aa:bbcc:ddee:ff00]", "1122:3344:5566:7788:99aa:bbcc:ddee:ff00" },
			{ "1122:0:0:7788:99aa:0:0:ff00"              , "1122::7788:99aa:0:0:ff00" },
			{ "1122:0:0:7788:0:0:0:ff00"                 , "1122:0:0:7788::ff00" },
			{ "::0"                                      , "::" },
			{ "::1"                                      , "::1" },
			{ "::1:2"                                    , "::1:2" },
			{ "::ddee:ff00"                              , "::ddee:ff00" },
			{ "11::ddee:ff00"                            , "11::ddee:ff00" },
			{ "1122:3344::bbcc:ddee:ff00"                , "1122:3344::bbcc:ddee:ff00" },
			{ "1122:3344:5::bbcc:ddee:ff00"              , "1122:3344:5::bbcc:ddee:ff00" },
			{ "1122:3344:55::bbcc:ddee:ff00"             , "1122:3344:55::bbcc:ddee:ff00" },
			{ "1122:3344:155::bbcc:ddee:ff00"            , "1122:3344:155::bbcc:ddee:ff00" },
			{ "1122:3344:5566::bbcc:ddee:ff00"           , "1122:3344:5566::bbcc:ddee:ff00" },
			{ "1122:3344::"                              , "1122:3344::" },
			{ "::ffff:192.168.123.242"                   , "::ffff:192.168.123.242" },
		};

		for (auto& t : Tests)
		{
			KIPError ec;
			KIPAddress6 IP(t.first, ec);
			INFO  ( t.first );
			CHECK ( ec.value() == 0 );
			CHECK ( IP.ToString(false, false) == t.second );
		}

		for (auto& t : Tests)
		{
			INFO  ( t.first );
			CHECK_NOTHROW ( KIPAddress6(t.first) );
		}
	}

	SECTION("IPv4 to IPv6")
	{
		{
			KIPAddress4 IPv4("124.156.34.21");
			CHECK ( IPv4.IsValid() );
			KIPAddress6 IPv6(IPv4);
			CHECK ( IPv6.IsValid() );
			CHECK ( IPv6.IsV4Mapped() );
			CHECK ( IPv6.ToString() == "::ffff:124.156.34.21");
		}
	}

	SECTION("IPv6 to IPv4")
	{
		{
			KIPAddress6 IPv6("::ffff:124.156.34.21");
			CHECK ( IPv6.IsValid() );
			CHECK ( IPv6.IsV4Mapped() );
			KIPAddress4 IPv4(IPv6);
			CHECK ( IPv4.IsValid() );
			CHECK ( IPv4.ToString() == "124.156.34.21");
		}
		{
			KIPAddress6 IPv6("fd99:2131:bbf7:37f6:14ee:7791:e8e8:fc50");
			CHECK ( IPv6.IsValid() );
			CHECK ( IPv6.IsV4Mapped() == false );
			KIPError ec;
			KIPAddress4 IPv4(IPv6, ec);
			CHECK ( ec.value() > 0 );
			CHECK ( IPv4.IsValid() == false );
			CHECK ( IPv4.ToString() == "0.0.0.0");
			CHECK_THROWS( KIPAddress4(IPv6) );
		}
	}

	SECTION("KIPAddress")
	{
		KIPAddress IP("192.168.17.231");
		CHECK ( IP.Is4() == true  );
		CHECK ( IP.Is6() == false );
		CHECK ( IP.ToString() == "192.168.17.231" );
		CHECK ( IP.IsConvertibleTo6() == true );
		CHECK ( IP.IsValid() == true );
		CHECK ( IP.IsLoopback() == false );
		CHECK ( IP.IsMulticast() == false );
		CHECK ( IP.IsUnspecified() == false );
		CHECK ( IP.To6().ToString() == "::ffff:192.168.17.231" );

		IP = KIPAddress("fd99:2131:bbf7:37f6:14ee:7791:e8e8:fc50");
		CHECK ( IP.Is4() == false );
		CHECK ( IP.Is6() == true );
		CHECK ( IP.ToString() == "fd99:2131:bbf7:37f6:14ee:7791:e8e8:fc50" );
		CHECK ( IP.IsConvertibleTo6() == true );
		CHECK ( IP.IsConvertibleTo4() == false );
		CHECK ( IP.IsValid() == true );
		CHECK ( IP.IsLoopback() == false );
		CHECK ( IP.IsMulticast() == false );
		CHECK ( IP.IsUnspecified() == false );

		IP = KIPAddress4("127.0.0.1");
		CHECK ( IP.Is4() == true  );
		CHECK ( IP.Is6() == false );
		CHECK ( IP.ToString() == "127.0.0.1" );
		CHECK ( IP.IsConvertibleTo6() == true );
		CHECK ( IP.IsConvertibleTo4() == true );
		CHECK ( IP.IsValid() == true );
		CHECK ( IP.IsLoopback() == true );
		CHECK ( IP.IsMulticast() == false );
		CHECK ( IP.IsUnspecified() == false );

		IP = KIPAddress4::Loopback();
		CHECK ( IP.Is4() == true  );
		CHECK ( IP.Is6() == false );
		CHECK ( IP.ToString() == "127.0.0.1" );
		CHECK ( IP.IsConvertibleTo6() == true );
		CHECK ( IP.IsConvertibleTo4() == true );
		CHECK ( IP.IsValid() == true );
		CHECK ( IP.IsLoopback() == true );
		CHECK ( IP.IsMulticast() == false );
		CHECK ( IP.IsUnspecified() == false );
	}

#if DEKAF2_IS_64_BITS
	SECTION("sizes")
	{
		KIPAddress4 ip4;
		CHECK ( sizeof(ip4) == 4 );
		KIPAddress6 ip6;
		CHECK ( sizeof(ip6) == 20 );
		KIPAddress  ip;
		CHECK ( sizeof(ip)  == 24 );
		CHECK ( alignof(KIPAddress4) == 1 );
		CHECK ( alignof(KIPAddress6) == 4 );
		CHECK ( alignof(KIPAddress ) == 4 );
	}
#endif

	SECTION("KIPAddress4 IsLinkLocal bad")
	{
		std::vector<KStringView> Tests {
			{ "192.168.1.132"  },
			{ "10.168.0.4"     },
			{ "169.255.0.0"    },
			{ "169.252.0.0"    },
		};

		for (auto& t : Tests)
		{
			KIPError ec;
			KIPAddress4 IP(t, ec);
			INFO  ( t );
			CHECK ( ec.value() == 0 );
			CHECK ( IP.IsLinkLocal() == false );
		}
	}

	SECTION("KIPAddress4 IsLinkLocal good")
	{
		std::vector<KStringView> Tests {
			{ "169.254.0.1"     },
			{ "169.254.12.143"  },
			{ "169.254.255.255" },
		};

		for (auto& t : Tests)
		{
			KIPError ec;
			KIPAddress4 IP(t, ec);
			INFO  ( t );
			CHECK ( ec.value() == 0 );
			CHECK ( IP.IsLinkLocal() == true );
		}
	}

	SECTION("KIPAddress4 IsPrivate bad")
	{
		static_assert(KIPAddress4(192,169,23,55).IsPrivate() == false, "must not be private");

		std::vector<KStringView> Tests {
			{ "192.164.1.132"  },
			{ "11.168.0.4"     },
			{ "169.255.0.0"    },
			{ "169.252.0.0"    },
		};

		for (auto& t : Tests)
		{
			KIPError ec;
			KIPAddress4 IP(t, ec);
			INFO  ( t );
			CHECK ( ec.value() == 0 );
			CHECK ( IP.IsPrivate() == false );
		}
	}

	SECTION("KIPAddress4 IsPrivate good")
	{
		static_assert(KIPAddress4(192,168,23,55).IsPrivate(), "must be private");

		std::vector<KStringView> Tests {
			{ "192.168.0.1"     },
			{ "192.168.170.11"  },
			{ "10.168.53.235"   },
			{ "172.30.34.53"    },
		};

		for (auto& t : Tests)
		{
			KIPError ec;
			KIPAddress4 IP(t, ec);
			INFO  ( t );
			CHECK ( ec.value() == 0 );
			CHECK ( IP.IsPrivate() == true );
		}
	}

	SECTION("KIPAddress4 IsCGNAT")
	{
		static_assert(KIPAddress4(100,100,11,31).IsCGNAT(), "must be CGNAT");
		static_assert(KIPAddress4(100,127,255,255).IsCGNAT(), "must be CGNAT");
		static_assert(KIPAddress4(100,128,0,0).IsCGNAT() == false, "must not be CGNAT");
	}

	SECTION("KIPAddress4 IsLoopback")
	{
		static_assert(KIPAddress4(127,0,0,1).IsLoopback(), "must be loopback");
		static_assert(KIPAddress4(127,127,255,255).IsLoopback(), "must be loopback");
		static_assert(KIPAddress4(128,128,0,0).IsLoopback() == false, "must not be loopback");
	}

	SECTION("KIPAddress6 IsLinkLocal bad")
	{
		std::vector<KStringView> Tests {
			{ "fe79::1"         },
			{ "::0"             },
			{ "::1"             },
			{ "fc3e:4563::1"    },
		};

		for (auto& t : Tests)
		{
			KIPError ec;
			KIPAddress6 IP(t, ec);
			INFO  ( t );
			CHECK ( ec.value() == 0 );
			CHECK ( IP.IsLinkLocal() == false );
		}
	}

	SECTION("KIPAddress6 IsLinkLocal good")
	{
		std::vector<KStringView> Tests {
			{ "fe80::1"              },
			{ "fe80:ffff:9728::1234" },
			{ "fe80:0123::1"         },
			{ "febf:0123::1"         },
		};

		for (auto& t : Tests)
		{
			KIPError ec;
			KIPAddress6 IP(t, ec);
			INFO  ( t );
			CHECK ( ec.value() == 0 );
			CHECK ( IP.IsLinkLocal() == true );
		}
	}

	SECTION("KIPAddress6 IsPrivate bad")
	{
		static_assert(KIPAddress6(0,0,0,0,0,0,0,0).IsPrivate() == false, "must not be private");

		std::vector<KStringView> Tests {
			{ "fe80::1"              },
			{ "fe80:ffff:9728::1234" },
			{ "fe80:0123::1"         },
			{ "::1"                  },
			{ "1122:3344:5566:7788:99aa:bbcc:ddee:ff00" }
		};

		for (auto& t : Tests)
		{
			KIPError ec;
			KIPAddress6 IP(t, ec);
			INFO  ( t );
			CHECK ( ec.value() == 0 );
			CHECK ( IP.IsPrivate() == false );
		}
	}

	SECTION("KIPAddress6 IsPrivate good")
	{
		static_assert(KIPAddress4(192,168,23,55).IsPrivate(), "must be private");

		std::vector<KStringView> Tests {
			{ "fd00::1"     },
			{ "fd99:2931:bafe:7af6:4ee:7e20:8d02:2a2c" },
			{ "fdc3:5af9:a9a1:0:9:b549:6b63:63c1"      },
		};

		for (auto& t : Tests)
		{
			KIPError ec;
			KIPAddress6 IP(t, ec);
			INFO  ( t );
			CHECK ( ec.value() == 0 );
			CHECK ( IP.IsPrivate() == true );
		}
	}

	SECTION("KIPAddress6 IsLoopback")
	{
		static_assert(KIPAddress6(0,0,0,0,0,0,0,1).IsLoopback(), "must be loopback");
		static_assert(KIPAddress6(0,0,0,0,0,0,0,0).IsLoopback() == false, "must not be loopback");
		static_assert(KIPAddress6(0xffff,0xfe35,0,0,0,0,0,0).IsLoopback() == false, "must not be loopback");
	}

	SECTION("KIPAddress IsLinkLocal bad")
	{
		std::vector<KStringView> Tests {
			{ "192.168.1.132"  },
			{ "10.168.0.4"     },
			{ "169.255.0.0"    },
			{ "169.252.0.0"    },
			{ "fe79::1"        },
			{ "::0"            },
			{ "::1"            },
			{ "fc3e:4563::1"   },
		};

		for (auto& t : Tests)
		{
			KIPError ec;
			KIPAddress IP(t, ec);
			INFO  ( t );
			CHECK ( ec.value() == 0 );
			CHECK ( IP.IsLinkLocal() == false );
		}
	}

	SECTION("KIPAddress IsLinkLocal good")
	{
		std::vector<KStringView> Tests {
			{ "169.254.0.1"     },
			{ "169.254.12.143"  },
			{ "169.254.255.255" },
			{ "fe80::1"              },
			{ "fe80:ffff:9728::1234" },
			{ "fe80:0123::1"         },
			{ "febf:0123::1"         },
		};

		for (auto& t : Tests)
		{
			KIPError ec;
			KIPAddress IP(t, ec);
			INFO  ( t );
			CHECK ( ec.value() == 0 );
			CHECK ( IP.IsLinkLocal() == true );
		}
	}

	SECTION("KIPAddress IsPrivate bad")
	{
		static_assert(KIPAddress4(192,169,23,55).IsPrivate() == false, "must not be private");

		std::vector<KStringView> Tests {
			{ "192.164.1.132"  },
			{ "11.168.0.4"     },
			{ "169.255.0.0"    },
			{ "169.252.0.0"    },
			{ "fe80::1"              },
			{ "fe80:ffff:9728::1234" },
			{ "fe80:0123::1"         },
			{ "::1"                  },
			{ "1122:3344:5566:7788:99aa:bbcc:ddee:ff00" }
		};

		for (auto& t : Tests)
		{
			KIPError ec;
			KIPAddress IP(t, ec);
			INFO  ( t );
			CHECK ( ec.value() == 0 );
			CHECK ( IP.IsPrivate() == false );
		}
	}

	SECTION("KIPAddress IsPrivate good")
	{
		static_assert(KIPAddress4(192,168,23,55).IsPrivate(), "must be private");

		std::vector<KStringView> Tests {
			{ "192.168.0.1"     },
			{ "192.168.170.11"  },
			{ "10.168.53.235"   },
			{ "172.30.34.53"    },
			{ "fd00::1"         },
			{ "fd99:2931:bafe:7af6:4ee:7e20:8d02:2a2c" },
			{ "fdc3:5af9:a9a1:0:9:b549:6b63:63c1"      },
		};

		for (auto& t : Tests)
		{
			KIPError ec;
			KIPAddress IP(t, ec);
			INFO  ( t );
			CHECK ( ec.value() == 0 );
			CHECK ( IP.IsPrivate() == true );
		}
	}

	SECTION("KIPAddress6 Scope from string (RFC 9844)")
	{
		SECTION("numeric scope IDs")
		{
			KIPError ec;
			KIPAddress6 a1("fe80::1%1", ec);
			CHECK ( ec.value() == 0 );
			CHECK ( a1.ToString() == "fe80::1%1" );
			CHECK ( a1.Scope() == 1 );

			KIPAddress6 a2("fe80::1%42", ec);
			CHECK ( ec.value() == 0 );
			CHECK ( a2.ToString() == "fe80::1%42" );
			CHECK ( a2.Scope() == 42 );

			KIPAddress6 a3("::1%0", ec);
			CHECK ( ec.value() == 0 );
			CHECK ( a3.ToString() == "::1" );
			CHECK ( a3.Scope() == 0 );
		}

		SECTION("no scope")
		{
			KIPError ec;
			KIPAddress6 a1("fe80::1", ec);
			CHECK ( ec.value() == 0 );
			CHECK ( a1.Scope() == 0 );
		}

		SECTION("empty zone after %")
		{
			KIPError ec;
			KIPAddress6 a1("fe80::1%", ec);
			CHECK ( ec.value() == 0 );
			CHECK ( a1.ToString() == "fe80::1" );
			CHECK ( a1.Scope() == 0 );
		}

		SECTION("scope with brackets")
		{
			KIPError ec;
			KIPAddress6 a1("[fe80::1%5]", ec);
			CHECK ( ec.value() == 0 );
			CHECK ( a1.ToString() == "fe80::1%5" );
			CHECK ( a1.Scope() == 5 );
		}

		SECTION("unknown interface name")
		{
			KIPError ec;
			KIPAddress6 a1("fe80::1%nonexistent_iface_xyz", ec);
			CHECK ( ec.value() == 0 );
			CHECK ( a1.ToString() == "fe80::1" );
			CHECK ( a1.Scope() == 0 );
		}

		SECTION("throwing constructors")
		{
			KIPAddress6 a1("fe80::1%7");
			CHECK ( a1.ToString() == "fe80::1%7" );
			CHECK ( a1.Scope() == 7 );

			KIPAddress6 a2("fe80::1");
			CHECK ( a2.Scope() == 0 );
		}

		SECTION("scope in link-local addresses")
		{
			KIPError ec;
			KIPAddress6 a1("fe80::abcd:1234%3", ec);
			CHECK ( ec.value() == 0 );
			CHECK ( a1.IsLinkLocal() == true );
			CHECK ( a1.Scope() == 3 );

			KIPAddress6 a2("fe80::abcd:1234%3", ec);
			KIPAddress6 a3("fe80::abcd:1234%4", ec);
			CHECK ( a2 != a3 );

			KIPAddress6 a4("fe80::abcd:1234%3", ec);
			CHECK ( a2 == a4 );
		}
	}

#if DEKAF2_HAS_CPP_20
	SECTION("KIPAddress IsCGNAT")
	{
		static_assert(KIPAddress(KIPAddress4(100,100,11,31)).IsCGNAT(), "must be CGNAT");
		static_assert(KIPAddress(KIPAddress4(100,127,255,255)).IsCGNAT(), "must be CGNAT");
		static_assert(KIPAddress(KIPAddress4(100,128,0,0)).IsCGNAT() == false, "must not be CGNAT");
	}

	SECTION("KIPAddress IsLoopback")
	{
		static_assert(KIPAddress(KIPAddress4(127,0,0,1)).IsLoopback(), "must be loopback");
		static_assert(KIPAddress(KIPAddress4(127,127,255,255)).IsLoopback(), "must be loopback");
		static_assert(KIPAddress(KIPAddress4(128,128,0,0)).IsLoopback() == false, "must not be loopback");
		static_assert(KIPAddress(KIPAddress6(0,0,0,0,0,0,0,1)).IsLoopback(), "must be loopback");
		static_assert(KIPAddress(KIPAddress6(0,0,0,0,0,0,0,0)).IsLoopback() == false, "must not be loopback");
		static_assert(KIPAddress(KIPAddress6(0xffff,0xfe35,0,0,0,0,0,0)).IsLoopback() == false, "must not be loopback");
	}
#endif

	SECTION("either family from a string")
	{
		// text with a colon goes to the IPv6 parser only, text without one to the
		// IPv4 parser first - the result has to be the same as from the parsers
		// of the families
		struct Sample
		{
			KStringView sAddress;
			bool        bIs6;
		};

		for (const auto& Good : std::vector<Sample> {
			{ "127.0.0.1"                    , false },
			{ "0.0.0.0"                      , false },
			{ "255.255.255.255"              , false },
			{ "::1"                          , true  },
			{ "::"                           , true  },
			{ "[::1]"                        , true  },
			{ "2001:db8:85a3::8a2e:370:7334" , true  },
			{ "::ffff:10.1.2.3"              , true  },
			{ "fe80::1%7"                    , true  } })
		{
			INFO ( Good.sAddress );
			KIPError ec;
			KIPAddress IP(Good.sAddress, ec);
			CHECK ( ec.value() == 0     );
			CHECK ( IP.Is6() == Good.bIs6 );

			if (Good.bIs6)
			{
				CHECK ( IP.To6() == KIPAddress6(Good.sAddress) );
			}
			else
			{
				CHECK ( IP.To4() == KIPAddress4(Good.sAddress) );
			}
		}

		CHECK ( KIPAddress("fe80::1%7").To6().Scope() == 7 );

		// an error that the caller comes with does not stay for an IPv6 address
		KIPError ecStale("an earlier error");
		KIPAddress Six("::1", ecStale);
		CHECK ( ecStale.value() == 0 );
		CHECK ( Six.To6() == KIPAddress6::Loopback() );

		for (KStringView sBad : { "", "1.2.3", "1.2.3.4.5", "256.1.1.1", "localhost", "not.an.ip.address",
		                          ":", "12:34", "1.2:3.4", "::1::", "[::1", "gggg::1",
		                          "1.2.3.4:80", "10.20.30.40:5", "1.2.3.4x", "1.2.3.4/24", "1.2.3.4 ", "::1x", "::1 " })
		{
			INFO ( sBad );
			KIPError ec;
			KIPAddress IP(sBad, ec);
			CHECK ( ec.value() > 0     );
			CHECK ( IP.IsUnspecified() );
			CHECK_THROWS_AS ( KIPAddress(sBad), KIPError );

			// the error is the one of the IPv6 parser, which is asked last
			KIPError ec6;
			KIPAddress6 IP6(sBad, ec6);
			CHECK ( ec.value() == ec6.value() );
			CHECK ( KStringView(ec.what()) == KStringView(ec6.what()) );
		}
	}

#if DEKAF2_IPADDRESS_HAS_SOCKADDR
	SECTION("from socket addresses")
	{
		// The system fills these structures with the address bytes in network byte
		// order. Copy the bytes in as the system does: an integer value for s_addr
		// would depend on the byte order of the host.
		const std::array<uint8_t, 4> Bytes4 {{ 192, 168, 1, 2 }};

		sockaddr_in sin4 {};
		std::memcpy(&sin4.sin_addr, Bytes4.data(), Bytes4.size());

		CHECK ( KIPAddress4(sin4.sin_addr).ToString() == "192.168.1.2" );
		CHECK ( KIPAddress4(sin4).ToString()          == "192.168.1.2" );
		CHECK ( KIPAddress4(sin4).ToBytes()           == Bytes4        );
		CHECK ( KIPAddress4(sin4).ToUInt()            == 0xC0A80102    );

		const std::array<uint8_t, 16> Bytes6 {{ 0x20, 0x01, 0x0d, 0xb8, 0, 0, 0, 0, 0, 0, 0, 0, 0x12, 0x34, 0x56, 0x78 }};

		sockaddr_in6 sin6 {};
		std::memcpy(&sin6.sin6_addr, Bytes6.data(), Bytes6.size());
		sin6.sin6_scope_id = 7;

		CHECK ( KIPAddress6(sin6.sin6_addr).ToString() == "2001:db8::1234:5678" );
		CHECK ( KIPAddress6(sin6).ToBytes()            == Bytes6                );
		CHECK ( KIPAddress6(sin6).Scope()              == 7                     );
	}
#endif

}
