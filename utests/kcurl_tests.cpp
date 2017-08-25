#include <dekaf2/kwebio.h>

#include <iostream>

#include "catch.hpp"
using namespace dekaf2;

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
	bool printHeader{true};
};

TEST_CASE("KCurl")
{

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
	}

	SECTION("KCurl Dummy Stream Test")
	{
		KCurl webIO;
		webIO.setEchoHeader(true);
		webIO.setEchoBody(true);
		webIO.setRequestURL("");
		CHECK(webIO.getEchoHeader());
		CHECK(webIO.getEchoBody());

		bool bSuccess = webIO.initiateRequest();

		CHECK_FALSE(bSuccess);

		KString randHeader("something");
		webIO.addToResponseHeader(randHeader);
		webIO.printResponseHeader();
	}
}
