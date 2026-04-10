
#include <cinttypes>
#include <dekaf2/time/duration/kprof.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/strings/kstringview.h>
#include <dekaf2/web/url/kurl.h>

using namespace dekaf2;

void kurl_bench()
{
	dekaf2::KProf ps("-KURL");

	KStringView svSimple  = "https://www.example.com/path/to/page";
	KStringView svQuery   = "https://user:pass@api.example.com:8443/v2/search?q=hello+world&lang=en&page=3#results";
	KStringView svEncoded = "https://example.com/path/to/p%C3%A4ge?name=%E4%B8%AD%E6%96%87&value=hello%20world";
	KStringView svLong    = "https://cdn.example.com/assets/images/2024/01/15/hero-banner-large.jpg?w=1920&h=1080&fit=crop&auto=format&q=80";

	{
		dekaf2::KProf prof("KURL parse simple");
		prof.SetMultiplier(20000);
		for (int ct = 0; ct < 20000; ++ct)
		{
			KURL url(svSimple);
			KProf::Force(&url);
		}
	}
	{
		dekaf2::KProf prof("KURL parse query");
		prof.SetMultiplier(20000);
		for (int ct = 0; ct < 20000; ++ct)
		{
			KURL url(svQuery);
			KProf::Force(&url);
		}
	}
	{
		dekaf2::KProf prof("KURL parse encoded");
		prof.SetMultiplier(20000);
		for (int ct = 0; ct < 20000; ++ct)
		{
			KURL url(svEncoded);
			KProf::Force(&url);
		}
	}
	{
		dekaf2::KProf prof("KURL parse long");
		prof.SetMultiplier(20000);
		for (int ct = 0; ct < 20000; ++ct)
		{
			KURL url(svLong);
			KProf::Force(&url);
		}
	}
	{
		KURL url(svSimple);
		dekaf2::KProf prof("KURL serialize simple");
		prof.SetMultiplier(20000);
		for (int ct = 0; ct < 20000; ++ct)
		{
			KString s = url.Serialize();
			KProf::Force(&s);
		}
	}
	{
		KURL url(svQuery);
		dekaf2::KProf prof("KURL serialize query");
		prof.SetMultiplier(20000);
		for (int ct = 0; ct < 20000; ++ct)
		{
			KString s = url.Serialize();
			KProf::Force(&s);
		}
	}
	{
		KURL url(svLong);
		dekaf2::KProf prof("KURL serialize long");
		prof.SetMultiplier(20000);
		for (int ct = 0; ct < 20000; ++ct)
		{
			KString s = url.Serialize();
			KProf::Force(&s);
		}
	}
	{
		dekaf2::KProf prof("KResource parse");
		prof.SetMultiplier(20000);
		KStringView svResource = "/v2/search?q=hello+world&lang=en&page=3#results";
		for (int ct = 0; ct < 20000; ++ct)
		{
			KResource res(svResource);
			KProf::Force(&res);
		}
	}
	{
		dekaf2::KProf prof("KResource serialize");
		prof.SetMultiplier(20000);
		KResource res("/v2/search?q=hello+world&lang=en&page=3#results");
		for (int ct = 0; ct < 20000; ++ct)
		{
			KString s = res.Serialize();
			KProf::Force(&s);
		}
	}
}
