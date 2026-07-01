#include <sstream>
#include <iostream>
#include <vector>
#include <map>
#include <set>
#include <cinttypes>
#include <dekaf2/time/duration/kprof.h>
#include <dekaf2/core/strings/kstring.h>

using namespace dekaf2;

namespace {

using KFSoCFindFn = KFindSetOfChars::size_type (KFindSetOfChars::*)(KStringView, KFindSetOfChars::size_type) const;

// measures only the cost of building a KFindSetOfChars from szNeedle, iIterations times -
// independent of any haystack, isolates the one-time construction cost
void BenchInit(const char* szLabel, const char* szNeedle, std::size_t iIterations)
{
	KString sNeedle(szNeedle);
	dekaf2::KProf prof(szLabel);
	prof.SetMultiplier(iIterations);
	for (std::size_t ct = 0; ct < iIterations; ++ct)
	{
		KProf::Force(&sNeedle);
		auto SoC = KFindSetOfChars(sNeedle.c_str());
		KProf::Force(&SoC);
	}
}

// measures construction AND search combined, on every iteration - this is what an
// ad-hoc (non-cached) call site actually pays.
// Find is a template (not runtime) parameter so the call below resolves to a direct,
// inlinable call just like the hand-written search-only benchmarks - a runtime function
// pointer would prevent inlining and make the comparison against those unfair.
template <KFSoCFindFn Find>
void BenchInitAndSearch(const char* szLabel, const char* szNeedle, KStringView sHaystackSrc, KFindSetOfChars::size_type iPos, std::size_t iIterations)
{
	KString sNeedle(szNeedle);
	// take our own copy of the haystack: the caller's buffer was just scanned
	// hundreds/thousands of times by the immediately preceding search-only
	// benchmark on the same block, which leaves it hot in cache and would
	// make this measurement look artificially cheap by comparison
	KString sHaystackCopy(sHaystackSrc);
	KStringView sHaystack(sHaystackCopy);
	dekaf2::KProf prof(szLabel);
	prof.SetMultiplier(iIterations);
	for (std::size_t ct = 0; ct < iIterations; ++ct)
	{
		KProf::Force(&sNeedle);
		KProf::Force(&sHaystack);
		auto SoC = KFindSetOfChars(sNeedle.c_str());
		KProf::Force(&SoC);
		if ((SoC.*Find)(sHaystack, iPos) < sHaystack.size()) KProf::Force();
	}
}

} // end of anonymous namespace

void kfindsetofchars()
{
	dekaf2::KProf ps("-KFindSetOfChars");

	KString sHaystackSource = "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmd tempor incididunt ut labore et dalore magna aliqua. ";
	/* Repeat sHaystackSource to over 5MB */
	sHaystackSource += sHaystackSource;
	sHaystackSource += sHaystackSource;
	sHaystackSource += sHaystackSource;
	sHaystackSource += sHaystackSource;
	sHaystackSource += sHaystackSource;
	sHaystackSource += sHaystackSource;
	sHaystackSource += sHaystackSource;
	sHaystackSource += sHaystackSource;
	sHaystackSource += sHaystackSource;
	sHaystackSource += sHaystackSource;
	sHaystackSource += sHaystackSource;
	sHaystackSource += sHaystackSource;
	sHaystackSource += sHaystackSource;
	sHaystackSource += sHaystackSource;
	sHaystackSource += sHaystackSource;
	sHaystackSource += sHaystackSource;

	auto SoC = KFindSetOfChars("defg");

	{
		dekaf2::KString s(20, '-');
		s.append("abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("soc find_first_of (20)");
		prof.SetMultiplier(200000);
		for (int ct = 0; ct < 200000; ++ct)
		{
			KProf::Force(&sv);
			if (SoC.find_first_in(sv) < 10) KProf::Force();
		}
		BenchInitAndSearch<&KFindSetOfChars::find_first_in>("soc find_first_of (20) [init+search]", "defg", sv, 0, 200000);
	}
	{
		dekaf2::KString s(200, '-');
		s.append("abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("soc find_first_of (200)");
		prof.SetMultiplier(200000);
		for (int ct = 0; ct < 200000; ++ct)
		{
			KProf::Force(&sv);
			if (SoC.find_first_in(sv) < 100) KProf::Force();
		}
		BenchInitAndSearch<&KFindSetOfChars::find_first_in>("soc find_first_of (200) [init+search]", "defg", sv, 0, 200000);
	}
	{
		dekaf2::KString s(5000, '-');
		s.append("abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("soc find_first_of (5000)");
		prof.SetMultiplier(200000);
		for (int ct = 0; ct < 200000; ++ct)
		{
			KProf::Force(&sv);
			if (SoC.find_first_in(sv) < 100) KProf::Force();
		}
		BenchInitAndSearch<&KFindSetOfChars::find_first_in>("soc find_first_of (5000) [init+search]", "defg", sv, 0, 200000);
	}
	{
		dekaf2::KString s(5000000, '-');
		s.append("abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("soc find_first_of (5M)");
		prof.SetMultiplier(200);
		for (int ct = 0; ct < 200; ++ct)
		{
			KProf::Force(&sv);
			if (SoC.find_first_in(sv) < 100) KProf::Force();
		}
		BenchInitAndSearch<&KFindSetOfChars::find_first_in>("soc find_first_of (5M) [init+search]", "defg", sv, 0, 200);
	}

	SoC = KFindSetOfChars("defghijw");

	{
		dekaf2::KString s(20, '-');
		s.append("abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("soc find_first_of (20) ns=8");
		prof.SetMultiplier(200000);
		for (int ct = 0; ct < 200000; ++ct)
		{
			KProf::Force(&sv);
			if (SoC.find_first_in(sv) < 10) KProf::Force();
		}
		BenchInitAndSearch<&KFindSetOfChars::find_first_in>("soc find_first_of (20) ns=8 [init+search]", "defghijw", sv, 0, 200000);
	}
	{
		dekaf2::KString s(200, '-');
		s.append("abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("soc find_first_of (200) ns=8");
		prof.SetMultiplier(200000);
		for (int ct = 0; ct < 200000; ++ct)
		{
			KProf::Force(&sv);
			if (SoC.find_first_in(sv) < 100) KProf::Force();
		}
		BenchInitAndSearch<&KFindSetOfChars::find_first_in>("soc find_first_of (200) ns=8 [init+search]", "defghijw", sv, 0, 200000);
	}
	{
		dekaf2::KString s(5000, '-');
		s.append("abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("soc find_first_of (5000) ns=8");
		prof.SetMultiplier(200000);
		for (int ct = 0; ct < 200000; ++ct)
		{
			KProf::Force(&sv);
			if (SoC.find_first_in(sv) < 100) KProf::Force();
		}
		BenchInitAndSearch<&KFindSetOfChars::find_first_in>("soc find_first_of (5000) ns=8 [init+search]", "defghijw", sv, 0, 200000);
	}
	{
		dekaf2::KString s(5000000, '-');
		s.append("abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("soc find_first_of (5M) ns=8");
		prof.SetMultiplier(200);
		for (int ct = 0; ct < 200; ++ct)
		{
			KProf::Force(&sv);
			if (SoC.find_first_in(sv) < 100) KProf::Force();
		}
		BenchInitAndSearch<&KFindSetOfChars::find_first_in>("soc find_first_of (5M) ns=8 [init+search]", "defghijw", sv, 0, 200);
	}

	SoC = KFindSetOfChars("abcdefghijklmnopqrstuvwxyz");

	{
		dekaf2::KString s(20, '-');
		s.append("abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("soc find_first_of >16 (20)");
		prof.SetMultiplier(200000);
		for (int ct = 0; ct < 200000; ++ct)
		{
			KProf::Force(&sv);
			if (SoC.find_first_in(sv) < 10) KProf::Force();
		}
		BenchInitAndSearch<&KFindSetOfChars::find_first_in>("soc find_first_of >16 (20) [init+search]", "abcdefghijklmnopqrstuvwxyz", sv, 0, 200000);
	}
	{
		dekaf2::KString s(200, '-');
		s.append("abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("soc find_first_of >16 (200)");
		prof.SetMultiplier(200000);
		for (int ct = 0; ct < 200000; ++ct)
		{
			KProf::Force(&sv);
			if (SoC.find_first_in(sv) < 100) KProf::Force();
		}
		BenchInitAndSearch<&KFindSetOfChars::find_first_in>("soc find_first_of >16 (200) [init+search]", "abcdefghijklmnopqrstuvwxyz", sv, 0, 200000);
	}
	{
		dekaf2::KString s(2000, '-');
		s.append("abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("soc find_first_of >16 (2000)");
		prof.SetMultiplier(200000);
		for (int ct = 0; ct < 200000; ++ct)
		{
			KProf::Force(&sv);
			if (SoC.find_first_in(sv) < 100) KProf::Force();
		}
		BenchInitAndSearch<&KFindSetOfChars::find_first_in>("soc find_first_of >16 (2000) [init+search]", "abcdefghijklmnopqrstuvwxyz", sv, 0, 200000);
	}
	{
		dekaf2::KString s(5000, '-');
		s.append("abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("soc find_first_of >16 (5000)");
		prof.SetMultiplier(200000);
		for (int ct = 0; ct < 200000; ++ct)
		{
			KProf::Force(&sv);
			if (SoC.find_first_in(sv) < 100) KProf::Force();
		}
		BenchInitAndSearch<&KFindSetOfChars::find_first_in>("soc find_first_of >16 (5000) [init+search]", "abcdefghijklmnopqrstuvwxyz", sv, 0, 200000);
	}
	{
		dekaf2::KString s(5000000, '-');
		s.append("abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("soc find_first_of >16 (5M)");
		prof.SetMultiplier(200);
		for (int ct = 0; ct < 200; ++ct)
		{
			KProf::Force(&sv);
			if (SoC.find_first_in(sv) < 100) KProf::Force();
		}
		BenchInitAndSearch<&KFindSetOfChars::find_first_in>("soc find_first_of >16 (5M) [init+search]", "abcdefghijklmnopqrstuvwxyz", sv, 0, 200);
	}

	SoC = KFindSetOfChars("&%-?");

	{
		dekaf2::KString s(20, '-');
		s.append("abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("soc find_first_not_of (20)");
		prof.SetMultiplier(200000);
		for (int ct = 0; ct < 200000; ++ct)
		{
			KProf::Force(&sv);
			if (SoC.find_first_not_in(sv) < 10) KProf::Force();
		}
		BenchInitAndSearch<&KFindSetOfChars::find_first_not_in>("soc find_first_not_of (20) [init+search]", "&%-?", sv, 0, 200000);
	}
	{
		dekaf2::KString s(200, '-');
		s.append("abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("soc find_first_not_of (200)");
		prof.SetMultiplier(200000);
		for (int ct = 0; ct < 200000; ++ct)
		{
			KProf::Force(&sv);
			if (SoC.find_first_not_in(sv) < 10) KProf::Force();
		}
		BenchInitAndSearch<&KFindSetOfChars::find_first_not_in>("soc find_first_not_of (200) [init+search]", "&%-?", sv, 0, 200000);
	}
	{
		dekaf2::KString s(5000, '-');
		s.append("abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("soc find_first_not_of (5000)");
		prof.SetMultiplier(200000);
		for (int ct = 0; ct < 200000; ++ct)
		{
			KProf::Force(&sv);
			if (SoC.find_first_not_in(sv) < 10) KProf::Force();
		}
		BenchInitAndSearch<&KFindSetOfChars::find_first_not_in>("soc find_first_not_of (5000) [init+search]", "&%-?", sv, 0, 200000);
	}
	{
		dekaf2::KString s(5000000, '-');
		s.append("abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("soc find_first_not_of (5M)");
		prof.SetMultiplier(200);
		for (int ct = 0; ct < 200; ++ct)
		{
			KProf::Force(&sv);
			if (SoC.find_first_not_in(sv) < 100) KProf::Force();
		}
		BenchInitAndSearch<&KFindSetOfChars::find_first_not_in>("soc find_first_not_of (5M) [init+search]", "&%-?", sv, 0, 200);
	}

	SoC = KFindSetOfChars("&%-!§&/()=?*#1234567890üöä");
	{
		dekaf2::KString s(20, '-');
		s.append("abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("soc find_first_not_of >16 (20)");
		prof.SetMultiplier(200000);
		for (int ct = 0; ct < 200000; ++ct)
		{
			KProf::Force(&sv);
			if (SoC.find_first_not_in(sv) < 10) KProf::Force();
		}
		BenchInitAndSearch<&KFindSetOfChars::find_first_not_in>("soc find_first_not_of >16 (20) [init+search]", "&%-!§&/()=?*#1234567890üöä", sv, 0, 200000);
	}
	{
		dekaf2::KString s(200, '-');
		s.append("abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("soc find_first_not_of >16 (200)");
		prof.SetMultiplier(200000);
		for (int ct = 0; ct < 200000; ++ct)
		{
			KProf::Force(&sv);
			if (SoC.find_first_not_in(sv) < 10) KProf::Force();
		}
		BenchInitAndSearch<&KFindSetOfChars::find_first_not_in>("soc find_first_not_of >16 (200) [init+search]", "&%-!§&/()=?*#1234567890üöä", sv, 0, 200000);
	}
	{
		dekaf2::KString s(2000, '-');
		s.append("abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("soc find_first_not_of >16 (2000)");
		prof.SetMultiplier(200000);
		for (int ct = 0; ct < 200000; ++ct)
		{
			KProf::Force(&sv);
			if (SoC.find_first_not_in(sv) < 10) KProf::Force();
		}
		BenchInitAndSearch<&KFindSetOfChars::find_first_not_in>("soc find_first_not_of >16 (2000) [init+search]", "&%-!§&/()=?*#1234567890üöä", sv, 0, 200000);
	}
	{
		dekaf2::KString s(5000, '-');
		s.append("abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("soc find_first_not_of >16 (5000)");
		prof.SetMultiplier(200000);
		for (int ct = 0; ct < 200000; ++ct)
		{
			KProf::Force(&sv);
			if (SoC.find_first_not_in(sv) < 10) KProf::Force();
		}
		BenchInitAndSearch<&KFindSetOfChars::find_first_not_in>("soc find_first_not_of >16 (5000) [init+search]", "&%-!§&/()=?*#1234567890üöä", sv, 0, 200000);
	}
	{
		dekaf2::KString s(5000000, '-');
		s.append("abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("soc find_first_not_of >16 (5M)");
		prof.SetMultiplier(200);
		for (int ct = 0; ct < 200; ++ct)
		{
			KProf::Force(&sv);
			if (SoC.find_first_not_in(sv) < 10) KProf::Force();
		}
		BenchInitAndSearch<&KFindSetOfChars::find_first_not_in>("soc find_first_not_of >16 (5M) [init+search]", "&%-!§&/()=?*#1234567890üöä", sv, 0, 200);
	}

	SoC = KFindSetOfChars("defg");
	{
		dekaf2::KString s(20, '-');
		s.insert(0, "abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("soc find_last_of (20)");
		prof.SetMultiplier(200000);
		for (int ct = 0; ct < 200000; ++ct)
		{
			KProf::Force(&sv);
			if (SoC.find_last_in(sv) > 100) KProf::Force();
		}
		BenchInitAndSearch<&KFindSetOfChars::find_last_in>("soc find_last_of (20) [init+search]", "defg", sv, KStringView::npos, 200000);
	}
	{
		dekaf2::KString s(200, '-');
		s.insert(0, "abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("soc find_last_of (200)");
		prof.SetMultiplier(200000);
		for (int ct = 0; ct < 200000; ++ct)
		{
			KProf::Force(&sv);
			if (SoC.find_last_in(sv) > 100) KProf::Force();
		}
		BenchInitAndSearch<&KFindSetOfChars::find_last_in>("soc find_last_of (200) [init+search]", "defg", sv, KStringView::npos, 200000);
	}
	{
		dekaf2::KString s(5000, '-');
		s.insert(0, "abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("soc find_last_of (5000)");
		prof.SetMultiplier(200000);
		for (int ct = 0; ct < 200000; ++ct)
		{
			KProf::Force(&sv);
			if (SoC.find_last_in(sv) > 100) KProf::Force();
		}
		BenchInitAndSearch<&KFindSetOfChars::find_last_in>("soc find_last_of (5000) [init+search]", "defg", sv, KStringView::npos, 200000);
	}

	{
		dekaf2::KString s(5000000, '-');
		s.insert(0, "abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("soc find_last_of (5M)");
		prof.SetMultiplier(200);
		for (int ct = 0; ct < 200; ++ct)
		{
			KProf::Force(&sv);
			if (SoC.find_last_in(sv) > 100) KProf::Force();
		}
		BenchInitAndSearch<&KFindSetOfChars::find_last_in>("soc find_last_of (5M) [init+search]", "defg", sv, KStringView::npos, 200);
	}

	SoC = KFindSetOfChars("abcdefghijklmnopqrstuvwxyz");

	{
		dekaf2::KString s(20, '-');
		s.insert(0, "abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("soc find_last_of >16 (20)");
		prof.SetMultiplier(200000);
		for (int ct = 0; ct < 200000; ++ct)
		{
			KProf::Force(&sv);
			if (SoC.find_last_in(sv) > 100) KProf::Force();
		}
		BenchInitAndSearch<&KFindSetOfChars::find_last_in>("soc find_last_of >16 (20) [init+search]", "abcdefghijklmnopqrstuvwxyz", sv, KStringView::npos, 200000);
	}
	{
		dekaf2::KString s(200, '-');
		s.insert(0, "abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("soc find_last_of >16 (200)");
		prof.SetMultiplier(200000);
		for (int ct = 0; ct < 200000; ++ct)
		{
			KProf::Force(&sv);
			if (SoC.find_last_in(sv) > 100) KProf::Force();
		}
		BenchInitAndSearch<&KFindSetOfChars::find_last_in>("soc find_last_of >16 (200) [init+search]", "abcdefghijklmnopqrstuvwxyz", sv, KStringView::npos, 200000);
	}
	{
		dekaf2::KString s(2000, '-');
		s.insert(0, "abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("soc find_last_of >16 (2000)");
		prof.SetMultiplier(200000);
		for (int ct = 0; ct < 200000; ++ct)
		{
			KProf::Force(&sv);
			if (SoC.find_last_in(sv) > 100) KProf::Force();
		}
		BenchInitAndSearch<&KFindSetOfChars::find_last_in>("soc find_last_of >16 (2000) [init+search]", "abcdefghijklmnopqrstuvwxyz", sv, KStringView::npos, 200000);
	}
	{
		dekaf2::KString s(5000, '-');
		s.insert(0, "abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("soc find_last_of >16 (5000)");
		prof.SetMultiplier(200000);
		for (int ct = 0; ct < 200000; ++ct)
		{
			KProf::Force(&sv);
			if (SoC.find_last_in(sv) > 100) KProf::Force();
		}
		BenchInitAndSearch<&KFindSetOfChars::find_last_in>("soc find_last_of >16 (5000) [init+search]", "abcdefghijklmnopqrstuvwxyz", sv, KStringView::npos, 200000);
	}
	{
		dekaf2::KString s(5000000, '-');
		s.insert(0, "abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("soc find_last_of >16 (5M)");
		prof.SetMultiplier(200);
		for (int ct = 0; ct < 200; ++ct)
		{
			KProf::Force(&sv);
			if (SoC.find_last_in(sv) > 100) KProf::Force();
		}
		BenchInitAndSearch<&KFindSetOfChars::find_last_in>("soc find_last_of >16 (5M) [init+search]", "abcdefghijklmnopqrstuvwxyz", sv, KStringView::npos, 200);
	}
	{
		dekaf2::KString s(20, '-');
		s.insert(0, "abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("soc find_last_not_of (20)");
		prof.SetMultiplier(200000);
		for (int ct = 0; ct < 200000; ++ct)
		{
			KProf::Force(&sv);
			if (SoC.find_last_in(sv) > 100) KProf::Force();
		}
	}

	SoC = KFindSetOfChars("&%-?");

	{
		dekaf2::KString s(200, '-');
		s.insert(0, "abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("soc find_last_not_of (200)");
		prof.SetMultiplier(200000);
		for (int ct = 0; ct < 200000; ++ct)
		{
			KProf::Force(&sv);
			if (SoC.find_last_not_in(sv) > 100) KProf::Force();
		}
		BenchInitAndSearch<&KFindSetOfChars::find_last_not_in>("soc find_last_not_of (200) [init+search]", "&%-?", sv, KStringView::npos, 200000);
	}
	{
		dekaf2::KString s(5000, '-');
		s.insert(0, "abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("soc find_last_not_of (5000)");
		prof.SetMultiplier(200000);
		for (int ct = 0; ct < 200000; ++ct)
		{
			KProf::Force(&sv);
			if (SoC.find_last_not_in(sv) > 100) KProf::Force();
		}
		BenchInitAndSearch<&KFindSetOfChars::find_last_not_in>("soc find_last_not_of (5000) [init+search]", "&%-?", sv, KStringView::npos, 200000);
	}
	{
		dekaf2::KString s(5000000, '-');
		s.insert(0, "abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("soc find_last_not_of (5M)");
		prof.SetMultiplier(200);
		for (int ct = 0; ct < 200; ++ct)
		{
			KProf::Force(&sv);
			if (SoC.find_last_not_in(sv) > 100) KProf::Force();
		}
		BenchInitAndSearch<&KFindSetOfChars::find_last_not_in>("soc find_last_not_of (5M) [init+search]", "&%-?", sv, KStringView::npos, 200);
	}

	SoC = KFindSetOfChars("&%-!§&/()=?*#1234567890üöä");

	{
		dekaf2::KString s(20, '-');
		s.insert(0, "abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("soc find_last_not_of >16 (20) ns=26");
		prof.SetMultiplier(200000);
		for (int ct = 0; ct < 200000; ++ct)
		{
			KProf::Force(&sv);
			if (SoC.find_last_not_in(sv) > 100) KProf::Force();
		}
		BenchInitAndSearch<&KFindSetOfChars::find_last_not_in>("soc find_last_not_of >16 (20) ns=26 [init+search]", "&%-!§&/()=?*#1234567890üöä", sv, KStringView::npos, 200000);
	}
	{
		dekaf2::KString s(200, '-');
		s.insert(0, "abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("soc find_last_not_of >16 (200) ns=26");
		prof.SetMultiplier(200000);
		for (int ct = 0; ct < 200000; ++ct)
		{
			KProf::Force(&sv);
			if (SoC.find_last_not_in(sv) > 100) KProf::Force();
		}
		BenchInitAndSearch<&KFindSetOfChars::find_last_not_in>("soc find_last_not_of >16 (200) ns=26 [init+search]", "&%-!§&/()=?*#1234567890üöä", sv, KStringView::npos, 200000);
	}
	{
		dekaf2::KString s(2000, '-');
		s.insert(0, "abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("soc find_last_not_of >16 (2000) ns=26");
		prof.SetMultiplier(200000);
		for (int ct = 0; ct < 200000; ++ct)
		{
			KProf::Force(&sv);
			if (SoC.find_last_not_in(sv) > 100) KProf::Force();
		}
		BenchInitAndSearch<&KFindSetOfChars::find_last_not_in>("soc find_last_not_of >16 (2000) ns=26 [init+search]", "&%-!§&/()=?*#1234567890üöä", sv, KStringView::npos, 200000);
	}
	{
		dekaf2::KString s(5000, '-');
		s.insert(0, "abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("soc find_last_not_of >16 (5000) ns=26");
		prof.SetMultiplier(200000);
		for (int ct = 0; ct < 200000; ++ct)
		{
			KProf::Force(&sv);
			if (SoC.find_last_not_in(sv) > 100) KProf::Force();
		}
		BenchInitAndSearch<&KFindSetOfChars::find_last_not_in>("soc find_last_not_of >16 (5000) ns=26 [init+search]", "&%-!§&/()=?*#1234567890üöä", sv, KStringView::npos, 200000);
	}
	{
		dekaf2::KString s(5000000, '-');
		s.insert(0, "abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("soc find_last_not_of >16 (5M) ns=26");
		prof.SetMultiplier(200);
		for (int ct = 0; ct < 200; ++ct)
		{
			KProf::Force(&sv);
			if (SoC.find_last_not_in(sv) > 100) KProf::Force();
		}
		BenchInitAndSearch<&KFindSetOfChars::find_last_not_in>("soc find_last_not_of >16 (5M) ns=26 [init+search]", "&%-!§&/()=?*#1234567890üöä", sv, KStringView::npos, 200);
	}

	SoC = KFindSetOfChars("wxyz");

	{
		dekaf2::KString s = sHaystackSource.substr(0, 20);
		s.append("tuvwxyz");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("soc find_first of (text 20) ns=4");
		prof.SetMultiplier(200000);
		for (int ct = 0; ct < 200000; ++ct)
		{
			KProf::Force(&sv);
			if (SoC.find_first_in(sv) < 10) KProf::Force();
		}
	}
	{
		dekaf2::KString s = sHaystackSource.substr(0, 200);
		s.append("tuvwxyz");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("soc find_first of (text 200) ns=4");
		prof.SetMultiplier(200000);
		for (int ct = 0; ct < 200000; ++ct)
		{
			KProf::Force(&sv);
			if (SoC.find_first_in(sv) < 100) KProf::Force();
		}
	}
	{
		dekaf2::KString s = sHaystackSource.substr(0, 5000);
		s.append("tuvwxyz");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("soc find_first of (text 5000) ns=4");
		prof.SetMultiplier(200000);
		for (int ct = 0; ct < 200000; ++ct)
		{
			KProf::Force(&sv);
			if (SoC.find_first_in(sv) < 100) KProf::Force();
		}
	}
	{
		dekaf2::KString s = sHaystackSource.substr(0, 5000000);
		s.append("tuvwxyz");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("soc find_first of (text 5M) ns=4");
		prof.SetMultiplier(200);
		for (int ct = 0; ct < 200; ++ct)
		{
			KProf::Force(&sv);
			if (SoC.find_first_in(sv) < 100) KProf::Force();
		}
	}

	SoC = KFindSetOfChars("wxyzWXYZ");

	{
		dekaf2::KString s = sHaystackSource.substr(0, 20);
		s.append("tuvwxyz");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("soc find_first of (text 20) ns=8");
		prof.SetMultiplier(200000);
		for (int ct = 0; ct < 200000; ++ct)
		{
			KProf::Force(&sv);
			if (SoC.find_first_in(sv) < 10) KProf::Force();
		}
	}
	{
		dekaf2::KString s = sHaystackSource.substr(0, 200);
		s.append("tuvwxyz");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("soc find_first of (text 200) ns=8");
		prof.SetMultiplier(200000);
		for (int ct = 0; ct < 200000; ++ct)
		{
			KProf::Force(&sv);
			if (SoC.find_first_in(sv) < 100) KProf::Force();
		}
	}
	{
		dekaf2::KString s = sHaystackSource.substr(0, 5000);
		s.append("tuvwxyz");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("soc find_first of (text 5000) ns=8");
		prof.SetMultiplier(200000);
		for (int ct = 0; ct < 200000; ++ct)
		{
			KProf::Force(&sv);
			if (SoC.find_first_in(sv) < 100) KProf::Force();
		}
	}
	{
		dekaf2::KString s = sHaystackSource.substr(0, 5000000);
		s.append("tuvwxyz");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("soc find_first of (text 5M) ns=8");
		prof.SetMultiplier(200);
		for (int ct = 0; ct < 200; ++ct)
		{
			KProf::Force(&sv);
			if (SoC.find_first_in(sv) < 100) KProf::Force();
		}
	}

	{
		KString sNeedle = "wxyz";

		{
			dekaf2::KString s = sHaystackSource.substr(0, 20);
			s.append("tuvwxyz");
			dekaf2::KStringView sv(s);
			dekaf2::KProf prof("soc init and find_first_of (20) ns=4");
			prof.SetMultiplier(200000);
			for (int ct = 0; ct < 200000; ++ct)
			{
				KProf::Force(&sv);
				KProf::Force(&sNeedle);
				SoC = KFindSetOfChars(sNeedle.c_str());
				KProf::Force(&SoC);
				if (SoC.find_first_in(sv) < 10) KProf::Force();
			}
		}
		{
			dekaf2::KString s = sHaystackSource.substr(0, 200);
			s.append("tuvwxyz");
			dekaf2::KStringView sv(s);
			dekaf2::KProf prof("soc init and find_first_of (200) ns=4");
			prof.SetMultiplier(200000);
			for (int ct = 0; ct < 200000; ++ct)
			{
				KProf::Force(&sv);
				KProf::Force(&sNeedle);
				SoC = KFindSetOfChars(sNeedle.c_str());
				KProf::Force(&SoC);
				if (SoC.find_first_in(sv) < 100) KProf::Force();
			}
		}
		{
			dekaf2::KString s = sHaystackSource.substr(0, 5000);
			s.append("tuvwxyz");
			dekaf2::KStringView sv(s);
			dekaf2::KProf prof("soc init and find_first_of (5000) ns=4");
			prof.SetMultiplier(200000);
			for (int ct = 0; ct < 200000; ++ct)
			{
				KProf::Force(&sv);
				KProf::Force(&sNeedle);
				SoC = KFindSetOfChars(sNeedle.c_str());
				KProf::Force(&SoC);
				if (SoC.find_first_in(sv) < 100) KProf::Force();
			}
		}
		{
			dekaf2::KString s = sHaystackSource.substr(0, 5000000);
			s.append("tuvwxyz");
			dekaf2::KStringView sv(s);
			dekaf2::KProf prof("soc init and find_first_of (5M) ns=4");
			prof.SetMultiplier(200);
			for (int ct = 0; ct < 200; ++ct)
			{
				KProf::Force(&sv);
				KProf::Force(&sNeedle);
				SoC = KFindSetOfChars(sNeedle.c_str());
				KProf::Force(&SoC);
				if (SoC.find_first_in(sv) < 100) KProf::Force();
			}
		}
	}

	{
		KString sNeedle = "wxyz%-!§&/()=?*#1234567890";

		{
			dekaf2::KString s = sHaystackSource.substr(0, 20);
			s.append("tuvwxyz");
			dekaf2::KStringView sv(s);
			dekaf2::KProf prof("soc init and find_first_of (20) ns=26");
			prof.SetMultiplier(200000);
			for (int ct = 0; ct < 200000; ++ct)
			{
				KProf::Force(&sv);
				KProf::Force(&sNeedle);
				SoC = KFindSetOfChars(sNeedle.c_str());
				KProf::Force(&SoC);
				if (SoC.find_first_in(sv) < 10) KProf::Force();
			}
		}
		{
			dekaf2::KString s = sHaystackSource.substr(0, 200);
			s.append("tuvwxyz");
			dekaf2::KStringView sv(s);
			dekaf2::KProf prof("soc init and find_first_of (200) ns=26");
			prof.SetMultiplier(200000);
			for (int ct = 0; ct < 200000; ++ct)
			{
				KProf::Force(&sv);
				KProf::Force(&sNeedle);
				SoC = KFindSetOfChars(sNeedle.c_str());
				KProf::Force(&SoC);
				if (SoC.find_first_in(sv) < 100) KProf::Force();
			}
		}
		{
			dekaf2::KString s = sHaystackSource.substr(0, 5000);
			s.append("tuvwxyz");
			dekaf2::KStringView sv(s);
			dekaf2::KProf prof("soc init and find_first_of (5000) ns=26");
			prof.SetMultiplier(200000);
			for (int ct = 0; ct < 200000; ++ct)
			{
				KProf::Force(&sv);
				KProf::Force(&sNeedle);
				SoC = KFindSetOfChars(sNeedle.c_str());
				KProf::Force(&SoC);
				if (SoC.find_first_in(sv) < 100) KProf::Force();
			}
		}
		{
			dekaf2::KString s = sHaystackSource.substr(0, 5000000);
			s.append("tuvwxyz");
			dekaf2::KStringView sv(s);
			dekaf2::KProf prof("soc init and find_first_of (5M) ns=26");
			prof.SetMultiplier(200);
			for (int ct = 0; ct < 200; ++ct)
			{
				KProf::Force(&sv);
				KProf::Force(&sNeedle);
				SoC = KFindSetOfChars(sNeedle.c_str());
				KProf::Force(&SoC);
				if (SoC.find_first_in(sv) < 100) KProf::Force();
			}
		}
	}

	// -----------------------------------------------------------------
	// construction-only cost: independent of haystack size/content,
	// so a single measurement per needle set is enough
	// -----------------------------------------------------------------
	BenchInit("soc init only (ns=4)",           "defg",                        200000);
	BenchInit("soc init only (ns=8)",           "defghijw",                    200000);
	BenchInit("soc init only (>16, alphabet)",  "abcdefghijklmnopqrstuvwxyz",  200000);
	BenchInit("soc init only not_of (ns=4)",    "&%-?",                        200000);
	BenchInit("soc init only not_of (>16)",     "&%-!§&/()=?*#1234567890üöä",  200000);
	BenchInit("soc init only text (ns=4)",      "wxyz",                        200000);
	BenchInit("soc init only text (ns=8)",      "wxyzWXYZ",                    200000);
	BenchInit("soc init only text (>16 ns=26)", "wxyz%-!§&/()=?*#1234567890",  200000);
}

void kfindsetofchars_bench()
{
	kfindsetofchars();
}


