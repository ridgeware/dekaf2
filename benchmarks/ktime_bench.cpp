
#include <cinttypes>
#include <dekaf2/time/duration/kprof.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/strings/kstringview.h>
#include <dekaf2/time/clock/ktime.h>

using namespace dekaf2;

void ktime_bench()
{
	dekaf2::KProf ps("-KTime");

	auto now = KUTCTime::now();

	KStringView svISO      = "2024-03-15T14:30:45Z";
	KStringView svHTTP     = "Fri, 15 Mar 2024 14:30:45 GMT";
	KStringView svCompact  = "20240315143045";
	KStringView svUSDate   = "03/15/2024 02:30:45 PM";

	{
		dekaf2::KProf prof("kFormTimestamp default");
		prof.SetMultiplier(20000);
		for (int ct = 0; ct < 20000; ++ct)
		{
			KString s = kFormTimestamp(now);
			KProf::Force(&s);
		}
	}
	{
		dekaf2::KProf prof("kFormTimestamp custom");
		prof.SetMultiplier(20000);
		for (int ct = 0; ct < 20000; ++ct)
		{
			KString s = kFormTimestamp(now, "{:%Y-%m-%d %H:%M:%S}");
			KProf::Force(&s);
		}
	}
	{
		dekaf2::KProf prof("kFormHTTPTimestamp");
		prof.SetMultiplier(20000);
		for (int ct = 0; ct < 20000; ++ct)
		{
			KString s = kFormHTTPTimestamp(now);
			KProf::Force(&s);
		}
	}
	{
		dekaf2::KProf prof("kParseTimestamp ISO");
		prof.SetMultiplier(20000);
		for (int ct = 0; ct < 20000; ++ct)
		{
			KUTCTime t = kParseTimestamp(svISO);
			KProf::Force(&t);
		}
	}
	{
		dekaf2::KProf prof("kParseTimestamp compact");
		prof.SetMultiplier(20000);
		for (int ct = 0; ct < 20000; ++ct)
		{
			KUTCTime t = kParseTimestamp(svCompact);
			KProf::Force(&t);
		}
	}
	{
		dekaf2::KProf prof("kParseTimestamp US date");
		prof.SetMultiplier(20000);
		for (int ct = 0; ct < 20000; ++ct)
		{
			KUTCTime t = kParseTimestamp(svUSDate);
			KProf::Force(&t);
		}
	}
	{
		dekaf2::KProf prof("kParseHTTPTimestamp");
		prof.SetMultiplier(20000);
		for (int ct = 0; ct < 20000; ++ct)
		{
			KUTCTime t = kParseHTTPTimestamp(svHTTP);
			KProf::Force(&t);
		}
	}
	{
		dekaf2::KProf prof("KUTCTime from string");
		prof.SetMultiplier(20000);
		for (int ct = 0; ct < 20000; ++ct)
		{
			KUTCTime t(svISO);
			KProf::Force(&t);
		}
	}
}
