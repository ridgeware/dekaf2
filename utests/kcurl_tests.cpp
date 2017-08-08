#include <kwebio.h>

#include <iostream>

#include "catch.hpp"
using namespace dekaf2;

#define kcurlDump 0
#define kcurlHead 0
#define kcurlBoth 0



//-----------------------------------------------------------------------------
class KCurlTest : public KWebIO
//-----------------------------------------------------------------------------
{
public:
	typedef std::vector<KString> StreamedBody;

	//-----------------------------------------------------------------------------
	KCurlTest(){}
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	KCurlTest(const KString& sRequestURL, KCurl::RequestType requestType, bool bEchoHeader = false, bool bEchoBody = false)
	    : KWebIO(sRequestURL,requestType, bEchoHeader, bEchoBody)
	{
	}
	~KCurlTest(){}
	virtual bool addToResponseBody(KString sBodyPart)
	{
		bool retVal = KCurl::addToResponseBody(sBodyPart);
		m_sBody.push_back(sBodyPart);
		return retVal;
	}

	bool printResponseBody()
	{
		bool retVal = KCurl::printResponseHeader();
		for (const auto& bodyPart : m_sBody)
		{
			std::cout << bodyPart;
		}
		return retVal;
	}

	StreamedBody m_sBody{nullptr};
};

TEST_CASE("KCurl")
{
	SECTION("KCurl Init Destroy Test")
	{
		KCurl myCurl;
		myCurl.setRequestURL("www.google.com");

		CHECK_FALSE(myCurl.getEchoBody());
		CHECK_FALSE(myCurl.getEchoHeader());

		myCurl.~KCurl();
	}

#if kcurlDump
//#if 1
	SECTION("KCurl Stream Test")
	{

		//KString url = "www.acme.com";
		KString url = "www.google.com";
		//KWebIO webIO(url);//, true, true);
		KCurlTest webIO(url, KCurl::GET, false, true);
		CHECK_FALSE(webIO.getEchoHeader());
		//CHECK(webIO.getEchoHeader());
		//CHECK_FALSE(webIO.getEchoBody());
		CHECK(webIO.getEchoBody());

		webIO.addRequestHeader("Agent", "foo=bar");
		webIO.addRequestHeader("Agent2", "foo=bar");
		webIO.setRequestHeader("Agent", "foo=fubar");
		KString headerVal;
		bool gotHeader = webIO.getRequestHeader("Agent", headerVal);
		CHECK(gotHeader);
		CHECK(headerVal.compare("foo=fubar") == 0);
		gotHeader = webIO.getRequestHeader("Agent2", headerVal);
		CHECK(headerVal.compare("foo=bar") == 0);
		CHECK(gotHeader);
		webIO.delRequestHeader("Agent2");
		gotHeader = webIO.getRequestHeader("Agent2", headerVal);
		CHECK(headerVal.compare("") != 0);

		webIO.addRequestCookie("foo", "bar");
		webIO.addRequestCookie("foo2", "bar2");
		webIO.setRequestCookie("foo", "fubar");
		KString cookieVal;
		bool gotCookie = webIO.getRequestCookie("foo", cookieVal);
		CHECK(gotCookie);
		CHECK(cookieVal.compare("fubar") == 0);
		gotCookie = webIO.getRequestCookie("foo2", cookieVal);
		CHECK(cookieVal.compare("bar2") == 0);
		CHECK(gotCookie);
		webIO.delRequestCookie("foo2");
		webIO.delRequestCookie("foo");
		gotHeader = webIO.getRequestCookie("foo2", cookieVal);
		CHECK(cookieVal.compare("") != 0);

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
	SECTION("KCurl Delayed Set Stream Test")
	{
		KString url = "www.google.com";
		//KWebIO webIO(url);//, true, true);
		KCurlTest webIO;
		webIO.setEchoBody(true);
		webIO.setRequestURL("");
		CHECK_FALSE(webIO.getEchoHeader());
		//CHECK(webIO.getEchoHeader());
		//CHECK_FALSE(webIO.getEchoBody());
		CHECK(webIO.getEchoBody());

		bool bSuccess = webIO.initiateRequest();
		CHECK_FALSE(bSuccess);
		webIO.setRequestURL(url);
		bSuccess = webIO.initiateRequest();
		CHECK(bSuccess);
		CHECK(webIO.requestInProgress());
		while (bSuccess)
		{
			bSuccess = webIO.getStreamChunk();
		}

		CHECK_FALSE(bSuccess);
	}
#endif
	SECTION("KCurl Dummy Stream Test")
	{
		KString url = "www.google.com";
		//KWebIO webIO(url);//, true, true);
		KCurl webIO;
		webIO.setEchoHeader(true);
		webIO.setEchoBody(true);
		webIO.setRequestURL("");
		//CHECK_FALSE(webIO.getEchoHeader());
		CHECK(webIO.getEchoHeader());
		//CHECK_FALSE(webIO.getEchoBody());
		CHECK(webIO.getEchoBody());

		bool bSuccess = webIO.initiateRequest();
		CHECK_FALSE(bSuccess);
		webIO.setRequestURL(url);
		bSuccess = webIO.initiateRequest();
		CHECK(bSuccess);
		CHECK(webIO.requestInProgress());
		while (bSuccess)
		{
			bSuccess = webIO.getStreamChunk();
		}

		CHECK_FALSE(bSuccess);

		KString randHeader("something");
		webIO.addToResponseHeader(randHeader);
		webIO.printResponseHeader();
	}
// TODO Make KCurl Grandchild which stores body so tests can selectively print
#if kcurlBoth
	SECTION("KCurl Stream Test With Headers, output header and body")
	{

		KString url = "www.google.com";
		KWebIO webIO(url, KCurl::GET, true, true);
		CHECK(webIO.getEchoHeader());
		CHECK(webIO.getEchoBody());

		webIO.addRequestHeader(xForwardedForHeader, "onelink-translations.com");
		webIO.addRequestHeader("Cust Header", "muh header...");
		webIO.addRequestCookie("yummy_cookie","chocolate chip");
		webIO.addRequestCookie("best cookie","mint chocolate chip");

		KString cookies;
		webIO.getRequestHeader(CookieHeader, cookies);
		//KCurl::KHeader cookies = webIO.getr
		CHECK(cookies.length() == 0);

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

		KString url = "www.google.com";
		KWebIO webIO(url, KCurl::GET, true);//, true);
		CHECK(webIO.getEchoHeader());
		CHECK_FALSE(webIO.getEchoBody());

		webIO.addRequestHeader(xForwardedForHeader, "onelink-translations.com");
		webIO.addRequestHeader("Cust Header", "muh header...");
		webIO.addRequestCookie("yummy_cookie","chocolate chip");
		webIO.addRequestCookie("best_cookie","mint chocolate chip");
		webIO.addRequestCookie("muh_cookie","muh cookie is best");

		KString cookies;
		webIO.getRequestHeader(CookieHeader, cookies);
		CHECK(cookies.length() == 0);

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
		KWebIO webIO(url, KCurl::GET, false, true);
		CHECK_FALSE(webIO.getEchoHeader());
		CHECK(webIO.getEchoBody());

		webIO.addRequestHeader(xForwardedForHeader, "onelink-translations.com");
		webIO.addRequestHeader("Cust Header", "muh header...");
		webIO.addRequestCookie("yummy_cookie","chocolate chip");
		webIO.addRequestCookie("best_cookie","mint chocolate chip");
		webIO.addRequestCookie("muh_cookie","muh cookie is best");

		KString cookies;
		webIO.getRequestHeader(CookieHeader, cookies);
		CHECK(cookies.length() == 0);

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
