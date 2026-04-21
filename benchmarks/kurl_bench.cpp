
#include <cinttypes>
#include <dekaf2/time/duration/kprof.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/strings/kstringview.h>
#include <dekaf2/web/url/kurl.h>
#include <dekaf2/web/url/kurlencode.h>
#include <dekaf2/core/strings/bits/kfindsetofchars.h>

using namespace dekaf2;

namespace {

constexpr int kIter = 20000;

template<typename Fn>
inline void bench(const char* sLabel, Fn&& fn)
{
	dekaf2::KProf prof(sLabel);
	prof.SetMultiplier(kIter);
	for (int ct = 0; ct < kIter; ++ct)
	{
		fn();
	}
}

} // end anonymous namespace

void kurl_bench()
{
	dekaf2::KProf ps("-KURL");

	// =============================================================
	// KURL parse scenarios (full URL string -> structured KURL)
	// =============================================================

	// minimal: http://host/
	KStringView svMinimal     = "http://host/";
	// simple: common web URL, no user/pass/port/query/fragment
	KStringView svSimple      = "https://www.example.com/path/to/page";
	// without trailing slash / no path
	KStringView svNoPath      = "https://www.example.com";
	// with explicit port
	KStringView svWithPort    = "http://host.example.com:8080/api/v1/status";
	// with user:password credentials
	KStringView svWithAuth    = "http://alice:s3cr3t@git.example.com/repo.git";
	// fragment only (no query)
	KStringView svFragment    = "https://docs.example.com/guide/chapter1#section-intro";
	// query string only (no fragment)
	KStringView svQueryOnly   = "https://search.example.com/find?q=hello&lang=en&page=3";
	// full URL: every component filled
	KStringView svQuery       = "https://user:pass@api.example.com:8443/v2/search?q=hello+world&lang=en&page=3#results";
	// percent-encoded path and query
	KStringView svEncoded     = "https://example.com/path/to/p%C3%A4ge?name=%E4%B8%AD%E6%96%87&value=hello%20world";
	// long URL with big query string (CDN-style)
	KStringView svLong        = "https://cdn.example.com/assets/images/2024/01/15/hero-banner-large.jpg?w=1920&h=1080&fit=crop&auto=format&q=80";
	// IPv4 literal host
	KStringView svIPv4        = "http://192.168.1.100:8080/status?ok=1";
	// IPv6 literal host (bracketed)
	KStringView svIPv6        = "http://[2001:db8::1]:8080/v1/ping";
	// file:/// triple slash (no host)
	KStringView svFile3       = "file:///var/log/system.log";
	// file:// with host
	KStringView svFile2       = "file://filesrv.example.com/share/docs/readme.txt";
	// mailto
	KStringView svMailto      = "mailto:somebody@example.com";
	// ftp
	KStringView svFtp         = "ftp://ftp.example.com/pub/releases/dekaf2-2.1.12.tar.gz";
	// unknown scheme (goes to UNKNOWN path in SetProto)
	KStringView svUnknown     = "opaquelocktoken://abc-def-123-456";
	// unix socket
	KStringView svUnix        = "unix:///var/run/server.sock";

	bench("KURL parse minimal      ", [&]{ KURL url(svMinimal);    KProf::Force(&url); });
	bench("KURL parse simple       ", [&]{ KURL url(svSimple);     KProf::Force(&url); });
	bench("KURL parse no-path      ", [&]{ KURL url(svNoPath);     KProf::Force(&url); });
	bench("KURL parse with-port    ", [&]{ KURL url(svWithPort);   KProf::Force(&url); });
	bench("KURL parse with-auth    ", [&]{ KURL url(svWithAuth);   KProf::Force(&url); });
	bench("KURL parse fragment     ", [&]{ KURL url(svFragment);   KProf::Force(&url); });
	bench("KURL parse query-only   ", [&]{ KURL url(svQueryOnly);  KProf::Force(&url); });
	bench("KURL parse full         ", [&]{ KURL url(svQuery);      KProf::Force(&url); });
	bench("KURL parse encoded      ", [&]{ KURL url(svEncoded);    KProf::Force(&url); });
	bench("KURL parse long         ", [&]{ KURL url(svLong);       KProf::Force(&url); });
	bench("KURL parse IPv4         ", [&]{ KURL url(svIPv4);       KProf::Force(&url); });
	bench("KURL parse IPv6         ", [&]{ KURL url(svIPv6);       KProf::Force(&url); });
	bench("KURL parse file:///     ", [&]{ KURL url(svFile3);      KProf::Force(&url); });
	bench("KURL parse file://host  ", [&]{ KURL url(svFile2);      KProf::Force(&url); });
	bench("KURL parse mailto       ", [&]{ KURL url(svMailto);     KProf::Force(&url); });
	bench("KURL parse ftp          ", [&]{ KURL url(svFtp);        KProf::Force(&url); });
	bench("KURL parse unknown      ", [&]{ KURL url(svUnknown);    KProf::Force(&url); });
	bench("KURL parse unix         ", [&]{ KURL url(svUnix);       KProf::Force(&url); });

	// =============================================================
	// KURL serialize scenarios (pre-parsed KURL -> KString)
	// =============================================================
	{
		KURL url(svSimple);    bench("KURL serialize simple  ", [&]{ KString s = url.Serialize(); KProf::Force(&s); });
	}
	{
		KURL url(svWithPort);  bench("KURL serialize port    ", [&]{ KString s = url.Serialize(); KProf::Force(&s); });
	}
	{
		KURL url(svWithAuth);  bench("KURL serialize auth    ", [&]{ KString s = url.Serialize(); KProf::Force(&s); });
	}
	{
		KURL url(svQuery);     bench("KURL serialize full    ", [&]{ KString s = url.Serialize(); KProf::Force(&s); });
	}
	{
		KURL url(svEncoded);   bench("KURL serialize encoded ", [&]{ KString s = url.Serialize(); KProf::Force(&s); });
	}
	{
		KURL url(svLong);      bench("KURL serialize long    ", [&]{ KString s = url.Serialize(); KProf::Force(&s); });
	}
	{
		KURL url(svIPv6);      bench("KURL serialize IPv6    ", [&]{ KString s = url.Serialize(); KProf::Force(&s); });
	}
	{
		KURL url(svFile3);     bench("KURL serialize file    ", [&]{ KString s = url.Serialize(); KProf::Force(&s); });
	}

	// =============================================================
	// KResource parse / serialize (path + query + no host)
	// =============================================================
	KStringView svResSimple   = "/index.html";
	KStringView svResPath     = "/v2/search/results/page/42";
	KStringView svResQuery    = "/find?q=hello&lang=en";
	KStringView svResFragment = "/guide#section-3";
	KStringView svResFull     = "/v2/search?q=hello+world&lang=en&page=3#results";

	bench("KResource parse simple  ", [&]{ KResource r(svResSimple);   KProf::Force(&r); });
	bench("KResource parse path    ", [&]{ KResource r(svResPath);     KProf::Force(&r); });
	bench("KResource parse query   ", [&]{ KResource r(svResQuery);    KProf::Force(&r); });
	bench("KResource parse fragmnt ", [&]{ KResource r(svResFragment); KProf::Force(&r); });
	bench("KResource parse full    ", [&]{ KResource r(svResFull);     KProf::Force(&r); });

	{
		KResource r(svResSimple);   bench("KResource serialize simple", [&]{ KString s = r.Serialize(); KProf::Force(&s); });
	}
	{
		KResource r(svResFull);     bench("KResource serialize full  ", [&]{ KString s = r.Serialize(); KProf::Force(&s); });
	}

	// =============================================================
	// KTCPEndPoint parse / serialize (host + port, used by HTTP
	// connection pools — latency-sensitive)
	// =============================================================
	KStringView svEndHost     = "www.example.com";
	KStringView svEndHostPort = "api.example.com:8443";
	KStringView svEndIPv4     = "192.168.1.100:8080";
	KStringView svEndIPv6     = "[2001:db8::1]:8080";
	KStringView svEndScheme   = "https://api.example.com:8443";
	KStringView svEndUnix     = "unix:///var/run/server.sock";

	bench("KTCPEndPoint host       ", [&]{ KTCPEndPoint e(svEndHost);     KProf::Force(&e); });
	bench("KTCPEndPoint host:port  ", [&]{ KTCPEndPoint e(svEndHostPort); KProf::Force(&e); });
	bench("KTCPEndPoint IPv4       ", [&]{ KTCPEndPoint e(svEndIPv4);     KProf::Force(&e); });
	bench("KTCPEndPoint IPv6       ", [&]{ KTCPEndPoint e(svEndIPv6);     KProf::Force(&e); });
	bench("KTCPEndPoint scheme+host", [&]{ KTCPEndPoint e(svEndScheme);   KProf::Force(&e); });
	bench("KTCPEndPoint unix       ", [&]{ KTCPEndPoint e(svEndUnix);     KProf::Force(&e); });

	{
		KTCPEndPoint e(svEndHostPort); bench("KTCPEndPoint serialize   ", [&]{ KString s = e.Serialize(); KProf::Force(&s); });
	}
	{
		KTCPEndPoint e(svEndIPv6);     bench("KTCPEndPoint serialize v6", [&]{ KString s = e.Serialize(); KProf::Force(&s); });
	}

	// =============================================================
	// Micro benchmarks for isolated hot sub-steps
	// (helps target specific optimizations)
	// =============================================================

	// KProtocol scheme lookup on its own (no URL allocation overhead)
	bench("KProtocol parse http    ", [&]{
		url::KProtocol p;
		auto sv = p.Parse("http://rest");
		KProf::Force(&sv);
	});
	bench("KProtocol parse https   ", [&]{
		url::KProtocol p;
		auto sv = p.Parse("https://rest");
		KProf::Force(&sv);
	});
	bench("KProtocol parse rare    ", [&]{
		url::KProtocol p;
		auto sv = p.Parse("ldaps://rest");  // near end of SetProto table
		KProf::Force(&sv);
	});
	bench("KProtocol parse unknown ", [&]{
		url::KProtocol p;
		auto sv = p.Parse("opaquelocktoken://rest");
		KProf::Force(&sv);
	});

	// kUrlDecode: no percent-encoding (fast path candidate) vs heavy
	bench("kUrlDecode clean 32B    ", [&]{
		KString s;
		kUrlDecode(KStringView{"/path/to/some/resource/file.ext"}, s);
		KProf::Force(&s);
	});
	bench("kUrlDecode clean 128B   ", [&]{
		KString s;
		kUrlDecode(KStringView{
			"/assets/images/2024/01/15/hero-banner-large-version-with-a-fairly-long-path/subdir/file.jpg"
		}, s);
		KProf::Force(&s);
	});
	bench("kUrlDecode escaped      ", [&]{
		KString s;
		kUrlDecode(KStringView{"/path/to/p%C3%A4ge/with%20space/and%2Fslash"}, s);
		KProf::Force(&s);
	});
	bench("kUrlDecode plus-as-space", [&]{
		KString s;
		kUrlDecode(KStringView{"hello+world+from+plus+encoded+query"}, s, true);
		KProf::Force(&s);
	});

	// kUrlEncode: short, medium, heavy-escape
	bench("kUrlEncode clean        ", [&]{
		KString s;
		kUrlEncode(KStringView{"/path/to/page"}, s, URIPart::Path);
		KProf::Force(&s);
	});
	bench("kUrlEncode heavy        ", [&]{
		KString s;
		kUrlEncode(KStringView{"hello world / with special? chars & stuff"}, s, URIPart::Query);
		KProf::Force(&s);
	});

	// =============================================================
	// KFindSetOfChars construction + find cost (for P3/P5/O3 decision)
	// =============================================================

	// Ad-hoc construction (current URIComponent::Parse pattern: const char*
	// literal is converted to KFindSetOfChars on every call)
	KStringView svHay = "www.example.com/path/to/page";
	bench("FindSet adhoc 5ch       ", [&]{
		auto p = svHay.find_first_of(":/;?#");
		KProf::Force(&p);
	});
	bench("FindSet adhoc 9ch       ", [&]{
		auto p = svHay.find_first_of(": \t@/;?=#");
		KProf::Force(&p);
	});

	// Static constexpr (candidate replacement)
	static constexpr KFindSetOfChars kSet5 {":/;?#"};
	static constexpr KFindSetOfChars kSet9 {": \t@/;?=#"};
	bench("FindSet static 5ch      ", [&]{
		auto p = svHay.find_first_of(kSet5);
		KProf::Force(&p);
	});
	bench("FindSet static 9ch      ", [&]{
		auto p = svHay.find_first_of(kSet9);
		KProf::Force(&p);
	});

	// Just the construction, no search
	bench("FindSet ctor 5ch        ", [&]{
		KFindSetOfChars s{":/;?#"};
		KProf::Force(&s);
	});
	bench("FindSet ctor 9ch        ", [&]{
		KFindSetOfChars s{": \t@/;?=#"};
		KProf::Force(&s);
	});
}
