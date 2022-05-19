#include <sstream>
#include <iostream>
#include <vector>
#include <map>
#include <set>
#include <cinttypes>
#include <dekaf2/kprof.h>
#include <dekaf2/kstring.h>
#include <dekaf2/klog.h>
#include <dekaf2/dekaf2.h>

using namespace dekaf2;

void SetEvery(KString& sStr, char ch, KString::size_type iEvery = 50)
{
	KString::size_type iPos = iEvery;

	while (iPos < sStr.size())
	{
		sStr[iPos] = ch;
		iPos += iEvery;
	}
}

void fast_kstring()
{
	dekaf2::KProf ps("-KString");
	
	{
		dekaf2::KString s(50000, '-');
		SetEvery(s, 'a');
		s.append("abcdefg");
		dekaf2::KProf prof("find (50000)");
		prof.SetMultiplier(100000);
		for (int ct = 0; ct < 100000; ++ct)
		{
			KProf::Force(&s);
			if (s.find("abcdefg") < 10) KProf::Force();
		}
	}
	{
		dekaf2::KString s(50000, '-');
		SetEvery(s, 'a');
		s.insert(0, "abcdefg");
		dekaf2::KProf prof("rfind (50000)");
		prof.SetMultiplier(100000);
		for (int ct = 0; ct < 100000; ++ct)
		{
			KProf::Force(&s);
			if (s.rfind("abcdefg") > 100) KProf::Force();
		}
	}

	{
		dekaf2::KString s(20, '-');
		s.append("abcdefg");
		dekaf2::KProf prof("find_first_of (20)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			KProf::Force(&s);
			if (s.find_first_of("defg") < 10) KProf::Force();
		}
	}
	{
		dekaf2::KString s(200, '-');
		s.append("abcdefg");
		dekaf2::KProf prof("find_first_of (200)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			KProf::Force(&s);
			if (s.find_first_of("defg") < 10) KProf::Force();
		}
	}
	{
		dekaf2::KString s(5000, '-');
		s.append("abcdefg");
		dekaf2::KProf prof("find_first_of (5000)");
		prof.SetMultiplier(100000);
		for (int ct = 0; ct < 100000; ++ct)
		{
			KProf::Force(&s);
			if (s.find_first_of("defg") < 100) KProf::Force();
		}
	}

	{
		dekaf2::KString s(20, '-');
		s.append("abcdefg");
		dekaf2::KProf prof("find_first_of >16 (20)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			KProf::Force(&s);
			if (s.find_first_of("abcdefghijklmnopqrstuvwxyz") < 10) KProf::Force();
		}
	}
	{
		dekaf2::KString s(200, '-');
		s.append("abcdefg");
		dekaf2::KProf prof("find_first_of >16 (200)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			KProf::Force(&s);
			if (s.find_first_of("abcdefghijklmnopqrstuvwxyz") < 10) KProf::Force();
		}
	}
	{
		dekaf2::KString s(5000, '-');
		s.append("abcdefg");
		dekaf2::KProf prof("find_first_of >16 (5000)");
		prof.SetMultiplier(100000);
		for (int ct = 0; ct < 100000; ++ct)
		{
			KProf::Force(&s);
			if (s.find_first_of("abcdefghijklmnopqrstuvwxyz") < 100) KProf::Force();
		}
	}

	{
		dekaf2::KString s(20, '-');
		s.append("abcdefg");
		dekaf2::KProf prof("find_first_not_of (20)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			KProf::Force(&s);
			if (s.find_first_not_of("&%-?") < 10) KProf::Force();
		}
	}
	{
		dekaf2::KString s(200, '-');
		s.append("abcdefg");
		dekaf2::KProf prof("find_first_not_of (200)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			KProf::Force(&s);
			if (s.find_first_not_of("&%-?") < 10) KProf::Force();
		}
	}
	{
		dekaf2::KString s(5000, '-');
		s.append("abcdefg");
		dekaf2::KProf prof("find_first_not_of (5000)");
		prof.SetMultiplier(100000);
		for (int ct = 0; ct < 100000; ++ct)
		{
			KProf::Force(&s);
			if (s.find_first_not_of("&%-?") < 10) KProf::Force();
		}
	}
	{
		dekaf2::KString s(20, '-');
		s.append("abcdefg");
		dekaf2::KProf prof("find_first_not_of >16 (20)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			KProf::Force(&s);
			if (s.find_first_not_of("&%-!§&/()=?*#1234567890üöä") < 10) KProf::Force();
		}
	}
	{
		dekaf2::KString s(200, '-');
		s.append("abcdefg");
		dekaf2::KProf prof("find_first_not_of >16 (200)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			KProf::Force(&s);
			if (s.find_first_not_of("&%-!§&/()=?*#1234567890üöä") < 10) KProf::Force();
		}
	}
	{
		dekaf2::KString s(5000, '-');
		s.append("abcdefg");
		dekaf2::KProf prof("find_first_not_of >16 (5000)");
		prof.SetMultiplier(100000);
		for (int ct = 0; ct < 100000; ++ct)
		{
			KProf::Force(&s);
			if (s.find_first_not_of("&%-!§&/()=?*#1234567890üöä") < 10) KProf::Force();
		}
	}

	{
		dekaf2::KString s(20, '-');
		s.insert(0, "abcdefg");
		dekaf2::KProf prof("find_last_of (20)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			KProf::Force(&s);
			if (s.find_last_of("defg") < 10) KProf::Force();
		}
	}
	{
		dekaf2::KString s(200, '-');
		s.insert(0, "abcdefg");
		dekaf2::KProf prof("find_last_of (200)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			KProf::Force(&s);
			if (s.find_last_of("defg") < 10) KProf::Force();
		}
	}
	{
		dekaf2::KString s(5000, '-');
		s.insert(0, "abcdefg");
		dekaf2::KProf prof("find_last_of (5000)");
		prof.SetMultiplier(100000);
		for (int ct = 0; ct < 100000; ++ct)
		{
			KProf::Force(&s);
			if (s.find_last_of("defg") < 100) KProf::Force();
		}
	}

	{
		dekaf2::KString s(20, '-');
		s.insert(0, "abcdefg");
		dekaf2::KProf prof("find_last_of >16 (20)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			KProf::Force(&s);
			if (s.find_last_of("abcdefghijklmnopqrstuvwxyz") < 10) KProf::Force();
		}
	}
	{
		dekaf2::KString s(200, '-');
		s.insert(0, "abcdefg");
		dekaf2::KProf prof("find_last_of >16 (200)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			KProf::Force(&s);
			if (s.find_last_of("abcdefghijklmnopqrstuvwxyz") < 10) KProf::Force();
		}
	}
	{
		dekaf2::KString s(5000, '-');
		s.insert(0, "abcdefg");
		dekaf2::KProf prof("find_last_of >16 (5000)");
		prof.SetMultiplier(100000);
		for (int ct = 0; ct < 100000; ++ct)
		{
			KProf::Force(&s);
			if (s.find_last_of("abcdefghijklmnopqrstuvwxyz") < 100) KProf::Force();
		}
	}

	{
		dekaf2::KString s(20, '-');
		s.insert(0, "abcdefg");
		dekaf2::KProf prof("find_last_not_of (20)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			KProf::Force(&s);
			if (s.find_last_not_of("&%-?") < 10) KProf::Force();
		}
	}
	{
		dekaf2::KString s(200, '-');
		s.insert(0, "abcdefg");
		dekaf2::KProf prof("find_last_not_of (200)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			KProf::Force(&s);
			if (s.find_last_not_of("&%-?") < 10) KProf::Force();
		}
	}
	{
		dekaf2::KString s(5000, '-');
		s.insert(0, "abcdefg");
		dekaf2::KProf prof("find_last_not_of (5000)");
		prof.SetMultiplier(100000);
		for (int ct = 0; ct < 100000; ++ct)
		{
			KProf::Force(&s);
			if (s.find_last_not_of("&%-?") < 100) KProf::Force();
		}
	}

	{
		dekaf2::KString s(20, '-');
		s.insert(0, "abcdefg");
		dekaf2::KProf prof("find_last_not_of >16 (20)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			KProf::Force(&s);
			if (s.find_last_not_of("&%-!§&/()=?*#1234567890üöä") < 10) KProf::Force();
		}
	}
	{
		dekaf2::KString s(200, '-');
		s.insert(0, "abcdefg");
		dekaf2::KProf prof("find_last_not_of >16 (200)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			KProf::Force(&s);
			if (s.find_last_not_of("&%-!§&/()=?*#1234567890üöä") < 10) KProf::Force();
		}
	}
	{
		dekaf2::KString s(5000, '-');
		s.insert(0, "abcdefg");
		dekaf2::KProf prof("find_last_not_of >16 (5000)");
		prof.SetMultiplier(100000);
		for (int ct = 0; ct < 100000; ++ct)
		{
			KProf::Force(&s);
			if (s.find_last_not_of("&%-!§&/()=?*#1234567890üöä") < 100) KProf::Force();
		}
	}
}

void fast_kstring_bench()
{
	fast_kstring();
}

int main()
{
	KLog::getInstance().SetName("sbench");
	Dekaf::getInstance().SetMultiThreading();
	Dekaf::getInstance().SetUnicodeLocale();

	fast_kstring_bench();

	kProfFinalize();

	return 0;
}


