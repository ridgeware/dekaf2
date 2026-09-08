#include "catch.hpp"

#include <dekaf2/core/init/kdefinitions.h>

#if DEKAF2_HAS_WEBVIEW

#include <dekaf2/web/app/kwebapp.h>
#include <dekaf2/util/misc/kversion.h>
#include <dekaf2/net/tcp/ktcpstream.h>
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
};

// a raw HTTP/1.1 request, to control every header the guard looks at
Reply Request(uint16_t iPort, KStringView sRequestLine, std::initializer_list<KStringView> Headers)
{
	Reply R;

	KTCPStream Stream(KTCPEndPoint(kFormat("127.0.0.1:{}", iPort)),
	                  KStreamOptions(KStreamOptions::CancelOnTimeout, chrono::seconds(2)));

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

	Stream.WriteLine("Connection: close");
	Stream.WriteLine("");
	Stream.Flush();

	KString sLine;

	if (!Stream.ReadLine(sLine))
	{
		return R;
	}

	// HTTP/1.1 403 Forbidden
	R.iStatus = sLine.Mid(9, 3).UInt16();

	// header names and values lowercased, the server writes the names in lowercase anyway
	while (Stream.ReadLine(sLine) && !sLine.empty())
	{
		R.sHeaders += sLine.ToLowerASCII();
		R.sHeaders += '\n';
	}

	Stream.ReadRemaining(R.sBody);

	return R;
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

	auto sHost   = kFormat("Host: 127.0.0.1:{}", iPort);
	auto sOrigin = kFormat("Origin: http://127.0.0.1:{}", iPort);
	auto sCookie = kFormat("Cookie: kwa={}", sToken);

	SECTION("no token")
	{
		CHECK ( Request(iPort, "GET / HTTP/1.1", { sHost }).iStatus == 403 );
		// also for paths that no route serves - the guard runs before routing
		CHECK ( Request(iPort, "GET /favicon.ico HTTP/1.1", { sHost }).iStatus == 403 );
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
		CHECK ( R.sHeaders.contains("location: /data") );
		CHECK ( R.sHeaders.contains(kFormat("set-cookie: kwa={}", sToken)) );
		CHECK ( R.sHeaders.contains("httponly") );
		CHECK ( R.sHeaders.contains("samesite=strict") );

		// a wrong token gets no cookie
		R = Request(iPort, "GET /_kwa/enter?token=0123456789abcdef&next=/data HTTP/1.1", { sHost });
		CHECK ( R.iStatus == 403 );
		CHECK ( R.sHeaders.contains("set-cookie") == false );

		// next stays on our origin
		R = Request(iPort, kFormat("GET /_kwa/enter?token={}&next=//evil.example/ HTTP/1.1", sToken), { sHost });
		CHECK ( R.iStatus == 302 );
		CHECK ( R.sHeaders.contains("location: /\n") );

		R = Request(iPort, kFormat("GET /_kwa/enter?token={}&next=http://evil.example/ HTTP/1.1", sToken), { sHost });
		CHECK ( R.iStatus == 302 );
		CHECK ( R.sHeaders.contains("location: /\n") );
	}

	SECTION("enter URL")
	{
		auto sURL = App.GetEnterURL("/data");
		CHECK ( sURL.starts_with(kFormat("http://127.0.0.1:{}/_kwa/enter?token={}&next=", iPort, sToken)) );

		// and it works as the first request
		auto R = Request(iPort, kFormat("GET {} HTTP/1.1", KStringView(sURL).Mid(sURL.find('/', 7))), { sHost });
		CHECK ( R.iStatus == 302 );
		CHECK ( R.sHeaders.contains("location: /data") );
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

#endif // DEKAF2_HAS_WEBVIEW
