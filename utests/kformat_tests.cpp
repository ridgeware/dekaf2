#include "catch.hpp"
#include <dekaf2/kformat.h>
#include <dekaf2/khttp_method.h>
#include <dekaf2/khttp_header.h>
#include <dekaf2/kcasestring.h>
#include <dekaf2/kurl.h>

using namespace dekaf2;

TEST_CASE("kFormat")
{
	SECTION("basics")
	{
		KString KS("hello");
		CHECK ( kFormat("{} {}", KS, "world"_ksv)              == "hello world" );
		CHECK ( kFormat("{}", KHTTPMethod(KHTTPMethod::POST))  == "POST" );
		CHECK ( kFormat("{}", KHTTPHeader(KHTTPHeader::DATE))  == "Date" );
		CHECK ( kFormat("{}", KCaseString("hello"))            == "hello" );
	}

	SECTION("KURL")
	{
		KURL URL("https://user:pass@domain.com:444/this%20is/the/path?to=parameta");
		CHECK ( kFormat("{}", URL) == "https://user:pass@domain.com:444/this%20is/the/path?to=parameta" );
		CHECK ( kFormat("{}", URL.Protocol)   == "https://" );
		CHECK ( kFormat("{}", URL.User)       == "user" );
		CHECK ( kFormat("{}", URL.Password)   == "pass" );
		CHECK ( kFormat("{}", URL.Domain)     == "domain.com" );
		CHECK ( kFormat("{}", URL.Port)       == "444" );
		CHECK ( kFormat("{}", URL.Path)       == "/this%20is/the/path" );
		CHECK ( kFormat("{}", URL.Query)      == "to=parameta" );
		CHECK ( kFormat("{}", KResource(URL)) == "/this%20is/the/path?to=parameta" );
	}

	SECTION("chrono")
	{
		using namespace std::literals::chrono_literals;
		CHECK ( kFormat("{} {}", 42s, 100ms) == "42s 100ms" );
		CHECK ( kFormat("{:%H:%M:%S}", 3h + 15min + 30s) == "03:15:30");
	}
}
