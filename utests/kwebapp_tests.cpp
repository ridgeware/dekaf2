#include "catch.hpp"

#include <dekaf2/core/init/kdefinitions.h>

#if DEKAF2_HAS_WEBVIEW

#include <dekaf2/web/app/kwebapp.h>
#include <dekaf2/util/misc/kversion.h>

using namespace dekaf2;

TEST_CASE("KWebApp")
{
	SECTION("webview version")
	{
		// the vendored amalgamation in from/webview reports a semantic version
		KVersion Version;
		CHECK ( Version.Parse(kGetWebViewVersion()) == true );
		CHECK ( Version.size()                      == 3    );
	}
}

#endif // DEKAF2_HAS_WEBVIEW
