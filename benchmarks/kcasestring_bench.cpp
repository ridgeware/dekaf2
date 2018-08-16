#include <iostream>
#include <cstring>
#include <dekaf2/kprof.h>
#include <dekaf2/kcasestring.h>

void kcasestring()
{
	{
		dekaf2::KString s(10, 'A');
		dekaf2::KString s2(9, 'A');
		s2 += '+';
		dekaf2::KProf prof("strcmp (10)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			if (!strcmp(s.c_str(), s2.c_str())) std::cout << "found";
		}
	}
	{
		dekaf2::KString s(10, 'A');
		dekaf2::KString s2(9, 'A');
		s2 += '+';
		dekaf2::KProf prof("strcasecmp (10)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			if (!strcasecmp(s.c_str(), s2.c_str())) std::cout << "found";
		}
	}
	{
		dekaf2::KString s(10, 'A');
		dekaf2::KString s2(9, 'A');
		s2 += '+';
		dekaf2::KProf prof("KString equal (10)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			if (s == s2) std::cout << "found";
		}
	}
	{
		dekaf2::KCaseString s(10, 'A');
		dekaf2::KString s2(9, 'A');
		s2 += '+';
		dekaf2::KProf prof("KCaseString equal (10)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			if (s == s2) std::cout << "found";
		}
	}
	{
		dekaf2::KCaseTrimString s(10, 'A');
		s.insert(0, "            \t");
		s += "    \r\n   ";
		dekaf2::KString s2(9, 'A');
		s2 += '+';
		dekaf2::KProf prof("KCaseTrimString equal (10)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			if (s == s2) std::cout << "found";
		}
	}
	{
		dekaf2::KString s(200, 'A');
		dekaf2::KString s2(199, 'A');
		s2 += '+';
		dekaf2::KProf prof("strcmp (200)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			if (!strcmp(s.c_str(), s2.c_str())) std::cout << "found";
		}
	}
	{
		dekaf2::KString s(200, 'A');
		dekaf2::KString s2(199, 'A');
		s2 += '+';
		dekaf2::KProf prof("strcasecmp (200)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			if (!strcasecmp(s.c_str(), s2.c_str())) std::cout << "found";
		}
	}
	{
		dekaf2::KString s(200, 'A');
		dekaf2::KString s2(199, 'A');
		s2 += '+';
		dekaf2::KProf prof("KString equal (200)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			if (s == s2) std::cout << "found";
		}
	}
	{
		dekaf2::KCaseString s(200, 'A');
		dekaf2::KString s2(199, 'A');
		s2 += '+';
		dekaf2::KProf prof("KCaseString equal (200)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			if (s == s2) std::cout << "found";
		}
	}
	{
		dekaf2::KCaseTrimString s(200, 'A');
		s.insert(0, "            \t");
		s += "    \r\n   ";
		dekaf2::KString s2(199, 'A');
		s2 += '+';
		dekaf2::KProf prof("KCaseTrimString equal (200)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			if (s == s2) std::cout << "found";
		}
	}
	{
		dekaf2::KString s(20000, 'A');
		dekaf2::KString s2(19999, 'A');
		s2 += '+';
		dekaf2::KProf prof("strcmp (20000)");
		prof.SetMultiplier(10000);
		for (int ct = 0; ct < 10000; ++ct)
		{
			if (!strcmp(s.c_str(), s2.c_str())) std::cout << "found";
		}
	}
	{
		dekaf2::KString s(20000, 'A');
		dekaf2::KString s2(19999, 'A');
		s2 += '+';
		dekaf2::KProf prof("strcasecmp (20000)");
		prof.SetMultiplier(10000);
		for (int ct = 0; ct < 10000; ++ct)
		{
			if (!strcasecmp(s.c_str(), s2.c_str())) std::cout << "found";
		}
	}
	{
		dekaf2::KString s(20000, 'A');
		dekaf2::KString s2(19999, 'A');
		s2 += '+';
		dekaf2::KProf prof("KString equal (20000)");
		prof.SetMultiplier(10000);
		for (int ct = 0; ct < 10000; ++ct)
		{
			if (s == s2) std::cout << "found";
		}
	}
	{
		dekaf2::KCaseString s(20000, 'A');
		dekaf2::KString s2(19999, 'A');
		s2 += '+';
		dekaf2::KProf prof("KCaseString equal (20000)");
		prof.SetMultiplier(10000);
		for (int ct = 0; ct < 10000; ++ct)
		{
			if (s == s2) std::cout << "found";
		}
	}
	{
		dekaf2::KCaseTrimString s(20000, 'A');
		s.insert(0, "            \t");
		s += "    \r\n   ";
		dekaf2::KString s2(19999, 'A');
		s2 += '+';
		dekaf2::KProf prof("KCaseTrimString equal (20000)");
		prof.SetMultiplier(10000);
		for (int ct = 0; ct < 10000; ++ct)
		{
			if (s == s2) std::cout << "found";
		}
	}

}

void kcasestring_bench()
{
	kcasestring();
}



