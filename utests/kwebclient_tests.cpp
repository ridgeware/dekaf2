#include "catch.hpp"

#include <dekaf2/http/client/kwebclient.h>
#include <dekaf2/net/tcp/ktcpserver.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/system/os/ksystem.h>
#include <dekaf2/rest/framework/krest.h>
#include <dekaf2/net/tls/ktlscontext.h>
#include <dekaf2/net/tls/ktlsstream.h>
#include <dekaf2/crypto/rsa/krsacert.h>
#include <dekaf2/crypto/rsa/krsakey.h>
#include <dekaf2/system/filesystem/kfilesystem.h>
#include <dekaf2/io/streams/kstringstream.h>
#include <openssl/opensslv.h>
#include <mutex>

#ifndef DEKAF2_IS_WINDOWS

using namespace dekaf2;

namespace {

std::atomic<bool> g_bDone { false };

KTempDir MySocketDir(true, 95);

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

		server.clear();

		// second round w/o basic auth in the url
		sRet = HTTP.Post("http://127.0.0.1:7653/path", "some body\r\n", KMIME::HTML_UTF8);

		CHECK( server.m_rx.size() == 8 );
		if (server.m_rx.size() == 8)
		{
			CHECK( server.m_rx[0] == "POST /path HTTP/1.1" );
			CHECK( server.m_rx[1] == "host: 127.0.0.1:7653");
			CHECK( server.m_rx[2] == kFormat("accept-encoding: {}", KHTTPCompression::GetCompressors()) );
			CHECK( server.m_rx[3] == "user-agent: dekaf/" DEKAF_VERSION );
			CHECK( server.m_rx[4] == "content-length: 11");
			CHECK( server.m_rx[5] == "content-type: text/html; charset=UTF-8" );
			CHECK( server.m_rx[6] == "");
			CHECK( server.m_rx[7] == "some body");
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

	SECTION("TLS identity")
	{
		// a server cert for localhost, with the loopback address as IP SAN
		KRSAKey  Key(2048);
		KRSACert Cert;
		REQUIRE ( Cert.Create(Key, "localhost", "US", "", "DNS:localhost,IP:127.0.0.1") );

		constexpr KRESTRoutes::FunctionTable RTable[]
		{
			{ "GET",  false, "/test0", rest_test_no_timeout, KRESTRoute::PLAIN },
		};

		KRESTRoutes Routes;
		Routes.AddFunctionTable(RTable);

		KREST::Options Options;
		Options.Type                 = KREST::HTTP;
		Options.iPort                = 7657;
		Options.bPollForDisconnect   = false;
		Options.bBlocking            = false;
		Options.bCreateEphemeralCert = false;
		Options.bPEMsAreFilenames    = false;
		Options.sCert                = Cert.GetPEM();
		Options.sKey                 = Key.GetPEM(true);

		KREST REST;
		REQUIRE ( REST.Execute(Options, Routes) );
		REQUIRE ( REST.GetTLSContext() != nullptr );

		// record the SNI of every handshake
		std::mutex SNIMutex;
		KString    sSNI;
		bool       bHadHandshake { false };

		REST.GetTLSContext()->SetSNICallback([&](const KTLSContext::ClientHello& Hello) -> std::shared_ptr<KTLSContext>
		{
			std::lock_guard<std::mutex> Lock(SNIMutex);
			sSNI          = Hello.sServerName;
			bHadHandshake = true;
			return nullptr;
		});

		auto LastSNI = [&]() -> KString
		{
			std::lock_guard<std::mutex> Lock(SNIMutex);
			KString sLast = bHadHandshake ? sSNI : KString("<no handshake>");
			bHadHandshake = false;
			sSNI.clear();
			return sLast;
		};

		KWebClient HTTP;
		HTTP.SetTimeout(chrono::seconds(2));

		// connected by name: the name is the SNI
		CHECK ( HTTP.Get("https://localhost:7657/test0") == "" );
		CHECK ( HTTP.GetStatusCode() == 200 );
		CHECK ( LastSNI() == "localhost" );
		HTTP.Disconnect();

		// connected by address: an IP literal is not permitted in SNI, none is sent
		CHECK ( HTTP.Get("https://127.0.0.1:7657/test0") == "" );
		CHECK ( HTTP.GetStatusCode() == 200 );
#if OPENSSL_VERSION_NUMBER >= 0x10101000L
		// the client hello callback sees every handshake, the older SNI callback only those with a name
		CHECK ( LastSNI() == "" );
#else
		LastSNI();
#endif

		// same connect address, but the request URL names the host: the name is the SNI,
		// and the open connection to the address is not reused for it
		KString sRet;
		KOutStringStream oss(sRet);
		CHECK ( HTTP.HttpRequest2Host(oss, "https://127.0.0.1:7657", "https://localhost/test0") );
		CHECK ( HTTP.GetStatusCode() == 200 );
		CHECK ( LastSNI() == "localhost" );
		HTTP.Disconnect();

		// an explicit name wins
		HTTP.SetTLSHostname("explicit.test");
		CHECK ( HTTP.Get("https://127.0.0.1:7657/test0") == "" );
		CHECK ( HTTP.GetStatusCode() == 200 );
		CHECK ( LastSNI() == "explicit.test" );
		HTTP.SetTLSHostname("");
		HTTP.Disconnect();

		// a name for one connection (KWebClient manages its connections itself and
		// keeps Connect() protected, so this is the KHTTPClient level)
		{
			KHTTPClient Client;
			CHECK ( Client.Connect("https://127.0.0.1:7657", {}, "perconnect.test") );
			CHECK ( LastSNI() == "perconnect.test" );
			CHECK ( Client.Resource("https://perconnect.test/test0", KHTTPMethod::GET) );
			CHECK ( Client.SendRequest() );
			CHECK ( Client.Response.iStatusCode == 200 );
			// the request went over the connection just made, no second handshake
			CHECK ( LastSNI() == "<no handshake>" );
		}

		// certificate verification, with a client context that trusts the test cert
		KTempDir TempDir;
		KString  sCertFile = kFormat("{}/cert.pem", TempDir.Name());
		REQUIRE ( kWriteFile(sCertFile, Cert.GetPEM()) );

		KTLSContext ClientCtx(false);
		ClientCtx.GetContext().load_verify_file(sCertFile.c_str());

		auto Verified = [&](KStringView sTLSHostname) -> bool
		{
			KTLSStream Stream(ClientCtx, chrono::seconds(2));

			if (!sTLSHostname.empty())
			{
				REQUIRE ( Stream.SetTLSHostname(sTLSHostname) );
			}

			if (!Stream.Connect(KTCPEndPoint("127.0.0.1:7657"), KStreamOptions(true)))
			{
				return false;
			}

			// the handshake runs with the first I/O
			Stream.Write("GET /test0 HTTP/1.0\r\nHost: localhost\r\n\r\n").Flush();

			KString sLine;
			return Stream.ReadLine(sLine) && sLine.starts_with("HTTP/1.1 200");
		};

		CHECK ( Verified("localhost")  == true  ); // the name in the cert, while connected to the address
		CHECK ( Verified("")           == true  ); // the address itself, checked against the IP SAN
		CHECK ( Verified("wrong.test") == false ); // a name the cert does not carry
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
