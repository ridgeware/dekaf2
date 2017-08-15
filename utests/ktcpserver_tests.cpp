#include "catch.hpp"

#include <dekaf2/ktcpserver.h>

using namespace dekaf2;

TEST_CASE("KTCPServer")
{

	KTCPServer server(6789);

// we cannot stop a running server.. so we do not start it at the moment
//	CHECK( server.start(10, true) == true );

}

