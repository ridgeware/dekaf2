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
	}
}

TEST_CASE("KHTTPLogParser")
{
	SECTION("Open")
	{
		auto sLogname = kFormat("{}/{}", TempDir.Name(), "read.log");

		KStringView sRawLog =
R"(172.31.71.1 - user1 [07/May/2025:16:55:52 +0000] "POST /page3/ HTTP/1.1" 200 29222 "https://host.testdomain.org/page1/" "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/18.4 Safari/605.1.15" "other.testdomain.org" 869 1811 68 63961µs; "192.168.1.1"
172.31.71.1 - user1 [07/May/2025:16:55:53 +0000] "GET /page4/ HTTP/1.1" 200 38969 "https://host.testdomain.org/page2/" "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/18.4 Safari/605.1.15" "other.testdomain.org" 741 5595 67 1289µs; "192.168.1.1" 
)";
		kWriteFile(sLogname, sRawLog);

		KHTTPLogParser Log(sLogname);
		CHECK ( Log.is_open() );

		if (!Log.Open(sLogname))
		{
			FAIL_CHECK( "Open returned false" );
		}
		else
		{
			CHECK( Log.is_open() == true );
			auto log = Log.Next();
			CHECK ( log.IsValid() );
			CHECK ( log.Method.Serialize()   == "POST" );
			CHECK ( log.Resource.Serialize() == "/page3/" );
			CHECK ( log.Timestamp            == KUnixTime("2025-05-07 16:55:52") );
			CHECK ( log.TotalTime.microseconds().count() == 63961UL );
			CHECK ( log.iContentLength       == 29222 );
			CHECK ( log.iHTTPStatus          == 200   );
			CHECK ( log.iProcessID           == 68    );
			CHECK ( log.iRXBytes             == 869   );
			CHECK ( log.iTXBytes             == 1811  );
			CHECK ( log.HTTPVersion.Serialize() == KHTTPVersion(KHTTPVersion::http11).Serialize() );
			CHECK ( log.sForwardedFor        == "192.168.1.1" );
			CHECK ( log.sHost                == "other.testdomain.org" );
			CHECK ( log.sReferrer            == "https://host.testdomain.org/page1/" );
			CHECK ( log.sRemoteIP            == "172.31.71.1" );
			CHECK ( log.sUser                == "user1" );
			CHECK ( log.sUserAgent           == "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/18.4 Safari/605.1.15" );
		}
	}
}

