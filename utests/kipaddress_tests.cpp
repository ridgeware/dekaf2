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
		CHECK ( IP.Is4() == false  );
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
}
