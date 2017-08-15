#include "catch.hpp"
#include <kwebio.h>

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
	SECTION("KWebIO setTest")
	{

		//KString url("www.acme.com");
		KString url("www.google.com");
		//KString url("http://www.bikespot.biz");
		//KWebIO webIO(url);//, true, true);
		KWebIO webIO(url);
		//KCurl webIO(url);
		CHECK_FALSE(webIO.getEchoHeader());
		CHECK_FALSE(webIO.getEchoBody());

		bool bSuccess = webIO.initiateRequest();
		CHECK(bSuccess);
		CHECK(webIO.requestInProgress());
		const KString& dummyCookie5 = webIO.getResponseCookie("asdfs");
		CHECK(dummyCookie5 == "");
		while (bSuccess)
		{
			bSuccess = webIO.getStreamChunk();
		}

		CHECK_FALSE(bSuccess);

#if kwebdebug
		// Verifying how header was parsed by printing
		std::cout << "    == Windows Parse Test Header ==" << std::endl;
		webIO.printResponseHeader();
#endif
	}

	SECTION("KWebIO delayedSetDest")
	{

		//KString url = "www.acme.com";
		KString url = "www.google.com";
		//KString url("http://www.bikespot.biz");
		//KWebIO webIO(url);//, true, true);
		KWebIO webIO;
		const KString& dummyCookie3 = webIO.getResponseCookie("asdfs");
		CHECK(dummyCookie3 == "");
		webIO.setRequestURL(url);
		webIO.setEchoHeader(false);
		webIO.setEchoBody(false);
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
#if kwebdebug
		// Verifying how header was parsed by printing
		std::cout << "    == Windows Parse Test Header ==" << std::endl;
		webIO.printResponseHeader();
#endif
	}


} // end test case KWebIO
