#include "catch.hpp"
#include <dekaf2/kcookie.h>
#include <dekaf2/kstringview.h>



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
		CHECK ( Cookie.Serialize("https:/www.test.domain/XY") == "JSESSIONID=1C00976BBD8D9D3E9A52A7B887EFB616" );
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

		CHECK ( Cookies.Serialize(URL) == "" );
		CHECK ( Cookies.Serialize("http://www.test.domain/XY") == "" );
		CHECK ( Cookies.Serialize("https://www.test.domain/XY") == "JSESSIONID=1C00976BBD8D9D3E9A52A7B887EFB616; Tom=Cat" );
		CHECK ( Cookies.Serialize("https://some.mice.www.test.domain/") == "Jerry=Mouse" );
		CHECK ( Cookies.Serialize("http://some.mice.www.test.domain/") == "" );
		CHECK ( Cookies.Serialize("https://any.of.my.domain.com/xyz") == "Paula=Grey" );
	}

}
