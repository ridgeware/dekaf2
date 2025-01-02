#include "catch.hpp"

#include <dekaf2/krest.h>
#include <dekaf2/khttperror.h>
#include <dekaf2/krestclient.h>

using namespace dekaf2;

void rest_test(KRESTServer& REST)
{
}

TEST_CASE("KREST")
{
	SECTION("HTTP SIM")
	{
		KRESTRoutes Routes;

		bool bCalledTest { false };

		Routes.AddRoute({ KHTTPMethod::GET, false, "/test", [&](KRESTServer& http)
		{
			bCalledTest = true;
			http.json.tx["response"] = "hello world";
		}});

		bool bCalledHelp { false };

		Routes.AddRoute({ KHTTPMethod::GET, false, "/help", [&](KRESTServer& http)
		{
			bCalledHelp = true;
		}, KRESTRoute::PLAIN });

		bool bCalledNoSlashPath { false };

		Routes.AddRoute({ KHTTPMethod::GET, false, "noslashpath", [&](KRESTServer& http)
		{
			bCalledNoSlashPath = true;
		}});

		KString sUID;

		Routes.AddRoute({ KHTTPMethod::GET, false, "/user/:UID", [&](KRESTServer& http)
		{
			sUID = http.Request.Resource.Query[":UID"];
		}});

		KString sAuth;

		Routes.AddRoute({ KHTTPMethod::GET, { KRESTRoute::Options::SSO_AUTH, KRESTRoute::Options::GENERIC_AUTH }, "/auth/:AUTH", [&](KRESTServer& http)
		{
			sAuth = http.Request.Resource.Query[":AUTH"];
		}});

		KString sName;

		Routes.AddRoute({ KHTTPMethod::GET, false, "/user/=NAME/address", [&](KRESTServer& http)
		{
			sName = http.GetQueryParmSafe("NAME");
		}});

		Routes.AddRoute({ KHTTPMethod::GET, false, "/throw", [&](KRESTServer& http)
		{
			throw KHTTPError{ KHTTPError::H4xx_BADREQUEST, "missing parameters" };
		}});

		bool bMatchedWildcardAtEnd { false };

		Routes.AddRoute({ KHTTPMethod::GET, false, "/wildcard/at/end/*", [&](KRESTServer& http)
		{
			bMatchedWildcardAtEnd = true;
		}});

		bool bMatchedWildcardFragment { false };

		Routes.AddRoute({ KHTTPMethod::GET, false, "/wildcard/*/middle", [&](KRESTServer& http)
		{
			bMatchedWildcardFragment = true;
		}});

		bool bMatchedMultiWildcards { false };

		Routes.AddRoute({ KHTTPMethod::GET, false, "/wildcard/*/middle/and/*", [&](KRESTServer& http)
		{
			bMatchedMultiWildcards = true;
		}});

		constexpr KStringView sWebContent = "<html><body>hello world</body></html>";
		KTempDir WebRoot;
		{
			KOutFile OutFile(kFormat("{}/index.html", WebRoot.Name()));
			CHECK ( OutFile.is_open() );
			OutFile.Write(sWebContent);
		}
		{
			KOutFile OutFile(kFormat("{}/test (spaced name).html", WebRoot.Name()));
			CHECK ( OutFile.is_open() );
			OutFile.Write(sWebContent);
		}

		Routes.AddRoute({ KHTTPMethod::GET, false, "/web/*", WebRoot.Name(), Routes, &KRESTRoutes::WebServer });

		KString sOut;
		KOutStringStream oss(sOut);

		KREST::Options Options;
		Options.bPrettyPrint = true;
		Options.Type = KREST::SIMULATE_HTTP;
		Options.AuthLevel = KRESTServer::Options::ALLOW_ALL_WITH_AUTH_HEADER;
		Options.AuthCallback = [&](KRESTServer& HTTP) -> KString
		{
			return "somebody";
		};

		KREST REST;

		CHECK ( bCalledTest == false );
		CHECK ( bCalledHelp == false );
		CHECK ( bCalledNoSlashPath == false );

		sOut.clear();
		KString sCompare = "HTTP/1.1 200 OK\r\ncontent-type: application/json\r\ncontent-length: 31\r\nconnection: close\r\n\r\n{\n\t\"response\": \"hello world\"\n}\n";
		CHECK ( REST.Simulate(Options, Routes, "/test", oss) == true );
		CHECK ( sOut == sCompare );
		CHECK ( bCalledTest == true  );
		CHECK ( bCalledHelp == false );
		CHECK ( bCalledNoSlashPath == false );

		sOut.clear();
		Options.sBaseRoute = "/base/route";
		CHECK ( REST.Simulate(Options, Routes, "/help", oss) == true );
		CHECK ( bCalledTest == true  );
		CHECK ( bCalledHelp == true  );
		CHECK ( bCalledNoSlashPath == false );
		CHECK ( REST.Simulate(Options, Routes, "/base/route/help", oss) == true );
		CHECK ( REST.Simulate(Options, Routes, "/help/", oss) == true );
		CHECK ( REST.Simulate(Options, Routes, "/base/route/help/", oss) == true );

		sOut.clear();
		bCalledHelp = false;
		Options.sBaseRoute = "/this/is/my/base/route";
		CHECK ( REST.Simulate(Options, Routes, "/this/is/my/base/route/help", oss) == true );
		Options.sBaseRoute.clear();
		CHECK ( bCalledTest == true  );
		CHECK ( bCalledHelp == true  );
		CHECK ( bCalledNoSlashPath == false );

		sOut.clear();
		Options.sBaseRoute = "/this/is/my/base";
		CHECK ( REST.Simulate(Options, Routes, "/this/is/my/base/route/help", oss) == false );
		Options.sBaseRoute.clear();
		CHECK ( bCalledTest == true  );
		CHECK ( bCalledHelp == true  );
		CHECK ( bCalledNoSlashPath == false );

		sOut.clear();
		CHECK ( REST.Simulate(Options, Routes, "/noslashpath", oss) == false );
		CHECK ( bCalledTest == true  );
		CHECK ( bCalledHelp == true  );
		CHECK ( bCalledNoSlashPath == false );
		CHECK ( sUID == "" );
		CHECK ( sName == "" );

		sOut.clear();
		CHECK ( REST.Simulate(Options, Routes, "/user", oss) == true );
		CHECK ( bCalledTest == true  );
		CHECK ( bCalledHelp == true  );
		CHECK ( bCalledNoSlashPath == false );
		CHECK ( sUID == "" );
		CHECK ( sName == "" );

		sOut.clear();
		CHECK ( REST.Simulate(Options, Routes, "/user/7654", oss) == true );
		CHECK ( bCalledTest == true  );
		CHECK ( bCalledHelp == true  );
		CHECK ( bCalledNoSlashPath == false );
		CHECK ( sUID == "7654" );
		CHECK ( sName == "" );

		sOut.clear();
		CHECK ( REST.Simulate(Options, Routes, "/user/Peter/address", oss) == true );
		CHECK ( bCalledTest == true  );
		CHECK ( bCalledHelp == true  );
		CHECK ( bCalledNoSlashPath == false );
		CHECK ( sUID == "7654" );
		CHECK ( sName == "Peter" );

		sOut.clear();
		CHECK ( REST.Simulate(Options, Routes, "/auth/authenticated", oss) == true );
		CHECK ( sAuth == "authenticated" );

		sOut.clear();
		CHECK ( REST.Simulate(Options, Routes, "/user/\"; DROP DATABASE CLIENTS/address", oss) == false );

		sOut.clear();
		sCompare = "HTTP/1.1 400 BAD REQUEST\r\ncontent-type: application/json\r\ncontent-length: 37\r\nconnection: close\r\n\r\n{\n\t\"message\": \"missing parameters\"\n}\n";
		CHECK ( REST.Simulate(Options, Routes, "/throw", oss) == false );
		CHECK ( sOut == sCompare );
		CHECK ( bCalledTest == true  );
		CHECK ( bCalledHelp == true  );
		CHECK ( bCalledNoSlashPath == false );
		CHECK ( sUID == "7654" );
		CHECK ( sName == "Peter" );

		sOut.clear();
		sCompare = "HTTP/1.1 404 NOT FOUND\r\ncontent-type: application/json\r\ncontent-length: 45\r\nconnection: close\r\n\r\n{\n\t\"message\": \"invalid path: GET /unknown\"\n}\n";
		CHECK ( REST.Simulate(Options, Routes, "/unknown", oss) == false );
		CHECK ( sOut == sCompare );
		CHECK ( bCalledTest == true  );
		CHECK ( bCalledHelp == true  );
		CHECK ( bCalledNoSlashPath == false );
		CHECK ( sUID == "7654" );
		CHECK ( sName == "Peter" );

		sOut.clear();
		bMatchedWildcardAtEnd = false;
		CHECK ( REST.Simulate(Options, Routes, "/wildcard/at/end/and/more", oss) == true );
		CHECK ( bMatchedWildcardAtEnd == true  );

		sOut.clear();
		bMatchedWildcardAtEnd = false;
		CHECK ( REST.Simulate(Options, Routes, "/wildcard/at/end", oss) == true );
		CHECK ( bMatchedWildcardAtEnd == true  );

		sOut.clear();
		bMatchedWildcardAtEnd = false;
		CHECK ( REST.Simulate(Options, Routes, "/wildcard/at/end/", oss) == true );
		CHECK ( bMatchedWildcardAtEnd == true  );

		sOut.clear();
		bMatchedWildcardAtEnd = false;
		CHECK ( REST.Simulate(Options, Routes, "/wildcard/at/ending", oss) == false );
		CHECK ( bMatchedWildcardAtEnd == false  );

		sOut.clear();
		bMatchedWildcardFragment = false;
		CHECK ( REST.Simulate(Options, Routes, "/wildcard/in/middle", oss) == true );
		CHECK ( bMatchedWildcardFragment == true  );

		sOut.clear();
		bMatchedWildcardFragment = false;
		CHECK ( REST.Simulate(Options, Routes, "/wildcard/in/the/middle", oss) == false );
		CHECK ( bMatchedWildcardFragment == false  );

		sOut.clear();
		bMatchedMultiWildcards = false;
		CHECK ( REST.Simulate(Options, Routes, "/wildcard/in/middle/and/end", oss) == true );
		CHECK ( bMatchedMultiWildcards == true  );

		sOut.clear();
		CHECK ( REST.Simulate(Options, Routes, "/web/index.html", oss) == true );
		auto iPos = sOut.find("\r\n\r\n");
		CHECK ( iPos != npos );
		if (iPos != npos)
		{
			sOut.erase(0, iPos+4);
		}
		CHECK ( sOut == sWebContent );

		sOut.clear();
		CHECK ( REST.Simulate(Options, Routes, "/web", oss) == true );
		CHECK ( sOut.starts_with("HTTP/1.1 301 ") );
		CHECK ( sOut.contains("\r\nlocation: /web/\r\n") );

		sOut.clear();
		CHECK ( REST.Simulate(Options, Routes, "/web/", oss) == true );
		iPos = sOut.find("\r\n\r\n");
		CHECK ( iPos != npos );
		if (iPos != npos)
		{
			sOut.erase(0, iPos+4);
		}
		CHECK ( sOut == sWebContent );

		sOut.clear();
		CHECK ( REST.Simulate(Options, Routes, "/web/test (spaced name).html", oss) == true );
		iPos = sOut.find("\r\n\r\n");
		CHECK ( iPos != npos );
		if (iPos != npos)
		{
			sOut.erase(0, iPos+4);
		}
		CHECK ( sOut == sWebContent );
	}

	SECTION("HTTP SIM STATIC TABLE")
	{
		constexpr KStringView sWebContent = "<html><body>hello world</body></html>";
		KTempDir WebRoot;
		{
			KOutFile OutFile(kFormat("{}/index.htm", WebRoot.Name()));
			CHECK ( OutFile.is_open() );
			OutFile.Write(sWebContent);
		}
		{
			KOutFile OutFile(kFormat("{}/test.html", WebRoot.Name()));
			CHECK ( OutFile.is_open() );
			OutFile.Write(sWebContent);
		}

		constexpr KRESTRoutes::FunctionTable RTable[]
		{
			{ "GET", false, "/test",               rest_test },
			{ "GET", false, "/help",               rest_test, KRESTRoute::PLAIN },
			{ "GET", false, "noslashpath",         rest_test },
			{ "GET", false, "/user/:NAME/address", rest_test },
			{ "GET", false, "/user/:UID",          rest_test },
		};

		class RClass
		{
		public:
			void rest_test2(KRESTServer& http) { }
		};

		RClass RR;

		KString sTestRoot = WebRoot.Name();
		sTestRoot += "/test.html";

		KRESTRoutes::MemberFunctionTable<RClass> MTable[]
		{
			{ "REWRITE",  false, "^/www/" , "/web/"  },
			{ "REDIRECT", false, "^/www2/", "/web/"  },

			{ "GET", false, "/rr/test",               &RClass::rest_test2 },
			{ "GET", false, "/rr/help",               &RClass::rest_test2, KRESTRoute::PLAIN },
			{ "GET", false, "rr/noslashpath",         &RClass::rest_test2, KRESTRoute::JSON  },
			{ "GET", false, "/rr/user/:NAME/address", &RClass::rest_test2 },
			{ "GET", false, "/rr/user/:UID",          &RClass::rest_test2 },
			{ "GET", false, "/test.html",             sTestRoot           },
			{ "GET", false, "/web/*",                 WebRoot.Name()      }
		};

		KRESTRoutes Routes;

		Routes.AddFunctionTable(RTable);
		Routes.AddMemberFunctionTable(RR, MTable);

		KString sOut;
		KOutStringStream oss(sOut);

		KREST::Options Options;
		Options.bPrettyPrint = true;
		Options.Type = KREST::SIMULATE_HTTP;

		KREST REST;

		sOut.clear();
		CHECK ( REST.Simulate(Options, Routes, "/test", oss) == true );

		sOut.clear();
		CHECK ( REST.Simulate(Options, Routes, "/test/", oss) == true );

		sOut.clear();
		CHECK ( REST.Simulate(Options, Routes, "/help", oss) == true );

		sOut.clear();
		CHECK ( REST.Simulate(Options, Routes, "/noslashpath", oss) == false );

		sOut.clear();
		CHECK ( REST.Simulate(Options, Routes, "/user/7654", oss) == true );

		sOut.clear();
		CHECK ( REST.Simulate(Options, Routes, "/user/Peter/address", oss) == true );

		sOut.clear();
		KString sCompare = "HTTP/1.1 404 NOT FOUND\r\ncontent-type: application/json\r\ncontent-length: 45\r\nconnection: close\r\n\r\n{\n\t\"message\": \"invalid path: GET /unknown\"\n}\n";

		CHECK ( REST.Simulate(Options, Routes, "/unknown", oss) == false );
		CHECK ( sOut == sCompare );

		sOut.clear();
		CHECK ( REST.Simulate(Options, Routes, "/rr/test", oss) == true );

		sOut.clear();
		CHECK ( REST.Simulate(Options, Routes, "/rr/help", oss) == true );

		sOut.clear();
		CHECK ( REST.Simulate(Options, Routes, "/rr/noslashpath", oss) == false );

		sOut.clear();
		CHECK ( REST.Simulate(Options, Routes, "/rr/user/7654", oss) == true );

		sOut.clear();
		CHECK ( REST.Simulate(Options, Routes, "/rr/user/Peter/address", oss) == true );

		sOut.clear();
		CHECK ( REST.Simulate(Options, Routes, "/web/", oss) == true );
		auto iPos = sOut.find("\r\n\r\n");
		CHECK ( iPos != npos );
		if (iPos != npos)
		{
			sOut.erase(0, iPos+4);
		}
		CHECK ( sOut == sWebContent );

		sOut.clear();
		CHECK ( REST.Simulate(Options, Routes, "/www/", oss) == true );
		auto iPos3 = sOut.find("\r\n\r\n");
		CHECK ( iPos3 != npos );
		if (iPos3 != npos)
		{
			sOut.erase(0, iPos3+4);
		}
		CHECK ( sOut == sWebContent );

		sOut.clear();
		CHECK ( REST.Simulate(Options, Routes, "/www2/path/to/index.html", oss) == false );
		CHECK ( sOut.contains("HTTP/1.1 301 MOVED PERMANENTLY") );
		CHECK ( sOut.contains("location: /web/path/to/index.html") );
		CHECK ( sOut.contains("content-length: 0") );

		sOut.clear();
		CHECK ( REST.Simulate(Options, Routes, "/test.html", oss) == true );
		auto iPos1 = sOut.find("\r\n\r\n");
		CHECK ( iPos1 != npos );
		if (iPos1 != npos)
		{
			sOut.erase(0, iPos1+4);
		}
		CHECK ( sOut == sWebContent );

		sOut.clear();
		CHECK ( REST.Simulate(Options, Routes, "/web/unknown.html", oss) == false );
		auto iPos2 = sOut.find("\r\n\r\n");
		CHECK ( iPos2 != npos );
		if (iPos2 != npos)
		{
			sOut.erase(0, iPos2+4);
		}
		CHECK ( sOut == "<html><head>HTTP Error 404</head><body><h2>404 file not found: /web/unknown.html </h2></body></html>\n" );
	}

	SECTION("HTTP keepalive")
	{
		KRESTRoutes Routes;

		uint16_t iCalledTest { 0 };

		Routes.AddRoute({ KHTTPMethod::GET, false, "/test", [&](KRESTServer& http)
		{
			++iCalledTest;
			http.json.tx["response"] = "hello world";
		}});

		KREST::Options Options;
		Options.bPrettyPrint = true;
		Options.Type      = KREST::HTTP;
		Options.iPort     = 30303;
		Options.bBlocking = false;
//		Options.iTimeout  = 300;

		KREST REST;

		if (!REST.Execute(Options, Routes))
		{
			CHECK ( REST.Error() == "" );
		}
		else
		{
			{
				iCalledTest = 0;
				KHTTPError ec;
				KJsonRestClient Client("http://localhost:30303");
				Client.RequestCompression(false);
//				Client.SetTimeout(300);
				Client.AllowConnectionRetry(false);

				auto jResult = Client.Get("test").SetError(ec).Request();

				CHECK (ec.value()   == 0  );
				CHECK (ec.message() == "" );
				CHECK (iCalledTest  == 1  );

				jResult = Client.Get("test").SetError(ec).Request();

				CHECK (ec.value()   == 0  );
				CHECK (ec.message() == "" );
				CHECK (iCalledTest  == 2  );
			}
			{
				iCalledTest = 0;
				KHTTPError ec;
				KJsonRestClient Client("http://localhost:30303");
				Client.RequestCompression(true);
//				Client.SetTimeout(300);
				Client.AllowConnectionRetry(false);

				auto jResult = Client.Get("test").SetError(ec).Request();

				CHECK (ec.value()   == 0  );
				CHECK (ec.message() == "" );
				CHECK (iCalledTest  == 1  );

				jResult = Client.Get("test").SetError(ec).Request();

				CHECK (ec.value()   == 0  );
				CHECK (ec.message() == "" );
				CHECK (iCalledTest  == 2  );
			}
		}
	}

}
