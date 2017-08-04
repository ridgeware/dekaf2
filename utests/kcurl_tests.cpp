#include <dekaf2/kwebio.h>

#include "catch.hpp"
using namespace dekaf2;

#define kcurlDump 0
#define kcurlHead 0
#define kcurlBoth 0

TEST_CASE("KCurl")
{
	SECTION("KCurl Stream Test")
	{

		KString url = "www.acme.com";
		KWebIO webIO(url);//, true, true);
		CHECK_FALSE(webIO.getEchoHeader());
		CHECK_FALSE(webIO.getEchoBody());

		bool bSuccess = webIO.initiateRequest();
		CHECK(bSuccess);
		CHECK(webIO.requestInProgress());
		while (bSuccess)
		{
			bSuccess = webIO.getStreamChunk();
		}

		CHECK_FALSE(bSuccess);
	}
// TODO Make KCurl Grandchild which stores body so tests can selectively print
#if kcurlBoth
	SECTION("KCurl Stream Test With Headers, output header and body")
	{

		KString url = "www.acme.com";
		KWebIO webIO(url, true, true);
		CHECK(webIO.getEchoHeader());
		CHECK(webIO.getEchoBody());

		webIO.addRequestHeader(xForwardedForHeader, "onelink-translations.com");
		webIO.addRequestHeader("Cust Header", "muh header...");
		webIO.addCookie("yummy_cookie","chocolate chip");
		webIO.addCookie("best cookie","mint chocolate chip");

		KString cookies;
		webIO.getRequestHeader(CookieHeader, cookies);
		CHECK(cookies.length() == 64);

		bool bSuccess = webIO.initiateRequest();
		CHECK(bSuccess);
		CHECK(webIO.requestInProgress());
		while (bSuccess)
		{
			bSuccess = webIO.getStreamChunk();
		}

		CHECK_FALSE(bSuccess);
	}
#endif
#if kcurlHead
	SECTION("KCurl Stream Test With Headers, output Header Only")
	{

		KString url = "www.acme.com";
		KWebIO webIO(url, true);//, true);
		CHECK(webIO.getEchoHeader());
		CHECK_FALSE(webIO.getEchoBody());

		webIO.addRequestHeader(xForwardedForHeader, "onelink-translations.com");
		webIO.addRequestHeader("Cust Header", "muh header...");
		webIO.addCookie("yummy_cookie","chocolate chip");
		webIO.addCookie("best_cookie","mint chocolate chip");
		webIO.addCookie("muh_cookie","muh cookie is best");

		KString cookies;
		webIO.getRequestHeader(CookieHeader, cookies);
		CHECK(cookies.length() == 94);

		bool bSuccess = webIO.initiateRequest();
		CHECK(bSuccess);
		CHECK(webIO.requestInProgress());
		while (bSuccess)
		{
			bSuccess = webIO.getStreamChunk();
		}

		CHECK_FALSE(bSuccess);
	}
#endif

#if kcurlDump
	SECTION("KCurl Stream Test With Headers, output Body Only")
	{

		KString url = "www.acme.com";
		KWebIO webIO(url, false, true);
		CHECK_FALSE(webIO.getEchoHeader());
		CHECK(webIO.getEchoBody());

		webIO.addRequestHeader(xForwardedForHeader, "onelink-translations.com");
		webIO.addRequestHeader("Cust Header", "muh header...");
		webIO.addCookie("yummy_cookie","chocolate chip");
		webIO.addCookie("best_cookie","mint chocolate chip");
		webIO.addCookie("muh_cookie","muh cookie is best");

		KString cookies;
		webIO.getRequestHeader(CookieHeader, cookies);
		CHECK(cookies.length() == 94);

		bool bSuccess = webIO.initiateRequest();
		CHECK(bSuccess);
		CHECK(webIO.requestInProgress());
		while (bSuccess)
		{
			bSuccess = webIO.getStreamChunk();
		}

		CHECK_FALSE(bSuccess);
	}
#endif
}
