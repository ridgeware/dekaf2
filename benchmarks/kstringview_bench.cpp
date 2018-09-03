#include <sstream>
#include <iostream>
#include <vector>
#include <map>
#include <set>
#include <cinttypes>
#include <dekaf2/kprof.h>
#include <dekaf2/kstring.h>

#if DEKAF2_GCC_VERSION < 5000
static const char* str0 = "";
static const char* str15 = "123456789012345";
static const char* str23 = "12345678901234567890123";
static const char* str250 = "12345678901234567890123456789012345678901234567890"
		"12345678901234567890123456789012345678901234567890"
		"12345678901234567890123456789012345678901234567890"
		"12345678901234567890123456789012345678901234567890"
		"12345678901234567890123456789012345678901234567890";
#endif

using namespace dekaf2;

void kstringview()
{
	dekaf2::KProf ps("-KStringView");
	
	{
		dekaf2::KString s(200, '-');
		s.append("abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("sv find (200)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			KProf::Force(&sv);
			if (sv.find("abcdefg") < 10) KProf::Force();
		}
	}
	{
		dekaf2::KString s(200, '-');
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("sv notfound (200)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			KProf::Force(&sv);
			if (sv.find("abcdefg") < 10) KProf::Force();
		}
	}
	{
		dekaf2::KString s(5000, '-');
		s.append("abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("sv find (5000)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			KProf::Force(&sv);
			if (sv.find("abcdefg") < 10) KProf::Force();
		}
	}
	{
		dekaf2::KString s(5000, '-');
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("sv notfound (5000)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			KProf::Force(&sv);
			if (sv.find("abcdefg") < 10) KProf::Force();
		}
	}
	{
		dekaf2::KString s(5000000, '-');
		s.append("abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("sv find (5M)");
		prof.SetMultiplier(1000);
		for (int ct = 0; ct < 1000; ++ct)
		{
			KProf::Force(&sv);
			if (sv.find("abcdefg") < 10) KProf::Force();
		}
	}
	{
		dekaf2::KString s(5000, 'a');
		s.append("abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("sv find worst case");
		prof.SetMultiplier(100000);
		for (int ct = 0; ct < 100000; ++ct)
		{
			KProf::Force(&sv);
			if (sv.find("abcdefg") < 10) KProf::Force();
		}
	}
	{
		dekaf2::KString s(200, '-');
		s.append("abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("sv find char (200)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			KProf::Force(&sv);
			if (sv.find('d') < 100) KProf::Force();

		}
	}
	{
		dekaf2::KString s(5000, '-');
		s.append("abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("sv find char (5000)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			KProf::Force(&sv);
			if (sv.find('d') < 100) KProf::Force();
		}
	}
	{
		dekaf2::KString s(5000000, '-');
		s.append("abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("sv find char (5M)");
		prof.SetMultiplier(1000);
		for (int ct = 0; ct < 1000; ++ct)
		{
			KProf::Force(&sv);
			if (sv.find('d') < 100) KProf::Force();
		}
	}
	{
		dekaf2::KString s(20, '-');
		s.append("abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("sv find_first_of (20)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			KProf::Force(&sv);
			if (sv.find_first_of("defg") < 10) KProf::Force();
		}
	}
	{
		dekaf2::KString s(200, '-');
		s.append("abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("sv find_first_of (200)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			KProf::Force(&sv);
			if (sv.find_first_of("defg") < 100) KProf::Force();
		}
	}
	{
		dekaf2::KString s(5000, '-');
		s.append("abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("sv find_first_of (5000)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			KProf::Force(&sv);
			if (sv.find_first_of("defg") < 100) KProf::Force();
		}
	}
	{
		dekaf2::KString s(5000000, '-');
		s.append("abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("sv find_first_of (5M)");
		prof.SetMultiplier(1000);
		for (int ct = 0; ct < 1000; ++ct)
		{
			KProf::Force(&sv);
			if (sv.find_first_of("defg") < 100) KProf::Force();
		}
	}
	{
		dekaf2::KString s(20, '-');
		s.append("abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("sv find_first_of >16 (20)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			KProf::Force(&sv);
			if (sv.find_first_of("abcdefghijklmnopqrstuvwxyz") < 10) KProf::Force();
		}
	}
	{
		dekaf2::KString s(200, '-');
		s.append("abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("sv find_first_of >16 (200)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			KProf::Force(&sv);
			if (sv.find_first_of("abcdefghijklmnopqrstuvwxyz") < 100) KProf::Force();
		}
	}
	{
		dekaf2::KString s(5000, '-');
		s.append("abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("sv find_first_of >16 (5000)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			KProf::Force(&sv);
			if (sv.find_first_of("abcdefghijklmnopqrstuvwxyz") < 100) KProf::Force();
		}
	}
	{
		dekaf2::KString s(5000000, '-');
		s.append("abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("sv find_first_of >16 (5M)");
		prof.SetMultiplier(1000);
		for (int ct = 0; ct < 1000; ++ct)
		{
			KProf::Force(&sv);
			if (sv.find_first_of("abcdefghijklmnopqrstuvwxyz") < 100) KProf::Force();
		}
	}
	{
		dekaf2::KString s(20, '-');
		s.append("abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("sv find_first_not_of (20)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			KProf::Force(&sv);
			if (sv.find_first_not_of("&%-?") < 10) KProf::Force();
		}
	}
	{
		dekaf2::KString s(200, '-');
		s.append("abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("sv find_first_not_of (200)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			KProf::Force(&sv);
			if (sv.find_first_not_of("&%-?") < 10) KProf::Force();
		}
	}
	{
		dekaf2::KString s(5000, '-');
		s.append("abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("sv find_first_not_of (5000)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			KProf::Force(&sv);
			if (sv.find_first_not_of("&%-?") < 10) KProf::Force();
		}
	}
	{
		dekaf2::KString s(5000000, '-');
		s.append("abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("sv find_first_not_of (5M)");
		prof.SetMultiplier(1000);
		for (int ct = 0; ct < 1000; ++ct)
		{
			KProf::Force(&sv);
			if (sv.find_first_not_of("&%-?") < 100) KProf::Force();
		}
	}
	{
		dekaf2::KString s(20, '-');
		s.append("abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("sv find_first_not_of >16 (20)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			KProf::Force(&sv);
			if (sv.find_first_not_of("&%-!§&/()=?*#1234567890üöä") < 10) KProf::Force();
		}
	}
	{
		dekaf2::KString s(200, '-');
		s.append("abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("sv find_first_not_of >16 (200)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			KProf::Force(&sv);
			if (sv.find_first_not_of("&%-!§&/()=?*#1234567890üöä") < 10) KProf::Force();
		}
	}
	{
		dekaf2::KString s(5000, '-');
		s.append("abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("sv find_first_not_of >16 (5000)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			KProf::Force(&sv);
			if (sv.find_first_not_of("&%-!§&/()=?*#1234567890üöä") < 10) KProf::Force();
		}
	}
	{
		dekaf2::KString s(5000000, '-');
		s.append("abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("sv find_first_not_of >16 (5M)");
		prof.SetMultiplier(1000);
		for (int ct = 0; ct < 1000; ++ct)
		{
			KProf::Force(&sv);
			if (sv.find_first_not_of("&%-!§&/()=?*#1234567890üöä") < 10) KProf::Force();
		}
	}
	{
		dekaf2::KString s(20, '-');
		s.insert(0, "abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("sv find_last_of (20)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			KProf::Force(&sv);
			if (sv.find_last_of("defg") > 100) KProf::Force();
		}
	}
	{
		dekaf2::KString s(200, '-');
		s.insert(0, "abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("sv find_last_of (200)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			KProf::Force(&sv);
			if (sv.find_last_of("defg") > 100) KProf::Force();
		}
	}
	{
		dekaf2::KString s(5000, '-');
		s.insert(0, "abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("sv find_last_of (5000)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			KProf::Force(&sv);
			if (sv.find_last_of("defg") > 100) KProf::Force();
		}
	}
	{
		dekaf2::KString s(20, '-');
		s.insert(0, "abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("sv find_last_of >16 (20)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			KProf::Force(&sv);
			if (sv.find_last_of("abcdefghijklmnopqrstuvwxyz") > 100) KProf::Force();
		}
	}
	{
		dekaf2::KString s(200, '-');
		s.insert(0, "abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("sv find_last_of >16 (200)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			KProf::Force(&sv);
			if (sv.find_last_of("abcdefghijklmnopqrstuvwxyz") > 100) KProf::Force();
		}
	}
	{
		dekaf2::KString s(5000, '-');
		s.insert(0, "abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("sv find_last_of >16 (5000)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			KProf::Force(&sv);
			if (sv.find_last_of("abcdefghijklmnopqrstuvwxyz") > 100) KProf::Force();
		}
	}
	{
		dekaf2::KString s(20, '-');
		s.insert(0, "abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("sv find_last_not_of (20)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			KProf::Force(&sv);
			if (sv.find_last_not_of("&%-?") > 100) KProf::Force();
		}
	}
	{
		dekaf2::KString s(200, '-');
		s.insert(0, "abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("sv find_last_not_of (200)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			KProf::Force(&sv);
			if (sv.find_last_not_of("&%-?") > 100) KProf::Force();
		}
	}
	{
		dekaf2::KString s(5000, '-');
		s.insert(0, "abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("sv find_last_not_of (5000)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			KProf::Force(&sv);
			if (sv.find_last_not_of("&%-?") > 100) KProf::Force();
		}
	}
	{
		dekaf2::KString s(20, '-');
		s.insert(0, "abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("sv find_last_not_of >16 (20)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			KProf::Force(&sv);
			if (sv.find_last_not_of("&%-!§&/()=?*#1234567890üöä") > 100) KProf::Force();
		}
	}
	{
		dekaf2::KString s(200, '-');
		s.insert(0, "abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("sv find_last_not_of >16 (200)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			KProf::Force(&sv);
			if (sv.find_last_not_of("&%-!§&/()=?*#1234567890üöä") > 100) KProf::Force();
		}
	}
	{
		dekaf2::KString s(5000, '-');
		s.insert(0, "abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("sv find_last_not_of >16 (5000)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			KProf::Force(&sv);
			if (sv.find_last_not_of("&%-!§&/()=?*#1234567890üöä") > 100) KProf::Force();
		}
	}
	{
		dekaf2::KString s(200, '-');
		s.insert(0, "abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("sv rfind (200)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			KProf::Force(&sv);
			if (sv.rfind("abcdefg") > 100) KProf::Force();
		}
	}
	{
		dekaf2::KString s(5000, '-');
		s.insert(0, "abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("sv rfind (5000)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			KProf::Force(&sv);
			if (sv.rfind("abcdefg") > 100) KProf::Force();
		}
	}
	{
		dekaf2::KString s(5000000, '-');
		s.insert(0, "abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("sv rfind (5M)");
		prof.SetMultiplier(1000);
		for (int ct = 0; ct < 1000; ++ct)
		{
			KProf::Force(&sv);
			if (sv.rfind("abcdefg") > 100) KProf::Force();
		}
	}
	{
		dekaf2::KString s(200, '-');
		s.insert(0, "abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("sv rfind char (200)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			KProf::Force(&sv);
			if (sv.rfind('d') > 100) KProf::Force();
		}
	}
	{
		dekaf2::KString s(5000, '-');
		s.insert(0, "abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("sv rfind char (5000)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			KProf::Force(&sv);
			if (sv.rfind('d') > 100) KProf::Force();
		}
	}
	{
		dekaf2::KString s(5000000, '-');
		s.insert(0, "abcdefg");
		dekaf2::KStringView sv(s);
		dekaf2::KProf prof("sv rfind char (5M)");
		prof.SetMultiplier(1000);
		for (int ct = 0; ct < 1000; ++ct)
		{
			KProf::Force(&sv);
			if (sv.rfind('d') > 100) KProf::Force();
		}
	}
}

void kstringview_bench()
{
	kstringview();
}


