#include "catch.hpp"

#include <dekaf2/khttp_request.h>
#include <dekaf2/krestserver.h>
#include <dekaf2/kfilesystem.h>

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
