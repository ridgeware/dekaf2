#include "catch.hpp"

#include <dekaf2/krestclient.h>
#include <dekaf2/krest.h>

using namespace dekaf2;

void rest_login(KRESTServer& REST)
{
	if (REST.GetQueryParm("user") == "Jason"
		&& REST.GetQueryParm("pass") == "secret")
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

void rest_help(KRESTServer& REST)
{
}

void rest_patch(KRESTServer& REST)
{
}

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
			{ "GET",   false, "/login",              rest_login },
			{ "POST",  false, "/help",               rest_help  },
			{ "PATCH", false, "/user/:NAME/address", rest_patch },
		};

		KRESTRoutes Routes;
		Routes.AddFunctionTable(RTable);

		KREST Server;
		CHECK( Server.Execute(Options, Routes) );

		KJsonRestClient Host("http://localhost:6780/", false);
		KHTTPError ec;

		auto oResponse = Host.Get("login")
							 .SetError(ec)
		                     .AddQuery("user", "Tom")
		                     .AddQuery("pass", "Jerry")
		                     .Request();

		CHECK ( ec.value() == 401 );
		CHECK ( ec == true );
		CHECK ( true == ec );
		CHECK ( ec.message() == "GET login: HTTP-401 NOT AUTHORIZED" );
		CHECK ( Host.HttpSuccess() == false );
		CHECK ( Host.HttpFailure() == true  );
		CHECK ( oResponse.empty()  == false );
		CHECK ( kjson::GetStringRef(oResponse, "message") == "bad user or pass" );

		oResponse = Host.Get("login")
						 .SetError(ec)
						 .AddQuery("user", "Jason")
						 .AddQuery("pass", "secret")
						 .Request();

		CHECK ( ec.value() == 0 );
		CHECK ( ec == false );
		CHECK ( Host.HttpSuccess() == true  );
		CHECK ( Host.HttpFailure() == false );
		CHECK ( oResponse.empty()  == false );
		CHECK ( kjson::GetStringRef(oResponse, "token")   == "1234567890" );
		CHECK ( kjson::GetStringRef(oResponse, "expires") == "then"       );

	}
}

