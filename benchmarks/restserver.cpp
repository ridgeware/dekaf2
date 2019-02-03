#include <dekaf2/dekaf2.h>
#include <dekaf2/krest.h>

using namespace dekaf2;

// a small but complete "Hello World" REST echo responder

int main(int argc, char** argv)
{
	// create a REST router
	KRESTRoutes Routes;

	// add our rest route to it: we listen for all requests
	Routes.AddRoute({ KHTTPMethod(""), "/*", [&](KRESTServer& http)
	{
		http.SetRawOutput("{\"message\":\"hello world\"}\n");
//		http.json.tx["message"] = "hello world";
	}});

	// create Options
	KREST::Options Options;
	Options.Type = KREST::HTTP;
	Options.iPort = 8888;
	Options.iMaxConnections = 20;
	Options.iTimeout = 10;
	Options.iMaxKeepaliveRounds = 5;

	// and start the REST server
	return KREST().Execute(Options, Routes);

}


