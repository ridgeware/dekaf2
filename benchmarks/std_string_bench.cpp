#include <sstream>
#include <iostream>
#include <vector>
#include <map>
#include <set>
#include <cinttypes>
#include <dekaf2/kprof.h>

static const char* str0 = "";
static const char* str15 = "123456789012345";
static const char* str23 = "12345678901234567890123";
static const char* str250 = "12345678901234567890123456789012345678901234567890"
		"12345678901234567890123456789012345678901234567890"
		"12345678901234567890123456789012345678901234567890"
		"12345678901234567890123456789012345678901234567890"
		"12345678901234567890123456789012345678901234567890";

void std_string()
{
	dekaf2::KProf ps("-std::string");
	
	{
		dekaf2::KProf prof("std short (0)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			std::string str(str0);
			if (str.size() > 100000) std::cout << "large";
		}
	}
	{
		dekaf2::KProf prof("std short (15)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			std::string str(str15);
			if (str.size() > 100000) std::cout << "large";
		}
	}
	{
		dekaf2::KProf prof("std short (23)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			std::string str(str23);
			if (str.size() > 100000) std::cout << "large";
		}
	}
	{
		dekaf2::KProf prof("std medium (250)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			std::string str(str250);
			if (str.size() > 100000) std::cout << "large";
		}
	}
	{
		char buffer[5001];
		memset(buffer, '-', 5000);
		buffer[5000] = 0;
		dekaf2::KProf prof("std large (5000)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			std::string str(buffer);
			if (str.size() > 100000) std::cout << "large";
		}
	}
	{
		std::string s(200, '-');
		s.append("abcdefg");
		dekaf2::KProf prof("std find (200)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			if (s.find("abcdefg") < 10) std::cout << "found";
		}
	}
	{
		std::string s(200, '-');
		dekaf2::KProf prof("std notfound (200)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			if (s.find("abcdefg") < 10) std::cout << "found";
		}
	}
	{
		std::string s(5000, '-');
		s.append("abcdefg");
		dekaf2::KProf prof("std find (5000)");
		prof.SetMultiplier(100000);
		for (int ct = 0; ct < 100000; ++ct)
		{
			if (s.find("abcdefg") < 10) std::cout << "found";
		}
	}
	{
		std::string s(5000, '-');
		dekaf2::KProf prof("std notfound (5000)");
		prof.SetMultiplier(100000);
		for (int ct = 0; ct < 100000; ++ct)
		{
			if (s.find("abcdefg") < 10) std::cout << "found";
		}
	}
	{
		std::string s(5000000, '-');
		s.append("abcdefg");
		dekaf2::KProf prof("std find (5M)");
		prof.SetMultiplier(1000);
		for (int ct = 0; ct < 1000; ++ct)
		{
			if (s.find("abcdefg") < 10) std::cout << "found";
		}
	}
	{
		std::string s(5000, 'a');
		s.append("abcdefg");
		dekaf2::KProf prof("std find worst case");
		prof.SetMultiplier(100000);
		for (int ct = 0; ct < 100000; ++ct)
		{
			if (s.find("abcdefg") < 10) std::cout << "found";
		}
	}
	{
		std::string s(200, '-');
		s.append("abcdefg");
		dekaf2::KProf prof("std find char (200)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			if (s.find('d') < 100) std::cout << "found";

		}
	}
	{
		std::string s(5000, '-');
		s.append("abcdefg");
		dekaf2::KProf prof("std find char (5000)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			if (s.find('d') < 100) std::cout << "found";
		}
	}
	{
		std::string s(5000000, '-');
		s.append("abcdefg");
		dekaf2::KProf prof("std find char (5M)");
		prof.SetMultiplier(1000);
		for (int ct = 0; ct < 1000; ++ct)
		{
			if (s.find('d') < 100) std::cout << "found";
		}
	}
	{
		std::string s(0, '-');
		dekaf2::KProf prof("std += short (0)");
		prof.SetMultiplier(100000);
		for (int ct = 0; ct < 100000; ++ct)
		{
			s += '-';
			s.pop_back();
		}
	}
	{
		std::string s(14, '-');
		dekaf2::KProf prof("std += short (15)");
		prof.SetMultiplier(100000);
		for (int ct = 0; ct < 100000; ++ct)
		{
			s += '-';
			s.pop_back();
		}
	}
	{
		std::string s(22, '-');
		dekaf2::KProf prof("std += short (23)");
		prof.SetMultiplier(100000);
		for (int ct = 0; ct < 100000; ++ct)
		{
			s += '-';
			s.pop_back();
		}
	}
	{
		std::string s(249, '-');
		dekaf2::KProf prof("std += medium (250)");
		prof.SetMultiplier(100000);
		for (int ct = 0; ct < 100000; ++ct)
		{
			s += '-';
			s.pop_back();
		}
	}
	{
		std::string s(5000, '-');
		dekaf2::KProf prof("std += large (5000)");
		prof.SetMultiplier(100000);
		for (int ct = 0; ct < 100000; ++ct)
		{
			s += '-';
			s.pop_back();
		}
	}
	{
		std::string s(20, '-');
		s.append("abcdefg");
		dekaf2::KProf prof("std find_first_of (20)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			if (s.find_first_of("defg") < 10) std::cout << "found";
		}
	}
	{
		std::string s(200, '-');
		s.append("abcdefg");
		dekaf2::KProf prof("std find_first_of (200)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			if (s.find_first_of("defg") < 100) std::cout << "found";
		}
	}
	{
		std::string s(5000, '-');
		s.append("abcdefg");
		dekaf2::KProf prof("std find_first_of (5000)");
		prof.SetMultiplier(100000);
		for (int ct = 0; ct < 100000; ++ct)
		{
			if (s.find_first_of("defg") < 100) std::cout << "found";
		}
	}
	{
		std::string s(5000000, '-');
		s.append("abcdefg");
		dekaf2::KProf prof("std find_first_of (5M)");
		prof.SetMultiplier(1000);
		for (int ct = 0; ct < 1000; ++ct)
		{
			if (s.find_first_of("defg") < 100) std::cout << "found";
		}
	}	{
		std::string s(20, '-');
		s.append("abcdefg");
		dekaf2::KProf prof("std find_first_not_of (20)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			if (s.find_first_not_of("&%-?") < 10) std::cout << "found";
		}
	}
	{
		std::string s(200, '-');
		s.append("abcdefg");
		dekaf2::KProf prof("std find_first_not_of (200)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			if (s.find_first_not_of("&%-?") < 10) std::cout << "found";
		}
	}
	{
		std::string s(5000, '-');
		s.append("abcdefg");
		dekaf2::KProf prof("std find_first_not_of (5000)");
		prof.SetMultiplier(100000);
		for (int ct = 0; ct < 100000; ++ct)
		{
			if (s.find_first_not_of("&%-?") < 10) std::cout << "found";
		}
	}
	{
		std::string s(5000000, '-');
		s.append("abcdefg");
		dekaf2::KProf prof("std find_first_not_of (5M)");
		prof.SetMultiplier(1000);
		for (int ct = 0; ct < 1000; ++ct)
		{
			if (s.find_first_not_of("&%-?") < 100) std::cout << "found";
		}
	}
	{
		std::string s(200, '-');
		s.insert(0, "abcdefg");
		dekaf2::KProf prof("std rfind (200)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			if (s.rfind("abcdefg") > 100) std::cout << "found";
		}
	}
	{
		std::string s(5000, '-');
		s.insert(0, "abcdefg");
		dekaf2::KProf prof("std rfind (5000)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			if (s.rfind("abcdefg") > 100) std::cout << "found";
		}
	}
	{
		std::string s(5000000, '-');
		s.insert(0, "abcdefg");
		dekaf2::KProf prof("std rfind (5M)");
		prof.SetMultiplier(1000);
		for (int ct = 0; ct < 1000; ++ct)
		{
			if (s.rfind("abcdefg") > 100) std::cout << "found";
		}
	}
	{
		std::string s(200, '-');
		s.insert(0, "abcdefg");
		dekaf2::KProf prof("std rfind char (200)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			if (s.rfind('d') > 100) std::cout << "found";
		}
	}
	{
		std::string s(5000, '-');
		s.insert(0, "abcdefg");
		dekaf2::KProf prof("std rfind char (5000)");
		prof.SetMultiplier(1000000);
		for (int ct = 0; ct < 1000000; ++ct)
		{
			if (s.rfind('d') > 100) std::cout << "found";
		}
	}
	{
		std::string s(5000000, '-');
		s.insert(0, "abcdefg");
		dekaf2::KProf prof("std rfind char (5M)");
		prof.SetMultiplier(1000);
		for (int ct = 0; ct < 1000; ++ct)
		{
			if (s.rfind('d') > 100) std::cout << "found";
		}
	}
}

void std_string_bench()
{
	std_string();
}


