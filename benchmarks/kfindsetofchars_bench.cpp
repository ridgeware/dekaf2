#include <sstream>
#include <iostream>
#include <vector>
#include <map>
#include <set>
#include <cinttypes>
#include <dekaf2/kprof.h>
#include <dekaf2/kstring.h>

using namespace dekaf2;

void kfindsetofchars()
{
	dekaf2::KProf ps("-KFindSetOfChars");

	auto SoC = KFindSetOfChars("defg");

	{
		dekaf2::KString s(20, '-');
		s.append("abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("soc find_first_of (20)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			KProf::Force(&sv);
			if (SoC.find_first_in(sv) < 10) KProf::Force();
		}
	}
	{
		dekaf2::KString s(200, '-');
		s.append("abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("soc find_first_of (200)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			KProf::Force(&sv);
			if (SoC.find_first_in(sv) < 100) KProf::Force();
		}
	}
	{
		dekaf2::KString s(5000, '-');
		s.append("abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("soc find_first_of (5000)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			KProf::Force(&sv);
			if (SoC.find_first_in(sv) < 100) KProf::Force();
		}
	}
	{
		dekaf2::KString s(5000000, '-');
		s.append("abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("soc find_first_of (5M)");
		prof.SetMultiplier(1000);
		for (int ct = 0; ct < 1000; ++ct)
		{
			KProf::Force(&sv);
			if (SoC.find_first_in(sv) < 100) KProf::Force();
		}
	}

	SoC = KFindSetOfChars("abcdefghijklmnopqrstuvwxyz");

	{
		dekaf2::KString s(20, '-');
		s.append("abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("soc find_first_of >16 (20)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			KProf::Force(&sv);
			if (SoC.find_first_in(sv) < 10) KProf::Force();
		}
	}
	{
		dekaf2::KString s(200, '-');
		s.append("abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("soc find_first_of >16 (200)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			KProf::Force(&sv);
			if (SoC.find_first_in(sv) < 100) KProf::Force();
		}
	}
	{
		dekaf2::KString s(5000, '-');
		s.append("abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("soc find_first_of >16 (5000)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			KProf::Force(&sv);
			if (SoC.find_first_in(sv) < 100) KProf::Force();
		}
	}
	{
		dekaf2::KString s(5000000, '-');
		s.append("abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("soc find_first_of >16 (5M)");
		prof.SetMultiplier(1000);
		for (int ct = 0; ct < 1000; ++ct)
		{
			KProf::Force(&sv);
			if (SoC.find_first_in(sv) < 100) KProf::Force();
		}
	}

	SoC = KFindSetOfChars("&%-?");

	{
		dekaf2::KString s(20, '-');
		s.append("abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("soc find_first_not_of (20)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			KProf::Force(&sv);
			if (SoC.find_first_not_in(sv) < 10) KProf::Force();
		}
	}
	{
		dekaf2::KString s(200, '-');
		s.append("abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("soc find_first_not_of (200)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			KProf::Force(&sv);
			if (SoC.find_first_not_in(sv) < 10) KProf::Force();
		}
	}
	{
		dekaf2::KString s(5000, '-');
		s.append("abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("soc find_first_not_of (5000)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			KProf::Force(&sv);
			if (SoC.find_first_not_in(sv) < 10) KProf::Force();
		}
	}
	{
		dekaf2::KString s(5000000, '-');
		s.append("abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("soc find_first_not_of (5M)");
		prof.SetMultiplier(1000);
		for (int ct = 0; ct < 1000; ++ct)
		{
			KProf::Force(&sv);
			if (SoC.find_first_not_in(sv) < 100) KProf::Force();
		}
	}

	SoC = KFindSetOfChars("&%-!§&/()=?*#1234567890üöä");
	{
		dekaf2::KString s(20, '-');
		s.append("abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("soc find_first_not_of >16 (20)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			KProf::Force(&sv);
			if (SoC.find_first_not_in(sv) < 10) KProf::Force();
		}
	}
	{
		dekaf2::KString s(200, '-');
		s.append("abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("soc find_first_not_of >16 (200)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			KProf::Force(&sv);
			if (SoC.find_first_not_in(sv) < 10) KProf::Force();
		}
	}
	{
		dekaf2::KString s(5000, '-');
		s.append("abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("soc find_first_not_of >16 (5000)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			KProf::Force(&sv);
			if (SoC.find_first_not_in(sv) < 10) KProf::Force();
		}
	}
	{
		dekaf2::KString s(5000000, '-');
		s.append("abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("soc find_first_not_of >16 (5M)");
		prof.SetMultiplier(1000);
		for (int ct = 0; ct < 1000; ++ct)
		{
			KProf::Force(&sv);
			if (SoC.find_first_not_in(sv) < 10) KProf::Force();
		}
	}

	SoC = KFindSetOfChars("defg");
	{
		dekaf2::KString s(20, '-');
		s.insert(0, "abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("soc find_last_of (20)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			KProf::Force(&sv);
			if (SoC.find_last_in(sv) > 100) KProf::Force();
		}
	}
	{
		dekaf2::KString s(200, '-');
		s.insert(0, "abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("soc find_last_of (200)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			KProf::Force(&sv);
			if (SoC.find_last_in(sv) > 100) KProf::Force();
		}
	}
	{
		dekaf2::KString s(5000, '-');
		s.insert(0, "abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("soc find_last_of (5000)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			KProf::Force(&sv);
			if (SoC.find_last_in(sv) > 100) KProf::Force();
		}
	}

	SoC = KFindSetOfChars("abcdefghijklmnopqrstuvwxyz");

	{
		dekaf2::KString s(20, '-');
		s.insert(0, "abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("soc find_last_of >16 (20)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			KProf::Force(&sv);
			if (SoC.find_last_in(sv) > 100) KProf::Force();
		}
	}
	{
		dekaf2::KString s(200, '-');
		s.insert(0, "abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("soc find_last_of >16 (200)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			KProf::Force(&sv);
			if (SoC.find_last_in(sv) > 100) KProf::Force();
		}
	}
	{
		dekaf2::KString s(5000, '-');
		s.insert(0, "abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("soc find_last_of >16 (5000)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			KProf::Force(&sv);
			if (SoC.find_last_in(sv) > 100) KProf::Force();
		}
	}
	{
		dekaf2::KString s(20, '-');
		s.insert(0, "abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("soc find_last_not_of (20)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
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
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			KProf::Force(&sv);
			if (SoC.find_last_not_in(sv) > 100) KProf::Force();
		}
	}
	{
		dekaf2::KString s(5000, '-');
		s.insert(0, "abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("soc find_last_not_of (5000)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			KProf::Force(&sv);
			if (SoC.find_last_not_in(sv) > 100) KProf::Force();
		}
	}

	SoC = KFindSetOfChars("&%-!§&/()=?*#1234567890üöä");

	{
		dekaf2::KString s(20, '-');
		s.insert(0, "abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("soc find_last_not_of >16 (20)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			KProf::Force(&sv);
			if (SoC.find_last_not_in(sv) > 100) KProf::Force();
		}
	}
	{
		dekaf2::KString s(200, '-');
		s.insert(0, "abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("soc find_last_not_of >16 (200)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			KProf::Force(&sv);
			if (SoC.find_last_not_in(sv) > 100) KProf::Force();
		}
	}
	{
		dekaf2::KString s(5000, '-');
		s.insert(0, "abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("soc find_last_not_of >16 (5000)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			KProf::Force(&sv);
			if (SoC.find_last_not_in(sv) > 100) KProf::Force();
		}
	}
}

void kfindsetofchars_bench()
{
	kfindsetofchars();
}


