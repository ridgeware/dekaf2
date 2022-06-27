
#include <memory>
#include <array>
#include <iostream>
#include <vector>
#include <cinttypes>
#include <atomic>
#include <thread>
#include <dekaf2/kctype.h>
#include <dekaf2/kprof.h>
#include <dekaf2/kstring.h>
#include <dekaf2/kstringview.h>
#include <dekaf2/kstringutils.h>
#include <dekaf2/dekaf2.h>

using namespace dekaf2;

enum { MAX_STORE = 10*1024*1024 };

void custom_alloc()
{
	static std::array<char, MAX_STORE> store;

	dekaf2::KProf ps("-Custom Alloc");

	char* pos = store.data();

	dekaf2::KProf prof("1024 (0)");
	prof.SetMultiplier(1024*1024);

	for (size_t count = 0; count < 1024*1024; ++count)
	{
		dekaf2::KProf::Force(pos);

		if (pos == store.data())
		{
			dekaf2::KProf::Force(pos);
		}

		dekaf2::KProf::Force(pos);

		if (pos - store.data() > 1024*1024)
		{
			dekaf2::KProf::Force(pos);
		}

		char* p = pos;
		pos += 1024;
		dekaf2::KProf::Force(p);
		dekaf2::KProf::Force(p);
	}
}

void std_alloc()
{
	dekaf2::KProf ps("-Std Alloc");

	dekaf2::KProf prof("1024 (1)");
	prof.SetMultiplier(1024*1024);

	for (size_t count = 0; count < 1024*1024; ++count)
	{
		char* p = static_cast<char*>(operator new(1024));
		dekaf2::KProf::Force(p);
		//		operator delete(p);
		dekaf2::KProf::Force(p);
	}
}

void regex1()
{
	dekaf2::KProf ps("-Regex1");
	{
		dekaf2::KString sInit = "-abce";
		dekaf2::KProf::Force(&sInit);
		sInit.ReplaceRegex("^[ \f\n\r\t\v\b-]+", ""); // remove leading whitespace and dashes
		dekaf2::KProf::Force(&sInit);
	}
	{
		dekaf2::KProf prof("no remove (1)");
		prof.SetMultiplier(1024*1024);

		dekaf2::KString sInit { "sdif" };

		for (size_t count = 0; count < 1024*1024; ++count)
		{
			dekaf2::KString sFirstPart{ sInit };
			dekaf2::KProf::Force(&sFirstPart[0]);
			if (dekaf2::kIsSpace(sFirstPart[0]) || ('-' == sFirstPart[0])) { // < ====== here
				sFirstPart.ReplaceRegex("^[ \f\n\r\t\v\b-]+", ""); // remove leading whitespace and dashes
			}
			dekaf2::KProf::Force(&sFirstPart);
		}
	}

	{
		dekaf2::KProf prof("remove (1)");
		prof.SetMultiplier(1024*1024);

		dekaf2::KString sInit { "- sdif" };

		for (size_t count = 0; count < 1024*1024; ++count)
		{
			dekaf2::KString sFirstPart{ sInit };
			dekaf2::KProf::Force(&sFirstPart[0]);
			if (dekaf2::kIsSpace(sFirstPart[0]) || ('-' == sFirstPart[0])) { // < ====== here
				dekaf2::KProf::Force();
				sFirstPart.ReplaceRegex("^[ \f\n\r\t\v\b-]+", ""); // remove leading whitespace and dashes
			}
			dekaf2::KProf::Force(&sFirstPart);
		}
	}
}

void regex2()
{
	dekaf2::KProf ps("-Regex2");
	{
		dekaf2::KString sInit = "-abce";
		dekaf2::KProf::Force(&sInit);
		sInit.ReplaceRegex("^[ \f\n\r\t\v\b-]+", ""); // remove leading whitespace and dashes
		dekaf2::KProf::Force(&sInit);
	}
	{
		dekaf2::KProf prof("no remove (2)");
		prof.SetMultiplier(1024*1024);

		dekaf2::KString sInit { "sdif" };

		for (size_t count = 0; count < 1024*1024; ++count)
		{
			dekaf2::KString sFirstPart{ sInit };
			dekaf2::KProf::Force(&sFirstPart[0]); // < ====== not here
			sFirstPart.ReplaceRegex("^[ \f\n\r\t\v\b-]+", ""); // remove leading whitespace and dashes
			dekaf2::KProf::Force(&sFirstPart);
		}
	}

	{
		dekaf2::KProf prof("remove (2)");
		prof.SetMultiplier(1024*1024);

		dekaf2::KString sInit { "- sdif" };

		for (size_t count = 0; count < 1024*1024; ++count)
		{
			dekaf2::KString sFirstPart{ sInit };
			dekaf2::KProf::Force(&sFirstPart[0]); // < ====== not here
			sFirstPart.ReplaceRegex("^[ \f\n\r\t\v\b-]+", ""); // remove leading whitespace and dashes
			dekaf2::KProf::Force(&sFirstPart);
		}
	}
}

void regex3()
{
	dekaf2::KProf ps("-Regex3");
	{
		dekaf2::KProf prof("no remove (3)");
		prof.SetMultiplier(1024*1024);

		dekaf2::KString sInit { "sdif" };

		for (size_t count = 0; count < 1024*1024; ++count)
		{
			dekaf2::KString sFirstPart{ sInit };
			dekaf2::KProf::Force(&sFirstPart[0]);
			if (dekaf2::kIsSpace(sFirstPart[0]) || ('-' == sFirstPart[0])) {
				sFirstPart.TrimLeft(" \f\n\r\t\v\b-"_ksv);
			}
			dekaf2::KProf::Force(&sFirstPart);
		}
	}

	{
		dekaf2::KProf prof("remove (3)");
		prof.SetMultiplier(1024*1024);

		dekaf2::KString sInit { "- sdif" };

		for (size_t count = 0; count < 1024*1024; ++count)
		{
			dekaf2::KString sFirstPart{ sInit };
			dekaf2::KProf::Force(&sFirstPart[0]);
			if (dekaf2::kIsSpace(sFirstPart[0]) || ('-' == sFirstPart[0])) {
				sFirstPart.TrimLeft(" \f\n\r\t\v\b-"_ksv);
			}
			dekaf2::KProf::Force(&sFirstPart);
		}
	}
}

void regex4()
{
	dekaf2::KProf ps("-Regex4");
	{
		dekaf2::KProf prof("no remove (4)");
		prof.SetMultiplier(1024*1024);

		dekaf2::KString sInit { "sdif" };

		for (size_t count = 0; count < 1024*1024; ++count)
		{
			dekaf2::KString sFirstPart{ sInit };
			dekaf2::KProf::Force(&sFirstPart[0]);
			sFirstPart.TrimLeft();
			dekaf2::KProf::Force(&sFirstPart);
		}
	}

	{
		dekaf2::KProf prof("remove (4)");
		prof.SetMultiplier(1024*1024);

		dekaf2::KString sInit { "\t sdif" };

		for (size_t count = 0; count < 1024*1024; ++count)
		{
			dekaf2::KString sFirstPart{ sInit };
			dekaf2::KProf::Force(&sFirstPart[0]);
			sFirstPart.TrimLeft();
			dekaf2::KProf::Force(&sFirstPart);
		}
	}
}

void regex5()
{
	dekaf2::KProf ps("-Regex5");
	{
		dekaf2::KProf prof("no remove (5)");
		prof.SetMultiplier(1024*1024);

		dekaf2::KString sInit { "sdif" };

		for (size_t count = 0; count < 1024*1024; ++count)
		{
			dekaf2::KString sFirstPart{ sInit };
			dekaf2::KProf::Force(&sFirstPart[0]);
			sFirstPart.CollapseAndTrim(" \f\n\r\t\v\b", ' ');
			dekaf2::KProf::Force(&sFirstPart);
		}
	}

	{
		dekaf2::KProf prof("remove (5)");
		prof.SetMultiplier(1024*1024);

		dekaf2::KString sInit { "\t sdif" };

		for (size_t count = 0; count < 1024*1024; ++count)
		{
			dekaf2::KString sFirstPart{ sInit };
			dekaf2::KProf::Force(&sFirstPart[0]);
			sFirstPart.CollapseAndTrim(" \f\n\r\t\v\b", ' ');
			dekaf2::KProf::Force(&sFirstPart);
		}
	}
}

void regex6()
{
	dekaf2::KProf ps("-Regex6");
	{
		dekaf2::KProf prof("no remove (6)");
		prof.SetMultiplier(1024*1024);

		dekaf2::KString sInit { "sdif" };

		for (size_t count = 0; count < 1024*1024; ++count)
		{
			dekaf2::KString sFirstPart{ sInit };
			dekaf2::KProf::Force(&sFirstPart[0]);
			sFirstPart.CollapseAndTrim();
			dekaf2::KProf::Force(&sFirstPart);
		}
	}

	{
		dekaf2::KProf prof("remove (6)");
		prof.SetMultiplier(1024*1024);

		dekaf2::KString sInit { "\t sdif" };

		for (size_t count = 0; count < 1024*1024; ++count)
		{
			dekaf2::KString sFirstPart{ sInit };
			dekaf2::KProf::Force(&sFirstPart[0]);
			sFirstPart.CollapseAndTrim();
			dekaf2::KProf::Force(&sFirstPart);
		}
	}
}

void collapse()
{
	dekaf2::KProf ps("-Collapse");
	{
		dekaf2::KProf prof("collapse (1)");
		prof.SetMultiplier(1024*1024);

		dekaf2::KString sInit { "  sdifsdflkjlas eljdfkhiuk \t jb" };

		for (size_t count = 0; count < 1024*1024; ++count)
		{
			dekaf2::KString sFirstPart{ sInit };
			dekaf2::KProf::Force(&sFirstPart[0]);
			sFirstPart.ReplaceRegex("[ \f\n\r\t\v\b]+", " ");
			dekaf2::KProf::Force(&sFirstPart);
		}
	}

	{
		dekaf2::KProf prof("collapse (2)");
		prof.SetMultiplier(1024*1024);

		dekaf2::KString sInit { "  sdifsdflkjlas eljdfkhiuk \t jb" };

		for (size_t count = 0; count < 1024*1024; ++count)
		{
			dekaf2::KString sFirstPart{ sInit };
			dekaf2::KProf::Force(&sFirstPart[0]);
			sFirstPart.Collapse(" \f\n\r\t\v\b", ' ');
			dekaf2::KProf::Force(&sFirstPart);
		}
	}

	{
		dekaf2::KProf prof("collapse (3)");
		prof.SetMultiplier(1024*1024);

		dekaf2::KString sInit { "  sdifsdflkjlas eljdfkhiuk \t jb" };

		for (size_t count = 0; count < 1024*1024; ++count)
		{
			dekaf2::KString sFirstPart{ sInit };
			dekaf2::KProf::Force(&sFirstPart[0]);
			sFirstPart.Collapse();
			dekaf2::KProf::Force(&sFirstPart);
		}
	}
}

void trim()
{
	dekaf2::KProf ps("-Trim");
	{
		dekaf2::KProf prof("trimLeft (1)");
		prof.SetMultiplier(1024*1024);

		dekaf2::KString sInit { "      \t\n sdifsdflkjlas" };

		for (size_t count = 0; count < 1024*1024; ++count)
		{
			dekaf2::KString sFirstPart{ sInit };
			dekaf2::KProf::Force(&sFirstPart[0]);
			kTrimLeft(sFirstPart);
			dekaf2::KProf::Force(&sFirstPart);
		}
	}

	{
		dekaf2::KProf prof("trimLeft (2)");
		prof.SetMultiplier(1024*1024);

		dekaf2::KString sInit { "      \t\n sdifsdflkjlas" };

		for (size_t count = 0; count < 1024*1024; ++count)
		{
			dekaf2::KString sFirstPart{ sInit };
			dekaf2::KProf::Force(&sFirstPart[0]);
			kTrimLeft(sFirstPart, " \f\n\r\t\v\b");
			dekaf2::KProf::Force(&sFirstPart);
		}
	}

	{
		dekaf2::KProf prof("trimLeft (3)");
		prof.SetMultiplier(1024*1024);

		dekaf2::KString sInit { "      \t\n sdifsdflkjlas" };

		for (size_t count = 0; count < 1024*1024; ++count)
		{
			dekaf2::KString sFirstPart{ sInit };
			dekaf2::KProf::Force(&sFirstPart[0]);
			kTrimLeft(sFirstPart, [](char ch)
			{
				return std::isspace(ch);
			});
			dekaf2::KProf::Force(&sFirstPart);
		}
	}

	{
		dekaf2::KProf prof("trimRight (1)");
		prof.SetMultiplier(1024*1024);

		dekaf2::KString sInit { "sdifsdflkjlas      \t\n " };

		for (size_t count = 0; count < 1024*1024; ++count)
		{
			dekaf2::KString sFirstPart{ sInit };
			dekaf2::KProf::Force(&sFirstPart[0]);
			kTrimRight(sFirstPart);
			dekaf2::KProf::Force(&sFirstPart);
		}
	}

	{
		dekaf2::KProf prof("trimRight (2)");
		prof.SetMultiplier(1024*1024);

		dekaf2::KString sInit { "sdifsdflkjlas      \t\n " };

		for (size_t count = 0; count < 1024*1024; ++count)
		{
			dekaf2::KString sFirstPart{ sInit };
			dekaf2::KProf::Force(&sFirstPart[0]);
			kTrimRight(sFirstPart, " \f\n\r\t\v\b");
			dekaf2::KProf::Force(&sFirstPart);
		}
	}

	{
		dekaf2::KProf prof("trimRight (3)");
		prof.SetMultiplier(1024*1024);

		dekaf2::KString sInit { "sdifsdflkjlas      \t\n " };

		for (size_t count = 0; count < 1024*1024; ++count)
		{
			dekaf2::KString sFirstPart{ sInit };
			dekaf2::KProf::Force(&sFirstPart[0]);
			kTrimRight(sFirstPart, [](char ch)
			{
				return std::isspace(ch);
			});
			dekaf2::KProf::Force(&sFirstPart);
		}
	}
}

void dtime()
{
	dekaf2::KProf ps("-Time");
	{
		dekaf2::KProf prof("time(nullptr)");
		prof.SetMultiplier(1024*1024);

		for (size_t count = 0; count < 1024*1024; ++count)
		{
			time_t a = time(nullptr);
			dekaf2::KProf::Force(&a);
		}
	}

	{
		dekaf2::KProf prof("Dekaf().GetCurrentTime()");
		prof.SetMultiplier(1024*1024);

		for (size_t count = 0; count < 1024*1024; ++count)
		{
			time_t a = Dekaf::getInstance().GetCurrentTime();
			dekaf2::KProf::Force(&a);
		}
	}
}

using var_t = uint32_t;

var_t non_atomic_var { 0 };
std::atomic<var_t> atomic_var { 0 };
static volatile bool run_now { false };

void atomic0(const char* label)
{
	while (!run_now) {};
	dekaf2::KProf prof(label);
	prof.SetMultiplier(1024*1024);

	for (size_t count = 0; count < 1024*1024; ++count)
	{
		KProf::Force();
	}
}

void atomic1(const char* label)
{
	while (!run_now) {};
	dekaf2::KProf prof(label);
	prof.SetMultiplier(1024*1024);

	for (size_t count = 0; count < 1024*1024; ++count)
	{
		KProf::Force(&non_atomic_var);
		var_t a = non_atomic_var;
		dekaf2::KProf::Force(&a);
	}
}

void atomic2(const char* label)
{
	while (!run_now) {};
	dekaf2::KProf prof(label);
	prof.SetMultiplier(1024*1024);

	for (size_t count = 0; count < 1024*1024; ++count)
	{
		KProf::Force(&atomic_var);
		var_t a = atomic_var;
		dekaf2::KProf::Force(&a);
	}
}

void atomic3(const char* label)
{
	while (!run_now) {};
	dekaf2::KProf prof(label);
	prof.SetMultiplier(1024*1024);

	var_t a = 0;

	for (size_t count = 0; count < 1024*1024; ++count)
	{
		KProf::Force(&a);
		non_atomic_var = a;
		dekaf2::KProf::Force(&non_atomic_var);
	}
}

void atomic4(const char* label)
{
	while (!run_now) {};
	dekaf2::KProf prof(label);
	prof.SetMultiplier(1024*1024);

	var_t a = 0;

	for (size_t count = 0; count < 1024*1024; ++count)
	{
		KProf::Force(&a);
		atomic_var = a;
		dekaf2::KProf::Force(&atomic_var);
	}
}

void atomic()
{
	KProf prof("-Atomic Single Thread");
	run_now = true;
	atomic0("empty loop");
	atomic1("non-atomic read");
	atomic2("atomic read");
	atomic3("non-atomic write");
	atomic4("atomic write");
}

void threads()
{
	static size_t n_threads { 4 };
	static KString sLabel { kFormat("-Threads: {}", n_threads) };
	dekaf2::KProf prof(sLabel.c_str());
	prof.SetMultiplier(10);

	for (size_t count = 0; count < 10; ++count)
	{
		{
			KProf prof("thread start");
			prof.SetMultiplier(n_threads);

			run_now = false;
			for (size_t ct = 0; ct < n_threads; ++ct)
			{
				std::thread(atomic0, "mt empty loop").detach();
			}
			run_now = true;
		}
		{
			std::vector<std::thread> threads;

			run_now = false;
			for (size_t ct = 0; ct < n_threads; ++ct)
			{
				threads.push_back(std::thread(atomic1, "mt non-atomic read"));
			}
			run_now = true;
			for (size_t ct = 0; ct < n_threads; ++ct)
			{
				threads[ct].join();
			}
		}
		{
			std::vector<std::thread> threads;

			run_now = false;
			for (size_t ct = 0; ct < n_threads; ++ct)
			{
				threads.push_back(std::thread(atomic2, "mt atomic read"));
			}
			run_now = true;
			for (size_t ct = 0; ct < n_threads; ++ct)
			{
				threads[ct].join();
			}
		}
		{
			std::vector<std::thread> threads;

			run_now = false;
			for (size_t ct = 0; ct < n_threads; ++ct)
			{
				threads.push_back(std::thread(atomic3, "mt non-atomic write"));
			}
			run_now = true;
			for (size_t ct = 0; ct < n_threads; ++ct)
			{
				threads[ct].join();
			}
		}
		{
			std::vector<std::thread> threads;

			run_now = false;
			for (size_t ct = 0; ct < n_threads; ++ct)
			{
				threads.push_back(std::thread(atomic4, "mt atomic write"));
			}
			run_now = true;
			for (size_t ct = 0; ct < n_threads; ++ct)
			{
				threads[ct].join();
			}
		}
	}
}

void other_bench()
{
	custom_alloc();
	std_alloc();
	regex1();
	regex2();
	regex3();
	regex4();
	regex5();
	regex6();
	collapse();
	trim();
	dtime();
	atomic();
	threads();
}


