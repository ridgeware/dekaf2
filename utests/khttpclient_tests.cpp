#include "catch.hpp"

#include <dekaf2/khttpclient.h>
#include <dekaf2/ktcpserver.h>
#include <dekaf2/kstring.h>
#include <dekaf2/ktimer.h>
#include <dekaf2/kfilesystem.h>
#include <dekaf2/koutstringstream.h>

#ifndef DEKAF2_IS_WINDOWS

using namespace dekaf2;

namespace {
KTempDir TempDir;
}

class KTinyHTTPServer : public KTCPServer
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

protected:

	virtual bool Accepted(KStream& stream, KStringView sRemoteEndpoint) override
	{
		stream.SetReaderRightTrim("\r\n");
		stream.SetWriterEndOfLine("\r\n");
		return true;
	}

	virtual KString Request(KString& qstr, Parameters& parameters) override
	{
		m_rx.push_back(qstr);
		KString tmp;
		if (qstr.empty())
		{
			tmp = "HTTP/1.1 200\r\nConnection : close\r\nContent-Length : 10\r\n\r\n0123456789";
		}
		return tmp;
	}

	KString m_sResponse;
};

TEST_CASE("KHTTPClient") {

	SECTION("check connection setup")
	{
		KTinyHTTPServer server(7654, false, 3);
		server.Start(2, false);
		server.clear();

		KURL URL("http://127.0.0.1:7654/path?query=val&another=here#fragment");
		auto cx = KConnection::Create(URL);
		CHECK( cx->Good() == true );
		if (cx->Good() == true)
		{
			CHECK( cx->Stream().OutStream().good() == true );
			CHECK( cx->Stream().InStream().good()  == true );
		}
		KHTTPClient cHTTP(std::move(cx));
		cHTTP.Resource(URL);
		CHECK( cHTTP.SendRequest() == true );
		KString shtml;
		cHTTP.Read(shtml);
		CHECK( shtml == "0123456789");
		CHECK( server.m_rx.size() == 4 );
		if (server.m_rx.size() == 4)
		{
			CHECK( server.m_rx[0] == "GET /path?query=val&another=here HTTP/1.1" );
			CHECK( server.m_rx[1] == "Host: 127.0.0.1:7654");
			CHECK( server.m_rx[2] == kFormat("Accept-Encoding: {}", KHTTPCompression::GetCompressors()) );
			CHECK( server.m_rx[3] == "");
		}
		server.Stop();
	}

	SECTION("check serialization")
	{
		KTinyHTTPServer server(7654, false);
		server.Start(2, false);
		server.clear();

		KURL URL("http://127.0.0.1:7654/path?query=val&another=here#fragment");
		auto cx = KConnection::Create(URL);
		CHECK( cx->Good() == true );
		if (cx->Good() == true)
		{
			CHECK( cx->Stream().OutStream().good() == true );
			CHECK( cx->Stream().InStream().good()  == true );
		}
		KHTTPClient cHTTP(std::move(cx));
		cHTTP.Resource(URL);
		CHECK( cHTTP.Serialize() == true );
		CHECK( cHTTP.Parse() == true );
		KString shtml;
		cHTTP.Read(shtml);
		CHECK( shtml == "0123456789");
		CHECK( server.m_rx.size() == 3 );
		if (server.m_rx.size() == 3)
		{
			CHECK( server.m_rx[0] == "GET /path?query=val&another=here HTTP/1.1" );
			CHECK( server.m_rx[1] == "Host: 127.0.0.1:7654");
			CHECK( server.m_rx[2] == "");
		}
	}

#ifdef DEKAF2_HAS_UNIX_SOCKETS
	SECTION("check unix sockets")
	{
		auto sSocketFile = kFormat("{}/test1.socket", TempDir.Name());
		KTinyHTTPServer server(sSocketFile);
		server.Start(2, false);
		server.clear();

		// unix sockets require a different URL for the connection (the file system path
		// to the socket) and the request (a relative path on the http server side)

		KURL URL("http://127.0.0.1:7654/path?query=val&another=here#fragment");

		KHTTPClient cHTTP;
		KURL ConnectURL(kFormat("unix://{}", sSocketFile));
		cHTTP.Connect(ConnectURL); // the file system path
		cHTTP.Resource(URL); // the request path (protocol and domain parts are not used)
		CHECK( cHTTP.Serialize() == true );
		CHECK( cHTTP.Parse() == true );
		KString shtml;
		cHTTP.Read(shtml);
		CHECK( shtml == "0123456789");
		CHECK( server.m_rx.size() == 3 );
		if (server.m_rx.size() == 3)
		{
			CHECK( server.m_rx[0] == "GET /path?query=val&another=here HTTP/1.1" );
			CHECK( server.m_rx[1] == "Host: 127.0.0.1:7654");
			CHECK( server.m_rx[2] == "");
		}

		// now repeat the connection (to simulate a reused client)
		server.clear();
		cHTTP.Connect(ConnectURL); // the file system path
		cHTTP.Resource(URL); // the request path (protocol and domain parts are not used)
		CHECK( cHTTP.Serialize() == true );
		CHECK( cHTTP.Parse() == true );
		shtml.clear();
		cHTTP.Read(shtml);
		CHECK( shtml == "0123456789");
		CHECK( server.m_rx.size() == 3 );
		if (server.m_rx.size() == 3)
		{
			CHECK( server.m_rx[0] == "GET /path?query=val&another=here HTTP/1.1" );
			CHECK( server.m_rx[1] == "Host: 127.0.0.1:7654");
			CHECK( server.m_rx[2] == "");
		}

	}
#endif

	SECTION("failing connection")
	{
		KHTTPClient Client("http://koltun-ballet-boston.com/");
		KStopTime Stop;
		CHECK( Client.SendRequest() == false );
		// allow for ten millisecond of fail time
		CHECK( Stop.microseconds() < 10000 );
	}

	SECTION("send a stream")
	{
		KTinyHTTPServer server(7654, false);
		server.Start(2, false);
		server.clear();

		KStringView sContent = "this is our streamed content\n";
		KInStringStream iss(sContent);

		KHTTPClient HTTP("http://127.0.0.1:7654/abc", KHTTPMethod::POST);
		HTTP.SendRequest(iss);
		KString shtml;
		HTTP.Read(shtml);
		CHECK( shtml == "0123456789");
		kMilliSleep(50);
		if (server.m_rx.size() == 11)
		{
			CHECK( server.m_rx[0] == "POST /abc HTTP/1.1" );
			CHECK( server.m_rx[1] == "Host: 127.0.0.1:7654");
			CHECK( server.m_rx[2] == "Transfer-Encoding: chunked");
			CHECK( server.m_rx[3] == "Content-Type: text/plain");
			CHECK( server.m_rx[4] == kFormat("Accept-Encoding: {}", KHTTPCompression::GetCompressors()) );
			CHECK( server.m_rx[5] == "");
			sContent.remove_suffix(1);
			// follows the response wrapped in the chunking protocol
			CHECK( server.m_rx[6] == "1D"     );
			CHECK( server.m_rx[7] == sContent );
			CHECK( server.m_rx[8] == ""       );
			CHECK( server.m_rx[9] == "0"      );
			CHECK( server.m_rx[10] == ""      );
		}
		else
		{
			KString sJoined = kJoined(server.m_rx);
			INFO (sJoined);
			CHECK( server.m_rx.size() == 11 );
		}
	}

	SECTION("null connection object")
	{
		KHTTPClient HTTP;
		std::unique_ptr<KConnection> Connection;
		HTTP.Connect(std::move(Connection));
	}

	SECTION("null connection stream")
	{
		KHTTPClient HTTP;
		auto Connection = std::make_unique<KConnection>();
		HTTP.Connect(std::move(Connection));
	}

	SECTION("moved KOutHTTPFilter")
	{
		KOutHTTPFilter OutFilter;
		KOutHTTPFilter Filter2 = std::move(OutFilter);
		CHECK ( OutFilter.Fail() == false );
		OutFilter.FilteredStream();
		CHECK ( OutFilter.Count() == 0 );
	}

	SECTION("moved KInHTTPFilter")
	{
		KInHTTPFilter InFilter;
		KInHTTPFilter Filter2 = std::move(InFilter);
		CHECK ( InFilter.Fail() == false );
		InFilter.FilteredStream();
		CHECK ( InFilter.Count() == 0 );
	}

}

#endif // !Windows
