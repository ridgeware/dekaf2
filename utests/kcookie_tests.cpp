#include "catch.hpp"
#include <dekaf2/http/cookie/kcookie.h>
#include <dekaf2/core/strings/kstringview.h>
#include <dekaf2/io/streams/kstringstream.h>


using namespace dekaf2;


TEST_CASE("KCookie")
{
	SECTION("KCookie")
	{
		KURL URL ("https:/www.test.domain/get");

		KCookie Cookie(URL, "JSESSIONID=1C00976BBD8D9D3E9A52A7B887EFB616; Path=/XY; Secure; HttpOnly;SameSite=None");
		CHECK ( Cookie.Name() == "JSESSIONID" );
		CHECK ( Cookie.Value() == "1C00976BBD8D9D3E9A52A7B887EFB616" );
		CHECK ( Cookie.Serialize(URL) == "" );
		CHECK ( Cookie.Serialize("http:/www.test.domain/XY") == "" );
		CHECK ( Cookie.Serialize("https:/www.test.domain/XY") == "JSESSIONID=1C00976BBD8D9D3E9A52A7B887EFB616" );
	}

	SECTION("the prefixes __Host- and __Secure- are part of the name")
	{
		KURL URL ("https://www.test.domain/get");

		KCookie Host(URL, "__Host-session=abc; Path=/; Secure; HttpOnly; SameSite=Strict");
		CHECK ( Host.Name()         == "__Host-session" );
		CHECK ( Host.Value()        == "abc" );
		CHECK ( Host.Serialize(URL) == "__Host-session=abc" );

		KCookie Secure(URL, "__Secure-id=xyz; Path=/; Secure");
		CHECK ( Secure.Name()         == "__Secure-id" );
		CHECK ( Secure.Serialize(URL) == "__Secure-id=xyz" );

		// both need the Secure attribute, and __Host- takes no Domain
		CHECK ( KCookie(URL, "__Host-session=abc; Path=/").empty() );
		CHECK ( KCookie(URL, "__Secure-id=xyz; Path=/").empty() );
		CHECK ( KCookie(URL, "__Host-session=abc; Path=/; Secure; Domain=www.test.domain").empty() );

		// the name survives the cookie file
		KCookies Cookies;
		Cookies.Parse(URL, "__Host-session=abc; Path=/; Secure");

		KString sStorage;
		KOutStringStream oss(sStorage);
		CHECK ( Cookies.Write(oss) == true );

		Cookies.clear();
		KInStringStream iss(sStorage);
		CHECK ( Cookies.Read(iss) == true );
		CHECK ( Cookies.Serialize(URL) == "__Host-session=abc" );
	}

	SECTION("KCookies")
	{
		KURL URL ("https:/www.test.domain/get");

		KCookies Cookies;
		Cookies.Parse(URL, "JSESSIONID=1C00976BBD8D9D3E9A52A7B887EFB616; Path=/XY; Secure; HttpOnly;SameSite=None");
		Cookies.Parse(URL, "Tom=Mouse; Path=/XY; Secure");
		Cookies.Parse(URL, "Tom=Cat; Path=/XY; Secure");
		Cookies.Parse(URL, "Jerry=Mouse; Domain=mice.www.test.domain; Secure");
		Cookies.Parse("https://this.is.my.domain.com", "Paula=Grey; Domain=domain.com; Secure");

		KString sStorage;
		KOutStringStream oss(sStorage);
		CHECK ( Cookies.Write(oss) == true );
//		kWriteLine(sStorage);

		CHECK ( Cookies.Serialize(URL) == "" );
		CHECK ( Cookies.Serialize("http://www.test.domain/XY") == "" );
		CHECK ( Cookies.Serialize("https://www.test.domain/XY") == "JSESSIONID=1C00976BBD8D9D3E9A52A7B887EFB616; Tom=Cat" );
		CHECK ( Cookies.Serialize("https://some.mice.www.test.domain/") == "Jerry=Mouse" );
		CHECK ( Cookies.Serialize("http://some.mice.www.test.domain/") == "" );
		CHECK ( Cookies.Serialize("https://any.of.my.domain.com/xyz") == "Paula=Grey" );

		Cookies.clear();
		KInStringStream iss(sStorage);
		CHECK ( Cookies.Read(iss) == true );

		CHECK ( Cookies.Serialize("http://www.test.domain/XY") == "" );
		CHECK ( Cookies.Serialize("https://www.test.domain/XY") == "JSESSIONID=1C00976BBD8D9D3E9A52A7B887EFB616; Tom=Cat" );
		CHECK ( Cookies.Serialize("https://some.mice.www.test.domain/") == "Jerry=Mouse" );
		CHECK ( Cookies.Serialize("http://some.mice.www.test.domain/") == "" );
		CHECK ( Cookies.Serialize("https://any.of.my.domain.com/xyz") == "Paula=Grey" );
	}

}
