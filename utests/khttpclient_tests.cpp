#include "catch.hpp"

#include <dekaf2/khttpclient.h>
#include <dekaf2/ktcpserver.h>
#include <dekaf2/kstring.h>
#include <dekaf2/ktimer.h>

#ifndef DEKAF2_IS_WINDOWS

using namespace dekaf2;

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
		server.Start(30, false);
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
			CHECK( server.m_rx[2] == "Accept-Encoding: gzip, bzip2, deflate");
			CHECK( server.m_rx[3] == "");
		}
	}

	SECTION("check serialization")
	{
		KTinyHTTPServer server(7654, false);
		server.Start(10, false);
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

	SECTION("failing connection")
	{
		KHTTPClient Client("http://koltun-ballet-boston.com/");
		KStopTime Stop;
		CHECK( Client.SendRequest() == false );
		// allow for a millisecond of fail time
		CHECK( Stop.microseconds() < 1000 );
	}

}

#endif // !Windows
