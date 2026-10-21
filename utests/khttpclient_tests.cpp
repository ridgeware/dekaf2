#include "catch.hpp"

#include <dekaf2/http/client/khttpclient.h>
#include <dekaf2/http/client/kwebclient.h>
#include <dekaf2/http/server/khttperror.h>
#include <dekaf2/net/tcp/ktcpserver.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/strings/bits/kfindsetofchars.h>
#include <dekaf2/core/format/kformat.h>
#include <dekaf2/containers/associative/kassociative.h>
#include <dekaf2/time/duration/kduration.h>
#include <dekaf2/system/filesystem/kfilesystem.h>
#include <dekaf2/system/os/ksystem.h>
#include <dekaf2/io/streams/koutstringstream.h>
#include <dekaf2/crypto/hash/kmessagedigest.h>
#include <atomic>


using namespace dekaf2;

namespace {

KTempDir TempDir(true, 95);

#ifdef DEKAF2_HAS_UNIX_SOCKETS
// the URL of a unix domain socket - on Windows, the path starts with a drive letter,
// which needs a slash before it, as in file:///C:/dir
KURL UnixSocketURL(KStringView sSocketFile)
{
	return KURL(kFormat("unix://{}{}", (!sSocketFile.empty() && sSocketFile.front() == '/') ? "" : "/", sSocketFile));
}
#endif

} // end of anonymous namespace

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
		server.Start(chrono::seconds(2), false);
		server.clear();

		KURL URL("http://127.0.0.1:7654/path?query=val&another=here#fragment");
		KStreamOptions Options;
		Options.SetKeepAliveInterval(chrono::seconds(60));
		Options.SetLingerTimeout(chrono::seconds(5));
		auto cx = KIOStreamSocket::Create(URL, false, Options);
		CHECK( cx->Good() == true );
		if (cx->Good() == true)
		{
			CHECK( cx->ostream().good() == true );
			CHECK( cx->istream().good() == true );
			CHECK( kGetTCPKeepAliveInterval(cx->GetNativeSocket()) == chrono::seconds(60) );
			CHECK( kGetLingerTimeout(cx->GetNativeSocket())        == chrono::seconds( 5) );
		}
		KHTTPClient cHTTP(std::move(cx));
		cHTTP.BasicAuthentication("testuser", "testpass");
		cHTTP.Resource(URL);
		CHECK( cHTTP.SendRequest() == true );
		KString shtml;
		cHTTP.Read(shtml);
		CHECK( shtml == "0123456789");
		CHECK( server.m_rx.size() == 6 );
		if (server.m_rx.size() == 6)
		{
			CHECK( server.m_rx[0] == "GET /path?query=val&another=here HTTP/1.1" );
			CHECK( server.m_rx[1] == "host: 127.0.0.1:7654");
			CHECK( server.m_rx[2] == "authorization: Basic dGVzdHVzZXI6dGVzdHBhc3M=" );
			CHECK( server.m_rx[3] == kFormat("accept-encoding: {}", KHTTPCompression::GetCompressors()) );
			CHECK( server.m_rx[4] == "user-agent: dekaf/" DEKAF_VERSION );
			CHECK( server.m_rx[5] == "");
		}

		// second round
		server.clear();
		cHTTP.TokenAuthentication("PIUHuuhisdfuUYI78329YRDtfuyighUGYyt");
		CHECK( cHTTP.SendRequest() == true );
		shtml.clear();
		cHTTP.Read(shtml);
		CHECK( shtml == "0123456789");
		CHECK( server.m_rx.size() == 6 );
		if (server.m_rx.size() == 6)
		{
			CHECK( server.m_rx[0] == "GET /path?query=val&another=here HTTP/1.1" );
			CHECK( server.m_rx[1] == "host: 127.0.0.1:7654");
			CHECK( server.m_rx[2] == "authorization: Bearer PIUHuuhisdfuUYI78329YRDtfuyighUGYyt" );
			CHECK( server.m_rx[3] == kFormat("accept-encoding: {}", KHTTPCompression::GetCompressors()) );
			CHECK( server.m_rx[4] == "user-agent: dekaf/" DEKAF_VERSION );
			CHECK( server.m_rx[5] == "");
		}

		server.Stop();
	}

	SECTION("request header with a line break fails the request")
	{
		KTinyHTTPServer server(7654, false, 3);
		server.Start(chrono::seconds(2), false);
		server.clear();

		KURL URL("http://127.0.0.1:7654/path");
		auto cx = KIOStreamSocket::Create(URL);
		CHECK( cx->Good() == true );
		KHTTPClient cHTTP(std::move(cx));
		cHTTP.Resource(URL);
		// a token read with its trailing newline
		cHTTP.AddHeader(KHTTPHeader::AUTHORIZATION, "Bearer PIUHuuhisdfuUYI78329YRDtfuyighUGYyt\n");
		CHECK( cHTTP.SendRequest() == false );
		CHECK( cHTTP.Error().contains("authorization") );
		// no network error, so no retry
		CHECK( cHTTP.GetStatusCode() != KHTTPError::H5xx_READTIMEOUT );

		server.Stop();
	}

	SECTION("check serialization")
	{
		KTinyHTTPServer server(7654, false, 3);
		server.Start(chrono::seconds(2), false);
		server.clear();

		KURL URL("http://127.0.0.1:7654/path?query=val&another=here#fragment");
		auto cx = KIOStreamSocket::Create(URL);
		CHECK( cx->Good() == true );
		if (cx->Good() == true)
		{
			CHECK( cx->ostream().good() == true );
			CHECK( cx->istream().good() == true );
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
			CHECK( server.m_rx[1] == "host: 127.0.0.1:7654");
			CHECK( server.m_rx[2] == "");
		}
	}

#ifdef DEKAF2_HAS_UNIX_SOCKETS
	SECTION("check unix sockets")
	{
		auto sSocketFile = kFormat("{}/test1.socket", TempDir.Name());
		KTinyHTTPServer server(sSocketFile, 3);
		server.Start(chrono::seconds(2), false);
		server.clear();

		// unix sockets require a different URL for the connection (the file system path
		// to the socket) and the request (a relative path on the http server side)

		KURL URL("http://127.0.0.1:7654/path?query=val&another=here#fragment");

		KHTTPClient cHTTP;
		KURL ConnectURL = UnixSocketURL(sSocketFile);
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
			CHECK( server.m_rx[1] == "host: 127.0.0.1:7654");
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
			CHECK( server.m_rx[1] == "host: 127.0.0.1:7654");
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
		CHECK( Stop.elapsed().microseconds() < chrono::microseconds(10000) );
	}

	SECTION("send a stream")
	{
		KTinyHTTPServer server(7654, false, 3);
		server.Start(chrono::seconds(2), false);
		server.clear();

		KStringView sContent = "this is our streamed content\n";
		KInStringStream iss(sContent);

		KHTTPClient HTTP("http://127.0.0.1:7654/abc", KHTTPMethod::POST);
		HTTP.SendRequest(iss);
		KString shtml;
		HTTP.Read(shtml);
		CHECK( shtml == "0123456789");
		kSleep(chrono::milliseconds(50));
		if (server.m_rx.size() == 11)
		{
			CHECK( server.m_rx[0] == "POST /abc HTTP/1.1" );
			CHECK( server.m_rx[1] == "host: 127.0.0.1:7654");
			CHECK( server.m_rx[2] == "transfer-encoding: chunked");
			CHECK( server.m_rx[3] == "content-type: text/plain");
			CHECK( server.m_rx[4] == kFormat("accept-encoding: {}", KHTTPCompression::GetCompressors()) );
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
			CHECK( server.m_rx.size() == 12 );
		}
	}

	SECTION("null connection object")
	{
		KHTTPClient HTTP;
		std::unique_ptr<KIOStreamSocket> Connection;
		HTTP.Connect(std::move(Connection));
	}

#if 0
	SECTION("null connection stream")
	{
		KHTTPClient HTTP;
		auto Connection = std::make_unique<KIOStreamSocket>();
		HTTP.Connect(std::move(Connection));
	}
#endif

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

	SECTION("DigestAuthenticator RFC 2617 HA1 formula")
	{
		// Verify that HA1 is computed as MD5(username:realm:password) per RFC 2617 §3.2.2.2.
		// The regression this guards against: an earlier version of dekaf2 computed
		// HA1 = MD5(username:password) (missing the realm), which causes every standards-
		// compliant Digest server (Apache, nginx, FritzBox, curl, ...) to reject us with 401.
		//
		// GetAuthHeader() picks a random cnonce, so we can't string-compare the whole header.
		// Instead we parse the emitted response= field and recompute the expected value from
		// the cnonce/nc the implementation reported, using the RFC-correct HA1.

		KHTTPClient::DigestAuthenticator auth(
			"Mufasa",                // username
			"Circle Of Life",        // password
			"testrealm@host.com",    // realm
			"dcd98b7102dd2f0e8b11d0f600bfb0c093", // nonce
			"5ccc069c403ebaf9f0171e9517f40e41",   // opaque
			"auth"                   // qop
		);

		KOutHTTPRequest Request;
		Request.Method            = KHTTPMethod::GET;
		Request.Resource          = KURL("http://example.com/dir/index.html");

		KString sHeader = auth.GetAuthHeader(Request, "");

		// must be a well-formed Digest header
		CHECK ( sHeader.starts_with("Digest ") );
		CHECK ( sHeader.contains("username=\"Mufasa\"") );
		CHECK ( sHeader.contains("realm=\"testrealm@host.com\"") );
		CHECK ( sHeader.contains("nonce=\"dcd98b7102dd2f0e8b11d0f600bfb0c093\"") );
		CHECK ( sHeader.contains("uri=\"/dir/index.html\"") );
		CHECK ( sHeader.contains("qop=auth") );

		// strip the "Digest " scheme prefix, then split the comma-separated
		// parameter list into a key/value map. bRespectQuotes=true removes
		// the surrounding quotes from quoted values.
		KStringView sParams(sHeader);
		sParams.remove_prefix("Digest ");

		constexpr KFindSetOfChars ByComma(",");
		auto Parts = sParams.Split<KMap<KStringView, KStringView>>(
		                 ByComma,                     // outer delimiter
		                 "=",                         // pair delimiter
		                 detail::kASCIISpacesSet,     // trim whitespace around tokens
		                 '\0',                        // no escape char
		                 false,                       // don't combine delimiters
		                 true                         // strip surrounding quotes
		             );

		auto sCNonce   = Parts["cnonce"];
		auto sNC       = Parts["nc"];
		auto sResponse = Parts["response"];

		INFO("Digest header: " << sHeader);
		CHECK ( !sCNonce.empty() );
		CHECK ( sNC == "00000001" );
		CHECK ( sResponse.size() == 32 );

		// RFC-2617-correct recomputation, mirroring the step-by-step KMD5 pattern
		// the implementation uses (to avoid any subtle difference from one-shot
		// KMD5 construction).
		KMD5 HA1;
		HA1  = KStringView("Mufasa");
		HA1 += ":";
		HA1 += "testrealm@host.com";
		HA1 += ":";
		HA1 += "Circle Of Life";

		KMD5 HA2;
		HA2  = KStringView("GET");
		HA2 += ":";
		HA2 += "/dir/index.html";

		KMD5 Expected;
		Expected  = HA1.HexDigest();
		Expected += ":";
		Expected += "dcd98b7102dd2f0e8b11d0f600bfb0c093";
		Expected += ":";
		Expected += sNC;
		Expected += ":";
		Expected += sCNonce;
		Expected += ":";
		Expected += "auth";
		Expected += ":";
		Expected += HA2.HexDigest();
		KString sExpected = Expected.HexDigest();

		INFO("HA1 = " << HA1.HexDigest());
		INFO("HA2 = " << HA2.HexDigest());
		CHECK ( sResponse == sExpected );

		// Also check the buggy formula (MD5(user:password) without realm) does NOT match
		KMD5 BuggyHA1;
		BuggyHA1  = KStringView("Mufasa");
		BuggyHA1 += ":";
		BuggyHA1 += "Circle Of Life";
		KMD5 Buggy;
		Buggy  = BuggyHA1.HexDigest();
		Buggy += ":";
		Buggy += "dcd98b7102dd2f0e8b11d0f600bfb0c093";
		Buggy += ":";
		Buggy += sNC;
		Buggy += ":";
		Buggy += sCNonce;
		Buggy += ":";
		Buggy += "auth";
		Buggy += ":";
		Buggy += HA2.HexDigest();
		CHECK ( sResponse != Buggy.HexDigest() );
	}

}

namespace {

// answers every request with a scripted response and closes the connection after
// it, without announcing that in a Connection header
class KScriptedHTTPServer : public KTCPServer
{

public:

	KScriptedHTTPServer(uint16_t iPort)
	: KTCPServer(iPort, false, 5)
	{
	}

	// the response head, a pause after it, and the body
	void Respond(KString sHead, KString sBody = {}, KDuration Pause = KDuration::zero())
	{
		m_sHead = std::move(sHead);
		m_sBody = std::move(sBody);
		m_Pause = Pause;
	}

	std::atomic<int> m_iConnections { 0 };

protected:

	virtual void Session(std::unique_ptr<KIOStreamSocket>& stream) override
	{
		++m_iConnections;

		stream->SetReaderRightTrim("\r\n");

		KString sLine;

		// the requests of these tests have no body
		while (stream->ReadLine(sLine) && !sLine.empty())
		{
		}

		stream->Write(m_sHead).Flush();

		if (m_Pause > KDuration::zero())
		{
			kSleep(m_Pause);
		}

		stream->Write(m_sBody).Flush();
	}

	KString   m_sHead;
	KString   m_sBody;
	KDuration m_Pause;

}; // KScriptedHTTPServer

} // end of anonymous namespace

TEST_CASE("KHTTPClient transport errors")
{
	KScriptedHTTPServer Server(7656);
	Server.Start(chrono::seconds(2), false);

	KStringViewZ sURL = "http://127.0.0.1:7656/path";

	SECTION("names")
	{
		CHECK ( KHTTPClient::ToString(KHTTPClient::TransportError::None)              == "none"               );
		CHECK ( KHTTPClient::ToString(KHTTPClient::TransportError::Connect)           == "connect"            );
		CHECK ( KHTTPClient::ToString(KHTTPClient::TransportError::Write)             == "write"              );
		CHECK ( KHTTPClient::ToString(KHTTPClient::TransportError::Timeout)           == "timeout"            );
		CHECK ( KHTTPClient::ToString(KHTTPClient::TransportError::Read)              == "read"               );
		CHECK ( KHTTPClient::ToString(KHTTPClient::TransportError::IncompleteBody)    == "incomplete body"    );
		CHECK ( KHTTPClient::ToString(KHTTPClient::TransportError::HeaderLimit)       == "header limit"       );
		CHECK ( KHTTPClient::ToString(KHTTPClient::TransportError::MalformedResponse) == "malformed response" );
	}

	SECTION("no connection")
	{
		// nothing listens on port 1
		KWebClient HTTP;
		HTTP.SetTimeout(chrono::seconds(1));
		HTTP.Get("http://127.0.0.1:1/path");
		CHECK ( HTTP.GetTransportError() == KHTTPClient::TransportError::Connect );
		// no 598 for a connection that could not be established, as before
		CHECK ( HTTP.GetStatusCode() == 0 );
		CHECK ( HTTP.HttpSuccess() == false );
	}

	SECTION("a 404 is no transport error")
	{
		Server.Respond("HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n");
		KWebClient HTTP;
		HTTP.Get(sURL);
		CHECK ( HTTP.GetTransportError() == KHTTPClient::TransportError::None );
		CHECK ( HTTP.GetStatusCode() == 404 );
		CHECK ( HTTP.HttpSuccess() == false );
	}

	SECTION("a complete response")
	{
		Server.Respond("HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\n", "hello");
		KWebClient HTTP;
		CHECK ( HTTP.Get(sURL) == "hello" );
		CHECK ( HTTP.GetTransportError() == KHTTPClient::TransportError::None );
		CHECK ( HTTP.GetStatusCode() == 200 );
		CHECK ( HTTP.HttpSuccess() == true );
	}

	SECTION("a body that ends before Content-Length")
	{
		Server.Respond("HTTP/1.1 200 OK\r\nContent-Length: 100\r\n\r\n", "short");
		KWebClient HTTP;
		CHECK ( HTTP.Get(sURL) == "short" );
		CHECK ( HTTP.GetTransportError() == KHTTPClient::TransportError::IncompleteBody );
		// during the transition, a transport error replaces a 2xx status with 598
		CHECK ( HTTP.GetStatusCode() == KHTTPError::H5xx_READTIMEOUT );
		CHECK ( HTTP.HttpSuccess() == false );
	}

	SECTION("a chunked body without the last chunk")
	{
		Server.Respond("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n", "5\r\nhello\r\n");
		KWebClient HTTP;
		CHECK ( HTTP.Get(sURL) == "hello" );
		CHECK ( HTTP.GetTransportError() == KHTTPClient::TransportError::IncompleteBody );
		CHECK ( HTTP.GetStatusCode() == KHTTPError::H5xx_READTIMEOUT );
		CHECK ( HTTP.HttpSuccess() == false );
	}

	SECTION("a timeout while reading the body")
	{
		Server.Respond("HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\n", "hello", chrono::milliseconds(1500));
		KWebClient HTTP;
		HTTP.SetTimeout(chrono::milliseconds(300));
		HTTP.Get(sURL);
		CHECK ( HTTP.GetTransportError() == KHTTPClient::TransportError::Timeout );
		CHECK ( HTTP.GetStatusCode() == KHTTPError::H5xx_READTIMEOUT );
		CHECK ( HTTP.HttpSuccess() == false );
	}

	SECTION("a response header above the limit")
	{
		Server.Respond("HTTP/1.1 200 OK\r\nX: " + KString(64 * 1024, 'a') + "\r\nContent-Length: 0\r\n\r\n");
		KWebClient HTTP;
		HTTP.Get(sURL);
		CHECK ( HTTP.GetTransportError() == KHTTPClient::TransportError::HeaderLimit );
		CHECK ( HTTP.Response.GetParseError() == KHTTPHeaders::FieldTooLong );
		CHECK ( HTTP.GetStatusCode() == KHTTPError::H5xx_READTIMEOUT );
		CHECK ( HTTP.HttpSuccess() == false );
	}

	SECTION("a malformed response")
	{
		Server.Respond("garbage\r\n\r\n");
		KWebClient HTTP;
		HTTP.Get(sURL);
		CHECK ( HTTP.GetTransportError() == KHTTPClient::TransportError::MalformedResponse );
		CHECK ( HTTP.GetStatusCode() == KHTTPError::H5xx_READTIMEOUT );
		CHECK ( HTTP.HttpSuccess() == false );
	}

	SECTION("a connection that closes without a response")
	{
		Server.Respond("");
		KWebClient HTTP;
		HTTP.Get(sURL);
		CHECK ( HTTP.GetTransportError() == KHTTPClient::TransportError::Read );
		CHECK ( HTTP.GetStatusCode() == KHTTPError::H5xx_READTIMEOUT );
		CHECK ( HTTP.HttpSuccess() == false );
	}

	SECTION("a reused connection that the server closed")
	{
		// the server closes each connection after its response, but the client
		// considers it reusable - the second request fails on it and is retried
		// on a new connection
		Server.Respond("HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\n", "hello");
		KWebClient HTTP;
		CHECK ( HTTP.Get(sURL) == "hello" );
		CHECK ( HTTP.Get(sURL) == "hello" );
		CHECK ( HTTP.GetTransportError() == KHTTPClient::TransportError::None );
		CHECK ( HTTP.HttpSuccess() == true );
		CHECK ( Server.m_iConnections == 2 );
	}

	Server.Stop();
}

TEST_CASE("KHTTPClient response header limits")
{
	KScriptedHTTPServer Server(7657);
	Server.Start(chrono::seconds(2), false);

	KStringViewZ sURL = "http://127.0.0.1:7657/path";

	// five fields of 60 KiB each: every field is below the field limit of 64 KiB,
	// together they exceed the section limit of 256 KiB
	KString sLargeSection = "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n";

	for (int i = 0; i < 5; ++i)
	{
		sLargeSection += kFormat("X-{}: {}\r\n", i, KString(60 * 1024, 'a'));
	}

	sLargeSection += "\r\n";

	KString sLongField = "HTTP/1.1 200 OK\r\nX: " + KString(64 * 1024, 'a') + "\r\nContent-Length: 0\r\n\r\n";

	SECTION("default limits")
	{
		KWebClient HTTP;
		CHECK ( HTTP.GetResponseHeaderLimits().iMaxFieldLength == KHTTPHeaders::DefaultResponseLimits.iMaxFieldLength );
		CHECK ( HTTP.GetResponseHeaderLimits().iMaxFieldCount  == KHTTPHeaders::DefaultResponseLimits.iMaxFieldCount  );
		CHECK ( HTTP.GetResponseHeaderLimits().iMaxSectionSize == KHTTPHeaders::DefaultResponseLimits.iMaxSectionSize );
	}

	SECTION("a field of 9 KiB is accepted")
	{
		// rejected with the former limit of 8 KiB for responses
		Server.Respond("HTTP/1.1 200 OK\r\nX: " + KString(9 * 1024, 'a') + "\r\nContent-Length: 5\r\n\r\n", "hello");
		KWebClient HTTP;
		CHECK ( HTTP.Get(sURL) == "hello" );
		CHECK ( HTTP.GetTransportError() == KHTTPClient::TransportError::None );
		CHECK ( HTTP.Response.Headers.Get("X").size() == 9 * 1024 );
	}

	SECTION("a section above 256 KiB is rejected")
	{
		Server.Respond(sLargeSection);
		KWebClient HTTP;
		HTTP.Get(sURL);
		CHECK ( HTTP.GetTransportError() == KHTTPClient::TransportError::HeaderLimit );
		CHECK ( HTTP.Response.GetParseError() == KHTTPHeaders::SectionTooLarge );
		CHECK ( HTTP.HttpSuccess() == false );
	}

	SECTION("raised limits accept a longer field and a larger section, for every request")
	{
		KWebClient HTTP;
		HTTP.SetResponseHeaderLimits({ 128 * 1024, 500, 512 * 1024 });

		Server.Respond(sLongField);
		HTTP.Get(sURL);
		CHECK ( HTTP.GetTransportError() == KHTTPClient::TransportError::None );
		CHECK ( HTTP.GetStatusCode() == 200 );

		Server.Respond(sLargeSection);
		HTTP.Get(sURL);
		CHECK ( HTTP.GetTransportError() == KHTTPClient::TransportError::None );
		CHECK ( HTTP.GetStatusCode() == 200 );
	}

	SECTION("lowered limits reject smaller headers")
	{
		Server.Respond("HTTP/1.1 200 OK\r\nX: " + KString(2 * 1024, 'a') + "\r\nContent-Length: 0\r\n\r\n");
		KWebClient HTTP;
		HTTP.SetResponseHeaderLimits({ 1024, 500, 0 });
		HTTP.Get(sURL);
		CHECK ( HTTP.GetTransportError() == KHTTPClient::TransportError::HeaderLimit );
		CHECK ( HTTP.Response.GetParseError() == KHTTPHeaders::FieldTooLong );
	}

	Server.Stop();
}
