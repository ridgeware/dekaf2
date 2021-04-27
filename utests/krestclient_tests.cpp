#include "catch.hpp"

#include <dekaf2/krestclient.h>
#include <dekaf2/krest.h>
#include <dekaf2/kmessagedigest.h>

#ifndef DEKAF2_IS_WINDOWS

using namespace dekaf2;

namespace {

void rest_login(KRESTServer& REST)
{
	if (REST.GetQueryParm("user") == "Jason"
		&& REST.GetQueryParm("pass") == KSHA256("secret").Digest())
	{
		REST.json.tx =
		{
			{ "token",   "1234567890" },
			{ "expires", "then"       }
		};
	}
	else
	{
		throw KHTTPError { KHTTPError::H4xx_NOTAUTH, "bad user or pass" };
	}
}

void rest_settings(KRESTServer& REST)
{
	if (REST.Request.Headers.Get(KHTTPHeader::AUTHORIZATION) != "1234567890")
	{
		throw KHTTPError { KHTTPError::H4xx_NOTAUTH, "not authenticated" };
	}

	if (REST.GetQueryParm(":NAME") == "Jason")
	{
		REST.json.tx = REST.json.rx;
		REST.SetStatus(KHTTPError::H2xx_CREATED);
	}
	else
	{
		throw KHTTPError { KHTTPError::H4xx_NOTFOUND, "unknown user" };
	}
}

void rest_patch(KRESTServer& REST)
{
	if (REST.Request.Headers.Get(KHTTPHeader::AUTHORIZATION) != "1234567890")
	{
		throw KHTTPError { KHTTPError::H4xx_NOTAUTH, "not authenticated" };
	}

	if (REST.GetQueryParm(":NAME") == "Jason")
	{
		REST.json.tx = REST.json.rx;
		REST.SetStatus(KHTTPError::H2xx_UPDATED);
	}
	else
	{
		throw KHTTPError { KHTTPError::H4xx_NOTFOUND, "unknown user" };
	}
}

} // end of anonymous namespace

TEST_CASE("KRESTCLIENT")
{
	SECTION("JSON")
	{
		KREST::Options Options;
		Options.Type = KREST::HTTP;
		Options.iPort = 6780;
		Options.bBlocking = false;

		KRESTRoutes::FunctionTable RTable[]
		{
			{ "GET",   false, "/login",              rest_login    },
			{ "POST",  false, "/user/:NAME",         rest_settings },
			{ "PATCH", false, "/user/:NAME/address", rest_patch    },
		};

		KRESTRoutes Routes;
		Routes.AddFunctionTable(RTable);

		KREST Server;
		CHECK( Server.Execute(Options, Routes) );

		KJsonRestClient Host("http://localhost:6780/");

		Host.SetErrorCallback([](const KJSON& json) -> KString
		{
			return kjson::GetStringRef(json, "message");
		});

		KHTTPError ec;

		auto oResponse = Host.Get("login")
							 .SetError(ec)
		                     .AddQuery("user", "Tom")
		                     .AddQuery("pass", KSHA256("Jerry").Digest())
		                     .Request();

		CHECK ( ec.value() == 401 );
		CHECK ( ec == true );
		CHECK ( true == ec );
		CHECK ( ec.message() == "GET login: HTTP-401 NOT AUTHORIZED, bad user or pass from http://localhost:6780/" );
		CHECK ( Host.HttpSuccess() == false );
		CHECK ( Host.HttpFailure() == true  );
		CHECK ( oResponse.empty()  == false );
		CHECK ( kjson::GetStringRef(oResponse, "message") == "bad user or pass" );

		oResponse = Host.Post("login")
						 .SetError(ec)
						 .AddQuery("user", "Tom")
						 .AddQuery("pass", KSHA256("Jerry").Digest())
						 .Request();

		CHECK ( ec.value() == 405 );
		CHECK ( ec == true );
		CHECK ( true == ec );
		CHECK ( ec.message() == "POST login: HTTP-405 METHOD NOT ALLOWED, request method POST not supported for path: /login from http://localhost:6780/" );
		CHECK ( Host.HttpSuccess() == false );
		CHECK ( Host.HttpFailure() == true  );
		CHECK ( oResponse.empty()  == false );
		CHECK ( kjson::GetStringRef(oResponse, "message") == "request method POST not supported for path: /login" );

		oResponse = Host.Get("login")
						 .SetError(ec)
						 .AddQuery("user", "Jason")
						 .AddQuery("pass", KSHA256("secret").Digest())
						 .Request();

		CHECK ( ec.value() == 0 );
		CHECK ( ec == false );
		CHECK ( Host.GetStatusCode() == 200  );
		CHECK ( Host.HttpSuccess()  == true  );
		CHECK ( Host.HttpFailure()  == false );
		CHECK ( oResponse.empty()   == false );
		CHECK ( kjson::GetStringRef(oResponse, "token")   == "1234567890" );
		CHECK ( kjson::GetStringRef(oResponse, "expires") == "then"       );

		KString sToken = kjson::GetStringRef(oResponse, "token");

		oResponse = Host.Post("user/Tom")
		                .AddHeader(KHTTPHeader::AUTHORIZATION, "12345")
		                .SetError(ec)
		                .Request(
		{
			{ "city"     , "Berlin"     },
			{ "street"   , "Tauentzien" },
			{ "number"   , "73a"        },
			{ "zip"      , "10241"      }
		});

		CHECK ( ec.value() == 401 );
		CHECK ( ec == true );
		CHECK ( ec.message() == "POST user/Tom: HTTP-401 NOT AUTHORIZED, not authenticated from http://localhost:6780/" );
		CHECK ( Host.HttpSuccess() == false );
		CHECK ( Host.HttpFailure() == true  );
		CHECK ( oResponse.empty()  == false );
		CHECK ( kjson::GetStringRef(oResponse, "message") == "not authenticated" );

		oResponse = Host.Post("user/Tom")
		                .AddHeader(KHTTPHeader::AUTHORIZATION, "1234567890")
		                .SetError(ec)
		                .Request(
		{
			{ "city"     , "Berlin"     },
			{ "street"   , "Tauentzien" },
			{ "number"   , "73a"        },
			{ "zip"      , "10241"      }
		});

		CHECK ( ec.value() == 404 );
		CHECK ( ec == true );
		CHECK ( ec.message() == "POST user/Tom: HTTP-404 NOT FOUND, unknown user from http://localhost:6780/" );
		CHECK ( Host.HttpSuccess() == false );
		CHECK ( Host.HttpFailure() == true  );
		CHECK ( oResponse.empty()  == false );
		CHECK ( kjson::GetStringRef(oResponse, "message") == "unknown user" );

		oResponse = Host.Post("user/Jason")
		                .AddHeader(KHTTPHeader::AUTHORIZATION, sToken)
		                .SetError(ec)
		                .Request(
		{
			{ "city"     , "Berlin"     },
			{ "street"   , "Tauentzien" },
			{ "number"   , "73a"        },
			{ "zip"      , "10241"      }
		});

		CHECK ( ec.value() == 0 );
		CHECK ( ec == false );
		CHECK ( Host.GetStatusCode() == 201  );
		CHECK ( Host.HttpSuccess()  == true  );
		CHECK ( Host.HttpFailure()  == false );
		CHECK ( oResponse.empty()   == false );
		CHECK ( kjson::GetStringRef(oResponse, "city") == "Berlin" );
		CHECK ( kjson::GetStringRef(oResponse, "zip" ) == "10241"  );

		oResponse = Host.Patch("user/Jason/address")
		                .AddHeader(KHTTPHeader::AUTHORIZATION, sToken)
		                .SetError(ec)
		                .Request(
		{
			{ "city"     , "Berlin"         },
			{ "street"   , "Bergmannstraße" },
			{ "number"   , "67"             },
			{ "zip"      , "10641"          }
		});

		CHECK ( ec.value() == 0 );
		CHECK ( ec == false );
		CHECK ( Host.GetStatusCode() == 201  );
		CHECK ( Host.HttpSuccess()  == true  );
		CHECK ( Host.HttpFailure()  == false );
		CHECK ( oResponse.empty()   == false );
		CHECK ( kjson::GetStringRef(oResponse, "city") == "Berlin" );
		CHECK ( kjson::GetStringRef(oResponse, "zip" ) == "10641"  );

	}
}

#endif // of DEKAF2_IS_WINDOWS
