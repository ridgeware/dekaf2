#include "catch.hpp"

// this test takes a long time to load its data in debug mode,
// therefore we do not run it per default
#if 0

#include <dekaf2/kuseragent.h>
#include <dekaf2/kstring.h>

using namespace dekaf2;

TEST_CASE("KHTTPUserAgent") {

	SECTION("setup")
	{
		KString sUA = "Mozilla/5.0 (iPhone; CPU iPhone OS 5_1_1 like Mac OS X) "
		"AppleWebKit/534.46 "
		"(KHTML, like Gecko) Version/5.1 Mobile/9B206 Safari/7534.48.3";

		auto UA = KHTTPUserAgent::Get(sUA);

		CHECK ( UA.GetBrowser().Get()     == "Mobile Safari 5.1.0" );
		CHECK ( UA.GetDevice().GetModel() == "iPhone"              );
		CHECK ( UA.GetOS().Get()          == "iOS 5.1.1"           );
		CHECK ( UA.IsSpider()             == false                 );

		CHECK ( KHTTPUserAgent::GetDeviceType(sUA) == KHTTPUserAgent::DeviceType::Mobile );
	}
}

#endif
