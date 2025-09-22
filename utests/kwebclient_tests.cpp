#include "catch.hpp"

#include <dekaf2/kwebclient.h>
#include <dekaf2/ktcpserver.h>
#include <dekaf2/kstring.h>
#include <dekaf2/ksystem.h>
#include <dekaf2/krest.h>

#ifndef DEKAF2_IS_WINDOWS

using namespace dekaf2;

namespace {

std::atomic<bool> g_bDone { false };

KTempDir MySocketDir;

void rest_test_no_timeout(KRESTServer& REST)
{
	REST.SetRawOutput(REST.GetRequestBody());
	REST.SetStatus(200);
}

void rest_test_timeout_1(KRESTServer& REST)
{
	kSleep(chrono::milliseconds(1100));
	REST.SetRawOutput(REST.GetRequestBody());
	REST.SetStatus(200);
	g_bDone = true;
}

class KTinyHTTPServer2 : public KTCPServer
{

public:

	using KTCPServer::KTCPServer;
	
	std::vector<KString> m_rx;

	void clear()
	{
		m_rx.clear();
		m_sResponse.clear();
	}

	void respond(const KStringView response)
	{
		m_sResponse = response;
	}

	void SetTimeout(int timeout)
	{
		m_iTimeout = timeout;
	}

protected:

	virtual bool Accepted(std::unique_ptr<KIOStreamSocket>& stream) override
	{
		stream->SetReaderRightTrim("\r\n");
		stream->SetWriterEndOfLine("\r\n");
		return true;
	}

	virtual KString Request(KStringRef& qstr, Parameters& parameters) override
	{
		m_rx.push_back(qstr);
		KString tmp;
		if (qstr == "some body")
		{
			if (m_iTimeout == 1)
			{
				kSleep(chrono::milliseconds(1100));
			}
			tmp = "HTTP/1.1 200\r\nConnection : close\r\nContent-Length : 10\r\n\r\n0123456789";
		}
		return tmp;
	}

	KString m_sResponse;
	int m_iTimeout { 0 };
};

} // end of anonymous namespace

TEST_CASE("KWebClient") {

	SECTION("basic")
	{
		KTinyHTTPServer2 server(7653, false, 3);
		server.Start(chrono::seconds(30), false);
		server.clear();

		KWebClient HTTP;
		auto sRet = HTTP.Post("http://user:pass@127.0.0.1:7653/path", "some body\r\n", KMIME::HTML_UTF8);

		CHECK( server.m_rx.size() == 9 );
		if (server.m_rx.size() == 9)
		{
			CHECK( server.m_rx[0] == "POST /path HTTP/1.1" );
			CHECK( server.m_rx[1] == "authorization: Basic dXNlcjpwYXNz");
			CHECK( server.m_rx[2] == "host: 127.0.0.1:7653");
			CHECK( server.m_rx[3] == "content-length: 11");
			CHECK( server.m_rx[4] == "content-type: text/html; charset=UTF-8" );
			CHECK( server.m_rx[5] == kFormat("accept-encoding: {}", KHTTPCompression::GetCompressors()) );
			CHECK( server.m_rx[6] == "user-agent: dekaf/" DEKAF_VERSION );
			CHECK( server.m_rx[7] == "");
			CHECK( server.m_rx[8] == "some body");
		}
		CHECK( sRet == "0123456789" );
	}

	SECTION("timeout TCP")
	{
		constexpr KRESTRoutes::FunctionTable RTable[]
		{
			{ "GET",  false, "/test0",      rest_test_no_timeout, KRESTRoute::PLAIN },
			{ "POST", false, "/test0",      rest_test_no_timeout, KRESTRoute::PLAIN },
			{ "POST", false, "/test1",      rest_test_timeout_1,  KRESTRoute::PLAIN },
		};

		KRESTRoutes Routes;

		Routes.AddFunctionTable(RTable);

		KREST::Options Options;
		Options.Type      = KREST::HTTP;
		Options.iPort     = 7653;
		Options.bPollForDisconnect = false;
		Options.bBlocking = false;
		Options.bCreateEphemeralCert = false;

		KREST REST;
		REST.Execute(Options, Routes);

		KString sRet;

		KWebClient HTTP;
		HTTP.SetTimeout(chrono::seconds(1));

		sRet = HTTP.Get("http://localhost:7653/test0");
		CHECK( sRet == "" );
		CHECK( HTTP.GetStatusCode() == 200 );
		CHECK( HTTP.Error() == "" );
		HTTP.Disconnect();

		sRet = HTTP.Get("http://localhost:7653/test0", "some body", KMIME::HTML_UTF8);
		CHECK( sRet == "some body" );
		CHECK( HTTP.GetStatusCode() == 200 );
		CHECK( HTTP.Error() == "" );
		HTTP.Disconnect();

		sRet = HTTP.Post("http://localhost:7653/test0", "some body", KMIME::HTML_UTF8);
		CHECK( sRet == "some body" );
		CHECK( HTTP.GetStatusCode() == 200 );
		CHECK( HTTP.Error() == "" );
		// leave connection open!
		sRet = HTTP.Post("http://localhost:7653/test0", "some other body", KMIME::HTML_UTF8);
		CHECK( sRet == "some other body" );
		CHECK( HTTP.GetStatusCode() == 200 );
		CHECK( HTTP.Error() == "" );
		HTTP.Disconnect();

		g_bDone = false;
		sRet = HTTP.Post("http://localhost:7653/test1", "some body", KMIME::HTML_UTF8);
		CHECK( sRet.empty() );
		CHECK( HTTP.GetStatusCode() == 598 );
		CHECK( HTTP.Error().contains("Operation canceled") );
//		HTTP.Disconnect();
		// leave connection untouched to test reconnect feature
		// it is difficult to know when the TCP server is done
		while (!g_bDone)
		{
			kSleep(chrono::milliseconds(10));
		}
		kDebug(2, "========= after timeout 1 ==========");

		sRet = HTTP.Post("http://localhost:7653/test0", "some other body", KMIME::HTML_UTF8);
		CHECK( sRet == "some other body" );
		CHECK( HTTP.GetStatusCode() == 200 );
		CHECK( HTTP.Error() == "" );
		HTTP.Disconnect();
	}

	SECTION("timeout Unix")
	{
		constexpr KRESTRoutes::FunctionTable RTable[]
		{
			{ "GET",  false, "/test0",      rest_test_no_timeout, KRESTRoute::PLAIN },
			{ "POST", false, "/test0",      rest_test_no_timeout, KRESTRoute::PLAIN },
			{ "POST", false, "/test1",      rest_test_timeout_1,  KRESTRoute::PLAIN },
		};

		KRESTRoutes Routes;

		Routes.AddFunctionTable(RTable);

		KREST::Options Options;
		Options.Type        = KREST::UNIX;
		Options.sSocketFile = kFormat("{}/socket", MySocketDir.Name());
		Options.bPollForDisconnect = false;
		Options.bBlocking   = false;
		Options.bCreateEphemeralCert = false;

		KREST REST;
		REST.Execute(Options, Routes);

		KString sRet;

		KWebClient HTTP;
		HTTP.SetTimeout(chrono::seconds(1));

		KOutStringStream oss(sRet);

		KURL ConnectURL = kFormat("unix://{}", Options.sSocketFile);

		HTTP.HttpRequest2Host(oss, ConnectURL, "localhost/test0", KHTTPMethod::GET);
		CHECK( sRet == "" );
		CHECK( HTTP.GetStatusCode() == 200 );
		CHECK( HTTP.Error() == "" );
		HTTP.Disconnect();
		sRet.clear();

		HTTP.HttpRequest2Host(oss, ConnectURL, "localhost/test0", KHTTPMethod::GET, "some body", KMIME::HTML_UTF8);
		CHECK( sRet == "some body" );
		CHECK( HTTP.GetStatusCode() == 200 );
		CHECK( HTTP.Error() == "" );
		HTTP.Disconnect();
		sRet.clear();

		HTTP.HttpRequest2Host(oss, ConnectURL, "localhost/test0", KHTTPMethod::POST, "some body", KMIME::HTML_UTF8);
		CHECK( sRet == "some body" );
		CHECK( HTTP.GetStatusCode() == 200 );
		CHECK( HTTP.Error() == "" );
		sRet.clear();
		// leave connection open!
		HTTP.HttpRequest2Host(oss, ConnectURL, "localhost/test0", KHTTPMethod::POST, "some other body", KMIME::HTML_UTF8);
		CHECK( sRet == "some other body" );
		CHECK( HTTP.GetStatusCode() == 200 );
		CHECK( HTTP.Error() == "" );
		HTTP.Disconnect();
		sRet.clear();

		g_bDone = false;
		HTTP.HttpRequest2Host(oss, ConnectURL, "localhost/test1", KHTTPMethod::POST, "some body", KMIME::HTML_UTF8);
		CHECK( sRet.empty() );
		CHECK( HTTP.GetStatusCode() == 598 );
		CHECK( HTTP.Error().contains("Operation canceled") );
		sRet.clear();
		// leave connection untouched to test reconnect feature
		// it is difficult to know when the TCP server is done
		while (!g_bDone)
		{
			kSleep(chrono::milliseconds(10));
		}
		kDebug(2, "========= after timeout 2 ==========");

		HTTP.HttpRequest2Host(oss, ConnectURL, "localhost/test0", KHTTPMethod::POST, "some other body", KMIME::HTML_UTF8);
		CHECK( sRet == "some other body" );
		CHECK( HTTP.GetStatusCode() == 200 );
		CHECK( HTTP.Error() == "" );
		HTTP.Disconnect();
		sRet.clear();
	}

}

#endif // !Windows
