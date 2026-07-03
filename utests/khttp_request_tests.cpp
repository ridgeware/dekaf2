#include "catch.hpp"

#include <dekaf2/http/protocol/khttp_request.h>
#include <dekaf2/rest/framework/krestserver.h>
#include <dekaf2/system/filesystem/kfilesystem.h>

using namespace dekaf2;

TEST_CASE("KHTTPRequest")
{

	SECTION("KInHTTPRequestLine")
	{
		KInHTTPRequestLine RL;
		CHECK (RL.IsValid()	   == false);
		CHECK (RL.Get()        == "");
		CHECK (RL.GetMethod()  == "");
		CHECK (RL.GetResource()== "");
		CHECK (RL.GetPath()    == "");
		CHECK (RL.GetQuery()   == "");
		CHECK (RL.GetVersion() == "");

		auto Words = RL.Parse("GET /This/is/a/test?with=parameters HTTP/1.1");
		CHECK (Words.size()    == 3);
		CHECK (Words[0]        == "GET");
		CHECK (Words[1]        == "/This/is/a/test?with=parameters");
		CHECK (Words[2]        == "HTTP/1.1");
		CHECK (RL.IsValid()	   == true);
		CHECK (RL.Get()        == "GET /This/is/a/test?with=parameters HTTP/1.1");
		CHECK (RL.GetMethod()  == "GET");
		CHECK (RL.GetResource()== "/This/is/a/test?with=parameters");
		CHECK (RL.GetPath()    == "/This/is/a/test");
		CHECK (RL.GetQuery()   == "with=parameters");
		CHECK (RL.GetVersion() == "HTTP/1.1");

		Words = RL.Parse(" GET /This/is/a/test?with=parameters HTTP/1.1");
		CHECK (Words.size()    == 0);
		CHECK (RL.IsValid()	   == false);
		CHECK (RL.Get()        == " GET /This/is/a/test?with=parameters HTTP/1.1");
		CHECK (RL.GetMethod()  == "");
		CHECK (RL.GetResource()== "");
		CHECK (RL.GetPath()    == "");
		CHECK (RL.GetQuery()   == "");
		CHECK (RL.GetVersion() == "");

		Words = RL.Parse("GET  /This/is/a/test?with=parameters HTTP/1.1");
		CHECK (Words.size()    == 0);
		CHECK (RL.IsValid()	   == false);
		CHECK (RL.Get()        == "GET  /This/is/a/test?with=parameters HTTP/1.1");
		CHECK (RL.GetMethod()  == "");
		CHECK (RL.GetResource()== "");
		CHECK (RL.GetPath()    == "");
		CHECK (RL.GetQuery()   == "");
		CHECK (RL.GetVersion() == "");

		Words = RL.Parse("GET /This/is/a/test?with=parameters  HTTP/1.1");
		CHECK (Words.size()    == 0);
		CHECK (RL.IsValid()	   == false);
		CHECK (RL.Get()        == "GET /This/is/a/test?with=parameters  HTTP/1.1");
		CHECK (RL.GetMethod()  == "");
		CHECK (RL.GetResource()== "");
		CHECK (RL.GetPath()    == "");
		CHECK (RL.GetQuery()   == "");
		CHECK (RL.GetVersion() == "");

		Words = RL.Parse("GET /This/is/a/test?with=parametersHTTP/1.1");
		CHECK (Words.size()    == 0);
		CHECK (RL.IsValid()	   == false);
		CHECK (RL.Get()        == "GET /This/is/a/test?with=parametersHTTP/1.1");
		CHECK (RL.GetMethod()  == "");
		CHECK (RL.GetResource()== "");
		CHECK (RL.GetPath()    == "");
		CHECK (RL.GetQuery()   == "");
		CHECK (RL.GetVersion() == "");

		Words = RL.Parse("GET /This/is/a/test?with=parameters FTP/1.1");
		CHECK (Words.size()    == 0);
		CHECK (RL.IsValid()	   == false);
		CHECK (RL.Get()        == "GET /This/is/a/test?with=parameters FTP/1.1");
		CHECK (RL.GetMethod()  == "");
		CHECK (RL.GetResource()== "");
		CHECK (RL.GetPath()    == "");
		CHECK (RL.GetQuery()   == "");
		CHECK (RL.GetVersion() == "");

		KString sRequest;
		sRequest.assign(256, 'G');
		sRequest += "GET /This/is/a/test?with=parameters HTTP/1.1";
		Words = RL.Parse(sRequest);
		CHECK (Words.size()    == 0);
		CHECK (RL.IsValid()	   == false);
		CHECK (RL.Get()        == sRequest);
		CHECK (RL.GetMethod()  == "");
		CHECK (RL.GetResource()== "");
		CHECK (RL.GetPath()    == "");
		CHECK (RL.GetQuery()   == "");
		CHECK (RL.GetVersion() == "");

		sRequest = "GET ";
		sRequest.append(KInHTTPRequestLine::MAX_REQUESTLINELENGTH, '/');
		sRequest += "/This/is/a/test?with=parameters HTTP/1.1";
		Words = RL.Parse(sRequest);
		CHECK (Words.size()    == 0);
		CHECK (RL.IsValid()	   == false);
		CHECK (RL.Get()        == "");
		CHECK (RL.GetMethod()  == "");
		CHECK (RL.GetResource()== "");
		CHECK (RL.GetPath()    == "");
		CHECK (RL.GetQuery()   == "");
		CHECK (RL.GetVersion() == "");

		sRequest = "GET /This/is/a/test?with=parameters HTTP/1.1";
		sRequest.append(256, '1');
		Words = RL.Parse(sRequest);
		CHECK (Words.size()    == 0);
		CHECK (RL.IsValid()	   == false);
		CHECK (RL.Get()        == sRequest);
		CHECK (RL.GetMethod()  == "");
		CHECK (RL.GetResource()== "");
		CHECK (RL.GetPath()    == "");
		CHECK (RL.GetQuery()   == "");
		CHECK (RL.GetVersion() == "");
	}

}

TEST_CASE("KInHTTPRequestLine::GetQuery")
{
	// request line without a query string: GetQuery() must return an empty view,
	// not an out-of-bounds view from an unsigned length underflow (reachable e.g.
	// through the %q access-log directive on any query-less request).
	{
		KInHTTPRequestLine RL;
		RL.Parse("GET /path HTTP/1.1");
		auto q = RL.GetQuery();
		CHECK ( q.empty() );
		CHECK ( q.size() == 0 );
		CHECK ( RL.GetPath() == "/path" );
	}

	// with a query string it is returned without the leading '?'
	{
		KInHTTPRequestLine RL;
		RL.Parse("GET /path?x=1&y=2 HTTP/1.1");
		CHECK ( RL.GetQuery() == "x=1&y=2" );
		CHECK ( RL.GetPath()  == "/path" );
	}

	// present but empty query ("path?")
	{
		KInHTTPRequestLine RL;
		RL.Parse("GET /path? HTTP/1.1");
		auto q = RL.GetQuery();
		CHECK ( q.empty() );
		CHECK ( q.size() == 0 );
		CHECK ( RL.GetPath() == "/path" );
	}

	// root path, no query
	{
		KInHTTPRequestLine RL;
		RL.Parse("GET / HTTP/1.1");
		CHECK ( RL.GetQuery().empty() );
		CHECK ( RL.GetPath() == "/" );
	}
}
