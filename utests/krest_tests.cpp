#include "catch.hpp"

#include <dekaf2/krest.h>
#include <dekaf2/khttperror.h>

using namespace dekaf2;

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
		}});

		bool bCalledNoSlashPath { false };

		Routes.AddRoute({ KHTTPMethod::GET, "noslashpath", [&](KRESTServer& http)
		{
			bCalledNoSlashPath = true;
		}});

		KString sName;

		Routes.AddRoute({ KHTTPMethod::GET, "/user/:NAME/address", [&](KRESTServer& http)
		{
			sName = http.Request.Resource.Query["NAME"];
		}});

		KString sUID;

		Routes.AddRoute({ KHTTPMethod::GET, "/user/:UID", [&](KRESTServer& http)
		{
			sUID = http.Request.Resource.Query["UID"];
		}});

		Routes.AddRoute({ KHTTPMethod::GET, "/throw", [&](KRESTServer& http)
		{
			throw KHTTPError{ KHTTPError::H4xx_BADREQUEST, "missing parameters" };
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
		KString sCompare = "HTTP/1.0 200 OK\r\nContent-Type: application/json\r\nContent-Length: 27\r\n\r\n{\"response\":\"hello world\"}\n";
#else
		KString sCompare = "HTTP/1.0 200 OK\r\nContent-Type: application/json\r\nContent-Length: 31\r\n\r\n{\n\t\"response\": \"hello world\"\n}\n";
#endif
		CHECK ( REST.ExecuteFromFile(Options, Routes, "/test", oss) == true );
		CHECK ( sOut == sCompare );
		CHECK ( bCalledTest == true  );
		CHECK ( bCalledHelp == false );
		CHECK ( bCalledNoSlashPath == false );

		sOut.clear();
		CHECK ( REST.ExecuteFromFile(Options, Routes, "/help", oss) == true );
		CHECK ( bCalledTest == true  );
		CHECK ( bCalledHelp == true  );
		CHECK ( bCalledNoSlashPath == false );

		sOut.clear();
		CHECK ( REST.ExecuteFromFile(Options, Routes, "/noslashpath", oss) == true );
		CHECK ( bCalledTest == true  );
		CHECK ( bCalledHelp == true  );
		CHECK ( bCalledNoSlashPath == true );
		CHECK ( sUID == "" );
		CHECK ( sName == "" );

		sOut.clear();
		CHECK ( REST.ExecuteFromFile(Options, Routes, "/user/7654", oss) == true );
		CHECK ( bCalledTest == true  );
		CHECK ( bCalledHelp == true  );
		CHECK ( bCalledNoSlashPath == true );
		CHECK ( sUID == "7654" );
		CHECK ( sName == "" );

		sOut.clear();
		CHECK ( REST.ExecuteFromFile(Options, Routes, "/user/Peter/address", oss) == true );
		CHECK ( bCalledTest == true  );
		CHECK ( bCalledHelp == true  );
		CHECK ( bCalledNoSlashPath == true );
		CHECK ( sUID == "7654" );
		CHECK ( sName == "Peter" );

		sOut.clear();
#ifdef NDEBUG
		sCompare = "HTTP/1.0 400 BAD REQUEST\r\nContent-Length: 33\r\nConnection: close\r\n\r\n{\"message\":\"missing parameters\"}\n";
#else
		sCompare = "HTTP/1.0 400 BAD REQUEST\r\nContent-Length: 37\r\nConnection: close\r\n\r\n{\n\t\"message\": \"missing parameters\"\n}\n";
#endif
		CHECK ( REST.ExecuteFromFile(Options, Routes, "/throw", oss) == false );
		CHECK ( sOut == sCompare );
		CHECK ( bCalledTest == true  );
		CHECK ( bCalledHelp == true  );
		CHECK ( bCalledNoSlashPath == true );
		CHECK ( sUID == "7654" );
		CHECK ( sName == "Peter" );

		sOut.clear();
#ifdef NDEBUG
		sCompare = "HTTP/1.0 404 NOT FOUND\r\nContent-Length: 44\r\nConnection: close\r\n\r\n{\"message\":\"unknown address: GET /unknown\"}\n";
#else
		sCompare = "HTTP/1.0 404 NOT FOUND\r\nContent-Length: 48\r\nConnection: close\r\n\r\n{\n\t\"message\": \"unknown address: GET /unknown\"\n}\n";
#endif
		CHECK ( REST.ExecuteFromFile(Options, Routes, "/unknown", oss) == false );
		CHECK ( sOut == sCompare );
		CHECK ( bCalledTest == true  );
		CHECK ( bCalledHelp == true  );
		CHECK ( bCalledNoSlashPath == true );
		CHECK ( sUID == "7654" );
		CHECK ( sName == "Peter" );

	}

}
