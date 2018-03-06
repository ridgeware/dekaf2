#include "catch.hpp"
#include <dekaf2/kwebio.h>
#include <dekaf2/khttpclient.h>

using namespace dekaf2;

#include <iostream>

#include <map>
#include <unordered_map>
#include <vector>
#include <functional>
#include <iterator>


// Optional debug printing. Turn this on if tests fail.
// This will print the headers giving an idea how they were parsed.
#define kwebdebug 0

TEST_CASE("KWebIO")
{
	SECTION("Normal Header Parse Test")
	{
		/*
		 * Content-type: text/html
		 * Cookie: foo=bar
		 * Set-Cookie: yummy_cookie=choco
		 * Set-Cookie: tasty_cookie=strawberry
		 * X-Forwarded-For: 192.0.2.43, 2001:db8:cafe::17
		 * Forwarded: for=192.0.2.43, for="[2001:db8:cafe::17]
		 */

		KWebIO webIO;
		KString sHeaderPart = "Content-type: text/html\nCookie: foo=bar\nSet-Cookie: yummy_cookie=choco\nSet-Cookie: tasty_cookie=strawberry\nX-Forwarded-For: 192.0.2.43, 2001:db8:cafe::17\nForwarded: for=192.0.2.43, for='[2001:db8:cafe::17]'\n\n";
		webIO.Parse(sHeaderPart);

		KCurl::KHeader sResponseHeaders = webIO.getResponseHeaders();
		CHECK_FALSE(sResponseHeaders.empty());
		CHECK(sResponseHeaders.size() == 6);
		CHECK(sResponseHeaders.IsMulti("set-cookie"));
		CHECK(sResponseHeaders.Count("cookie") == 1);
		CHECK(sResponseHeaders.Count("set-cookie") == 2);
		CHECK(sResponseHeaders.Count("content-type") == 1);
		CHECK(sResponseHeaders.Count("forwarded") == 1);
		CHECK(sResponseHeaders.Count("x-forwarded-for") == 1);
#if kwebdebug
		// Verifying how header was parsed by printing
		std::cout << "    == Normal Parse Test Header ==" << std::endl;
		webIO.Serialize();
#endif
	}

	SECTION("Complex Header Parse Test")
	{
		KCurl::KHeader theHeader;
		theHeader.Add("Content-type", "text/html");
		theHeader.Add("Cookies", "foo=bar;muh=cookie");
		theHeader.Add("Cookies", "foo2=bar2;muhs=cookies");

		CHECK(theHeader.find("content-type")->first == "Content-type");
		KString value = theHeader.Get("Fubar");
		CHECK(value.empty());
	}

	SECTION("Windows Header Parse Test")
	{
		/*
		 * Content-type: text/html
		 * Cookie: foo=bar
		 * Set-Cookie: yummy_cookie=choco
		 * Set-Cookie: tasty_cookie=strawberry
		 * X-Forwarded-For: 192.0.2.43, 2001:db8:cafe::17
		 * Forwarded: for=192.0.2.43, for="[2001:db8:cafe::17]
		 */
		KWebIO webIO;
		KString sHeaderPart = "Content-type: text/html\r\nCookie: foo=bar\r\nSet-Cookie: yummy_cookie=choco\r\nSet-Cookie: tasty_cookie=strawberry\r\nX-Forwarded-For: 192.0.2.43, 2001:db8:cafe::17\r\nForwarded: for=192.0.2.43, for='[2001:db8:cafe::17]'\r\n\r\n";
		webIO.Parse(sHeaderPart);

		// print response header for debugging:
		//for (auto header : webIO.getResponseHeaders().begin())
		//auto iter = webIO.getResponseHeaders().begin();
		//for (auto iter = webIO.getResponseHeaders().begin(); iter == webIO.getResponseBody().end(); iter = iter)
		KCurl::KHeader  sResponseHeaders = webIO.getResponseHeaders();
		CHECK_FALSE(sResponseHeaders.empty());
		CHECK(sResponseHeaders.size() == 6);
		CHECK(sResponseHeaders.IsMulti("set-cookie"));
		CHECK(sResponseHeaders.Count("cookie") == 1);
		CHECK(sResponseHeaders.Count("set-cookie") == 2);
		CHECK(sResponseHeaders.Count("content-type") == 1);
		CHECK(sResponseHeaders.Count("forwarded") == 1);
		CHECK(sResponseHeaders.Count("x-forwarded-for") == 1);
#if kwebdebug
		// Verifying how header was parsed by printing
		std::cout << "    == Windows Parse Test Header ==" << std::endl;
		webIO.Serialize();
#endif
	}

	SECTION("Garbage Header Parse Test")
	{
		/*
		 * Content-type: text/html
		 * Cookie: foo=bar
		 * Set-Cookie: yummy_cookie=choco
		 * Set-Cookie: tasty_cookie=strawberry
		 * X-Forwarded-For: 192.0.2.43, 2001:db8:cafe::17
		 * Forwarded: for=192.0.2.43, for="[2001:db8:cafe::17]
		 */
		KWebIO webIO;
		KString sHeaderPart = "Content-type: text/html\nCookie: foo=bar\nthis is a garbage line, no colons\nSet-Cookie: yummy_cookie=choco\nSet-Cookie: tasty_cookie=strawberry\nX-Forwarded-For: 192.0.2.43, 2001:db8:cafe::17\nForwarded: for=192.0.2.43, for='[2001:db8:cafe::17]'\n\n";
		webIO.Parse(sHeaderPart);

		// print response header for debugging:
		//for (auto header : webIO.getResponseHeaders().begin())
		//auto iter = webIO.getResponseHeaders().begin();
		//for (auto iter = webIO.getResponseHeaders().begin(); iter == webIO.getResponseBody().end(); iter = iter)
		KCurl::KHeader  sResponseHeaders = webIO.getResponseHeaders();
		CHECK_FALSE(sResponseHeaders.empty());
		CHECK(sResponseHeaders.size() == 7);
		CHECK(sResponseHeaders.IsMulti("set-cookie"));
		CHECK(sResponseHeaders.Count("cookie") == 1);
		CHECK(sResponseHeaders.Count("set-cookie") == 2);
		CHECK(sResponseHeaders.Count("content-type") == 1);
		CHECK(sResponseHeaders.Count("forwarded") == 1);
		CHECK(sResponseHeaders.Count("x-forwarded-for") == 1);
#if kwebdebug
		// Verifying how header was parsed by printing
		std::cout << "    == Garbage Test Header ==" << std::endl;
		webIO.Serialize();
#endif
	}

	SECTION("Streamed Header Parse Test")
	{
		/*
		 * Content-type: text/html
		 * Cookie: foo=bar
		 * Set-Cookie: yummy_cookie=choco
		 * Set-Cookie: tasty_cookie=strawberry
		 * X-Forwarded-For: 192.0.2.43, 2001:db8:cafe::17
		 * Forwarded: for=192.0.2.43, for="[2001:db8:cafe::17]
		 */
		KWebIO webIO;
		KString sHeaderPart1 = "Content-type: text/html\nCookie: foo=bar;foobar=fubar\nSet-Cookie: yummy_cookie=choco\nSet-Cookie: tasty_cookie=strawberry\n";
		KString sHeaderPart2 = "X-Forwarded-For: 192.0.2.43, 2001:db8:cafe::17\nForwarded: for=192.0.2.43, for='[2001:db8:cafe::17]'\n\n";
		webIO.Parse(sHeaderPart1);
		webIO.Parse(sHeaderPart2);

		// print response header for debugging:
		//for (auto header : webIO.getResponseHeaders().begin())
		//auto iter = webIO.getResponseHeaders().begin();
		//for (auto iter = webIO.getResponseHeaders().begin(); iter == webIO.getResponseBody().end(); iter = iter)
		KCurl::KHeader  sResponseHeaders = webIO.getResponseHeaders();
		CHECK_FALSE(sResponseHeaders.empty());
		CHECK(sResponseHeaders.size() == 6);
		CHECK(sResponseHeaders.IsMulti("set-cookie"));
		CHECK(sResponseHeaders.Count("cookie") == 1);
		CHECK(sResponseHeaders.Count("set-cookie") == 2);
		CHECK(sResponseHeaders.Count("content-type") == 1);
		CHECK(sResponseHeaders.Count("forwarded") == 1);
		CHECK(sResponseHeaders.Count("x-forwarded-for") == 1);

		KString xForwarded = sResponseHeaders.Get(KHTTPHeader::x_forwarded_for);
		CHECK(xForwarded.compare(" 192.0.2.43, 2001:db8:cafe::17\n") == 0);
		CHECK(xForwarded == " 192.0.2.43, 2001:db8:cafe::17\n");
		//const KString* pFooCookie = webIO.getResponseCookie("foo");
		const KString& pFooCookie = webIO.getResponseCookie("foo");
		CHECK(pFooCookie == "bar");
		//const KString* pBadCookie = webIO.getResponseCookie("ddf");
		const KString& pBadCookie = webIO.getResponseCookie("ddf");
		CHECK(pBadCookie == "");
		//CHECK(pBadCookie.empty());
#if kwebdebug
//		KCurl::KHeader allCookies = webIO.getResponseCookies();
//		std::cout << "    == Cookies ==" << std::endl;
//		for (const auto& iter : allCookies)
//		{
//			std::cout << iter.second.first << "=" << iter.second.second << std::endl;
//		}
		// Verifying how header was parsed by printing
		std::cout << "    == Streamed Test Header ==" << std::endl;
		webIO.Serialize();
#endif
	}

	SECTION("Streamed Cut Header Parse Test")
	{
		/*
		 * Content-type: text/html
		 * Cookie: foo=bar
		 * Set-Cookie: yummy_cookie=choco
		 * Set-Cookie: tasty_cookie=strawberry
		 * X-Forwarded-For: 192.0.2.43, 2001:db8:cafe::17
		 * Forwarded: for=192.0.2.43, for="[2001:db8:cafe::17]
		 */
		KWebIO webIO;
		KString sHeaderPart1 = "Content-type: text/html\nCookie: ";
		KString sHeaderPart2 = "foo=bar\nSet-Cookie: yummy_cookie=choco\nSet-Cookie: tasty_cookie=strawberry\n";
		KString sHeaderPart3 = "X-Forwarded-For: 192.0.2.43, 2001:db8:cafe::17";
		KString sHeaderPart4 = "\nForwarded: for=192.0.2.43, for='[2001:db8:cafe::17]'\n\n";
		webIO.Parse(sHeaderPart1);
		webIO.Parse(sHeaderPart2);
		webIO.Parse(sHeaderPart3);
		webIO.Parse(sHeaderPart4);

		// print response header for debugging:
		//for (auto header : webIO.getResponseHeaders().begin())
		//auto iter = webIO.getResponseHeaders().begin();
		//for (auto iter = webIO.getResponseHeaders().begin(); iter == webIO.getResponseBody().end(); iter = iter)
		KCurl::KHeader  sResponseHeaders = webIO.getResponseHeaders();
		CHECK_FALSE(sResponseHeaders.empty());
		CHECK(sResponseHeaders.size() == 6);
		CHECK(sResponseHeaders.IsMulti("set-cookie"));
		CHECK(sResponseHeaders.Count("cookie") == 1);
		CHECK(sResponseHeaders.Count("set-cookie") == 2);
		CHECK(sResponseHeaders.Count("content-type") == 1);
		CHECK(sResponseHeaders.Count("forwarded") == 1);
		CHECK(sResponseHeaders.Count("x-forwarded-for") == 1);
#if kwebdebug
		// Verifying how header was parsed by printing
		std::cout << "    == Cut Stream Test Header ==" << std::endl;
		webIO.Serialize();
#endif
	}

	SECTION("Streamed Cut Header Parse Test II")
	{
		/*
		 * Content-type: text/html
		 * Cookie: foo=bar
		 * Set-Cookie: yummy_cookie=choco
		 * Set-Cookie: tasty_cookie=strawberry
		 * X-Forwarded-For: 192.0.2.43, 2001:db8:cafe::17
		 * Forwarded: for=192.0.2.43, for="[2001:db8:cafe::17]
		 */
		KWebIO webIO;
		KString sHeaderPart1 = "Content-type: ";
		KString sHeaderPart2 = "text/html\nCookie: ";
		KString sHeaderPart3 = "foo=bar, muh=cooki";
		KString sHeaderPart4 = "e\nSet-Cookie: yummy_cooki";
		KString sHeaderPart5 = "e=choco\nSet-Cookie: tasty_cookie=strawberry\n";
		KString sHeaderPart6 = "X-Forwarded-For: 192.0.2.43, 2001:";
		KString sHeaderPart7 = "db8:cafe::17";
		KString sHeaderPart8 = "\nForwarded: for=192.0.";
		KString sHeaderPart9 = "2.43, for='[2001:db8:cafe::17]'\n\n";
		webIO.Parse(sHeaderPart1);
		webIO.Parse(sHeaderPart2);
		webIO.Parse(sHeaderPart3);
		webIO.Parse(sHeaderPart4);
		webIO.Parse(sHeaderPart5);
		webIO.Parse(sHeaderPart6);
		webIO.Parse(sHeaderPart7);
		webIO.Parse(sHeaderPart8);
		webIO.Parse(sHeaderPart9);

		KCurl::KHeader  sResponseHeaders = webIO.getResponseHeaders();
		CHECK_FALSE(sResponseHeaders.empty());
		CHECK(sResponseHeaders.size() == 6);
		CHECK(sResponseHeaders.IsMulti("set-cookie"));
		CHECK(sResponseHeaders.Count("cookie") == 1);
		CHECK(sResponseHeaders.Count("set-cookie") == 2);
		CHECK(sResponseHeaders.Count("content-type") == 1);
		CHECK(sResponseHeaders.Count("forwarded") == 1);
		CHECK(sResponseHeaders.Count("x-forwarded-for") == 1);
#if kwebdebug
		// Verifying how header was parsed by printing
		std::cout << "    == Cut Stream Test Header II ==" << std::endl;
		webIO.Serialize();
#endif
	}

	SECTION("Windows Streamed Cut Header Parse Test")
	{
		/*
		 * Content-type: text/html
		 * Cookie: foo=bar
		 * Set-Cookie: yummy_cookie=choco
		 * Set-Cookie: tasty_cookie=strawberry
		 * X-Forwarded-For: 192.0.2.43, 2001:db8:cafe::17
		 * Forwarded: for=192.0.2.43, for="[2001:db8:cafe::17]
		 */
		KWebIO webIO;
		KString sHeaderPart1 = "Content-type: ";
		KString sHeaderPart2 = "text/html\r\nCookie: ";
		KString sHeaderPart3 = "foo=bar, muh=cooki";
		KString sHeaderPart4 = "e\r\nSet-Cookie: yummy_cooki";
		KString sHeaderPart5 = "e=choco\r\nSet-Cookie: tasty_cookie=strawberry\r\n";
		KString sHeaderPart6 = "X-Forwarded-For: 192.0.2.43, 2001:";
		KString sHeaderPart7 = "db8:cafe::17";
		KString sHeaderPart8 = "\r\nForwarded: for=192.0.";
		KString sHeaderPart9 = "2.43, for='[2001:db8:cafe::17]'\r\n\r\n";
		webIO.Parse(sHeaderPart1);
		webIO.Parse(sHeaderPart2);
		webIO.Parse(sHeaderPart3);
		webIO.Parse(sHeaderPart4);
		webIO.Parse(sHeaderPart5);
		webIO.Parse(sHeaderPart6);
		webIO.Parse(sHeaderPart7);
		webIO.Parse(sHeaderPart8);
		webIO.Parse(sHeaderPart9);

		KCurl::KHeader  sResponseHeaders = webIO.getResponseHeaders();
		CHECK_FALSE(sResponseHeaders.empty());
		CHECK(sResponseHeaders.size() == 6);
		CHECK(sResponseHeaders.IsMulti("set-cookie"));
		CHECK(sResponseHeaders.Count("cookie") == 1);
		CHECK(sResponseHeaders.Count("set-cookie") == 2);
		CHECK(sResponseHeaders.Count("content-type") == 1);
		CHECK(sResponseHeaders.Count("forwarded") == 1);
		CHECK(sResponseHeaders.Count("x-forwarded-for") == 1);
#if kwebdebug
		// Verifying how header was parsed by printing
		std::cout << "    == Windows Cut Stream Test Header ==" << std::endl;
		webIO.Serialize();
#endif
	}

	SECTION("Windows Streamed Cut Header Parse Test II")
	{
		/*
		 * Content-type: text/html
		 * Cookie: foo=bar
		 * Set-Cookie: yummy_cookie=choco
		 * Set-Cookie: tasty_cookie=strawberry
		 * X-Forwarded-For: 192.0.2.43, 2001:db8:cafe::17
		 * Forwarded: for=192.0.2.43, for="[2001:db8:cafe::17]
		 */
		KWebIO webIO;
		KString sHeaderPart1 = "Content-type: ";
		KString sHeaderPart2 = "text/html\r\nCookie: ";
		KString sHeaderPart3 = "foo=bar; muh=cooki";
		KString sHeaderPart4 = "e\r\nSet-Cookie: yummy_cooki";
		KString sHeaderPart5 = "e=choco\r\nSet-Cookie: tasty_cookie=strawberry\r\n";
		KString sHeaderPart6 = "X-Forwarded-For: 192.0.2.43, 2001:";
		KString sHeaderPart7 = "db8:cafe::17\r";
		KString sHeaderPart8 = "\nForwarded: for=192.0.";
		KString sHeaderPart9 = "2.43, for='[2001:db8:cafe::17]'\r\n";
		KString sHeaderPart10 = "\r\n";
		webIO.Parse(sHeaderPart1);
		webIO.Parse(sHeaderPart2);
		webIO.Parse(sHeaderPart3);
		webIO.Parse(sHeaderPart4);
		webIO.Parse(sHeaderPart5);
		webIO.Parse(sHeaderPart6);
		webIO.Parse(sHeaderPart7);
		webIO.Parse(sHeaderPart8);
		webIO.Parse(sHeaderPart9);
		webIO.Parse(sHeaderPart10);

		KCurl::KHeader  sResponseHeaders = webIO.getResponseHeaders();
		CHECK_FALSE(sResponseHeaders.empty());
		CHECK(sResponseHeaders.size() == 6);
		CHECK(sResponseHeaders.IsMulti("set-cookie"));
		CHECK(sResponseHeaders.Count("cookie") == 1);
		CHECK(sResponseHeaders.Count("set-cookie") == 2);
		CHECK(sResponseHeaders.Count("content-type") == 1);
		CHECK(sResponseHeaders.Count("forwarded") == 1);
		CHECK(sResponseHeaders.Count("x-forwarded-for") == 1);
#if kwebdebug
		// Verifying how header was parsed by printing
		std::cout << "    == Windows Cut Stream Test Header II==" << std::endl;
		webIO.Serialize();
#endif
	}

	SECTION("Complex Header Real Test")
	{
		/*
		 * Content-type: text/html
		 * Cookie: foo=bar;
		 * Set-Cookie: yummy_cookie=choco
		 * Set-Cookie: tasty_cookie=strawberry
		 * X-Forwarded-For: 192.0.2.43, 2001:db8:cafe::17
		 * Forwarded: for=192.0.2.43, for="[2001:db8:cafe::17]
		 */
		KWebIO webIO;
		KString sHeaderPart1 = "Content-type: ";
		KString sHeaderPart2 = "text/html\r\nCookie: ";
		KString sHeaderPart3 = "foo=bar; muh=cooki"; // 18 char
		KString sHeaderPart4 = "e;\r\nSet-Cookie: yummy_cooki";
		KString sHeaderPart5 = "e=choco\r\nSet-Cookie: tasty_cookie=strawberry\r\n";
		KString sHeaderPart6 = "X-Forwarded-For: 192.0.2.43, 2001:";
		KString sHeaderPart7 = "db8:cafe::17\r";
		KString sHeaderPart8 = "\nForwarded: for=192.0.";
		KString sHeaderPart9 = "2.43, for='[2001:db8:cafe::17]'\r\n";
		KString sHeaderPart10 = "\r\n";
		webIO.Parse(sHeaderPart1);
		webIO.Parse(sHeaderPart2);
		webIO.Parse(sHeaderPart3);
		webIO.Parse(sHeaderPart4);
		webIO.Parse(sHeaderPart5);
		webIO.Parse(sHeaderPart6);
		webIO.Parse(sHeaderPart7);
		webIO.Parse(sHeaderPart8);
		webIO.Parse(sHeaderPart9);
		webIO.Parse(sHeaderPart10);

		KCurl::KHeader  sResponseHeaders = webIO.getResponseHeaders();
		CHECK_FALSE(sResponseHeaders.empty());
		CHECK(sResponseHeaders.size() == 6);
		CHECK(sResponseHeaders.IsMulti("set-cookie"));
		CHECK(sResponseHeaders.Count("cookie") == 1);
		const KString& sCookieVal = webIO.getResponseCookie("muh");
		CHECK(sCookieVal.length() == 9);
		CHECK(sResponseHeaders.Count("set-cookie") == 2);
		CHECK(sResponseHeaders.Count("content-type") == 1);
		const KString& sContentType = webIO.getResponseHeader("content-type");
		CHECK(sContentType.length() == 12);
		CHECK(sResponseHeaders.Count("forwarded") == 1);
		CHECK(sResponseHeaders.Count("x-forwarded-for") == 1);
#if kwebdebug
		// Verifying how header was parsed by printing
		std::cout << "    == Windows Parse Test Header ==" << std::endl;
		webIO.Serialize();
		std::cout << std::endl;
#endif
	}

	SECTION("Complex Header Real Test 2 (With Extra Spaces)")
	{
		/*
		 * Content-type: text/html
		 * Cookie: foo=bar;
		 * Set-Cookie: yummy_cookie=choco
		 * Set-Cookie: tasty_cookie=strawberry
		 * X-Forwarded-For: 192.0.2.43, 2001:db8:cafe::17
		 * Forwarded: for=192.0.2.43, for="[2001:db8:cafe::17]
		 */
		KWebIO webIO;
		KString sHeaderPart1 = "Content-type: ";
		KString sHeaderPart2 = "text/html\r\nCookie: ";
		KString sHeaderPart3 = "foo=bar; muh=cooki";
		KString sHeaderPart4 = "e;  \r\nSet-Cookie: yummy_cooki";
		KString sHeaderPart5 = "e=choco\r\nSet-Cookie: tasty_cookie=strawberry\r\n";
		KString sHeaderPart6 = "X-Forwarded-For: 192.0.2.43, 2001:";
		KString sHeaderPart7 = "db8:cafe::17\r";
		KString sHeaderPart8 = "\nForwarded: for=192.0.";
		KString sHeaderPart9 = "2.43, for='[2001:db8:cafe::17]'\r\n";
		KString sHeaderPart10 = "\r\n";
		webIO.Parse(sHeaderPart1);
		webIO.Parse(sHeaderPart2);
		webIO.Parse(sHeaderPart3);
		webIO.Parse(sHeaderPart4);
		webIO.Parse(sHeaderPart5);
		webIO.Parse(sHeaderPart6);
		webIO.Parse(sHeaderPart7);
		webIO.Parse(sHeaderPart8);
		webIO.Parse(sHeaderPart9);
		webIO.Parse(sHeaderPart10);

		KCurl::KHeader  sResponseHeaders = webIO.getResponseHeaders();
		CHECK_FALSE(sResponseHeaders.empty());
		CHECK(sResponseHeaders.size() == 6);
		CHECK(sResponseHeaders.IsMulti("set-cookie"));
		CHECK(sResponseHeaders.Count("cookie") == 1);
		const KString& sCookieVal = webIO.getResponseCookie("muh");
		CHECK(sCookieVal.length() == 11);
		CHECK(sResponseHeaders.Count("set-cookie") == 2);
		CHECK(sResponseHeaders.Count("content-type") == 1);
		CHECK(sResponseHeaders.Count("forwarded") == 1);
		CHECK(sResponseHeaders.Count("x-forwarded-for") == 1);
#if kwebdebug
		// Verifying how header was parsed by printing
		std::cout << "    == Windows Parse Test Header ==" << std::endl;
		webIO.Serialize();
#endif
	}

	SECTION("Complex Header Real Test 3")
	{
		/*
		 * Content-type: text/html
		 * Cookie: foo=bar;
		 * Set-Cookie: yummy_cookie=choco
		 * Set-Cookie: tasty_cookie=strawberry
		 * X-Forwarded-For: 192.0.2.43, 2001:db8:cafe::17
		 * Forwarded: for=192.0.2.43, for="[2001:db8:cafe::17]
		 */
		KWebIO webIO;
		KString sHeaderPart1 = "Content-type: ";
		KString sHeaderPart2 = "text/html  \r\nCookie: ";
		KString sHeaderPart3 = "foo=bar; muh=cooki";
		KString sHeaderPart4 = "e  \r\nSet-Cookie: yummy_cooki";
		KString sHeaderPart5 = "e=choco;\r\nSet-Cookie: tasty_cookie=strawberry\r\n";
		KString sHeaderPart6 = "X-Forwarded-For: 192.0.2.43, 2001:";
		KString sHeaderPart7 = "db8:cafe::17\r";
		KString sHeaderPart8 = "\nForwarded: for=192.0.";
		KString sHeaderPart9 = "2.43, for='[2001:db8:cafe::17]'\r\n";
		KString sHeaderPart10 = "\r\n";
		webIO.Parse(sHeaderPart1);
		webIO.Parse(sHeaderPart2);
		webIO.Parse(sHeaderPart3);
		webIO.Parse(sHeaderPart4);
		webIO.Parse(sHeaderPart5);
		webIO.Parse(sHeaderPart6);
		webIO.Parse(sHeaderPart7);
		webIO.Parse(sHeaderPart8);
		webIO.Parse(sHeaderPart9);
		webIO.Parse(sHeaderPart10);

		KCurl::KHeader  sResponseHeaders = webIO.getResponseHeaders();
		CHECK_FALSE(sResponseHeaders.empty());
		CHECK(sResponseHeaders.size() == 6);
		CHECK(sResponseHeaders.IsMulti("set-cookie"));
		CHECK(sResponseHeaders.Count("cookie") == 1);
		const KString& sCookieVal = webIO.getResponseCookie("muh");
		CHECK(sCookieVal.length() == 10);
		CHECK(sResponseHeaders.Count("set-cookie") == 2);
		CHECK(sResponseHeaders.Count("content-type") == 1);
		const KString& sContentType = webIO.getResponseHeader("content-type");
		CHECK(sContentType.length() == 14);
		CHECK(sResponseHeaders.Count("forwarded") == 1);
		CHECK(sResponseHeaders.Count("x-forwarded-for") == 1);
#if kwebdebug
		//#if 1
		// Verifying how header was parsed by printing
		std::cout << "    == Windows Parse Test Header ==" << std::endl;
		webIO.Serialize();
#endif
	}

	SECTION("Complex Header Real Test 4 (unix newlines)")
	{
		/*
		 * Content-type: text/html
		 * Cookie: foo=bar;
		 * Set-Cookie: yummy_cookie=choco
		 * Set-Cookie: tasty_cookie=strawberry
		 * X-Forwarded-For: 192.0.2.43, 2001:db8:cafe::17
		 * Forwarded: for=192.0.2.43, for="[2001:db8:cafe::17]
		 */
		KWebIO webIO;
		KString sHeaderPart1 = "Content-type: ";
		KString sHeaderPart2 = "text/html  \nCookie: ";
		KString sHeaderPart3 = "foo=bar; muh=cooki";
		KString sHeaderPart4 = "e  \nSet-Cookie: yummy_cooki";
		KString sHeaderPart5 = "e=choco;\nSet-Cookie: tasty_cookie=strawberry\n";
		KString sHeaderPart6 = "X-Forwarded-For: 192.0.2.43, 2001:";
		KString sHeaderPart7 = "db8:cafe::17";
		KString sHeaderPart8 = "\nForwarded: for=192.0.";
		KString sHeaderPart9 = "2.43, for='[2001:db8:cafe::17]'\n";
		KString sHeaderPart10 = "\n";
		webIO.Parse(sHeaderPart1);
		webIO.Parse(sHeaderPart2);
		webIO.Parse(sHeaderPart3);
		webIO.Parse(sHeaderPart4);
		webIO.Parse(sHeaderPart5);
		webIO.Parse(sHeaderPart6);
		webIO.Parse(sHeaderPart7);
		webIO.Parse(sHeaderPart8);
		webIO.Parse(sHeaderPart9);
		webIO.Parse(sHeaderPart10);

		KCurl::KHeader  sResponseHeaders = webIO.getResponseHeaders();
		CHECK_FALSE(sResponseHeaders.empty());
		CHECK(sResponseHeaders.size() == 6);
		CHECK(sResponseHeaders.IsMulti("set-cookie"));
		CHECK(sResponseHeaders.Count("cookie") == 1);
		const KString& sCookieVal = webIO.getResponseCookie("muh");
		CHECK(sCookieVal.length() == 9);
		CHECK(sResponseHeaders.Count("set-cookie") == 2);
		CHECK(sResponseHeaders.Count("content-type") == 1);
		const KString& sContentType = webIO.getResponseHeader("content-type");
		CHECK(sContentType.length() == 13);
		CHECK(sResponseHeaders.Count("forwarded") == 1);
		CHECK(sResponseHeaders.Count("x-forwarded-for") == 1);
#if kwebdebug
		//#if 1
		// Verifying how header was parsed by printing
		std::cout << "    == Complex Header Real Test 4 (unix newlines) ==" << std::endl;
		webIO.Serialize();
#endif
	}

	SECTION("Continuation Line Test (unix newlines)")
	{
		/*
		 * Content-type: text/html
		 * Cookie: foo=bar;
		 * Set-Cookie: yummy_cookie=choco
		 * Set-Cookie: tasty_cookie=strawberry
		 * X-Forwarded-For: 192.0.2.43, 2001:db8:cafe::17
		 * Forwarded: for=192.0.2.43, for="[2001:db8:cafe::17]
		 */
		KWebIO webIO;
		KString sHeaderPart1 = "Content-type: ";
		KString sHeaderPart2 = "text/html  \nCookie: ";
		KString sHeaderPart3 = "foo=bar;\n muh=cooki";
		KString sHeaderPart4 = "e;  \nSet-Cookie: yummy_cooki";
		KString sHeaderPart5 = "e\n =choco;\nSet-Cookie: tasty_cookie=strawberry\n";
		KString sHeaderPart6 = "X-Forwarded-For: 192.0.2.43, 2001:";
		KString sHeaderPart7 = "db8:cafe::17,\n 192.168.1.100, 192.168.1.1";
		KString sHeaderPart8 = "\nForwarded: for=192.0.";
		KString sHeaderPart9 = "2.43, for='[2001:db8:cafe::17]'\n";
		KString sHeaderPart10 = "\n";
		webIO.Parse(sHeaderPart1);
		webIO.Parse(sHeaderPart2);
		webIO.Parse(sHeaderPart3);
		webIO.Parse(sHeaderPart4);
		webIO.Parse(sHeaderPart5);
		webIO.Parse(sHeaderPart6);
		webIO.Parse(sHeaderPart7);
		webIO.Parse(sHeaderPart8);
		webIO.Parse(sHeaderPart9);
		webIO.Parse(sHeaderPart10);

		KCurl::KHeader  sResponseHeaders = webIO.getResponseHeaders();
		CHECK_FALSE(sResponseHeaders.empty());
		CHECK(sResponseHeaders.size() == 6);
		CHECK(sResponseHeaders.IsMulti("set-cookie"));
		CHECK(sResponseHeaders.Count("cookie") == 1);
		const KString& sCookieVal = webIO.getResponseCookie("muh");
		CHECK(sCookieVal.length() == 10);
		CHECK(sResponseHeaders.Count("set-cookie") == 2);
		CHECK(sResponseHeaders.Count("content-type") == 1);
		const KString& sContentType = webIO.getResponseHeader("content-type");
		CHECK(sContentType.length() == 13);
		CHECK(sResponseHeaders.Count("forwarded") == 1);
		CHECK(sResponseHeaders.Count("x-forwarded-for") == 1);
#if kwebdebug
		//#if 1
		// Verifying how header was parsed by printing
		std::cout << "    == Continuation Line Test (unix newlines) ==" << std::endl;
		webIO.Serialize();
#endif
	}

	SECTION("Continuation Line Test (windows  newlines)")
	{
		/*
		 * Content-type: text/html
		 * Cookie: foo=bar;
		 * Set-Cookie: yummy_cookie=choco
		 * Set-Cookie: tasty_cookie=strawberry
		 * X-Forwarded-For: 192.0.2.43, 2001:db8:cafe::17
		 * Forwarded: for=192.0.2.43, for="[2001:db8:cafe::17]
		 */
		KWebIO webIO;
		KString sHeaderPart1 = "Content-type: ";
		KString sHeaderPart2 = "text/html  \r\nCookie: ";
		KString sHeaderPart3 = "foo=bar;\r\n muh=cooki";
		KString sHeaderPart4 = "e;  \r\nSet-Cookie: yummy_cooki";
		KString sHeaderPart5 = "e\r\n =choco;\r\nSet-Cookie: tasty_cookie=strawberry\r\n";
		KString sHeaderPart6 = "X-Forwarded-For: 192.0.2.43, 2001:";
		KString sHeaderPart7 = "db8:cafe::17,\r\n 192.168.1.100, 192.168.1.1\r";
		KString sHeaderPart8 = "\nForwarded: for=192.0.";
		KString sHeaderPart9 = "2.43, for='[2001:db8:cafe::17]'\r\n";
		KString sHeaderPart10 = "\r\n";
		webIO.Parse(sHeaderPart1);
		webIO.Parse(sHeaderPart2);
		webIO.Parse(sHeaderPart3);
		webIO.Parse(sHeaderPart4);
		webIO.Parse(sHeaderPart5);
		webIO.Parse(sHeaderPart6);
		webIO.Parse(sHeaderPart7);
		webIO.Parse(sHeaderPart8);
		webIO.Parse(sHeaderPart9);
		webIO.Parse(sHeaderPart10);

		KCurl::KHeader  sResponseHeaders = webIO.getResponseHeaders();
		CHECK_FALSE(sResponseHeaders.empty());
		CHECK(sResponseHeaders.size() == 6);
		CHECK(sResponseHeaders.IsMulti("set-cookie"));
		CHECK(sResponseHeaders.Count("cookie") == 1);
		const KString& sCookieVal = webIO.getResponseCookie("muh");
		CHECK(sCookieVal.length() == 11);
		CHECK(sResponseHeaders.Count("set-cookie") == 2);
		CHECK(sResponseHeaders.Count("content-type") == 1);
		const KString& sContentType = webIO.getResponseHeader("content-type");
		CHECK(sContentType.length() == 14);
		CHECK(sResponseHeaders.Count("forwarded") == 1);
		CHECK(sResponseHeaders.Count("x-forwarded-for") == 1);
#if kwebdebug
		// Verifying how header was parsed by printing
		std::cout << "    == Continuation Line Test (windows  newlines) ==" << std::endl;
		webIO.Serialize();
#endif
	}

	SECTION("Invalid Cookie Line Test (windows  newlines)")
	{
		/*
		 * Content-type: text/html
		 * Cookie: foo=bar;
		 * Set-Cookie: yummy_cookie=choco
		 * Set-Cookie: tasty_cookie=strawberry
		 * X-Forwarded-For: 192.0.2.43, 2001:db8:cafe::17
		 * Forwarded: for=192.0.2.43, for="[2001:db8:cafe::17]
		 */
		KWebIO webIO;
		KString sHeaderPart1 = "Content-type: ";
		KString sHeaderPart2 = "text/html  \r\nCookie: ";
		KString sHeaderPart3 = "fubar";
		KString sHeaderPart4 = " \r\nSet-Cookie: yummy_cooki";
		KString sHeaderPart5 = "e =choco;\r\nSet-Cookie: tasty_cookie=strawberry\r\n";
		KString sHeaderPart6 = "X-Forwarded-For: 192.0.2.43, 2001:";
		KString sHeaderPart7 = "db8:cafe::17,  192.168.1.100, 192.168.1.1\r";
		KString sHeaderPart8 = "\nForwarded: for=192.0.";
		KString sHeaderPart9 = "2.43, for='[2001:db8:cafe::17]'\r\n";
		KString sHeaderPart10 = "\r\n";
		webIO.Parse(sHeaderPart1);
		webIO.Parse(sHeaderPart2);
		webIO.Parse(sHeaderPart3);
		webIO.Parse(sHeaderPart4);
		webIO.Parse(sHeaderPart5);
		webIO.Parse(sHeaderPart6);
		webIO.Parse(sHeaderPart7);
		webIO.Parse(sHeaderPart8);
		webIO.Parse(sHeaderPart9);
		webIO.Parse(sHeaderPart10);

		KCurl::KHeader  sResponseHeaders = webIO.getResponseHeaders();
		CHECK_FALSE(sResponseHeaders.empty());
		CHECK(sResponseHeaders.size() == 6);
		CHECK(webIO.getResponseCookies().size() == 1);
		CHECK(sResponseHeaders.IsMulti("set-cookie"));
		CHECK(sResponseHeaders.Count("cookie") == 1);
		const KString& sCookieVal = webIO.getResponseCookie("muh");
		CHECK(sCookieVal.empty());
		CHECK(sResponseHeaders.Count("set-cookie") == 2);
		CHECK(sResponseHeaders.Count("content-type") == 1);
		const KString& sContentType = webIO.getResponseHeader("content-type");
		CHECK(sContentType.length() == 14);
		CHECK(sResponseHeaders.Count("forwarded") == 1);
		CHECK(sResponseHeaders.Count("x-forwarded-for") == 1);
#if kwebdebug
		// Verifying how header was parsed by printing
		std::cout << "    == Invalid Cookie Line Test (windows  newlines) ==" << std::endl;
		webIO.Serialize();
#endif
	}

	SECTION("Invalid Cookie Line Test 2(windows  newlines)")
	{
		/*
		 * Content-type: text/html
		 * Cookie: foo=bar;
		 * Set-Cookie: yummy_cookie=choco
		 * Set-Cookie: tasty_cookie=strawberry
		 * X-Forwarded-For: 192.0.2.43, 2001:db8:cafe::17
		 * Forwarded: for=192.0.2.43, for="[2001:db8:cafe::17]
		 */
		KWebIO webIO;
		KString sHeaderPart1 = "Content-type: ";
		KString sHeaderPart2 = "text/html  \r\nCookie: ";
		KString sHeaderPart3 = "fubar";
		KString sHeaderPart4 = " ; \r\nSet-Cookie: yummy_cooki";
		KString sHeaderPart5 = "e =choco;\r\nSet-Cookie: tasty_cookie=strawberry\r\n";
		KString sHeaderPart6 = "X-Forwarded-For: 192.0.2.43, 2001:";
		KString sHeaderPart7 = "db8:cafe::17,  192.168.1.100, 192.168.1.1\r";
		KString sHeaderPart8 = "\nForwarded: for=192.0.";
		KString sHeaderPart9 = "2.43, for='[2001:db8:cafe::17]'\r\n";
		KString sHeaderPart10 = "\r\n";
		webIO.Parse(sHeaderPart1);
		webIO.Parse(sHeaderPart2);
		webIO.Parse(sHeaderPart3);
		webIO.Parse(sHeaderPart4);
		webIO.Parse(sHeaderPart5);
		webIO.Parse(sHeaderPart6);
		webIO.Parse(sHeaderPart7);
		webIO.Parse(sHeaderPart8);
		webIO.Parse(sHeaderPart9);
		webIO.Parse(sHeaderPart10);

		KCurl::KHeader  sResponseHeaders = webIO.getResponseHeaders();
		CHECK_FALSE(sResponseHeaders.empty());
		CHECK(sResponseHeaders.size() == 6);
		CHECK(webIO.getResponseCookies().size() == 1);
		CHECK(sResponseHeaders.IsMulti("set-cookie"));
		CHECK(sResponseHeaders.Count("cookie") == 1);
		const KString& sCookieVal = webIO.getResponseCookie("muh");
		CHECK(sCookieVal.empty());
		CHECK(sResponseHeaders.Count("set-cookie") == 2);
		CHECK(sResponseHeaders.Count("content-type") == 1);
		const KString& sContentType = webIO.getResponseHeader("content-type");
		CHECK(sContentType.length() == 14);
		CHECK(sResponseHeaders.Count("forwarded") == 1);
		CHECK(sResponseHeaders.Count("x-forwarded-for") == 1);
#if kwebdebug
		// Verifying how header was parsed by printing
		std::cout << "    == Invalid Cookie Line Test 2 (windows  newlines) ==" << std::endl;
		webIO.Serialize();
#endif
	}

	SECTION("Invalid Cookie Line Test 3(windows  newlines)")
	{
		/*
		 * Content-type: text/html
		 * Cookie: foo=bar;
		 * Set-Cookie: yummy_cookie=choco
		 * Set-Cookie: tasty_cookie=strawberry
		 * X-Forwarded-For: 192.0.2.43, 2001:db8:cafe::17
		 * Forwarded: for=192.0.2.43, for="[2001:db8:cafe::17]
		 */
		KWebIO webIO;
		KString sHeaderPart1 = "Content-type: ";
		KString sHeaderPart2 = "text/html  \r\nCookie: ";
		KString sHeaderPart3 = "fubar";
		KString sHeaderPart4 = " ;= \r\nSet-Cookie: yummy_cooki";
		KString sHeaderPart5 = "e =choco;\r\nSet-Cookie: tasty_cookie=strawberry\r\n";
		KString sHeaderPart6 = "X-Forwarded-For: 192.0.2.43, 2001:";
		KString sHeaderPart7 = "db8:cafe::17,  192.168.1.100, 192.168.1.1\r";
		KString sHeaderPart8 = "\nForwarded: for=192.0.";
		KString sHeaderPart9 = "2.43, for='[2001:db8:cafe::17]'\r\n";
		KString sHeaderPart10 = "\r\n";
		webIO.Parse(sHeaderPart1);
		webIO.Parse(sHeaderPart2);
		webIO.Parse(sHeaderPart3);
		webIO.Parse(sHeaderPart4);
		webIO.Parse(sHeaderPart5);
		webIO.Parse(sHeaderPart6);
		webIO.Parse(sHeaderPart7);
		webIO.Parse(sHeaderPart8);
		webIO.Parse(sHeaderPart9);
		webIO.Parse(sHeaderPart10);

		KCurl::KHeader  sResponseHeaders = webIO.getResponseHeaders();
		CHECK_FALSE(sResponseHeaders.empty());
		CHECK(sResponseHeaders.size() == 6);
		CHECK(webIO.getResponseCookies().size() == 2);
		CHECK(sResponseHeaders.IsMulti("set-cookie"));
		CHECK(sResponseHeaders.Count("cookie") == 1);
		const KString& sCookieVal = webIO.getResponseCookie("muh");
		CHECK(sCookieVal.empty());
		CHECK(sResponseHeaders.Count("set-cookie") == 2);
		CHECK(sResponseHeaders.Count("content-type") == 1);
		const KString& sContentType = webIO.getResponseHeader("content-type");
		CHECK(sContentType.length() == 14);
		CHECK(sResponseHeaders.Count("forwarded") == 1);
		CHECK(sResponseHeaders.Count("x-forwarded-for") == 1);
#if kwebdebug
		// Verifying how header was parsed by printing
		std::cout << "    == Invalid Cookie Line Test 3 (windows  newlines) ==" << std::endl;
		webIO.Serialize();
#endif
	}

	SECTION("Invalid Cookie Line Test 4 (windows  newlines)")
	{
		/*
		 * Content-type: text/html
		 * Cookie: foo=bar;
		 * Set-Cookie: yummy_cookie=choco
		 * Set-Cookie: tasty_cookie=strawberry
		 * X-Forwarded-For: 192.0.2.43, 2001:db8:cafe::17
		 * Forwarded: for=192.0.2.43, for="[2001:db8:cafe::17]
		 */
		KWebIO webIO;
		KString sHeaderPart1 = "Content-type: ";
		KString sHeaderPart2 = "text/html  \r\nCookie: ";
		KString sHeaderPart3 = "fubar";
		KString sHeaderPart4 = "  =  ;  =  \r\nSet-Cookie: yummy_cooki";
		KString sHeaderPart5 = "e =choco;\r\nSet-Cookie: tasty_cookie=strawberry\r\n";
		KString sHeaderPart6 = "X-Forwarded-For: 192.0.2.43, 2001:";
		KString sHeaderPart7 = "db8:cafe::17,  192.168.1.100, 192.168.1.1\r";
		KString sHeaderPart8 = "\nForwarded: for=192.0.";
		KString sHeaderPart9 = "2.43, for='[2001:db8:cafe::17]'\r\n";
		KString sHeaderPart10 = "\r\n";
		webIO.Parse(sHeaderPart1);
		webIO.Parse(sHeaderPart2);
		webIO.Parse(sHeaderPart3);
		webIO.Parse(sHeaderPart4);
		webIO.Parse(sHeaderPart5);
		webIO.Parse(sHeaderPart6);
		webIO.Parse(sHeaderPart7);
		webIO.Parse(sHeaderPart8);
		webIO.Parse(sHeaderPart9);
		webIO.Parse(sHeaderPart10);

		KCurl::KHeader  sResponseHeaders = webIO.getResponseHeaders();
		CHECK_FALSE(sResponseHeaders.empty());
		CHECK(sResponseHeaders.size() == 6);
		CHECK(webIO.getResponseCookies().size() == 2);
		CHECK(sResponseHeaders.IsMulti("set-cookie"));
		CHECK(sResponseHeaders.Count("cookie") == 1);
		const KString& sCookieVal = webIO.getResponseCookie("muh");
		CHECK(sCookieVal.empty());
		CHECK(sResponseHeaders.Count("set-cookie") == 2);
		CHECK(sResponseHeaders.Count("content-type") == 1);
		const KString& sContentType = webIO.getResponseHeader("content-type");
		CHECK(sContentType.length() == 14);
		CHECK(sResponseHeaders.Count("forwarded") == 1);
		CHECK(sResponseHeaders.Count("x-forwarded-for") == 1);
#if kwebdebug
		//#if 1
		// Verifying how header was parsed by printing
		std::cout << "    == Invalid Cookie Line Test 4 (windows  newlines) ==" << std::endl;
		webIO.Serialize();
#endif
	}

	SECTION("Invalid Cookie Line Test 5 (windows  newlines)")
	{
		/*
		* Content-type: text/html
		* Cookie: foo=bar;
		* Set-Cookie: yummy_cookie=choco
		* Set-Cookie: tasty_cookie=strawberry
		* X-Forwarded-For: 192.0.2.43, 2001:db8:cafe::17
		* Forwarded: for=192.0.2.43, for="[2001:db8:cafe::17]
		*/
		KWebIO webIO;
		KString sHeaderPart1 = "Content-type: ";
		KString sHeaderPart2 = "text/html  \r\nCookie: ";
		KString sHeaderPart3 = "fubar";
		KString sHeaderPart4 = "  =  ;  =  ;\r\nSet-Cookie: yummy_cooki";
		KString sHeaderPart5 = "e =choco;\r\nSet-Cookie: tasty_cookie=strawberry\r\n";
		KString sHeaderPart6 = "X-Forwarded-For: 192.0.2.43, 2001:";
		KString sHeaderPart7 = "db8:cafe::17,  192.168.1.100, 192.168.1.1\r";
		KString sHeaderPart8 = "\nForwarded: for=192.0.";
		KString sHeaderPart9 = "2.43, for='[2001:db8:cafe::17]'\r\n";
		KString sHeaderPart10 = "\r\n";
		webIO.Parse(sHeaderPart1);
		webIO.Parse(sHeaderPart2);
		webIO.Parse(sHeaderPart3);
		webIO.Parse(sHeaderPart4);
		webIO.Parse(sHeaderPart5);
		webIO.Parse(sHeaderPart6);
		webIO.Parse(sHeaderPart7);
		webIO.Parse(sHeaderPart8);
		webIO.Parse(sHeaderPart9);
		webIO.Parse(sHeaderPart10);

		KCurl::KHeader  sResponseHeaders = webIO.getResponseHeaders();
		CHECK_FALSE(sResponseHeaders.empty());
		CHECK(sResponseHeaders.size() == 6);
		CHECK(webIO.getResponseCookies().size() == 2);
		CHECK(sResponseHeaders.IsMulti("set-cookie"));
		CHECK(sResponseHeaders.Count("cookie") == 1);
		const KString& sCookieVal = webIO.getResponseCookie("muh");
		CHECK(sCookieVal.empty());
		CHECK(sResponseHeaders.Count("set-cookie") == 2);
		CHECK(sResponseHeaders.Count("content-type") == 1);
		const KString& sContentType = webIO.getResponseHeader("content-type");
		CHECK(sContentType.length() == 14);
		CHECK(sResponseHeaders.Count("forwarded") == 1);
		CHECK(sResponseHeaders.Count("x-forwarded-for") == 1);
#if kwebdebug
		//#if 1
		// Verifying how header was parsed by printing
		std::cout << "    == Invalid Cookie Line Test 5 (windows  newlines) ==" << std::endl;
		webIO.Serialize();
#endif
	}

	SECTION("Invalid Cookie Line Test 6 (windows  newlines)")
	{
		/*
		* Content-type: text/html
		* Cookie: foo=bar;
		* Set-Cookie: yummy_cookie=choco
		* Set-Cookie: tasty_cookie=strawberry
		* X-Forwarded-For: 192.0.2.43, 2001:db8:cafe::17
		* Forwarded: for=192.0.2.43, for="[2001:db8:cafe::17]
		*/
		KWebIO webIO;
		KString sHeaderPart1 = "Content-type: ";
		KString sHeaderPart2 = "text/html  \r\nCookie: ";
		KString sHeaderPart3 = "fubar";
		KString sHeaderPart4 = "  =  ;  =  ; blah \r\nSet-Cookie: yummy_cooki";
		KString sHeaderPart5 = "e =choco;\r\nSet-Cookie: tasty_cookie=strawberry\r\n";
		KString sHeaderPart6 = "X-Forwarded-For: 192.0.2.43, 2001:";
		KString sHeaderPart7 = "db8:cafe::17,  192.168.1.100, 192.168.1.1\r";
		KString sHeaderPart8 = "\nForwarded: for=192.0.";
		KString sHeaderPart9 = "2.43, for='[2001:db8:cafe::17]'\r\n";
		KString sHeaderPart10 = "\r\n";
		webIO.Parse(sHeaderPart1);
		webIO.Parse(sHeaderPart2);
		webIO.Parse(sHeaderPart3);
		webIO.Parse(sHeaderPart4);
		webIO.Parse(sHeaderPart5);
		webIO.Parse(sHeaderPart6);
		webIO.Parse(sHeaderPart7);
		webIO.Parse(sHeaderPart8);
		webIO.Parse(sHeaderPart9);
		webIO.Parse(sHeaderPart10);

		KCurl::KHeader  sResponseHeaders = webIO.getResponseHeaders();
		CHECK_FALSE(sResponseHeaders.empty());
		CHECK(sResponseHeaders.size() == 6);
		CHECK(webIO.getResponseCookies().size() == 3);
		CHECK(sResponseHeaders.IsMulti("set-cookie"));
		CHECK(sResponseHeaders.Count("cookie") == 1);
		const KString& sCookieVal = webIO.getResponseCookie("muh");
		CHECK(sCookieVal.empty());
		CHECK(sResponseHeaders.Count("set-cookie") == 2);
		CHECK(sResponseHeaders.Count("content-type") == 1);
		const KString& sContentType = webIO.getResponseHeader("content-type");
		CHECK(sContentType.length() == 14);
		CHECK(sResponseHeaders.Count("forwarded") == 1);
		CHECK(sResponseHeaders.Count("x-forwarded-for") == 1);
#if kwebdebug
		// Verifying how header was parsed by printing
		std::cout << "    == Invalid Cookie Line Test 6 (windows  newlines) ==" << std::endl;
		webIO.Serialize();
#endif
	}

	SECTION("Invalid Cookie Line Test 7 (windows  newlines)")
	{
		/*
		* Content-type: text/html
		* Cookie: foo=bar;
		* Set-Cookie: yummy_cookie=choco
		* Set-Cookie: tasty_cookie=strawberry
		* X-Forwarded-For: 192.0.2.43, 2001:db8:cafe::17
		* Forwarded: for=192.0.2.43, for="[2001:db8:cafe::17]
		*/
		KWebIO webIO;
		KString sHeaderPart1 = "Content-type: ";
		KString sHeaderPart2 = "text/html  \r\nCookie: ";
		KString sHeaderPart3 = "";
		KString sHeaderPart4 = "\r\nSet-Cookie: yummy_cooki";
		KString sHeaderPart5 = "e =choco;\r\nSet-Cookie: tasty_cookie=strawberry\r\n";
		KString sHeaderPart6 = "X-Forwarded-For: 192.0.2.43, 2001:";
		KString sHeaderPart7 = "db8:cafe::17,  192.168.1.100, 192.168.1.1\r";
		KString sHeaderPart8 = "\nForwarded: for=192.0.";
		KString sHeaderPart9 = "2.43, for='[2001:db8:cafe::17]'\r\n";
		KString sHeaderPart10 = "\r\n";
		webIO.Parse(sHeaderPart1);
		webIO.Parse(sHeaderPart2);
		webIO.Parse(sHeaderPart3);
		webIO.Parse(sHeaderPart4);
		webIO.Parse(sHeaderPart5);
		webIO.Parse(sHeaderPart6);
		webIO.Parse(sHeaderPart7);
		webIO.Parse(sHeaderPart8);
		webIO.Parse(sHeaderPart9);
		webIO.Parse(sHeaderPart10);

		KCurl::KHeader  sResponseHeaders = webIO.getResponseHeaders();
		CHECK_FALSE(sResponseHeaders.empty());
		CHECK(sResponseHeaders.size() == 6);
		CHECK(webIO.getResponseCookies().size() == 1);
		CHECK(sResponseHeaders.IsMulti("set-cookie"));
		CHECK(sResponseHeaders.Count("cookie") == 1);
		const KString& sCookieVal = webIO.getResponseCookie("muh");
		CHECK(sCookieVal.empty());
		CHECK(sResponseHeaders.Count("set-cookie") == 2);
		CHECK(sResponseHeaders.Count("content-type") == 1);
		const KString& sContentType = webIO.getResponseHeader("content-type");
		CHECK(sContentType.length() == 14);
		CHECK(sResponseHeaders.Count("forwarded") == 1);
		CHECK(sResponseHeaders.Count("x-forwarded-for") == 1);
#if kwebdebug
		// Verifying how header was parsed by printing
		std::cout << "    == Invalid Cookie Line Test 7 (windows  newlines) ==" << std::endl;
		webIO.Serialize();
#endif
	}

	SECTION("Invalid Cookie Line Test 8 (windows  newlines)")
	{
		/*
		* Content-type: text/html
		* Cookie: foo=bar;
		* Set-Cookie: yummy_cookie=choco
		* Set-Cookie: tasty_cookie=strawberry
		* X-Forwarded-For: 192.0.2.43, 2001:db8:cafe::17
		* Forwarded: for=192.0.2.43, for="[2001:db8:cafe::17]
		*/
		KWebIO webIO;
		KString sHeaderPart1 = "Content-type: ";
		KString sHeaderPart2 = "text/html  \r\nCookie: ";
		KString sHeaderPart3 = "foo";
		KString sHeaderPart4 = "\r\nSet-Cookie: yummy_cooki";
		KString sHeaderPart5 = "e =choco;\r\nSet-Cookie: tasty_cookie=strawberry\r\n";
		KString sHeaderPart6 = "X-Forwarded-For: 192.0.2.43, 2001:";
		KString sHeaderPart7 = "db8:cafe::17,  192.168.1.100, 192.168.1.1\r";
		KString sHeaderPart8 = "\nForwarded: for=192.0.";
		KString sHeaderPart9 = "2.43, for='[2001:db8:cafe::17]'\r\n";
		KString sHeaderPart10 = "\r\n";
		webIO.Parse(sHeaderPart1);
		webIO.Parse(sHeaderPart2);
		webIO.Parse(sHeaderPart3);
		webIO.Parse(sHeaderPart4);
		webIO.Parse(sHeaderPart5);
		webIO.Parse(sHeaderPart6);
		webIO.Parse(sHeaderPart7);
		webIO.Parse(sHeaderPart8);
		webIO.Parse(sHeaderPart9);
		webIO.Parse(sHeaderPart10);

		KCurl::KHeader  sResponseHeaders = webIO.getResponseHeaders();
		CHECK_FALSE(sResponseHeaders.empty());
		CHECK(sResponseHeaders.size() == 6);
		CHECK(webIO.getResponseCookies().size() == 1);
		CHECK(sResponseHeaders.IsMulti("set-cookie"));
		CHECK(sResponseHeaders.Count("cookie") == 1);
		const KString& sCookieVal = webIO.getResponseCookie("muh");
		CHECK(sCookieVal.empty());
		CHECK(sResponseHeaders.Count("set-cookie") == 2);
		CHECK(sResponseHeaders.Count("content-type") == 1);
		const KString& sContentType = webIO.getResponseHeader("content-type");
		CHECK(sContentType.length() == 14);
		CHECK(sResponseHeaders.Count("forwarded") == 1);
		CHECK(sResponseHeaders.Count("x-forwarded-for") == 1);
#if kwebdebug
		// Verifying how header was parsed by printing
		std::cout << "    == Invalid Cookie Line Test 8 (windows  newlines) ==" << std::endl;
		webIO.Serialize();
#endif
	}

	SECTION("Invalid Cookie Line Test (unix  newlines)")
	{
		/*
		 * Content-type: text/html
		 * Cookie: foo=bar;
		 * Set-Cookie: yummy_cookie=choco
		 * Set-Cookie: tasty_cookie=strawberry
		 * X-Forwarded-For: 192.0.2.43, 2001:db8:cafe::17
		 * Forwarded: for=192.0.2.43, for="[2001:db8:cafe::17]
		 */
		KWebIO webIO;
		KString sHeaderPart1 = "Content-type: ";
		KString sHeaderPart2 = "text/html  \nCookie: ";
		KString sHeaderPart3 = "fubar";
		KString sHeaderPart4 = " \nSet-Cookie: yummy_cooki";
		KString sHeaderPart5 = "e =choco;\nSet-Cookie: tasty_cookie=strawberry\n";
		KString sHeaderPart6 = "X-Forwarded-For: 192.0.2.43, 2001:";
		KString sHeaderPart7 = "db8:cafe::17,  192.168.1.100, 192.168.1.1";
		KString sHeaderPart8 = "\nForwarded: for=192.0.";
		KString sHeaderPart9 = "2.43, for='[2001:db8:cafe::17]'\n";
		KString sHeaderPart10 = "\n";
		webIO.Parse(sHeaderPart1);
		webIO.Parse(sHeaderPart2);
		webIO.Parse(sHeaderPart3);
		webIO.Parse(sHeaderPart4);
		webIO.Parse(sHeaderPart5);
		webIO.Parse(sHeaderPart6);
		webIO.Parse(sHeaderPart7);
		webIO.Parse(sHeaderPart8);
		webIO.Parse(sHeaderPart9);
		webIO.Parse(sHeaderPart10);

		KCurl::KHeader  sResponseHeaders = webIO.getResponseHeaders();
		CHECK_FALSE(sResponseHeaders.empty());
		CHECK(sResponseHeaders.size() == 6);
		CHECK(webIO.getResponseCookies().size() == 1);
		CHECK(sResponseHeaders.IsMulti("set-cookie"));
		CHECK(sResponseHeaders.Count("cookie") == 1);
		const KString& sCookieVal = webIO.getResponseCookie("muh");
		CHECK(sCookieVal.empty());
		CHECK(sResponseHeaders.Count("set-cookie") == 2);
		CHECK(sResponseHeaders.Count("content-type") == 1);
		const KString& sContentType = webIO.getResponseHeader("content-type");
		CHECK(sContentType.length() == 13);
		CHECK(sResponseHeaders.Count("forwarded") == 1);
		CHECK(sResponseHeaders.Count("x-forwarded-for") == 1);
#if kwebdebug
		// Verifying how header was parsed by printing
		std::cout << "    == Invalid Cookie Line Test (unix  newlines) ==" << std::endl;
		webIO.Serialize();
#endif
	}

	SECTION("Invalid Cookie Line Test 2(unix  newlines)")
	{
		/*
		 * Content-type: text/html
		 * Cookie: foo=bar;
		 * Set-Cookie: yummy_cookie=choco
		 * Set-Cookie: tasty_cookie=strawberry
		 * X-Forwarded-For: 192.0.2.43, 2001:db8:cafe::17
		 * Forwarded: for=192.0.2.43, for="[2001:db8:cafe::17]
		 */
		KWebIO webIO;
		KString sHeaderPart1 = "Content-type: ";
		KString sHeaderPart2 = "text/html  \nCookie: ";
		KString sHeaderPart3 = "fubar";
		KString sHeaderPart4 = " ; \nSet-Cookie: yummy_cooki";
		KString sHeaderPart5 = "e =choco;\nSet-Cookie: tasty_cookie=strawberry\n";
		KString sHeaderPart6 = "X-Forwarded-For: 192.0.2.43, 2001:";
		KString sHeaderPart7 = "db8:cafe::17,  192.168.1.100, 192.168.1.1";
		KString sHeaderPart8 = "\nForwarded: for=192.0.";
		KString sHeaderPart9 = "2.43, for='[2001:db8:cafe::17]'\n";
		KString sHeaderPart10 = "\n";
		webIO.Parse(sHeaderPart1);
		webIO.Parse(sHeaderPart2);
		webIO.Parse(sHeaderPart3);
		webIO.Parse(sHeaderPart4);
		webIO.Parse(sHeaderPart5);
		webIO.Parse(sHeaderPart6);
		webIO.Parse(sHeaderPart7);
		webIO.Parse(sHeaderPart8);
		webIO.Parse(sHeaderPart9);
		webIO.Parse(sHeaderPart10);

		KCurl::KHeader  sResponseHeaders = webIO.getResponseHeaders();
		CHECK_FALSE(sResponseHeaders.empty());
		CHECK(sResponseHeaders.size() == 6);
		CHECK(webIO.getResponseCookies().size() == 1);
		CHECK(sResponseHeaders.IsMulti("set-cookie"));
		CHECK(sResponseHeaders.Count("cookie") == 1);
		const KString& sCookieVal = webIO.getResponseCookie("muh");
		CHECK(sCookieVal.empty());
		CHECK(sResponseHeaders.Count("set-cookie") == 2);
		CHECK(sResponseHeaders.Count("content-type") == 1);
		const KString& sContentType = webIO.getResponseHeader("content-type");
		CHECK(sContentType.length() == 13);
		CHECK(sResponseHeaders.Count("forwarded") == 1);
		CHECK(sResponseHeaders.Count("x-forwarded-for") == 1);
#if kwebdebug
		// Verifying how header was parsed by printing
		std::cout << "    == Invalid Cookie Line Test 2 (unix  newlines) ==" << std::endl;
		webIO.Serialize();
#endif
	}

	SECTION("Invalid Cookie Line Test 3(unix  newlines)")
	{
		/*
		 * Content-type: text/html
		 * Cookie: foo=bar;
		 * Set-Cookie: yummy_cookie=choco
		 * Set-Cookie: tasty_cookie=strawberry
		 * X-Forwarded-For: 192.0.2.43, 2001:db8:cafe::17
		 * Forwarded: for=192.0.2.43, for="[2001:db8:cafe::17]
		 */
		KWebIO webIO;
		KString sHeaderPart1 = "Content-type: ";
		KString sHeaderPart2 = "text/html  \nCookie: ";
		KString sHeaderPart3 = "fubar";
		KString sHeaderPart4 = " ;= \nSet-Cookie: yummy_cooki";
		KString sHeaderPart5 = "e =choco;\nSet-Cookie: tasty_cookie=strawberry\n";
		KString sHeaderPart6 = "X-Forwarded-For: 192.0.2.43, 2001:";
		KString sHeaderPart7 = "db8:cafe::17,  192.168.1.100, 192.168.1.1";
		KString sHeaderPart8 = "\nForwarded: for=192.0.";
		KString sHeaderPart9 = "2.43, for='[2001:db8:cafe::17]'\n";
		KString sHeaderPart10 = "\n";
		webIO.Parse(sHeaderPart1);
		webIO.Parse(sHeaderPart2);
		webIO.Parse(sHeaderPart3);
		webIO.Parse(sHeaderPart4);
		webIO.Parse(sHeaderPart5);
		webIO.Parse(sHeaderPart6);
		webIO.Parse(sHeaderPart7);
		webIO.Parse(sHeaderPart8);
		webIO.Parse(sHeaderPart9);
		webIO.Parse(sHeaderPart10);

		KCurl::KHeader  sResponseHeaders = webIO.getResponseHeaders();
		CHECK_FALSE(sResponseHeaders.empty());
		CHECK(sResponseHeaders.size() == 6);
		CHECK(webIO.getResponseCookies().size() == 2);
		CHECK(sResponseHeaders.IsMulti("set-cookie"));
		CHECK(sResponseHeaders.Count("cookie") == 1);
		const KString& sCookieVal = webIO.getResponseCookie("muh");
		CHECK(sCookieVal.empty());
		CHECK(sResponseHeaders.Count("set-cookie") == 2);
		CHECK(sResponseHeaders.Count("content-type") == 1);
		const KString& sContentType = webIO.getResponseHeader("content-type");
		CHECK(sContentType.length() == 13);
		CHECK(sResponseHeaders.Count("forwarded") == 1);
		CHECK(sResponseHeaders.Count("x-forwarded-for") == 1);
#if kwebdebug
		// Verifying how header was parsed by printing
		std::cout << "    == Invalid Cookie Line Test 3 (unix  newlines) ==" << std::endl;
		webIO.Serialize();
#endif
	}

	SECTION("Invalid Cookie Line Test 4 (unix  newlines)")
	{
		/*
		 * Content-type: text/html
		 * Cookie: foo=bar;
		 * Set-Cookie: yummy_cookie=choco
		 * Set-Cookie: tasty_cookie=strawberry
		 * X-Forwarded-For: 192.0.2.43, 2001:db8:cafe::17
		 * Forwarded: for=192.0.2.43, for="[2001:db8:cafe::17]
		 */
		KWebIO webIO;
		KString sHeaderPart1 = "Content-type: ";
		KString sHeaderPart2 = "text/html  \nCookie: ";
		KString sHeaderPart3 = "fubar";
		KString sHeaderPart4 = "  =  ;  =  \nSet-Cookie: yummy_cooki";
		KString sHeaderPart5 = "e =choco;\nSet-Cookie: tasty_cookie=strawberry\n";
		KString sHeaderPart6 = "X-Forwarded-For: 192.0.2.43, 2001:";
		KString sHeaderPart7 = "db8:cafe::17,  192.168.1.100, 192.168.1.1";
		KString sHeaderPart8 = "\nForwarded: for=192.0.";
		KString sHeaderPart9 = "2.43, for='[2001:db8:cafe::17]'\n";
		KString sHeaderPart10 = "\n";
		webIO.Parse(sHeaderPart1);
		webIO.Parse(sHeaderPart2);
		webIO.Parse(sHeaderPart3);
		webIO.Parse(sHeaderPart4);
		webIO.Parse(sHeaderPart5);
		webIO.Parse(sHeaderPart6);
		webIO.Parse(sHeaderPart7);
		webIO.Parse(sHeaderPart8);
		webIO.Parse(sHeaderPart9);
		webIO.Parse(sHeaderPart10);

		KCurl::KHeader  sResponseHeaders = webIO.getResponseHeaders();
		CHECK_FALSE(sResponseHeaders.empty());
		CHECK(sResponseHeaders.size() == 6);
		CHECK(webIO.getResponseCookies().size() == 2);
		CHECK(sResponseHeaders.IsMulti("set-cookie"));
		CHECK(sResponseHeaders.Count("cookie") == 1);
		const KString& sCookieVal = webIO.getResponseCookie("muh");
		CHECK(sCookieVal.empty());
		CHECK(sResponseHeaders.Count("set-cookie") == 2);
		CHECK(sResponseHeaders.Count("content-type") == 1);
		const KString& sContentType = webIO.getResponseHeader("content-type");
		CHECK(sContentType.length() == 13);
		CHECK(sResponseHeaders.Count("forwarded") == 1);
		CHECK(sResponseHeaders.Count("x-forwarded-for") == 1);
#if kwebdebug
		// Verifying how header was parsed by printing
		std::cout << "    == Invalid Cookie Line Test 4 (unix  newlines) ==" << std::endl;
		webIO.Serialize();
#endif
	}

	SECTION("Invalid Cookie Line Test 5 (unix  newlines)")
	{
		/*
		* Content-type: text/html
		* Cookie: foo=bar;
		* Set-Cookie: yummy_cookie=choco
		* Set-Cookie: tasty_cookie=strawberry
		* X-Forwarded-For: 192.0.2.43, 2001:db8:cafe::17
		* Forwarded: for=192.0.2.43, for="[2001:db8:cafe::17]
		*/
		KWebIO webIO;
		KString sHeaderPart1 = "Content-type: ";
		KString sHeaderPart2 = "text/html  \nCookie: ";
		KString sHeaderPart3 = "fubar";
		KString sHeaderPart4 = "  =  ;  =  ;  \nSet-Cookie: yummy_cooki";
		KString sHeaderPart5 = "e =choco;\nSet-Cookie: tasty_cookie=strawberry\n";
		KString sHeaderPart6 = "X-Forwarded-For: 192.0.2.43, 2001:";
		KString sHeaderPart7 = "db8:cafe::17,  192.168.1.100, 192.168.1.1";
		KString sHeaderPart8 = "\nForwarded: for=192.0.";
		KString sHeaderPart9 = "2.43, for='[2001:db8:cafe::17]'\n";
		KString sHeaderPart10 = "\n";
		webIO.Parse(sHeaderPart1);
		webIO.Parse(sHeaderPart2);
		webIO.Parse(sHeaderPart3);
		webIO.Parse(sHeaderPart4);
		webIO.Parse(sHeaderPart5);
		webIO.Parse(sHeaderPart6);
		webIO.Parse(sHeaderPart7);
		webIO.Parse(sHeaderPart8);
		webIO.Parse(sHeaderPart9);
		webIO.Parse(sHeaderPart10);

		KCurl::KHeader  sResponseHeaders = webIO.getResponseHeaders();
		CHECK_FALSE(sResponseHeaders.empty());
		CHECK(sResponseHeaders.size() == 6);
		CHECK(webIO.getResponseCookies().size() == 2);
		CHECK(sResponseHeaders.IsMulti("set-cookie"));
		CHECK(sResponseHeaders.Count("cookie") == 1);
		const KString& sCookieVal = webIO.getResponseCookie("muh");
		CHECK(sCookieVal.empty());
		CHECK(sResponseHeaders.Count("set-cookie") == 2);
		CHECK(sResponseHeaders.Count("content-type") == 1);
		const KString& sContentType = webIO.getResponseHeader("content-type");
		CHECK(sContentType.length() == 13);
		CHECK(sResponseHeaders.Count("forwarded") == 1);
		CHECK(sResponseHeaders.Count("x-forwarded-for") == 1);
#if kwebdebug
		// Verifying how header was parsed by printing
		std::cout << "    == Invalid Cookie Line Test 5 (unix  newlines) ==" << std::endl;
		webIO.Serialize();
#endif
	}

	SECTION("Invalid Cookie Line Test 6 (unix  newlines)")
	{
		/*
		* Content-type: text/html
		* Cookie: foo=bar;
		* Set-Cookie: yummy_cookie=choco
		* Set-Cookie: tasty_cookie=strawberry
		* X-Forwarded-For: 192.0.2.43, 2001:db8:cafe::17
		* Forwarded: for=192.0.2.43, for="[2001:db8:cafe::17]
		*/
		KWebIO webIO;
		KString sHeaderPart1 = "Content-type: ";
		KString sHeaderPart2 = "text/html  \nCookie: ";
		KString sHeaderPart3 = "fubar";
		KString sHeaderPart4 = "  =  ;  =  ; blah \nSet-Cookie: yummy_cooki";
		KString sHeaderPart5 = "e =choco;\nSet-Cookie: tasty_cookie=strawberry\n";
		KString sHeaderPart6 = "X-Forwarded-For: 192.0.2.43, 2001:";
		KString sHeaderPart7 = "db8:cafe::17,  192.168.1.100, 192.168.1.1";
		KString sHeaderPart8 = "\nForwarded: for=192.0.";
		KString sHeaderPart9 = "2.43, for='[2001:db8:cafe::17]'\n";
		KString sHeaderPart10 = "\n";
		webIO.Parse(sHeaderPart1);
		webIO.Parse(sHeaderPart2);
		webIO.Parse(sHeaderPart3);
		webIO.Parse(sHeaderPart4);
		webIO.Parse(sHeaderPart5);
		webIO.Parse(sHeaderPart6);
		webIO.Parse(sHeaderPart7);
		webIO.Parse(sHeaderPart8);
		webIO.Parse(sHeaderPart9);
		webIO.Parse(sHeaderPart10);

		KCurl::KHeader  sResponseHeaders = webIO.getResponseHeaders();
		CHECK_FALSE(sResponseHeaders.empty());
		CHECK(sResponseHeaders.size() == 6);
		CHECK(webIO.getResponseCookies().size() == 3);
		CHECK(sResponseHeaders.IsMulti("set-cookie"));
		CHECK(sResponseHeaders.Count("cookie") == 1);
		const KString& sCookieVal = webIO.getResponseCookie("muh");
		CHECK(sCookieVal.empty());
		CHECK(sResponseHeaders.Count("set-cookie") == 2);
		CHECK(sResponseHeaders.Count("content-type") == 1);
		const KString& sContentType = webIO.getResponseHeader("content-type");
		CHECK(sContentType.length() == 13);
		CHECK(sResponseHeaders.Count("forwarded") == 1);
		CHECK(sResponseHeaders.Count("x-forwarded-for") == 1);
#if kwebdebug
		// Verifying how header was parsed by printing
		std::cout << "    == Invalid Cookie Line Test 6 (unix  newlines) ==" << std::endl;
		webIO.Serialize();
#endif
	}

	SECTION("Invalid Cookie Line Test 7 (unix  newlines)")
	{
		/*
		* Content-type: text/html
		* Cookie: foo=bar;
		* Set-Cookie: yummy_cookie=choco
		* Set-Cookie: tasty_cookie=strawberry
		* X-Forwarded-For: 192.0.2.43, 2001:db8:cafe::17
		* Forwarded: for=192.0.2.43, for="[2001:db8:cafe::17]
		*/
		KWebIO webIO;
		KString sHeaderPart1 = "Content-type: ";
		KString sHeaderPart2 = "text/html  \nCookie: ";
		KString sHeaderPart3 = "";
		KString sHeaderPart4 = "\nSet-Cookie: yummy_cooki";
		KString sHeaderPart5 = "e =choco;\nSet-Cookie: tasty_cookie=strawberry\n";
		KString sHeaderPart6 = "X-Forwarded-For: 192.0.2.43, 2001:";
		KString sHeaderPart7 = "db8:cafe::17,  192.168.1.100, 192.168.1.1";
		KString sHeaderPart8 = "\nForwarded: for=192.0.";
		KString sHeaderPart9 = "2.43, for='[2001:db8:cafe::17]'\n";
		KString sHeaderPart10 = "\n";
		webIO.Parse(sHeaderPart1);
		webIO.Parse(sHeaderPart2);
		webIO.Parse(sHeaderPart3);
		webIO.Parse(sHeaderPart4);
		webIO.Parse(sHeaderPart5);
		webIO.Parse(sHeaderPart6);
		webIO.Parse(sHeaderPart7);
		webIO.Parse(sHeaderPart8);
		webIO.Parse(sHeaderPart9);
		webIO.Parse(sHeaderPart10);

		KCurl::KHeader  sResponseHeaders = webIO.getResponseHeaders();
		CHECK_FALSE(sResponseHeaders.empty());
		CHECK(sResponseHeaders.size() == 6);
		CHECK(webIO.getResponseCookies().size() == 1);
		CHECK(sResponseHeaders.IsMulti("set-cookie"));
		CHECK(sResponseHeaders.Count("cookie") == 1);
		const KString& sCookieVal = webIO.getResponseCookie("muh");
		CHECK(sCookieVal.empty());
		CHECK(sResponseHeaders.Count("set-cookie") == 2);
		CHECK(sResponseHeaders.Count("content-type") == 1);
		const KString& sContentType = webIO.getResponseHeader("content-type");
		CHECK(sContentType.length() == 13);
		CHECK(sResponseHeaders.Count("forwarded") == 1);
		CHECK(sResponseHeaders.Count("x-forwarded-for") == 1);
#if kwebdebug
		// Verifying how header was parsed by printing
		std::cout << "    == Invalid Cookie Line Test 7 (unix  newlines) ==" << std::endl;
		webIO.Serialize();
#endif
	}

	SECTION("Invalid Cookie Line Test 8 (unix  newlines)")
	{
		/*
		* Content-type: text/html
		* Cookie: foo=bar;
		* Set-Cookie: yummy_cookie=choco
		* Set-Cookie: tasty_cookie=strawberry
		* X-Forwarded-For: 192.0.2.43, 2001:db8:cafe::17
		* Forwarded: for=192.0.2.43, for="[2001:db8:cafe::17]
		*/
		KWebIO webIO;
		KString sHeaderPart1 = "Content-type: ";
		KString sHeaderPart2 = "text/html  \nCookie: ";
		KString sHeaderPart3 = "foo";
		KString sHeaderPart4 = "\nSet-Cookie: yummy_cooki";
		KString sHeaderPart5 = "e =choco;\nSet-Cookie: tasty_cookie=strawberry\n";
		KString sHeaderPart6 = "X-Forwarded-For: 192.0.2.43, 2001:";
		KString sHeaderPart7 = "db8:cafe::17,  192.168.1.100, 192.168.1.1";
		KString sHeaderPart8 = "\nForwarded: for=192.0.";
		KString sHeaderPart9 = "2.43, for='[2001:db8:cafe::17]'\n";
		KString sHeaderPart10 = "\n";
		webIO.Parse(sHeaderPart1);
		webIO.Parse(sHeaderPart2);
		webIO.Parse(sHeaderPart3);
		webIO.Parse(sHeaderPart4);
		webIO.Parse(sHeaderPart5);
		webIO.Parse(sHeaderPart6);
		webIO.Parse(sHeaderPart7);
		webIO.Parse(sHeaderPart8);
		webIO.Parse(sHeaderPart9);
		webIO.Parse(sHeaderPart10);

		KCurl::KHeader  sResponseHeaders = webIO.getResponseHeaders();
		CHECK_FALSE(sResponseHeaders.empty());
		CHECK(sResponseHeaders.size() == 6);
		CHECK(webIO.getResponseCookies().size() == 1);
		CHECK(sResponseHeaders.IsMulti("set-cookie"));
		CHECK(sResponseHeaders.Count("cookie") == 1);
		const KString& sCookieVal = webIO.getResponseCookie("muh");
		CHECK(sCookieVal.empty());
		CHECK(sResponseHeaders.Count("set-cookie") == 2);
		CHECK(sResponseHeaders.Count("content-type") == 1);
		const KString& sContentType = webIO.getResponseHeader("content-type");
		CHECK(sContentType.length() == 13);
		CHECK(sResponseHeaders.Count("forwarded") == 1);
		CHECK(sResponseHeaders.Count("x-forwarded-for") == 1);
#if kwebdebug
		// Verifying how header was parsed by printing
		std::cout << "    == Invalid Cookie Line Test 8 (unix  newlines) ==" << std::endl;
		webIO.Serialize();
#endif
	}

	SECTION("Invalid Header Line Test (windows  newlines)")
	{
		/*
		 * Content-type: text/html
		 * Cookie: foo=bar;
		 * Set-Cookie: yummy_cookie=choco
		 * Set-Cookie: tasty_cookie=strawberry
		 * X-Forwarded-For: 192.0.2.43, 2001:db8:cafe::17
		 * Forwarded: for=192.0.2.43, for="[2001:db8:cafe::17]
		 */
		KWebIO webIO;
		KString sHeaderPart1 = "HTTP/1.1 200 OK\r\nContent-type: ";
		KString sHeaderPart2 = "text/html  \r\nCookie: ";
		KString sHeaderPart3 = "fubar";
		KString sHeaderPart4 = " \r\nSet-Cookie: yummy_cooki";
		KString sHeaderPart5 = "e =choco;\r\nSet-Cookie: tasty_cookie=strawberry\r\n";
		KString sHeaderPart6 = "Randombadheader = really badd header no colon \r\n with continuation lines \r\n by lines I meant lines \r\nX-Forwarded-For: 192.0.2.43, 2001:";
		KString sHeaderPart7 = "db8:cafe::17,  192.168.1.100, 192.168.1.1\r";
		KString sHeaderPart8 = "\nForwarded: for=192.0.";
		KString sHeaderPart9 = "2.43, for='[2001:db8:cafe::17]'\r\n";
		KString sHeaderPart10 = "\r\n";
		webIO.Parse(sHeaderPart1);
		webIO.Parse(sHeaderPart2);
		webIO.Parse(sHeaderPart3);
		webIO.Parse(sHeaderPart4);
		webIO.Parse(sHeaderPart5);
		webIO.Parse(sHeaderPart6);
		webIO.Parse(sHeaderPart7);
		webIO.Parse(sHeaderPart8);
		webIO.Parse(sHeaderPart9);
		webIO.Parse(sHeaderPart10);

		KCurl::KHeader  sResponseHeaders = webIO.getResponseHeaders();
		CHECK_FALSE(sResponseHeaders.empty());
		CHECK(sResponseHeaders.size() == 7);
		CHECK(webIO.getResponseCookies().size() == 1);
		CHECK(sResponseHeaders.Count(KWebIO::svBrokenHeader) == 1);
		CHECK(sResponseHeaders.IsMulti("set-cookie"));
		CHECK(sResponseHeaders.Count("cookie") == 1);
		const KString& sCookieVal = webIO.getResponseCookie("muh");
		CHECK(sCookieVal.empty());
		CHECK(sResponseHeaders.Count("set-cookie") == 2);
		CHECK(sResponseHeaders.Count("content-type") == 1);
		const KString& sContentType = webIO.getResponseHeader("content-type");
		CHECK(sContentType.length() == 14);
		CHECK(sResponseHeaders.Count("forwarded") == 1);
		CHECK(sResponseHeaders.Count("x-forwarded-for") == 1);
#if kwebdebug
		// Verifying how header was parsed by printing
		std::cout << "    == Invalid Header Line Test (windows  newlines) ==" << std::endl;
		webIO.Serialize();
#endif
	}

	SECTION("Invalid Header Line Test (unix  newlines)")
	{
		/*
		 * Content-type: text/html
		 * Cookie: foo=bar;
		 * Set-Cookie: yummy_cookie=choco
		 * Set-Cookie: tasty_cookie=strawberry
		 * X-Forwarded-For: 192.0.2.43, 2001:db8:cafe::17
		 * Forwarded: for=192.0.2.43, for="[2001:db8:cafe::17]
		 */
		KWebIO webIO;
		KString sHeaderPart1 = "HTTP/1.1 200 OK\nContent-type: ";
		KString sHeaderPart2 = "text/html  \nCookie: ";
		KString sHeaderPart3 = "fubar";
		KString sHeaderPart4 = " \nSet-Cookie: yummy_cooki";
		KString sHeaderPart5 = "e =choco;\nSet-Cookie: tasty_cookie=strawberry\n";
		KString sHeaderPart6 = "Randombadheader = really badd header no colon \n with continuation lines \n by lines I meant lines \nX-Forwarded-For: 192.0.2.43, 2001:";
		KString sHeaderPart7 = "db8:cafe::17,  192.168.1.100, 192.168.1.1";
		KString sHeaderPart8 = "\nForwarded: for=192.0.";
		KString sHeaderPart9 = "2.43, for='[2001:db8:cafe::17]'\n";
		KString sHeaderPart10 = "\n";
		webIO.Parse(sHeaderPart1);
		webIO.Parse(sHeaderPart2);
		webIO.Parse(sHeaderPart3);
		webIO.Parse(sHeaderPart4);
		webIO.Parse(sHeaderPart5);
		webIO.Parse(sHeaderPart6);
		webIO.Parse(sHeaderPart7);
		webIO.Parse(sHeaderPart8);
		webIO.Parse(sHeaderPart9);
		webIO.Parse(sHeaderPart10);

		KCurl::KHeader  sResponseHeaders = webIO.getResponseHeaders();
		CHECK_FALSE(sResponseHeaders.empty());
		CHECK(sResponseHeaders.size() == 7);
		CHECK(webIO.getResponseCookies().size() == 1);
		CHECK(sResponseHeaders.Count(KWebIO::svBrokenHeader) == 1);
		CHECK(sResponseHeaders.IsMulti("set-cookie"));
		CHECK(sResponseHeaders.Count("cookie") == 1);
		const KString& sCookieVal = webIO.getResponseCookie("muh");
		CHECK(sCookieVal.empty());
		CHECK(sResponseHeaders.Count("set-cookie") == 2);
		CHECK(sResponseHeaders.Count("content-type") == 1);
		const KString& sContentType = webIO.getResponseHeader("content-type");
		CHECK(sContentType.length() == 13);
		CHECK(sResponseHeaders.Count("forwarded") == 1);
		CHECK(sResponseHeaders.Count("x-forwarded-for") == 1);
#if kwebdebug
		// Verifying how header was parsed by printing
		std::cout << "    == Invalid Header Line Test (unix  newlines) ==" << std::endl;
		webIO.Serialize();
#endif
	}

	SECTION("Valid Last Header Line Test (windows  newlines)")
	{
		/*
		 * Content-type: text/html
		 * Cookie: foo=bar;
		 * Set-Cookie: yummy_cookie=choco
		 * Set-Cookie: tasty_cookie=strawberry
		 * X-Forwarded-For: 192.0.2.43, 2001:db8:cafe::17
		 * Forwarded: for=192.0.2.43, for="[2001:db8:cafe::17]
		 */
		KWebIO webIO;
		KString sHeaderPart1 = "Content-type: ";
		KString sHeaderPart2 = "text/html  \r\nCookie: ";
		KString sHeaderPart3 = "fubar";
		KString sHeaderPart4 = " \r\nSet-Cookie: yummy_cooki";
		KString sHeaderPart5 = "e =choco;\r\nSet-Cookie: tasty_cookie=strawberry\r\n";
		KString sHeaderPart6 = "Randombadheader = really badd header no colon \r\n with continuation lines \r\n by lines I meant lines \r\nX-Forwarded-For: 192.0.2.43, 2001:";
		KString sHeaderPart7 = "db8:cafe::17,  192.168.1.100, 192.168.1.1\r";
		KString sHeaderPart8 = "\nForwarded: for=192.0.";
		KString sHeaderPart9 = "2.43, for='[2001:db8:cafe::17]'\r\n\r\n";
		//KString sHeaderPart10 = "\r\n";
		webIO.Parse(sHeaderPart1);
		webIO.Parse(sHeaderPart2);
		webIO.Parse(sHeaderPart3);
		webIO.Parse(sHeaderPart4);
		webIO.Parse(sHeaderPart5);
		webIO.Parse(sHeaderPart6);
		webIO.Parse(sHeaderPart7);
		webIO.Parse(sHeaderPart8);
		webIO.Parse(sHeaderPart9);
		//webIO.Parse(sHeaderPart10);

		KCurl::KHeader  sResponseHeaders = webIO.getResponseHeaders();
		CHECK_FALSE(sResponseHeaders.empty());
		CHECK(sResponseHeaders.size() == 7);
		CHECK(webIO.getResponseCookies().size() == 1);
		CHECK(sResponseHeaders.Count(KWebIO::svBrokenHeader) == 1);
		CHECK(sResponseHeaders.IsMulti("set-cookie"));
		CHECK(sResponseHeaders.Count("cookie") == 1);
		const KString& sCookieVal = webIO.getResponseCookie("muh");
		CHECK(sCookieVal.empty());
		CHECK(sResponseHeaders.Count("set-cookie") == 2);
		CHECK(sResponseHeaders.Count("content-type") == 1);
		const KString& sContentType = webIO.getResponseHeader("content-type");
		CHECK(sContentType.length() == 14);
		CHECK(sResponseHeaders.Count("forwarded") == 1);
		CHECK(sResponseHeaders.Count("x-forwarded-for") == 1);
#if kwebdebug
		// Verifying how header was parsed by printing
		std::cout << "    == Valid Last Header Line Test (windows  newlines) ==" << std::endl;
		webIO.Serialize();
#endif
	}

	SECTION("Valid Last Header Line Test (unix  newlines)")
	{
		/*
		 * Content-type: text/html
		 * Cookie: foo=bar;
		 * Set-Cookie: yummy_cookie=choco
		 * Set-Cookie: tasty_cookie=strawberry
		 * X-Forwarded-For: 192.0.2.43, 2001:db8:cafe::17
		 * Forwarded: for=192.0.2.43, for="[2001:db8:cafe::17]
		 */
		KWebIO webIO;
		KString sHeaderPart1 = "Content-type: ";
		KString sHeaderPart2 = "text/html  \nCookie: ";
		KString sHeaderPart3 = "fubar";
		KString sHeaderPart4 = " \nSet-Cookie: yummy_cooki";
		KString sHeaderPart5 = "e =choco;\nSet-Cookie: tasty_cookie=strawberry\n";
		KString sHeaderPart6 = "Randombadheader = really badd header no colon \n with continuation lines \n by lines I meant lines \nX-Forwarded-For: 192.0.2.43, 2001:";
		KString sHeaderPart7 = "db8:cafe::17,  192.168.1.100, 192.168.1.1";
		KString sHeaderPart8 = "\nForwarded: for=192.0.";
		KString sHeaderPart9 = "2.43, for='[2001:db8:cafe::17]'\n\n";
		//KString sHeaderPart10 = "\n";
		webIO.Parse(sHeaderPart1);
		webIO.Parse(sHeaderPart2);
		webIO.Parse(sHeaderPart3);
		webIO.Parse(sHeaderPart4);
		webIO.Parse(sHeaderPart5);
		webIO.Parse(sHeaderPart6);
		webIO.Parse(sHeaderPart7);
		webIO.Parse(sHeaderPart8);
		webIO.Parse(sHeaderPart9);
		//webIO.Parse(sHeaderPart10);

		KCurl::KHeader  sResponseHeaders = webIO.getResponseHeaders();
		CHECK_FALSE(sResponseHeaders.empty());
		CHECK(sResponseHeaders.size() == 7);
		CHECK(webIO.getResponseCookies().size() == 1);
		CHECK(sResponseHeaders.Count(KWebIO::svBrokenHeader) == 1);
		CHECK(sResponseHeaders.IsMulti("set-cookie"));
		CHECK(sResponseHeaders.Count("cookie") == 1);
		const KString& sCookieVal = webIO.getResponseCookie("muh");
		CHECK(sCookieVal.empty());
		CHECK(sResponseHeaders.Count("set-cookie") == 2);
		CHECK(sResponseHeaders.Count("content-type") == 1);
		const KString& sContentType = webIO.getResponseHeader("content-type");
		CHECK(sContentType.length() == 13);
		CHECK(sResponseHeaders.Count("forwarded") == 1);
		CHECK(sResponseHeaders.Count("x-forwarded-for") == 1);
#if kwebdebug
		// Verifying how header was parsed by printing
		std::cout << "    == Invalid Last Header Line Test (unix  newlines) ==" << std::endl;
		webIO.Serialize();
#endif
	}

	SECTION("Invalid Last Header Line Test (windows  newlines)")
	{
		/*
		 * Content-type: text/html
		 * Cookie: foo=bar;
		 * Set-Cookie: yummy_cookie=choco
		 * Set-Cookie: tasty_cookie=strawberry
		 * X-Forwarded-For: 192.0.2.43, 2001:db8:cafe::17
		 * Forwarded: for=192.0.2.43, for="[2001:db8:cafe::17]
		 */
		KWebIO webIO;
		KString sHeaderPart1 = "HTTP/1.1 200 OK\r\nContent-type: ";
		KString sHeaderPart2 = "text/html  \r\nCookie: ";
		KString sHeaderPart3 = "fubar";
		KString sHeaderPart4 = " \r\nSet-Cookie: yummy_cooki";
		KString sHeaderPart5 = "e =choco;\r\nSet-Cookie: tasty_cookie=strawberry\r\n";
		KString sHeaderPart6 = "X-Forwarded-For: 192.0.2.43, 2001:";
		KString sHeaderPart7 = "db8:cafe::17,  192.168.1.100, 192.168.1.1\r";
		KString sHeaderPart8 = "\nForwarded: for=192.0.";
		KString sHeaderPart9 = "2.43, for='[2001:db8:cafe::17]'\r\n";
		KString sHeaderPart10 = "Randombadheader = really badd header no colon \r\n with continuation lines \r\n by lines I meant lines \r\n\r\n";
		webIO.Parse(sHeaderPart1);
		webIO.Parse(sHeaderPart2);
		webIO.Parse(sHeaderPart3);
		webIO.Parse(sHeaderPart4);
		webIO.Parse(sHeaderPart5);
		webIO.Parse(sHeaderPart6);
		webIO.Parse(sHeaderPart7);
		webIO.Parse(sHeaderPart8);
		webIO.Parse(sHeaderPart9);
		webIO.Parse(sHeaderPart10);

		KCurl::KHeader  sResponseHeaders = webIO.getResponseHeaders();
		CHECK_FALSE(sResponseHeaders.empty());
		CHECK(sResponseHeaders.size() == 7);
		CHECK(webIO.getResponseCookies().size() == 1);
		CHECK(sResponseHeaders.Count(KWebIO::svBrokenHeader) == 1);
		CHECK(sResponseHeaders.IsMulti("set-cookie"));
		CHECK(sResponseHeaders.Count("cookie") == 1);
		const KString& sCookieVal = webIO.getResponseCookie("muh");
		CHECK(sCookieVal.empty());
		CHECK(sResponseHeaders.Count("set-cookie") == 2);
		CHECK(sResponseHeaders.Count("content-type") == 1);
		const KString& sContentType = webIO.getResponseHeader("content-type");
		CHECK(sContentType.length() == 14);
		CHECK(sResponseHeaders.Count("forwarded") == 1);
		CHECK(sResponseHeaders.Count("x-forwarded-for") == 1);
#if kwebdebug
		// Verifying how header was parsed by printing
		std::cout << "    == Invalid Last Header Line Test (windows  newlines) ==" << std::endl;
		webIO.Serialize();
#endif
	}

	SECTION("Invalid Last Header Line Test (unix  newlines)")
	{
		/*
		 * Content-type: text/html
		 * Cookie: foo=bar;
		 * Set-Cookie: yummy_cookie=choco
		 * Set-Cookie: tasty_cookie=strawberry
		 * X-Forwarded-For: 192.0.2.43, 2001:db8:cafe::17
		 * Forwarded: for=192.0.2.43, for="[2001:db8:cafe::17]
		 */
		KWebIO webIO;
		KString sHeaderPart1 = "HTTP/1.1 200 OK\nContent-type: ";
		KString sHeaderPart2 = "text/html  \nCookie: ";
		KString sHeaderPart3 = "fubar";
		KString sHeaderPart4 = " \nSet-Cookie: yummy_cooki";
		KString sHeaderPart5 = "e =choco;\nSet-Cookie: tasty_cookie=strawberry\n";
		KString sHeaderPart6 = "X-Forwarded-For: 192.0.2.43, 2001:";
		KString sHeaderPart7 = "db8:cafe::17,  192.168.1.100, 192.168.1.1";
		KString sHeaderPart8 = "\nForwarded: for=192.0.";
		KString sHeaderPart9 = "2.43, for='[2001:db8:cafe::17]'\n";
		KString sHeaderPart10 = "Randombadheader = really badd header no colon \n with continuation lines \n by lines I meant lines \n\n";
		webIO.Parse(sHeaderPart1);
		webIO.Parse(sHeaderPart2);
		webIO.Parse(sHeaderPart3);
		webIO.Parse(sHeaderPart4);
		webIO.Parse(sHeaderPart5);
		webIO.Parse(sHeaderPart6);
		webIO.Parse(sHeaderPart7);
		webIO.Parse(sHeaderPart8);
		webIO.Parse(sHeaderPart9);
		webIO.Parse(sHeaderPart10);

		KCurl::KHeader  sResponseHeaders = webIO.getResponseHeaders();
		CHECK_FALSE(sResponseHeaders.empty());
		CHECK(sResponseHeaders.size() == 7);
		CHECK(webIO.getResponseCookies().size() == 1);
		CHECK(sResponseHeaders.Count(KWebIO::svBrokenHeader) == 1);
		CHECK(sResponseHeaders.IsMulti("set-cookie"));
		CHECK(sResponseHeaders.Count("cookie") == 1);
		const KString& sCookieVal = webIO.getResponseCookie("muh");
		CHECK(sCookieVal.empty());
		CHECK(sResponseHeaders.Count("set-cookie") == 2);
		CHECK(sResponseHeaders.Count("content-type") == 1);
		const KString& sContentType = webIO.getResponseHeader("content-type");
		CHECK(sContentType.length() == 13);
		CHECK(sResponseHeaders.Count("forwarded") == 1);
		CHECK(sResponseHeaders.Count("x-forwarded-for") == 1);
#if kwebdebug
		// Verifying how header was parsed by printing
		std::cout << "    == Invalid Last Header Line Test (unix  newlines) ==" << std::endl;
		webIO.Serialize();
#endif
	}

	SECTION("Multiple Cookie Line Test (windows  newlines)")
	{
		/*
		* Content-type: text/html
		* Cookie: foo=bar;
		* Set-Cookie: yummy_cookie=choco
		* Set-Cookie: tasty_cookie=strawberry
		* X-Forwarded-For: 192.0.2.43, 2001:db8:cafe::17
		* Forwarded: for=192.0.2.43, for="[2001:db8:cafe::17]
		*/
		KWebIO webIO;
		KString sHeaderPart1 = "HTTP/1.1 200 OK\r\nContent-type: ";
		KString sHeaderPart2 = "text/html  \r\nCookie: ";
		KString sHeaderPart3 = "fubar";
		KString sHeaderPart4 = "  =  ;  =  ;  \r\nSet-Cookie: yummy_cooki";
		KString sHeaderPart5 = "e =choco;\r\nSet-Cookie: tasty_cookie=strawberry\r\n";
		KString sHeaderPart6 = "Randombadheader = really badd header no colon \r\n with continuation lines \r\n by lines I meant lines \r\nX-Forwarded-For: 192.0.2.43, 2001:";
		KString sHeaderPart7 = "db8:cafe::17,  192.168.1.100, 192.168.1.1\r";
		KString sHeaderPart8 = "\nForwarded: for=192.0.";
		KString sHeaderPart9 = "2.43, for='[2001:db8:cafe::17]'\r\n";
		KString sHeaderPart10 = "COOKIE: muhCookie=mostYummy; \r\n  ohYeahCookies=yaahhh;\r\n\r\n";
		webIO.Parse(sHeaderPart1);
		webIO.Parse(sHeaderPart2);
		webIO.Parse(sHeaderPart3);
		webIO.Parse(sHeaderPart4);
		webIO.Parse(sHeaderPart5);
		webIO.Parse(sHeaderPart6);
		webIO.Parse(sHeaderPart7);
		webIO.Parse(sHeaderPart8);
		webIO.Parse(sHeaderPart9);
		webIO.Parse(sHeaderPart10);

		KCurl::KHeader  sResponseHeaders = webIO.getResponseHeaders();
		CHECK_FALSE(sResponseHeaders.empty());
		CHECK(sResponseHeaders.size() == 8);
		CHECK(webIO.getResponseCookies().size() == 5);
		CHECK(sResponseHeaders.IsMulti("set-cookie"));
		CHECK(sResponseHeaders.Count("cookie") == 2);
		const KString& sCookieVal = webIO.getResponseCookie("muh");
		CHECK(sCookieVal.empty());
		CHECK(sResponseHeaders.Count("set-cookie") == 2);
		CHECK(sResponseHeaders.Count("content-type") == 1);
		const KString& sContentType = webIO.getResponseHeader("content-type");
		CHECK(sContentType.length() == 14);
		CHECK(sResponseHeaders.Count("forwarded") == 1);
		CHECK(sResponseHeaders.Count("x-forwarded-for") == 1);
#if kwebdebug
		// Verifying how header was parsed by printing
		std::cout << "    == Multiple Cookie Line Test (windows  newlines) ==" << std::endl;
		webIO.Serialize();
#endif
	}

	SECTION("Multiple Cookie Line Test (unix  newlines)")
	{
		/*
		* Content-type: text/html
		* Cookie: foo=bar;
		* Set-Cookie: yummy_cookie=choco
		* Set-Cookie: tasty_cookie=strawberry
		* X-Forwarded-For: 192.0.2.43, 2001:db8:cafe::17
		* Forwarded: for=192.0.2.43, for="[2001:db8:cafe::17]
		*/
		KWebIO webIO;
		KString sHeaderPart1 = "HTTP/1.1 200 OK\nContent-type: ";
		KString sHeaderPart2 = "text/html  \nCookie: ";
		KString sHeaderPart3 = "fubar";
		KString sHeaderPart4 = "  =  ;  =  ;  \nSet-Cookie: yummy_cooki";
		KString sHeaderPart5 = "e =choco;\nSet-Cookie: tasty_cookie=strawberry\n";
		KString sHeaderPart6 = "Randombadheader = really badd header no colon \n with continuation lines \n by lines I meant lines \nX-Forwarded-For: 192.0.2.43, 2001:";
		KString sHeaderPart7 = "db8:cafe::17,  192.168.1.100, 192.168.1.1";
		KString sHeaderPart8 = "\nForwarded: for=192.0.";
		KString sHeaderPart9 = "2.43, for='[2001:db8:cafe::17]'\n";
		KString sHeaderPart10 = "COOKIE: muhCookie=mostYummy; \n  ohYeahCookies=yaahhh;\n\n";
		webIO.Parse(sHeaderPart1);
		webIO.Parse(sHeaderPart2);
		webIO.Parse(sHeaderPart3);
		webIO.Parse(sHeaderPart4);
		webIO.Parse(sHeaderPart5);
		webIO.Parse(sHeaderPart6);
		webIO.Parse(sHeaderPart7);
		webIO.Parse(sHeaderPart8);
		webIO.Parse(sHeaderPart9);
		webIO.Parse(sHeaderPart10);

		KCurl::KHeader  sResponseHeaders = webIO.getResponseHeaders();
		CHECK_FALSE(sResponseHeaders.empty());
		CHECK(sResponseHeaders.size() == 8);
		CHECK(webIO.getResponseCookies().size() == 5);
		CHECK(sResponseHeaders.IsMulti("set-cookie"));
		CHECK(sResponseHeaders.Count("cookie") == 2);
		const KString& sCookieVal = webIO.getResponseCookie("muh");
		CHECK(sCookieVal.empty());
		CHECK(sResponseHeaders.Count("set-cookie") == 2);
		CHECK(sResponseHeaders.Count("content-type") == 1);
		const KString& sContentType = webIO.getResponseHeader("content-type");
		CHECK(sContentType.length() == 13);
		CHECK(sResponseHeaders.Count("forwarded") == 1);
		CHECK(sResponseHeaders.Count("x-forwarded-for") == 1);
#if kwebdebug
		// Verifying how header was parsed by printing
		std::cout << "    == Multiple Cookie Line Test (unix  newlines) ==" << std::endl;
		webIO.Serialize();
#endif
	}

	SECTION("Stuff After header test (windows  newlines)")
	{
		/*
		* Content-type: text/html
		* Cookie: foo=bar;
		* Set-Cookie: yummy_cookie=choco
		* Set-Cookie: tasty_cookie=strawberry
		* X-Forwarded-For: 192.0.2.43, 2001:db8:cafe::17
		* Forwarded: for=192.0.2.43, for="[2001:db8:cafe::17]
		*/
		KWebIO webIO;
		KString sHeaderPart1 = "HTTP/1.1 200 OK\r\nContent-type: ";
		KString sHeaderPart2 = "text/html  \r\nCookie: ";
		KString sHeaderPart3 = "fubar";
		KString sHeaderPart4 = "  =  ;  =  ;  \r\nSet-Cookie: yummy_cooki";
		KString sHeaderPart5 = "e =choco;\r\nSet-Cookie: tasty_cookie=strawberry\r\n";
		KString sHeaderPart6 = "Randombadheader = really badd header no colon \r\n with continuation lines \r\n by lines I meant lines \r\nX-Forwarded-For: 192.0.2.43, 2001:";
		KString sHeaderPart7 = "db8:cafe::17,  192.168.1.100, 192.168.1.1\r";
		KString sHeaderPart8 = "\nForwarded: for=192.0.";
		KString sHeaderPart9 = "2.43, for='[2001:db8:cafe::17]'\r\n";
		KString sHeaderPart10 = "COOKIE: muhCookie=mostYummy; \r\n ohYeahCookies=yaahhh;\r\n\r\n<html><body>HelloWorld!</body></html>";
		webIO.Parse(sHeaderPart1);
		webIO.Parse(sHeaderPart2);
		webIO.Parse(sHeaderPart3);
		webIO.Parse(sHeaderPart4);
		webIO.Parse(sHeaderPart5);
		webIO.Parse(sHeaderPart6);
		webIO.Parse(sHeaderPart7);
		webIO.Parse(sHeaderPart8);
		webIO.Parse(sHeaderPart9);
		webIO.Parse(sHeaderPart10);

		KCurl::KHeader  sResponseHeaders = webIO.getResponseHeaders();
		CHECK_FALSE(sResponseHeaders.empty());
		CHECK(sResponseHeaders.size() == 8);
		CHECK(webIO.getResponseCookies().size() == 5);
		CHECK(sResponseHeaders.IsMulti("set-cookie"));
		CHECK(sResponseHeaders.Count("cookie") == 2);
		const KString& sCookieVal = webIO.getResponseCookie("muh");
		CHECK(sCookieVal.empty());
		CHECK(sResponseHeaders.Count("set-cookie") == 2);
		CHECK(sResponseHeaders.Count("content-type") == 1);
		const KString& sContentType = webIO.getResponseHeader("content-type");
		CHECK(sContentType.length() == 14);
		CHECK(sResponseHeaders.Count("forwarded") == 1);
		CHECK(sResponseHeaders.Count("x-forwarded-for") == 1);
#if kwebdebug
		// Verifying how header was parsed by printing
		std::cout << "    == Stuff After header test (windows  newlines) ==" << std::endl;
		webIO.Serialize();
#endif
	}

	SECTION("Stuff After header test 2 (windows  newlines)")
	{
		/*
		* Content-type: text/html
		* Cookie: foo=bar;
		* Set-Cookie: yummy_cookie=choco
		* Set-Cookie: tasty_cookie=strawberry
		* X-Forwarded-For: 192.0.2.43, 2001:db8:cafe::17
		* Forwarded: for=192.0.2.43, for="[2001:db8:cafe::17]
		*/
		KWebIO webIO;
		KString sHeaderPart1 = "HTTP/1.1 200 OK\r\nContent-type: ";
		KString sHeaderPart2 = "text/html  \r\nCookie: ";
		KString sHeaderPart3 = "fubar";
		KString sHeaderPart4 = "  =  ;  =  ;  \r\nSet-Cookie: yummy_cooki";
		KString sHeaderPart5 = "e =choco;\r\nSet-Cookie: tasty_cookie=strawberry\r\n";
		KString sHeaderPart6 = "Randombadheader = really badd header no colon \r\n with continuation lines \r\n by lines I meant lines \r\nX-Forwarded-For: 192.0.2.43, 2001:";
		KString sHeaderPart7 = "db8:cafe::17,  192.168.1.100, 192.168.1.1\r";
		KString sHeaderPart8 = "\nForwarded: for=192.0.";
		KString sHeaderPart9 = "2.43, for='[2001:db8:cafe::17]'\r\n";
		KString sHeaderPart10 = "COOKIE: muhCookie=mostYummy; \r\n ohYeahCookies=yaahhh;\r\n\r\n  <html><body>HelloWorld!</body></html>";
		webIO.Parse(sHeaderPart1);
		webIO.Parse(sHeaderPart2);
		webIO.Parse(sHeaderPart3);
		webIO.Parse(sHeaderPart4);
		webIO.Parse(sHeaderPart5);
		webIO.Parse(sHeaderPart6);
		webIO.Parse(sHeaderPart7);
		webIO.Parse(sHeaderPart8);
		webIO.Parse(sHeaderPart9);
		webIO.Parse(sHeaderPart10);

		KCurl::KHeader  sResponseHeaders = webIO.getResponseHeaders();
		CHECK_FALSE(sResponseHeaders.empty());
		CHECK(sResponseHeaders.size() == 8);
		CHECK(webIO.getResponseCookies().size() == 5);
		CHECK(sResponseHeaders.IsMulti("set-cookie"));
		CHECK(sResponseHeaders.Count("cookie") == 2);
		const KString& sCookieVal = webIO.getResponseCookie("muh");
		CHECK(sCookieVal.empty());
		CHECK(sResponseHeaders.Count("set-cookie") == 2);
		CHECK(sResponseHeaders.Count("content-type") == 1);
		const KString& sContentType = webIO.getResponseHeader("content-type");
		CHECK(sContentType.length() == 14);
		CHECK(sResponseHeaders.Count("forwarded") == 1);
		CHECK(sResponseHeaders.Count("x-forwarded-for") == 1);
#if kwebdebug
		// Verifying how header was parsed by printing
		std::cout << "    == Stuff After header test 2 (windows  newlines) ==" << std::endl;
		webIO.Serialize();
#endif
	}

	SECTION("Stuff After header test 3 (windows  newlines)")
	{
		/*
		* Content-type: text/html
		* Cookie: foo=bar;
		* Set-Cookie: yummy_cookie=choco
		* Set-Cookie: tasty_cookie=strawberry
		* X-Forwarded-For: 192.0.2.43, 2001:db8:cafe::17
		* Forwarded: for=192.0.2.43, for="[2001:db8:cafe::17]
		*/
		KWebIO webIO;
		KString sHeaderPart1 = "HTTP/1.1 200 OK\r\nContent-type: ";
		KString sHeaderPart2 = "text/html  \r\nCookie: ";
		KString sHeaderPart3 = "fubar";
		KString sHeaderPart4 = "  =  ;  =  ;  \r\nSet-Cookie: yummy_cooki";
		KString sHeaderPart5 = "e =choco;\r\nSet-Cookie: tasty_cookie=strawberry\r\n";
		KString sHeaderPart6 = "Randombadheader = really badd header no colon \r\n with continuation lines \r\n by lines I meant lines \r\nX-Forwarded-For: 192.0.2.43, 2001:";
		KString sHeaderPart7 = "db8:cafe::17,  192.168.1.100, 192.168.1.1\r";
		KString sHeaderPart8 = "\nForwarded: for=192.0.";
		KString sHeaderPart9 = "2.43, for='[2001:db8:cafe::17]'\r\n";
		KString sHeaderPart10 = "COOKIE: muhCookie=mostYummy; \r\n ohYeahCookies=yaahhh;\r\n\r\n  <html><body><h1>HelloWorld!</h1> \r\n <p>There are multiple lines here on page.</p>\r\n More than 1 or 2  \r\n\r\n\r\n </body></html>";
		webIO.Parse(sHeaderPart1);
		webIO.Parse(sHeaderPart2);
		webIO.Parse(sHeaderPart3);
		webIO.Parse(sHeaderPart4);
		webIO.Parse(sHeaderPart5);
		webIO.Parse(sHeaderPart6);
		webIO.Parse(sHeaderPart7);
		webIO.Parse(sHeaderPart8);
		webIO.Parse(sHeaderPart9);
		webIO.Parse(sHeaderPart10);

		KCurl::KHeader  sResponseHeaders = webIO.getResponseHeaders();
		CHECK_FALSE(sResponseHeaders.empty());
		CHECK(sResponseHeaders.size() == 8);
		CHECK(webIO.getResponseCookies().size() == 5);
		CHECK(sResponseHeaders.IsMulti("set-cookie"));
		CHECK(sResponseHeaders.Count("cookie") == 2);
		const KString& sCookieVal = webIO.getResponseCookie("muh");
		CHECK(sCookieVal.empty());
		CHECK(sResponseHeaders.Count("set-cookie") == 2);
		CHECK(sResponseHeaders.Count("content-type") == 1);
		const KString& sContentType = webIO.getResponseHeader("content-type");
		CHECK(sContentType.length() == 14);
		CHECK(sResponseHeaders.Count("forwarded") == 1);
		CHECK(sResponseHeaders.Count("x-forwarded-for") == 1);
#if kwebdebug
		// Verifying how header was parsed by printing
		std::cout << "    == Stuff After header test 3 (windows  newlines) ==" << std::endl;
		webIO.Serialize();
#endif
	}

	SECTION("Stuff After header test (unix  newlines)")
	{
		/*
		* Content-type: text/html
		* Cookie: foo=bar;
		* Set-Cookie: yummy_cookie=choco
		* Set-Cookie: tasty_cookie=strawberry
		* X-Forwarded-For: 192.0.2.43, 2001:db8:cafe::17
		* Forwarded: for=192.0.2.43, for="[2001:db8:cafe::17]
		*/
		KWebIO webIO;
		KString sHeaderPart1 = "HTTP/1.1 200 OK\nContent-type: ";
		KString sHeaderPart2 = "text/html  \nCookie: ";
		KString sHeaderPart3 = "fubar";
		KString sHeaderPart4 = "  =  ;  =  ;  \nSet-Cookie: yummy_cooki";
		KString sHeaderPart5 = "e =choco;\nSet-Cookie: tasty_cookie=strawberry\n";
		KString sHeaderPart6 = "Randombadheader = really badd header no colon \n with continuation lines \n by lines I meant lines \nX-Forwarded-For: 192.0.2.43, 2001:";
		KString sHeaderPart7 = "db8:cafe::17,  192.168.1.100, 192.168.1.1";
		KString sHeaderPart8 = "\nForwarded: for=192.0.";
		KString sHeaderPart9 = "2.43, for='[2001:db8:cafe::17]'\n";
		KString sHeaderPart10 = "COOKIE: muhCookie=mostYummy; \n ohYeahCookies=yaahhh;\n\n<html><body>HelloWorld!</body></html>";
		webIO.Parse(sHeaderPart1);
		webIO.Parse(sHeaderPart2);
		webIO.Parse(sHeaderPart3);
		webIO.Parse(sHeaderPart4);
		webIO.Parse(sHeaderPart5);
		webIO.Parse(sHeaderPart6);
		webIO.Parse(sHeaderPart7);
		webIO.Parse(sHeaderPart8);
		webIO.Parse(sHeaderPart9);
		webIO.Parse(sHeaderPart10);

		KCurl::KHeader  sResponseHeaders = webIO.getResponseHeaders();
		CHECK_FALSE(sResponseHeaders.empty());
		CHECK(sResponseHeaders.size() == 8);
		CHECK(webIO.getResponseCookies().size() == 5);
		CHECK(sResponseHeaders.IsMulti("set-cookie"));
		CHECK(sResponseHeaders.Count("cookie") == 2);
		const KString& sCookieVal = webIO.getResponseCookie("muh");
		CHECK(sCookieVal.empty());
		CHECK(sResponseHeaders.Count("set-cookie") == 2);
		CHECK(sResponseHeaders.Count("content-type") == 1);
		const KString& sContentType = webIO.getResponseHeader("content-type");
		CHECK(sContentType.length() == 13);
		CHECK(sResponseHeaders.Count("forwarded") == 1);
		CHECK(sResponseHeaders.Count("x-forwarded-for") == 1);
#if kwebdebug
		// Verifying how header was parsed by printing
		std::cout << "    == Stuff After header test (unix  newlines) ==" << std::endl;
		webIO.Serialize();
#endif
	}

	SECTION("Stuff After header test 2 (unix  newlines)")
	{
		/*
		* Content-type: text/html
		* Cookie: foo=bar;
		* Set-Cookie: yummy_cookie=choco
		* Set-Cookie: tasty_cookie=strawberry
		* X-Forwarded-For: 192.0.2.43, 2001:db8:cafe::17
		* Forwarded: for=192.0.2.43, for="[2001:db8:cafe::17]
		*/
		KWebIO webIO;
		KString sHeaderPart1 = "HTTP/1.1 200 OK\nContent-type: ";
		KString sHeaderPart2 = "text/html  \nCookie: ";
		KString sHeaderPart3 = "fubar";
		KString sHeaderPart4 = "  =  ;  =  ;  \nSet-Cookie: yummy_cooki";
		KString sHeaderPart5 = "e =choco;\nSet-Cookie: tasty_cookie=strawberry\n";
		KString sHeaderPart6 = "Randombadheader = really badd header no colon \n with continuation lines \n by lines I meant lines \nX-Forwarded-For: 192.0.2.43, 2001:";
		KString sHeaderPart7 = "db8:cafe::17,  192.168.1.100, 192.168.1.1";
		KString sHeaderPart8 = "\nForwarded: for=192.0.";
		KString sHeaderPart9 = "2.43, for='[2001:db8:cafe::17]'\n";
		KString sHeaderPart10 = "COOKIE: muhCookie=mostYummy; \n ohYeahCookies=yaahhh;\n\n  <html><body>HelloWorld!</body></html>";
		webIO.Parse(sHeaderPart1);
		webIO.Parse(sHeaderPart2);
		webIO.Parse(sHeaderPart3);
		webIO.Parse(sHeaderPart4);
		webIO.Parse(sHeaderPart5);
		webIO.Parse(sHeaderPart6);
		webIO.Parse(sHeaderPart7);
		webIO.Parse(sHeaderPart8);
		webIO.Parse(sHeaderPart9);
		webIO.Parse(sHeaderPart10);

		KCurl::KHeader  sResponseHeaders = webIO.getResponseHeaders();
		CHECK_FALSE(sResponseHeaders.empty());
		CHECK(sResponseHeaders.size() == 8);
		CHECK(webIO.getResponseCookies().size() == 5);
		CHECK(sResponseHeaders.IsMulti("set-cookie"));
		CHECK(sResponseHeaders.Count("cookie") == 2);
		const KString& sCookieVal = webIO.getResponseCookie("muh");
		CHECK(sCookieVal.empty());
		CHECK(sResponseHeaders.Count("set-cookie") == 2);
		CHECK(sResponseHeaders.Count("content-type") == 1);
		const KString& sContentType = webIO.getResponseHeader("content-type");
		CHECK(sContentType.length() == 13);
		CHECK(sResponseHeaders.Count("forwarded") == 1);
		CHECK(sResponseHeaders.Count("x-forwarded-for") == 1);
#if kwebdebug
		// Verifying how header was parsed by printing
		std::cout << "    == Stuff After header test 2 (unix  newlines) ==" << std::endl;
		webIO.Serialize();
#endif
	}

	SECTION("Stuff After header test 3 (unix  newlines)")
	{
		/*
			* Content-type: text/html
			* Cookie: foo=bar;
			* Set-Cookie: yummy_cookie=choco
			* Set-Cookie: tasty_cookie=strawberry
			* X-Forwarded-For: 192.0.2.43, 2001:db8:cafe::17
			* Forwarded: for=192.0.2.43, for="[2001:db8:cafe::17]
			*/
		KWebIO webIO;
		KString sHeaderPart1 = "HTTP/1.1 200 OK\nContent-type: ";
		KString sHeaderPart2 = "text/html  \nCookie: ";
		KString sHeaderPart3 = "fubar";
		KString sHeaderPart4 = "  =  ;  =  ;  \nSet-Cookie: yummy_cooki";
		KString sHeaderPart5 = "e =choco;\nSet-Cookie: tasty_cookie=strawberry\n";
		KString sHeaderPart6 = "Randombadheader = really badd header no colon \n with continuation lines \n by lines I meant lines \nX-Forwarded-For: 192.0.2.43, 2001:";
		KString sHeaderPart7 = "db8:cafe::17,  192.168.1.100, 192.168.1.1";
		KString sHeaderPart8 = "\nForwarded: for=192.0.";
		KString sHeaderPart9 = "2.43, for='[2001:db8:cafe::17]'\n";
		KString sHeaderPart10 = "COOKIE: muhCookie=mostYummy; \n ohYeahCookies=yaahhh\n\n  <html><body><h1>HelloWorld!</h1> \n <p>There are multiple lines here on page.</p>\n More than 1 or 2  \n\n\n </body></html>";
		webIO.Parse(sHeaderPart1);
		webIO.Parse(sHeaderPart2);
		webIO.Parse(sHeaderPart3);
		webIO.Parse(sHeaderPart4);
		webIO.Parse(sHeaderPart5);
		webIO.Parse(sHeaderPart6);
		webIO.Parse(sHeaderPart7);
		webIO.Parse(sHeaderPart8);
		webIO.Parse(sHeaderPart9);
		webIO.Parse(sHeaderPart10);

		KCurl::KHeader  sResponseHeaders = webIO.getResponseHeaders();
		CHECK_FALSE(sResponseHeaders.empty());
		CHECK(sResponseHeaders.size() == 8);
		CHECK(webIO.getResponseCookies().size() == 5);
		CHECK(sResponseHeaders.IsMulti("set-cookie"));
		CHECK(sResponseHeaders.Count("cookie") == 2);
		const KString& sCookieVal = webIO.getResponseCookie("muh");
		CHECK(sCookieVal.empty());
		CHECK(sResponseHeaders.Count("set-cookie") == 2);
		CHECK(sResponseHeaders.Count("content-type") == 1);
		const KString& sContentType = webIO.getResponseHeader("content-type");
		CHECK(sContentType.length() == 13);
		CHECK(sResponseHeaders.Count("forwarded") == 1);
		CHECK(sResponseHeaders.Count("x-forwarded-for") == 1);
#if kwebdebug
		// Verifying how header was parsed by printing
		std::cout << "    == Stuff After header test 3 (unix  newlines) ==" << std::endl;
		webIO.Serialize();
#endif
	}

}
