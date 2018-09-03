#include <iostream>
#include <cstring>
#include <dekaf2/kprof.h>
#include <dekaf2/kcasestring.h>

using namespace dekaf2;

void kcasestring()
{
	dekaf2::KProf ps("-KCaseString");
	
	{
		dekaf2::KString s(10, 'A');
		dekaf2::KString s2(9, 'A');
		s2 += '+';
		dekaf2::KProf prof("strcmp (10)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			KProf::Force(&s);
			if (!strcmp(s.c_str(), s2.c_str())) KProf::Force();
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
			KProf::Force(&s);
			if (!strcasecmp(s.c_str(), s2.c_str())) KProf::Force();
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
			KProf::Force(&s);
			if (s == s2) KProf::Force();
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
			KProf::Force(&s);
			if (s == s2) KProf::Force();
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
			KProf::Force(&s);
			if (s == s2) KProf::Force();
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
			KProf::Force(&s);
			if (!strcmp(s.c_str(), s2.c_str())) KProf::Force();
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
			KProf::Force(&s);
			if (!strcasecmp(s.c_str(), s2.c_str())) KProf::Force();
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
			KProf::Force(&s);
			if (s == s2) KProf::Force();
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
			KProf::Force(&s);
			if (s == s2) KProf::Force();
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
			KProf::Force(&s);
			if (s == s2) KProf::Force();
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
			KProf::Force(&s);
			if (!strcmp(s.c_str(), s2.c_str())) KProf::Force();
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
			KProf::Force(&s);
			if (!strcasecmp(s.c_str(), s2.c_str())) KProf::Force();
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
			KProf::Force(&s);
			if (s == s2) KProf::Force();
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
			KProf::Force(&s);
			if (s == s2) KProf::Force();
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
			KProf::Force(&s);
			if (s == s2) KProf::Force();
		}
	}

}

void kcasestring_bench()
{
	kcasestring();
}



