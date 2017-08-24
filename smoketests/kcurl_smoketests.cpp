#include <dekaf2/kwebio.h>

#include <iostream>

#include "catch.hpp"
using namespace dekaf2;


// Smoketests are silent, if something fails these will help debugging
// In appropriate tests head or body will be printed if these are turned on
#define kcurlHead 0
#define kcurlBody 0

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
	//-----------------------------------------------------------------------------
	{
	}

	//-----------------------------------------------------------------------------
	~KCurlTest(){}
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	virtual bool addToResponseBody(KString& sBodyPart)
	//-----------------------------------------------------------------------------
	{
		bool retVal = KCurl::addToResponseBody(sBodyPart);
		m_sBody.push_back(sBodyPart);
		return retVal;
	}

	//-----------------------------------------------------------------------------
	virtual bool addToResponseHeader(KString& sHeaderPart)
	//-----------------------------------------------------------------------------
	{
		if (printHeader || !getEchoHeader())
		{
			KWebIO::addToResponseHeader(sHeaderPart);
		}
		else // getEchoHeader() == true && printHeader == false;
		{
			setEchoHeader(false);
			KWebIO::addToResponseHeader(sHeaderPart);
			setEchoHeader(true);
		}

		return true;
	}

	//-----------------------------------------------------------------------------
	bool printResponseBody()
	//-----------------------------------------------------------------------------
	{
		bool retVal = KCurl::printResponseHeader();
		for (const auto& bodyPart : m_sBody)
		{
			std::cout << bodyPart;
		}
		return retVal;
	}

	StreamedBody m_sBody{nullptr};
#if kcurlHead
	bool printHeader{true};
#else
	bool printHeader{false};
#endif
};

TEST_CASE("KCurl")
{

	SECTION("KCurl Stream Test")
	{

		KString url = "www.google.com";
		KCurlTest webIO(url, KCurl::GET, false, true);
		CHECK_FALSE(webIO.getEchoHeader());
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

#if kcurlBody
		std::cout << std::endl;
		webIO.printResponseBody();
		std::cout << std::endl;
#endif

	}

	SECTION("KCurl Delayed Set Stream Test")
	{
		KString url = "www.google.com";
		KCurlTest webIO;
		webIO.setEchoBody(true);
		webIO.setRequestURL("");
		CHECK_FALSE(webIO.getEchoHeader());
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

#if kcurlBody
		std::cout << std::endl;
		webIO.printResponseBody();
		std::cout << std::endl;
#endif

	}

	SECTION("KCurl Dummy Stream Test")
	{
		KString url = "www.google.com";
		KCurlTest webIO;
		webIO.setEchoHeader(true);
		webIO.setEchoBody(true);
		webIO.setRequestURL("");
		CHECK(webIO.getEchoHeader());
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
		if (webIO.printHeader) {
			webIO.printResponseHeader();
		}

#if kcurlBody
		std::cout << std::endl;
		webIO.printResponseBody();
		std::cout << std::endl;
#endif

	}

	SECTION("KCurl Stream Test With Headers, output header and body")
	{

		KString url = "www.google.com";
		KCurlTest webIO(url, KCurl::GET, true, true);
		CHECK(webIO.getEchoHeader());
		CHECK(webIO.getEchoBody());

		webIO.addRequestHeader(xForwardedForHeader, "onelink-translations.com");
		webIO.addRequestHeader("Cust Header", "muh header...");
		webIO.addRequestCookie("yummy_cookie","chocolate chip");
		webIO.addRequestCookie("best cookie","mint chocolate chip");

		KString cookies;
		webIO.getRequestHeader(CookieHeader, cookies);
		CHECK(cookies.length() == 0);

		KString requestHeader;
		bool bSuccess = webIO.initiateRequest();
		CHECK(bSuccess);
		CHECK(webIO.requestInProgress());
		while (bSuccess)
		{
			bSuccess = webIO.getStreamChunk();
		}

		CHECK_FALSE(bSuccess);
	}

	SECTION("KCurl Stream Test With Headers, output Header Only")
	{

		KString url = "www.google.com";
		KCurlTest webIO(url, KCurl::GET, true);
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

	SECTION("KCurl Stream Test With Headers, output Body Only")
	{

		KString url = "www.google.com";
		KCurlTest webIO(url, KCurl::GET, false, true);
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

#if kcurlBody
		std::cout << std::endl;
		webIO.printResponseBody();
		std::cout << std::endl;
#endif
	}

}
