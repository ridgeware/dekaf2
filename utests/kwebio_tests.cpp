#include "catch.hpp"
#include <dekaf2/kwebio.h>

using namespace dekaf2;

#include <iostream>

#include <map>
#include <unordered_map>
#include <vector>
#include <functional>
#include <iterator>

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
/*
	//apparently Get(key) on this KProps returns std::pair<KString,KString> where .empty() is true for them.
		typedef std::pair<KString,KString> KCurl::KHeaderPair;
		typedef KPropsTemplate<KString, KCurl::KHeaderPair> KNoCaseHeader; // map for header info

		KNoCaseHeader theHeader;
		KHeaderPair myHeader;
		myHeader.first = "Content-type";
		myHeader.second = "text/html";
		KString headerKey(myHeader.first);
		headerKey.MakeLower();
		theHeader.Add(headerKey, KHeaderPair(myHeader.first,myHeader.second));
		KHeaderPair myHeader2;
		myHeader2.first = "Cookies";
		myHeader2.second = "foo=bar;muh=cookie";
		KString headerKey2(myHeader2.first);
		headerKey2.MakeLower();
		theHeader.Add(headerKey2, myHeader2);
		KHeaderPair myHeader3;
		myHeader3.first = "Cookies";
		myHeader3.second = "foo2=bar2;muhs=cookies";
		KString headerKey3(myHeader3.first);
		headerKey3.MakeLower();
		theHeader.Add(headerKey3, myHeader3);

		KHeaderPair myHeader4;
		myHeader4 = theHeader.Get("content-type");
		CHECK(myHeader4.first == myHeader.first);
		std::cout << "headerName: " << myHeader.first << "; headerVal: " << myHeader.second << std::endl;
		std::cout << "header2Name: " << myHeader2.first << "; header2Val: " << myHeader2.second << std::endl;
		std::cout << "header3Name: " << myHeader3.first << "; header3Val: " << myHeader3.second << std::endl;
		std::cout << "header4Name: " << myHeader4.first << "; header4Val: " << myHeader4.second << std::endl;
		myHeader4 = theHeader.Get("Fubar");
		std::cout << "header4Name: " << myHeader4.first << "; header4Val: " << myHeader4.second << std::endl;
		std::cout << "theHeader[0]Name: '" << theHeader[0].first << "'; theHeader[0]Val: '<" << theHeader[0].second.first << ": " << theHeader[0].second.second << ">'" << std::endl;
		std::cout << "theHeader[1]Name: '" << theHeader[1].first << "'; theHeader[1]Val: '<" << theHeader[1].second.first << ": " << theHeader[1].second.second << ">'" << std::endl;
		std::cout << "theHeader[2]Name: '" << theHeader[2].first << "'; theHeader[2]Val: '<" << theHeader[2].second.first << ": " << theHeader[2].second.second << ">'" << std::endl;
		CHECK(myHeader4.first.empty());
		CHECK(myHeader4.second.empty());
*/

		KWebIO webIO;
		KString sHeaderPart = "Content-type: text/html\nCookie: foo=bar\nSet-Cookie: yummy_cookie=choco\nSet-Cookie: tasty_cookie=strawberry\nX-Forwarded-For: 192.0.2.43, 2001:db8:cafe::17\nForwarded: for=192.0.2.43, for='[2001:db8:cafe::17]'\n\n";
		webIO.addToResponseHeader(sHeaderPart);

		// print response header for debugging:
		//for (auto header : webIO.getResponseHeaders().begin())
		//auto iter = webIO.getResponseHeaders().begin();
		//for (auto iter = webIO.getResponseHeaders().begin(); iter == webIO.getResponseBody().end(); iter = iter)
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
		webIO.printResponseHeader();
#endif
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
		webIO.addToResponseHeader(sHeaderPart);

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
		webIO.printResponseHeader();
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
		webIO.addToResponseHeader(sHeaderPart);

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
		webIO.printResponseHeader();
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
		KString sHeaderPart1 = "Content-type: text/html\nCookie: foo=bar\nSet-Cookie: yummy_cookie=choco\nSet-Cookie: tasty_cookie=strawberry\n";
		KString sHeaderPart2 = "X-Forwarded-For: 192.0.2.43, 2001:db8:cafe::17\nForwarded: for=192.0.2.43, for='[2001:db8:cafe::17]'\n\n";
		webIO.addToResponseHeader(sHeaderPart1);
		webIO.addToResponseHeader(sHeaderPart2);

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

		KString xForwarded = sResponseHeaders.Get(xForwardedForHeader).second;
		CHECK(xForwarded.compare(" 192.0.2.43, 2001:db8:cafe::17\n") == 0);
		CHECK(xForwarded == " 192.0.2.43, 2001:db8:cafe::17\n");
		KString fooCookie = webIO.getResponseCookie("foo");
		CHECK(fooCookie == "bar\n");
		KString badCookie = webIO.getResponseCookie("asdf");
		CHECK(badCookie == "");
#if kwebdebug
//#if 1
		KCurl::KHeader allCookies = webIO.getResponseCookies();
		std::cout << "    == Cookies ==" << std::endl;
		for (const auto& iter : allCookies)
		{
			std::cout << iter.second.first << "=" << iter.second.second << std::endl;
		}
		// Verifying how header was parsed by printing
		std::cout << "    == Streamed Test Header ==" << std::endl;
		webIO.printResponseHeader();
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
		webIO.addToResponseHeader(sHeaderPart1);
		webIO.addToResponseHeader(sHeaderPart2);
		webIO.addToResponseHeader(sHeaderPart3);
		webIO.addToResponseHeader(sHeaderPart4);

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
		webIO.printResponseHeader();
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
		webIO.addToResponseHeader(sHeaderPart1);
		webIO.addToResponseHeader(sHeaderPart2);
		webIO.addToResponseHeader(sHeaderPart3);
		webIO.addToResponseHeader(sHeaderPart4);
		webIO.addToResponseHeader(sHeaderPart5);
		webIO.addToResponseHeader(sHeaderPart6);
		webIO.addToResponseHeader(sHeaderPart7);
		webIO.addToResponseHeader(sHeaderPart8);
		webIO.addToResponseHeader(sHeaderPart9);

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
		std::cout << "    == Cut Stream Test Header II ==" << std::endl;
		webIO.printResponseHeader();
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
		webIO.addToResponseHeader(sHeaderPart1);
		webIO.addToResponseHeader(sHeaderPart2);
		webIO.addToResponseHeader(sHeaderPart3);
		webIO.addToResponseHeader(sHeaderPart4);
		webIO.addToResponseHeader(sHeaderPart5);
		webIO.addToResponseHeader(sHeaderPart6);
		webIO.addToResponseHeader(sHeaderPart7);
		webIO.addToResponseHeader(sHeaderPart8);
		webIO.addToResponseHeader(sHeaderPart9);

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
		std::cout << "    == Windows Cut Stream Test Header ==" << std::endl;
		webIO.printResponseHeader();
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
		KString sHeaderPart3 = "foo=bar, muh=cooki";
		KString sHeaderPart4 = "e\r\nSet-Cookie: yummy_cooki";
		KString sHeaderPart5 = "e=choco\r\nSet-Cookie: tasty_cookie=strawberry\r\n";
		KString sHeaderPart6 = "X-Forwarded-For: 192.0.2.43, 2001:";
		KString sHeaderPart7 = "db8:cafe::17\r";
		KString sHeaderPart8 = "\nForwarded: for=192.0.";
		KString sHeaderPart9 = "2.43, for='[2001:db8:cafe::17]'\r\n";
		KString sHeaderPart10 = "\r\n";
		webIO.addToResponseHeader(sHeaderPart1);
		webIO.addToResponseHeader(sHeaderPart2);
		webIO.addToResponseHeader(sHeaderPart3);
		webIO.addToResponseHeader(sHeaderPart4);
		webIO.addToResponseHeader(sHeaderPart5);
		webIO.addToResponseHeader(sHeaderPart6);
		webIO.addToResponseHeader(sHeaderPart7);
		webIO.addToResponseHeader(sHeaderPart8);
		webIO.addToResponseHeader(sHeaderPart9);
		webIO.addToResponseHeader(sHeaderPart10);

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
		std::cout << "    == Windows Cut Stream Test Header II==" << std::endl;
		webIO.printResponseHeader();
#endif
	}
}
