#include "catch.hpp"

#include <dekaf2/core/init/kdefinitions.h>

#if DEKAF2_HAS_WEBVIEW

#include <dekaf2/web/app/kwebapp.h>
#include <dekaf2/util/misc/kversion.h>
#include <dekaf2/net/tcp/ktcpstream.h>
#include <dekaf2/net/tls/ktlsstream.h>
#include <dekaf2/core/format/kformat.h>
#include <dekaf2/system/os/ksystem.h>
#include <thread>

using namespace dekaf2;

namespace {

struct Reply
{
	uint16_t iStatus { 0 };
	KString  sHeaders;
	KString  sBody;

	// the value of a response header, by its lowercase name
	KString Header(KStringView sName) const
	{
		for (auto sLine : sHeaders.Split("\n"))
		{
			if (sLine.starts_with(sName) && sLine.size() > sName.size() && sLine[sName.size()] == ':')
			{
				return KString(sLine.substr(sName.size() + 1)).Trim();
			}
		}
		return {};
	}
};

// a raw HTTP/1.1 request over an open stream, to control every header the guards look at
Reply Exchange(KIOStreamSocket& Stream, KStringView sRequestLine, std::initializer_list<KStringView> Headers, KStringView sBody = KStringView{})
{
	Reply R;

	if (!Stream.Good())
	{
		return R;
	}

	Stream.SetWriterEndOfLine("\r\n");
	Stream.SetReaderRightTrim("\r\n");
	Stream.WriteLine(sRequestLine);

	for (auto sHeader : Headers)
	{
		Stream.WriteLine(sHeader);
	}

	if (!sBody.empty())
	{
		Stream.WriteLine("Content-Type: application/x-www-form-urlencoded");
		Stream.WriteLine(kFormat("Content-Length: {}", sBody.size()));
	}

	Stream.WriteLine("Connection: close");
	Stream.WriteLine("");
	Stream.Write(sBody);
	Stream.Flush();

	KString sLine;

	if (!Stream.ReadLine(sLine))
	{
		return R;
	}

	// HTTP/1.1 403 Forbidden
	R.iStatus = sLine.Mid(9, 3).UInt16();

	// header names lowercased, values as they are
	while (Stream.ReadLine(sLine) && !sLine.empty())
	{
		auto iColon = sLine.find(':');
		R.sHeaders += KStringView(sLine).substr(0, iColon).ToLowerASCII();
		R.sHeaders += KStringView(sLine).substr(iColon);
		R.sHeaders += '\n';
	}

	Stream.ReadRemaining(R.sBody);

	return R;
}

// plain HTTP to the loopback server
Reply Request(uint16_t iPort, KStringView sRequestLine, std::initializer_list<KStringView> Headers)
{
	KTCPStream Stream(KTCPEndPoint(kFormat("127.0.0.1:{}", iPort)),
	                  KStreamOptions(KStreamOptions::CancelOnTimeout, chrono::seconds(2)));
	return Exchange(Stream, sRequestLine, Headers);
}

// TLS to the network server, the ephemeral certificate is not verified
Reply TLSRequest(uint16_t iPort, KStringView sRequestLine, std::initializer_list<KStringView> Headers, KStringView sBody = KStringView{})
{
	KTLSStream Stream(KTCPEndPoint(kFormat("127.0.0.1:{}", iPort)),
	                  KStreamOptions(KStreamOptions::None, chrono::seconds(5)));
	return Exchange(Stream, sRequestLine, Headers, sBody);
}

} // end of anonymous namespace

TEST_CASE("KWebApp")
{
	SECTION("webview version")
	{
		// the vendored amalgamation in from/webview reports a semantic version
		KVersion Version;
		CHECK ( Version.Parse(kGetWebViewVersion()) == true );
		CHECK ( Version.size()                      == 3    );
	}

	KRESTRoutes Routes;

	Routes.AddRoute({ KHTTPMethod::GET, false, "", [](KRESTServer& HTTP)
	{
		HTTP.json.tx["page"] = "home";
	}});

	Routes.AddRoute({ KHTTPMethod::GET, false, "/data", [](KRESTServer& HTTP)
	{
		HTTP.json.tx["page"] = "data";
	}});

	// the server alone, no window
	KWebApp::Options Options;
	Options.bWindow = false;

	KWebApp App(std::move(Options), Routes);
	REQUIRE ( App.HasError() == false );

	auto iPort  = App.GetPort();
	auto sToken = KString(App.GetToken());

	CHECK ( iPort         != 0  );
	CHECK ( sToken.size() == 64 );
	CHECK ( App.GetNetworkPort() == 0 );

	auto sHost   = kFormat("Host: 127.0.0.1:{}", iPort);
	auto sOrigin = kFormat("Origin: http://127.0.0.1:{}", iPort);
	auto sCookie = kFormat("Cookie: kwa={}", sToken);

	SECTION("no token")
	{
		CHECK ( Request(iPort, "GET / HTTP/1.1", { sHost }).iStatus == 403 );
		// also for paths that no route serves - the guard runs before routing
		CHECK ( Request(iPort, "GET /favicon.ico HTTP/1.1", { sHost }).iStatus == 403 );
		// and for the network-only paths
		CHECK ( Request(iPort, "GET /healthz HTTP/1.1", { sHost }).iStatus == 403 );
		CHECK ( Request(iPort, "GET /login HTTP/1.1",   { sHost }).iStatus == 403 );
	}

	SECTION("wrong token")
	{
		CHECK ( Request(iPort, "GET / HTTP/1.1", { sHost, "Cookie: kwa=0123456789abcdef" }).iStatus == 403 );
	}

	SECTION("token")
	{
		auto R = Request(iPort, "GET / HTTP/1.1", { sHost, sCookie });
		CHECK ( R.iStatus == 200 );
		CHECK ( R.sBody.contains("home") );

		R = Request(iPort, "GET /data HTTP/1.1", { sHost, sCookie });
		CHECK ( R.iStatus == 200 );
		CHECK ( R.sBody.contains("data") );

		// the network paths do not exist for the window
		CHECK ( Request(iPort, "GET /healthz HTTP/1.1", { sHost, sCookie }).iStatus == 404 );
		CHECK ( Request(iPort, "GET /login HTTP/1.1",   { sHost, sCookie }).iStatus == 404 );
	}

	SECTION("wrong host")
	{
		CHECK ( Request(iPort, "GET / HTTP/1.1", { kFormat("Host: localhost:{}", iPort), sCookie }).iStatus == 403 );
		CHECK ( Request(iPort, "GET / HTTP/1.1", { kFormat("Host: 127.0.0.1:{}", iPort + 1), sCookie }).iStatus == 403 );
	}

	SECTION("origin")
	{
		CHECK ( Request(iPort, "GET / HTTP/1.1", { sHost, sCookie, sOrigin }).iStatus == 200 );
		CHECK ( Request(iPort, "GET / HTTP/1.1", { sHost, sCookie, "Origin: http://evil.example" }).iStatus == 403 );
		CHECK ( Request(iPort, "GET / HTTP/1.1", { sHost, sCookie, "Origin: http://localhost:80" }).iStatus == 403 );
	}

	SECTION("fetch metadata")
	{
		CHECK ( Request(iPort, "GET / HTTP/1.1", { sHost, sCookie, "Sec-Fetch-Site: same-origin" }).iStatus == 200 );
		CHECK ( Request(iPort, "GET / HTTP/1.1", { sHost, sCookie, "Sec-Fetch-Site: none"        }).iStatus == 200 );
		CHECK ( Request(iPort, "GET / HTTP/1.1", { sHost, sCookie, "Sec-Fetch-Site: cross-site"  }).iStatus == 403 );
		CHECK ( Request(iPort, "GET / HTTP/1.1", { sHost, sCookie, "Sec-Fetch-Site: same-site"   }).iStatus == 403 );
	}

	SECTION("websocket upgrade needs an origin")
	{
		CHECK ( Request(iPort, "GET / HTTP/1.1", { sHost, sCookie, "Upgrade: websocket" }).iStatus == 403 );
	}

	SECTION("enter")
	{
		auto R = Request(iPort, kFormat("GET /_kwa/enter?token={}&next=/data HTTP/1.1", sToken), { sHost });
		CHECK ( R.iStatus == 302 );
		CHECK ( R.Header("location") == "/data" );
		CHECK ( R.Header("set-cookie").starts_with(kFormat("kwa={}", sToken)) );
		CHECK ( R.Header("set-cookie").contains("HttpOnly") );
		CHECK ( R.Header("set-cookie").contains("SameSite=Strict") );

		// a wrong token gets no cookie
		R = Request(iPort, "GET /_kwa/enter?token=0123456789abcdef&next=/data HTTP/1.1", { sHost });
		CHECK ( R.iStatus == 403 );
		CHECK ( R.Header("set-cookie").empty() );

		// next stays on our origin
		R = Request(iPort, kFormat("GET /_kwa/enter?token={}&next=//evil.example/ HTTP/1.1", sToken), { sHost });
		CHECK ( R.iStatus == 302 );
		CHECK ( R.Header("location") == "/" );

		R = Request(iPort, kFormat("GET /_kwa/enter?token={}&next=http://evil.example/ HTTP/1.1", sToken), { sHost });
		CHECK ( R.iStatus == 302 );
		CHECK ( R.Header("location") == "/" );
	}

	SECTION("enter URL")
	{
		auto sURL = App.GetEnterURL("/data");
		CHECK ( sURL.starts_with(kFormat("http://127.0.0.1:{}/_kwa/enter?token={}&next=", iPort, sToken)) );

		// and it works as the first request
		auto R = Request(iPort, kFormat("GET {} HTTP/1.1", KStringView(sURL).Mid(sURL.find('/', 7))), { sHost });
		CHECK ( R.iStatus == 302 );
		CHECK ( R.Header("location") == "/data" );
	}

	SECTION("run without window until quit")
	{
		std::thread Quitter([&]
		{
			kSleep(chrono::milliseconds(200));
			// the server still answers while running
			CHECK ( Request(iPort, "GET / HTTP/1.1", { sHost, sCookie }).iStatus == 200 );
			App.Quit();
		});

		CHECK ( App.Run() == 0 );
		Quitter.join();

		// the server is gone after Run()
		CHECK ( Request(iPort, "GET / HTTP/1.1", { sHost, sCookie }).iStatus == 0 );
	}
}

TEST_CASE("KWebApp network")
{
	KRESTRoutes Routes;

	Routes.AddRoute({ KHTTPMethod::GET, false, "", [](KRESTServer& HTTP)
	{
		HTTP.json.tx["page"] = "home";
	}});

	Routes.AddRoute({ KHTTPMethod::GET, false, "/native", [](KRESTServer& HTTP)
	{
		HTTP.json.tx["page"] = "native";
	}});

	// the servers alone, the network one on a loopback address with a fresh
	// ephemeral certificate that is not written to disk
	KWebApp::Options Options;
	Options.bWindow                     = false;
	Options.bNetwork                    = true;
	Options.Network.sBindAddress        = "127.0.0.1";
	Options.Network.bStoreEphemeralCert = false;
	Options.WindowOnlyPaths             = { "/native" };
	Options.Authenticate                = [](KStringView sUser, KStringView sPassword)
	{
		return sUser == "alice" && sPassword == "secret";
	};

	KWebApp App(std::move(Options), Routes);
	REQUIRE ( App.HasError() == false );

	auto iPort = App.GetNetworkPort();
	CHECK ( iPort        != 0 );
	CHECK ( App.GetPort() != 0 );
	CHECK ( App.GetPort() != iPort );

	auto sHost = kFormat("Host: 127.0.0.1:{}", iPort);

	// logs in and returns the session cookie for further requests
	auto LogIn = [&]() -> KString
	{
		auto R = TLSRequest(iPort, "POST /login HTTP/1.1", { sHost }, "user=alice&password=secret&next=/");
		CHECK ( R.iStatus == 302 );
		CHECK ( R.Header("location") == "/" );
		auto sSetCookie = R.Header("set-cookie");
		CHECK ( sSetCookie.starts_with("__Host-session=") );
		CHECK ( sSetCookie.contains("Secure")   );
		CHECK ( sSetCookie.contains("HttpOnly") );
		return kFormat("Cookie: {}", KStringView(sSetCookie).substr(0, sSetCookie.find(';')));
	};

	SECTION("health without login, but not on the loopback server")
	{
		auto R = TLSRequest(iPort, "GET /healthz HTTP/1.1", { sHost });
		CHECK ( R.iStatus == 200 );
		CHECK ( R.sBody   == "ok\n" );
	}

	SECTION("security headers on every response")
	{
		auto R = TLSRequest(iPort, "GET /healthz HTTP/1.1", { sHost });
		CHECK ( R.Header("strict-transport-security").starts_with("max-age=") );
		CHECK ( R.Header("x-content-type-options")   == "nosniff" );
		CHECK ( R.Header("x-frame-options")          == "DENY" );
		CHECK ( R.Header("referrer-policy")          == "same-origin" );
		CHECK ( R.Header("content-security-policy").contains("default-src 'self'") );
	}

	SECTION("login required")
	{
		auto R = TLSRequest(iPort, "GET /?dir=x HTTP/1.1", { sHost });
		CHECK ( R.iStatus == 302 );
		CHECK ( R.Header("location") == "/login?next=/?dir%3Dx" );

		CHECK ( TLSRequest(iPort, "POST / HTTP/1.1", { sHost }, "a=b").iStatus == 401 );
		CHECK ( TLSRequest(iPort, "GET / HTTP/1.1", { sHost, "Upgrade: websocket" }).iStatus == 401 );
	}

	SECTION("login page")
	{
		auto R = TLSRequest(iPort, "GET /login?next=/data HTTP/1.1", { sHost });
		CHECK ( R.iStatus == 200 );
		CHECK ( R.Header("content-type").starts_with("text/html") );
		CHECK ( R.sBody.contains("name=\"user\"")     );
		CHECK ( R.sBody.contains("type=\"password\"") );
		CHECK ( R.sBody.contains("value=\"/data\"")   );
		CHECK ( R.sBody.contains("Wrong user name") == false );

		R = TLSRequest(iPort, "GET /login?error=1 HTTP/1.1", { sHost });
		CHECK ( R.sBody.contains("Wrong user name") );
	}

	SECTION("wrong password")
	{
		auto R = TLSRequest(iPort, "POST /login HTTP/1.1", { sHost }, "user=alice&password=nope&next=/data");
		CHECK ( R.iStatus == 302 );
		CHECK ( R.Header("location") == "/login?error=1&next=/data" );
		CHECK ( R.Header("set-cookie").empty() );
	}

	SECTION("session")
	{
		auto sCookie = LogIn();

		auto R = TLSRequest(iPort, "GET / HTTP/1.1", { sHost, sCookie });
		CHECK ( R.iStatus == 200 );
		CHECK ( R.sBody.contains("home") );

		// the window-only path is refused, although logged in
		R = TLSRequest(iPort, "GET /native HTTP/1.1", { sHost, sCookie });
		CHECK ( R.iStatus == 403 );

		// logout ends the session
		R = TLSRequest(iPort, "POST /logout HTTP/1.1", { sHost, sCookie });
		CHECK ( R.iStatus == 302 );
		CHECK ( R.Header("location") == "/login" );
		CHECK ( R.Header("set-cookie").starts_with("__Host-session=") );

		R = TLSRequest(iPort, "GET / HTTP/1.1", { sHost, sCookie });
		CHECK ( R.iStatus == 302 );
	}

	SECTION("login attempts are limited per address")
	{
		uint16_t iLast { 0 };

		for (int i = 0; i < 12 && iLast != 429; ++i)
		{
			iLast = TLSRequest(iPort, "POST /login HTTP/1.1", { sHost }, "user=alice&password=nope").iStatus;
		}

		CHECK ( iLast == 429 );
	}

	SECTION("the window side is untouched")
	{
		auto sToken = KString(App.GetToken());
		CHECK ( Request(App.GetPort(), "GET /native HTTP/1.1", { kFormat("Host: 127.0.0.1:{}", App.GetPort()), kFormat("Cookie: kwa={}", sToken) }).iStatus == 200 );
		CHECK ( Request(App.GetPort(), "GET /healthz HTTP/1.1", { kFormat("Host: 127.0.0.1:{}", App.GetPort()), kFormat("Cookie: kwa={}", sToken) }).iStatus == 404 );
	}
}

#endif // DEKAF2_HAS_WEBVIEW
