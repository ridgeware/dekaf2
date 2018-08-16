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

void kstring()
{
	dekaf2::KProf("-KString");
	{
		dekaf2::KProf prof("short (0)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			dekaf2::KString str(str0);
			if (str.size() > 100000) std::cout << "large";
		}
	}
	{
		dekaf2::KProf prof("short (15)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			dekaf2::KString str(str15);
			if (str.size() > 100000) std::cout << "large";
		}
	}
	{
		dekaf2::KProf prof("short (23)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			dekaf2::KString str(str23);
			if (str.size() > 100000) std::cout << "large";
		}
	}
	{
		dekaf2::KProf prof("medium (250)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			dekaf2::KString str(str250);
			if (str.size() > 100000) std::cout << "large";
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
			dekaf2::KString str(buffer);
			if (str.size() > 100000) std::cout << "large";
		}
	}
	{
		dekaf2::KString s(200, '-');
		s.append("abcdefg");
		dekaf2::KProf prof("find (200)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			s.find("abcdefg");
		}
	}
	{
		dekaf2::KString s(200, '-');
		dekaf2::KProf prof("notfound (200)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			s.find("abcdefg");
		}
	}
	{
		dekaf2::KString s(50000, '-');
		s.append("abcdefg");
		dekaf2::KProf prof("find (50000)");
		prof.SetMultiplier(100000);
		for (int ct = 0; ct < 100000; ++ct)
		{
			s.find("abcdefg");
		}
	}
	{
		dekaf2::KString s(50000, '-');
		dekaf2::KProf prof("notfound (50000)");
		prof.SetMultiplier(100000);
		for (int ct = 0; ct < 100000; ++ct)
		{
			s.find("abcdefg");
		}
	}
	{
		dekaf2::KString s(50000, 'a');
		s.append("abcdefg");
		dekaf2::KProf prof("find worst case");
		prof.SetMultiplier(100000);
		for (int ct = 0; ct < 100000; ++ct)
		{
			s.find("abcdefg");
		}
	}
	{
		dekaf2::KString s(200, '-');
		s.append("abcdefg");
		dekaf2::KProf prof("find char (200)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			if (s.find('d') < 100) std::cout << "found";

		}
	}
	{
		dekaf2::KString s(5000, '-');
		s.append("abcdefg");
		dekaf2::KProf prof("find char (5000)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			if (s.find('d') < 100) std::cout << "found";
		}
	}
	{
		dekaf2::KString s(0, '-');
		dekaf2::KProf prof("+= short (0)");
		prof.SetMultiplier(100000);
		for (int ct = 0; ct < 100000; ++ct)
		{
			s += '-';
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
			if (s.find_first_of("defg") < 10) std::cout << "found";
		}
	}
	{
		dekaf2::KString s(200, '-');
		s.append("abcdefg");
		dekaf2::KProf prof("find_first_of (200)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			if (s.find_first_of("defg") < 100) std::cout << "found";
		}
	}
	{
		dekaf2::KString s(5000, '-');
		s.append("abcdefg");
		dekaf2::KProf prof("find_first_of (5000)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			if (s.find_first_of("defg") < 100) std::cout << "found";
		}
	}
	{
		dekaf2::KString s(5000000, '-');
		s.append("abcdefg");
		dekaf2::KProf prof("find_first_of (5M)");
		prof.SetMultiplier(1000);
		for (int ct = 0; ct < 1000; ++ct)
		{
			if (s.find_first_of("defg") < 100) std::cout << "found";
		}
	}	{
		dekaf2::KString s(20, '-');
		s.append("abcdefg");
		dekaf2::KProf prof("find_first_not_of (20)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			if (s.find_first_not_of("&%-?") < 10) std::cout << "found";
		}
	}
	{
		dekaf2::KString s(200, '-');
		s.append("abcdefg");
		dekaf2::KProf prof("find_first_not_of (200)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			if (s.find_first_not_of("&%-?") < 10) std::cout << "found";
		}
	}
	{
		dekaf2::KString s(5000, '-');
		s.append("abcdefg");
		dekaf2::KProf prof("find_first_not_of (5000)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			if (s.find_first_not_of("&%-?") < 10) std::cout << "found";
		}
	}
	{
		dekaf2::KString s(5000000, '-');
		s.append("abcdefg");
		dekaf2::KProf prof("find_first_not_of (5M)");
		prof.SetMultiplier(1000);
		for (int ct = 0; ct < 1000; ++ct)
		{
			if (s.find_first_not_of("&%-?") < 100) std::cout << "found";
		}
	}
}

void kstring_bench()
{
	kstring();
}


