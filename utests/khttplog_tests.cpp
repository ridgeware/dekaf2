#include "catch.hpp"

#include <dekaf2/khttplog.h>
#include <dekaf2/krestserver.h>
#include <dekaf2/kfilesystem.h>

using namespace dekaf2;

namespace {
KTempDir TempDir;
}

TEST_CASE("KHTTPLog")
{

	SECTION("Open")
	{
		KHTTPLog Logger;
		CHECK( Logger.is_open() == false );
		auto sLogname = kFormat("{}/{}", TempDir.Name(), "test.log");
		if (!Logger.Open(KHTTPLog::LOG_FORMAT::EXTENDED, sLogname))
		{
			FAIL_CHECK( "Open returned false" );
		}
		CHECK( Logger.is_open() == true );
	}

}

