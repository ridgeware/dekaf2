#include "catch.hpp"
#include <dekaf2/core/format/kformat.h>
#include <dekaf2/http/protocol/khttp_method.h>
#include <dekaf2/http/protocol/khttp_header.h>
#include <dekaf2/web/url/kurl.h>
#include <dekaf2/core/strings/kcasestring.h>
#include <dekaf2/io/readwrite/kwriter.h>
#include <dekaf2/system/filesystem/kfilesystem.h>
#include <dekaf2/core/init/kcompatibility.h>
#include <locale>

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
		strcpy_n(buffer, "test 123456789012345678901234567890", 70);
		CHECK ( kFormat("hello {}", buffer) == "hello test 123456789012345678901234567890" );
	}

	SECTION("KStringView")
	{
		char buffer[70];
		std::memset(buffer, '+', 70);
		strcpy_n(buffer, "test 123456789012345678901234567890", 70);
		CHECK ( kFormat("hello {}", KStringView(buffer)) == "hello test 123456789012345678901234567890" );
	}

	SECTION("std::array")
	{
		std::array<char, 70> buffer;
		std::memset(buffer.data(), '!', buffer.size());
		strcpy_n(buffer.data(), "test 123456789012345678901234567890", 70);
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

namespace {

// a locale that groups thousands with '.', independent of the locales installed on the system
struct DotGrouping : public std::numpunct<char>
{
	char        do_thousands_sep() const override { return '.';  }
	std::string do_grouping()      const override { return "\3"; }
};

} // end of anonymous namespace

TEST_CASE("kFormatAppend")
{
	SECTION("basics")
	{
		KString sOut = "start:";
		kFormatAppend(sOut, " {} {}", "hello"_ksv, 42);
		CHECK ( sOut == "start: hello 42" );
		kFormatAppend(sOut, " no args");
		CHECK ( sOut == "start: hello 42 no args" );
	}

	SECTION("std::string")
	{
		std::string sOut = "x";
		kFormatAppend(sOut, "{:>5}", 7);
		CHECK ( sOut == "x    7" );
	}

	SECTION("same output as kFormat")
	{
		KString sOut = ">";
		kFormatAppend(sOut, "| {:<3.3} | {:<5.5} | {:>8} | {:x} | {} |", KString("1"), "2"_ksv, 3.5, 255, 'c');
		CHECK ( sOut == ">" + kFormat("| {:<3.3} | {:<5.5} | {:>8} | {:x} | {} |", KString("1"), "2"_ksv, 3.5, 255, 'c') );
	}

	SECTION("long output")
	{
		KString sOut = "prefix";
		KString sLong(1000, 'a');
		kFormatAppend(sOut, "|{}|{}|", sLong, 123);
		CHECK ( sOut == "prefix|" + sLong + "|123|" );
	}

	SECTION("locale")
	{
		std::locale Locale(std::locale::classic(), new DotGrouping);
		KString sOut = "n=";
		kFormatAppend(Locale, sOut, "{:L}", 1234567);
		CHECK ( sOut == "n=1.234.567" );
	}

	SECTION("runtime format string")
	{
		KString sFormat = "{}-{}";
		KString sOut = "a";
		kFormatAppend(sOut, KRuntimeFormat(sFormat), 1, 2);
		CHECK ( sOut == "a1-2" );
	}

	SECTION("bad format arguments append nothing")
	{
		// the first replacement field is written before the second one fails
		KString sFormat = "{} {:d}";
		KString sOut = "unchanged";
		kFormatAppend(sOut, KRuntimeFormat(sFormat), "text", "not a number");
		CHECK ( sOut == "unchanged" );
	}
}
