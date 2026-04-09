
#include <cinttypes>
#include <dekaf2/kprof.h>
#include <dekaf2/kstring.h>
#include <dekaf2/kstringview.h>
#include <dekaf2/kstringutils.h>

using namespace dekaf2;

void kreplace_bench()
{
	dekaf2::KProf ps("-kReplace");

	KStringView svSource = "The quick brown fox jumps over the lazy dog and the quick brown fox rests";

	KString sLong(5000, '-');
	for (std::size_t i = 0; i < sLong.size(); i += 50)
	{
		sLong.replace(i, 5, "hello");
	}

	{
		dekaf2::KProf prof("kReplace string few hits");
		prof.SetMultiplier(50000);
		for (int ct = 0; ct < 50000; ++ct)
		{
			KString s(svSource);
			kReplace(s, "quick", "slow");
			KProf::Force(&s);
		}
	}
	{
		dekaf2::KProf prof("kReplace string no hit");
		prof.SetMultiplier(50000);
		for (int ct = 0; ct < 50000; ++ct)
		{
			KString s(svSource);
			kReplace(s, "xyz123", "replaced");
			KProf::Force(&s);
		}
	}
	{
		dekaf2::KProf prof("kReplace string many hits");
		prof.SetMultiplier(50000);
		for (int ct = 0; ct < 50000; ++ct)
		{
			KString s(svSource);
			kReplace(s, "the", "THE");
			KProf::Force(&s);
		}
	}
	{
		dekaf2::KProf prof("kReplace string grow");
		prof.SetMultiplier(50000);
		for (int ct = 0; ct < 50000; ++ct)
		{
			KString s(svSource);
			kReplace(s, "the", "a very long replacement string");
			KProf::Force(&s);
		}
	}
	{
		dekaf2::KProf prof("kReplace string shrink");
		prof.SetMultiplier(50000);
		for (int ct = 0; ct < 50000; ++ct)
		{
			KString s(svSource);
			kReplace(s, "the quick brown fox", "X");
			KProf::Force(&s);
		}
	}
	{
		dekaf2::KProf prof("kReplace char");
		prof.SetMultiplier(50000);
		for (int ct = 0; ct < 50000; ++ct)
		{
			KString s(svSource);
			kReplace(s, ' ', '_');
			KProf::Force(&s);
		}
	}
	{
		dekaf2::KProf prof("kReplace long many hits");
		prof.SetMultiplier(5000);
		for (int ct = 0; ct < 5000; ++ct)
		{
			KString s(sLong);
			kReplace(s, "hello", "world!");
			KProf::Force(&s);
		}
	}
}
