#include "catch.hpp"

#include <dekaf2/krest.h>
#include <dekaf2/khttperror.h>

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

		Routes.AddRoute({ KHTTPMethod::GET, "/test", [&](KRESTServer& http)
		{
			bCalledTest = true;
			http.json.tx["response"] = "hello world";
		}});

		bool bCalledHelp { false };

		Routes.AddRoute({ KHTTPMethod::GET, "/help", [&](KRESTServer& http)
		{
			bCalledHelp = true;
		}, KRESTRoute::PLAIN });

		bool bCalledNoSlashPath { false };

		Routes.AddRoute({ KHTTPMethod::GET, "noslashpath", [&](KRESTServer& http)
		{
			bCalledNoSlashPath = true;
		}});

		KString sUID;

		Routes.AddRoute({ KHTTPMethod::GET, "/user/:UID", [&](KRESTServer& http)
		{
			sUID = http.Request.Resource.Query["UID"];
		}});

		KString sName;

		Routes.AddRoute({ KHTTPMethod::GET, "/user/:NAME/address", [&](KRESTServer& http)
		{
			sName = http.Request.Resource.Query["NAME"];
		}});

		Routes.AddRoute({ KHTTPMethod::GET, "/throw", [&](KRESTServer& http)
		{
			throw KHTTPError{ KHTTPError::H4xx_BADREQUEST, "missing parameters" };
		}});

		bool bMatchedWildcardAtEnd { false };

		Routes.AddRoute({ KHTTPMethod::GET, "/wildcard/at/end/*", [&](KRESTServer& http)
		{
			bMatchedWildcardAtEnd = true;
		}});

		bool bMatchedWildcardFragment { false };

		Routes.AddRoute({ KHTTPMethod::GET, "/wildcard/*/middle", [&](KRESTServer& http)
		{
			bMatchedWildcardFragment = true;
		}});

		bool bMatchedMultiWildcards { false };

		Routes.AddRoute({ KHTTPMethod::GET, "/wildcard/*/middle/and/*", [&](KRESTServer& http)
		{
			bMatchedMultiWildcards = true;
		}});

		KString sOut;
		KOutStringStream oss(sOut);

		KREST::Options Options;
		Options.Type = KREST::SIMULATE_HTTP;

		KREST REST;

		CHECK ( bCalledTest == false );
		CHECK ( bCalledHelp == false );
		CHECK ( bCalledNoSlashPath == false );

		sOut.clear();
#ifdef NDEBUG
		KString sCompare = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: 27\r\nConnection: close\r\n\r\n{\"response\":\"hello world\"}\n";
#else
		KString sCompare = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: 31\r\nConnection: close\r\n\r\n{\n\t\"response\": \"hello world\"\n}\n";
#endif
		CHECK ( REST.Simulate(Options, Routes, "/test", oss) == true );
		CHECK ( sOut == sCompare );
		CHECK ( bCalledTest == true  );
		CHECK ( bCalledHelp == false );
		CHECK ( bCalledNoSlashPath == false );

		sOut.clear();
		CHECK ( REST.Simulate(Options, Routes, "/help", oss) == true );
		CHECK ( bCalledTest == true  );
		CHECK ( bCalledHelp == true  );
		CHECK ( bCalledNoSlashPath == false );

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
#ifdef NDEBUG
		sCompare = "HTTP/1.1 400 BAD REQUEST\r\nContent-Type: application/json\r\nContent-Length: 33\r\nConnection: close\r\n\r\n{\"message\":\"missing parameters\"}\n";
#else
		sCompare = "HTTP/1.1 400 BAD REQUEST\r\nContent-Type: application/json\r\nContent-Length: 37\r\nConnection: close\r\n\r\n{\n\t\"message\": \"missing parameters\"\n}\n";
#endif
		CHECK ( REST.Simulate(Options, Routes, "/throw", oss) == false );
		CHECK ( sOut == sCompare );
		CHECK ( bCalledTest == true  );
		CHECK ( bCalledHelp == true  );
		CHECK ( bCalledNoSlashPath == false );
		CHECK ( sUID == "7654" );
		CHECK ( sName == "Peter" );

		sOut.clear();
#ifdef NDEBUG
		sCompare = "HTTP/1.1 404 NOT FOUND\r\nContent-Type: application/json\r\nContent-Length: 41\r\nConnection: close\r\n\r\n{\"message\":\"invalid path: GET /unknown\"}\n";
#else
		sCompare = "HTTP/1.1 404 NOT FOUND\r\nContent-Type: application/json\r\nContent-Length: 45\r\nConnection: close\r\n\r\n{\n\t\"message\": \"invalid path: GET /unknown\"\n}\n";
#endif
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

	}

	SECTION("HTTP SIM STATIC TABLE")
	{
		constexpr KRESTRoutes::FunctionTable RTable[]
		{
			{ "GET", "/test",               rest_test },
			{ "GET", "/help",               rest_test, KRESTRoute::PLAIN },
			{ "GET", "noslashpath",         rest_test },
			{ "GET", "/user/:NAME/address", rest_test },
			{ "GET", "/user/:UID",          rest_test },
		};

		class RClass
		{
		public:
			void rest_test2(KRESTServer& http) { }
		};

		RClass RR;

		constexpr KRESTRoutes::MemberFunctionTable<RClass> MTable[]
		{
			{ "GET", "/rr/test",               &RClass::rest_test2 },
			{ "GET", "/rr/help",               &RClass::rest_test2, KRESTRoute::PLAIN },
			{ "GET", "rr/noslashpath",         &RClass::rest_test2, KRESTRoute::JSON  },
			{ "GET", "/rr/user/:NAME/address", &RClass::rest_test2 },
			{ "GET", "/rr/user/:UID",          &RClass::rest_test2 },
		};

		KRESTRoutes Routes;

		Routes.AddFunctionTable(RTable);
		Routes.AddMemberFunctionTable(RR, MTable);

		KString sOut;
		KOutStringStream oss(sOut);

		KREST::Options Options;
		Options.Type = KREST::SIMULATE_HTTP;

		KREST REST;

		sOut.clear();
		CHECK ( REST.Simulate(Options, Routes, "/test", oss) == true );

		sOut.clear();
		CHECK ( REST.Simulate(Options, Routes, "/help", oss) == true );

		sOut.clear();
		CHECK ( REST.Simulate(Options, Routes, "/noslashpath", oss) == false );

		sOut.clear();
		CHECK ( REST.Simulate(Options, Routes, "/user/7654", oss) == true );

		sOut.clear();
		CHECK ( REST.Simulate(Options, Routes, "/user/Peter/address", oss) == true );

		sOut.clear();
#ifdef NDEBUG
		KString sCompare = "HTTP/1.1 404 NOT FOUND\r\nContent-Type: application/json\r\nContent-Length: 41\r\nConnection: close\r\n\r\n{\"message\":\"invalid path: GET /unknown\"}\n";
#else
		KString sCompare = "HTTP/1.1 404 NOT FOUND\r\nContent-Type: application/json\r\nContent-Length: 45\r\nConnection: close\r\n\r\n{\n\t\"message\": \"invalid path: GET /unknown\"\n}\n";
#endif
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

	}

}
