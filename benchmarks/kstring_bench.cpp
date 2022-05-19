#include <sstream>
#include <iostream>
#include <vector>
#include <map>
#include <set>
#include <cinttypes>
#include <dekaf2/kprof.h>
#include <dekaf2/kstring.h>

static const char* str0 = "";
static const char* str15 = "123456789012345";
static const char* str23 = "12345678901234567890123";
static const char* str250 = "12345678901234567890123456789012345678901234567890"
		"12345678901234567890123456789012345678901234567890"
		"12345678901234567890123456789012345678901234567890"
		"12345678901234567890123456789012345678901234567890"
		"12345678901234567890123456789012345678901234567890";

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

void kstring()
{
	dekaf2::KProf ps("-KString");
	
	{
		dekaf2::KProf prof("short (0)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			KProf::Force(&str0);
			dekaf2::KString str(str0);
			if (str.size() > 100000) KProf::Force();
		}
	}
	{
		dekaf2::KProf prof("short (15)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			KProf::Force(&str15);
			dekaf2::KString str(str15);
			if (str.size() > 100000) KProf::Force();
		}
	}
	{
		dekaf2::KProf prof("short (23)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			KProf::Force(&str23);
			dekaf2::KString str(str23);
			if (str.size() > 100000) KProf::Force();
		}
	}
	{
		dekaf2::KProf prof("medium (250)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			KProf::Force(&str250);
			dekaf2::KString str(str250);
			if (str.size() > 100000) KProf::Force();
		}
	}
	{
		char buffer[5001];
		memset(buffer, '-', 5000);
		buffer[5000] = 0;
		dekaf2::KProf prof("large (5000)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			KProf::Force(buffer);
			dekaf2::KString str(buffer);
			if (str.size() > 100000) KProf::Force();
		}
	}
	{
		dekaf2::KString s(200, '-');
		SetEvery(s, 'a');
		s.append("abcdefg");
		dekaf2::KProf prof("find (200)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			KProf::Force(&s);
			if (s.find("abcdefg") < 10) KProf::Force();
		}
	}
	{
		dekaf2::KString s(200, '-');
		SetEvery(s, 'a');
		dekaf2::KProf prof("notfound (200)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			KProf::Force(&s);
			if (s.find("abcdefg") < 10) KProf::Force();
		}
	}
	{
		dekaf2::KString s(5000, '-');
		SetEvery(s, 'a');
		s.append("abcdefg");
		dekaf2::KProf prof("find (5000)");
		prof.SetMultiplier(100000);
		for (int ct = 0; ct < 100000; ++ct)
		{
			KProf::Force(&s);
			if (s.find("abcdefg") < 10) KProf::Force();
		}
	}
	{
		dekaf2::KString s(5000, '-');
		SetEvery(s, 'a');
		dekaf2::KProf prof("notfound (5000)");
		prof.SetMultiplier(100000);
		for (int ct = 0; ct < 100000; ++ct)
		{
			KProf::Force(&s);
			if (s.find("abcdefg") < 10) KProf::Force();
		}
	}
	{
		dekaf2::KString s(5000000, '-');
		SetEvery(s, 'a');
		s.append("abcdefg");
		dekaf2::KProf prof("find (5M)");
		prof.SetMultiplier(1000);
		for (int ct = 0; ct < 1000; ++ct)
		{
			KProf::Force(&s);
			if (s.find("abcdefg") < 10) KProf::Force();
		}
	}
	{
		dekaf2::KString s(5000, 'a');
		s.append("abcdefg");
		dekaf2::KProf prof("find worst case");
		prof.SetMultiplier(100000);
		for (int ct = 0; ct < 100000; ++ct)
		{
			KProf::Force(&s);
			if (s.find("abcdefg") < 10) KProf::Force();
		}
	}
	{
		dekaf2::KString s(200, '-');
		s.append("abcdefg");
		dekaf2::KProf prof("find char (200)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			KProf::Force(&s);
			if (s.find('d') < 100) KProf::Force();

		}
	}
	{
		dekaf2::KString s(5000, '-');
		s.append("abcdefg");
		dekaf2::KProf prof("find char (5000)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			KProf::Force(&s);
			if (s.find('d') < 100) KProf::Force();
		}
	}
	{
		dekaf2::KString s(5000000, '-');
		s.append("abcdefg");
		dekaf2::KProf prof("find char (5M)");
		prof.SetMultiplier(1000);
		for (int ct = 0; ct < 1000; ++ct)
		{
			KProf::Force(&s);
			if (s.find('d') < 100) KProf::Force();
		}
	}
	{
		dekaf2::KString s(0, '-');
		dekaf2::KProf prof("+= short (0)");
		prof.SetMultiplier(100000);
		for (int ct = 0; ct < 100000; ++ct)
		{
			s += '-';
			KProf::Force(&s);
			s.pop_back();
		}
	}
	{
		dekaf2::KString s(14, '-');
		dekaf2::KProf prof("+= short (15)");
		prof.SetMultiplier(100000);
		for (int ct = 0; ct < 100000; ++ct)
		{
			s += '-';
			KProf::Force(&s);
			s.pop_back();
		}
	}
	{
		dekaf2::KString s(22, '-');
		dekaf2::KProf prof("+= short (23)");
		prof.SetMultiplier(100000);
		for (int ct = 0; ct < 100000; ++ct)
		{
			s += '-';
			KProf::Force(&s);
			s.pop_back();
		}
	}
	{
		dekaf2::KString s(249, '-');
		dekaf2::KProf prof("+= medium (250)");
		prof.SetMultiplier(100000);
		for (int ct = 0; ct < 100000; ++ct)
		{
			s += '-';
			KProf::Force(&s);
			s.pop_back();
		}
	}
	{
		dekaf2::KString s(5000, '-');
		dekaf2::KProf prof("+= large (5000)");
		prof.SetMultiplier(100000);
		for (int ct = 0; ct < 100000; ++ct)
		{
			s += '-';
			KProf::Force(&s);
			s.pop_back();
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
			if (s.find_first_of("defg") < 100) KProf::Force();
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
		dekaf2::KString s(5000000, '-');
		s.append("abcdefg");
		dekaf2::KProf prof("find_first_of (5M)");
		prof.SetMultiplier(1000);
		for (int ct = 0; ct < 1000; ++ct)
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
			if (s.find_first_of("abcdefghijklmnopqrstuvwxyz") < 100) KProf::Force();
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
		dekaf2::KString s(5000000, '-');
		s.append("abcdefg");
		dekaf2::KProf prof("find_first_of >16 (5M)");
		prof.SetMultiplier(1000);
		for (int ct = 0; ct < 1000; ++ct)
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
		dekaf2::KString s(5000000, '-');
		s.append("abcdefg");
		dekaf2::KProf prof("find_first_not_of (5M)");
		prof.SetMultiplier(1000);
		for (int ct = 0; ct < 1000; ++ct)
		{
			KProf::Force(&s);
			if (s.find_first_not_of("&%-?") < 100) KProf::Force();
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
		dekaf2::KString s(5000000, '-');
		s.append("abcdefg");
		dekaf2::KProf prof("find_first_not_of >16 (5M)");
		prof.SetMultiplier(1000);
		for (int ct = 0; ct < 1000; ++ct)
		{
			KProf::Force(&s);
			if (s.find_first_not_of("&%-!§&/()=?*#1234567890üöä") < 100) KProf::Force();
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
			if (s.find_last_of("defg") < 100) KProf::Force();
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
			if (s.find_last_of("abcdefghijklmnopqrstuvwxyz") < 100) KProf::Force();
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
			if (s.find_last_not_of("&%-?") < 100) KProf::Force();
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
			if (s.find_last_not_of("&%-!§&/()=?*#1234567890üöä") < 100) KProf::Force();
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
	{
		dekaf2::KString s(200, '-');
		SetEvery(s, 'a');
		s.insert(0, "abcdefg");
		dekaf2::KProf prof("rfind (200)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			KProf::Force(&s);
			if (s.rfind("abcdefg") > 100) KProf::Force();
		}
	}
	{
		dekaf2::KString s(5000, '-');
		SetEvery(s, 'a');
		s.insert(0, "abcdefg");
		dekaf2::KProf prof("rfind (5000)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			KProf::Force(&s);
			if (s.rfind("abcdefg") > 100) KProf::Force();
		}
	}
	{
		dekaf2::KString s(5000000, '-');
		SetEvery(s, 'a');
		s.insert(0, "abcdefg");
		dekaf2::KProf prof("rfind (5M)");
		prof.SetMultiplier(1000);
		for (int ct = 0; ct < 1000; ++ct)
		{
			KProf::Force(&s);
			if (s.rfind("abcdefg") > 100) KProf::Force();
		}
	}
	{
		dekaf2::KString s(200, '-');
		SetEvery(s, 'a');
		s.insert(0, "abcdefg");
		dekaf2::KProf prof("rfind char (200)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			KProf::Force(&s);
			if (s.rfind('d') > 100) KProf::Force();
		}
	}
	{
		dekaf2::KString s(5000, '-');
		SetEvery(s, 'a');
		s.insert(0, "abcdefg");
		dekaf2::KProf prof("rfind char (5000)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			KProf::Force(&s);
			if (s.rfind('d') > 100) KProf::Force();
		}
	}
	{
		dekaf2::KString s(5000000, '-');
		SetEvery(s, 'a');
		s.insert(0, "abcdefg");
		dekaf2::KProf prof("rfind char (5M)");
		prof.SetMultiplier(1000);
		for (int ct = 0; ct < 1000; ++ct)
		{
			KProf::Force(&s);
			if (s.rfind('d') > 100) KProf::Force();
		}
	}
}

void kstring_bench()
{
	kstring();
}


