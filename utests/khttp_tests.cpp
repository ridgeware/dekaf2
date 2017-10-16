#include "catch.hpp"

#include <dekaf2/khttp.h>
#include <dekaf2/ktcpserver.h>
#include <dekaf2/kstring.h>

using namespace dekaf2;

class KTinyHTTPServer : public KTCPServer
{

public:

	template<typename... Args>
	KTinyHTTPServer(Args&&... args)
	    : KTCPServer(std::forward<Args>(args)...)
	{}

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

	virtual bool Accepted(KStream& stream, const endpoint_type& remote_endpoint) override
	{
		stream.SetReaderRightTrim("\r\n");
		stream.SetWriterEndOfLine("\r\n");
		return true;
	}

	virtual KString Request(const KString& qstr, Parameters& parameters) override
	{
		m_rx.push_back(qstr);
		KString tmp;
		if (qstr.empty())
		{
			tmp = "Connection : close\r\nContent-Length : 10\r\n\r\n0123456789";
		}
		return tmp;
	}

	KString m_sResponse;
};

TEST_CASE("KHTTP") {

	SECTION("check conection setup")
	{
		KTinyHTTPServer server(7654, false);
		server.Start(300, false);
		server.clear();

		url::KDomain Domain("127.0.0.1");
		url::KPort   Port("7654");
		KConnection  cx(Domain, Port);
		KHTTP        cHTTP(cx);
		KURL         URL("http://127.0.0.1:7654/path?query=val&another=here#fragment");
		cHTTP.Resource(URL);
		cHTTP.Request();
		KString shtml;
		CHECK( cHTTP.size() == 10 );
		cHTTP.Read(shtml);
		CHECK( shtml == "0123456789");
		CHECK( server.m_rx.size() == 3 );
		if (server.m_rx.size() == 3)
		{
			CHECK( server.m_rx[0] == "GET /path?query=val&another=here#fragment HTTP/1.1" );
			CHECK( server.m_rx[1] == "Host: 127.0.0.1");
			CHECK( server.m_rx[2] == "");
		}
	}

}

