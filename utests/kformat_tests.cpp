#include "catch.hpp"
#include <dekaf2/kformat.h>
#include <dekaf2/khttp_method.h>
#include <dekaf2/khttp_header.h>
#include <dekaf2/kurl.h>
#include <dekaf2/kcasestring.h>
#include <dekaf2/kwriter.h>
#include <dekaf2/kfilesystem.h>

using namespace dekaf2;

TEST_CASE("kFormat")
{
	SECTION("basics")
	{
		KString KS("hello");
		CHECK ( kFormat("{} {}", KS, "world"_ksv)              == "hello world" );
		CHECK ( kFormat("{}", KHTTPMethod(KHTTPMethod::POST))  == "POST" );
		CHECK ( kFormat("{}", KHTTPHeader(KHTTPHeader::DATE))  == "date" );
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
#if DEKAF2_BITS > 32
		// this works on 32 bits but spits out a compiler warning
		// about a stringop overflow in fmt::format - which is not
		// _our_ problem ..
		CHECK ( kFormat("{} {}", 42s, 100ms) == "42s 100ms" );
#endif
		CHECK ( kFormat("{:%H:%M:%S}", 3h + 15min + 30s) == "03:15:30");
	}

	SECTION("char*")
	{
		CHECK ( kFormat("hello {}", "test 123456789012345678901234567890") == "hello test 123456789012345678901234567890" );
	}

	SECTION("char[]")
	{
		char buffer[70];
		std::memset(buffer, '-', 70);
		strcpy(buffer, "test 123456789012345678901234567890");
		CHECK ( kFormat("hello {}", buffer) == "hello test 123456789012345678901234567890" );
	}

	SECTION("KStringView")
	{
		char buffer[70];
		std::memset(buffer, '+', 70);
		strcpy(buffer, "test 123456789012345678901234567890");
		CHECK ( kFormat("hello {}", KStringView(buffer)) == "hello test 123456789012345678901234567890" );
	}

	SECTION("std::array")
	{
		std::array<char, 70> buffer;
		std::memset(buffer.data(), '!', buffer.size());
		strcpy(buffer.data(), "test 123456789012345678901234567890");
		CHECK ( kFormat("hello {}", buffer.data()) == "hello test 123456789012345678901234567890" );
	}

	SECTION("KWriter")
	{
		KTempDir TempDir;
		KString sName = kFormat("{}{}test.txt", TempDir.Name(), kDirSep);
		KOutFile Out(sName);
		Out.WriteLine("line 1");
		Out.FormatLine("line {}", 2);
		Out.FormatLine("line without formatting");
	}

	SECTION("Formatting")
	{
		auto s = kFormat(">| {:<3.3} | {:<5.5} | {:<5.5} | {:<19.19} | {:<7.7} | {}{}/p>",
						 KString("1"), KString("2"), KString("3"), KString("4"), KString("5"), KString("6"), '<');
		CHECK ( s == ">| 1   | 2     | 3     | 4                   | 5       | 6</p>" );
	}
}
