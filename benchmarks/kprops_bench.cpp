#include <sstream>
#include <iostream>
#include <vector>
#include <map>
#include <set>
#include <cinttypes>
#include <dekaf2/kprof.h>
#include <dekaf2/kstring.h>
#include <dekaf2/kstringutils.h>
#include <dekaf2/kprops.h>
#include <dekaf2/kurlencode.h>

using namespace std;
using namespace dekaf2;

size_t test_thread_local()
{
	thread_local static size_t counter{0};
	return ++counter;
}

size_t test_no_thread_local()
{
	static size_t counter{0};
	return ++counter;
}

void test()
{
	std::string string("124.45.236.32");
	std::wstring wstring(L"124.45.236.32");
	std::vector<uint32_t> vector{'1','2','4','.','4','5','.','2','3','6','.','3','2'};

	{
		KProf pp("strings");
		for (uint32_t ct = 0; ct < 1000000; ++ct)
		{
			if (kCountChar(string, ':') > 4) return;
		}
	}
	{
		KProf pp("wstrings");
		for (uint32_t ct = 0; ct < 1000000; ++ct)
		{
			if (kCountChar(wstring, ':') > 4) return;
		}
	}
	{
		KProf pp("vectors");
		for (uint32_t ct = 0; ct < 1000000; ++ct)
		{
			if (kCountChar(vector, ':') > 4) return;
		}
	}
	{
		KProf pp("char*");
		for (uint32_t ct = 0; ct < 1000000; ++ct)
		{
			if (kCountChar(string.c_str(), ':') > 4) return;
		}
	}
}

template <class KProps>
void run_kprops_bench(KProps&& kprops, const char* label1, const char* label2)
{
	KString value("value");
	KString key("key");
	KString key2;
	KString value2;
	for (uint32_t ii = 0; ii < 100; ++ii)
	{
		for (uint32_t ct = 0; ct < 1000; ++ct)
		{
			key2.Format("{} {}", key, ct);
			value2.Format("{} {}", value, ct);
			KProf pp(label1);
			kprops.Add(std::move(key2), std::move(value));
			pp.stop();
		}
		for (uint32_t ct = 0; ct < 1000; ++ct)
		{
			key2.Format("{} {}", key, ct);
			KProf pp(label2);
			kprops.Get(key2);
			pp.stop();
		}
		kprops.clear();
	}
}

template <class KProps>
void run_kprops_bench_nomove(KProps&& kprops, const char* label1, const char* label2)
{
	KString value("value");
	KString key("key");
	KString key2;
	KString value2;
	for (uint32_t ii = 0; ii < 100; ++ii)
	{
		for (uint32_t ct = 0; ct < 1000; ++ct)
		{
			key2.Format("{} {}", key, ct);
			value2.Format("{} {}", value, ct);
			KProf pp(label1);
			kprops.Add(key2, value);
			pp.stop();
		}
		for (uint32_t ct = 0; ct < 1000; ++ct)
		{
			key2.Format("{} {}", key, ct);
			KProf pp(label2);
			kprops.Get(key2);
			pp.stop();
		}
		kprops.clear();
	}
}

void kprops()
{
	{
		KProf p("-KProps");
		{
			KProf p("-non-sequential, unique");
			KProps<KString, KString, false, true> kprops;
			run_kprops_bench(std::move(kprops), "KProps.Add (1)", "KProps.Get (1)");
		}
		{
			KProf p("-non-sequential, non-unique");
			KProps<KString, KString, false, false> kprops;
			run_kprops_bench(std::move(kprops), "KProps.Add (2)", "KProps.Get (2)");
		}
		{
			KProf p("-sequential, unique");
			KProps<KString, KString, true, true> kprops;
			run_kprops_bench(std::move(kprops), "KProps.Add (3)", "KProps.Get (3)");
		}
		{
			KProf p("-sequential, non-unique");
			KProps<KString, KString, true, false> kprops;
			run_kprops_bench(std::move(kprops), "KProps.Add (4)", "KProps.Get (4)");
		}
	}

	{
		KProf p2("-KProps no move");
		{
			KProf p("-non-sequential, unique 3");
			KProps<KString, KString, false, true> kprops;
			run_kprops_bench_nomove(std::move(kprops), "KProps.Add (5)", "KProps.Get (5)");
		}
		{
			KProf p("-non-sequential, non-unique 3");
			KProps<KString, KString, false, false> kprops;
			run_kprops_bench_nomove(std::move(kprops), "KProps.Add (6)", "KProps.Get (6)");
		}
		{
			KProf p("-sequential, unique 3");
			KProps<KString, KString, true, true> kprops;
			run_kprops_bench_nomove(std::move(kprops), "KProps.Add (7)", "KProps.Get (7)");
		}
		{
			KProf p("-sequential, non-unique 3");
			KProps<KString, KString, true, false> kprops;
			run_kprops_bench_nomove(std::move(kprops), "KProps.Add (8)", "KProps.Get (8)");
		}
	}
}


void thread_local_bench()
{
	size_t result;
	{
		KProf p("thread local");
		for (uint32_t ct = 0; ct < 1000000; ++ct)
		{
			test_thread_local();
		}
		result = test_thread_local();
	}
	fprintf(stdout, "result 1 is %lu\n", result);
	{
		KProf p("no thread local");
		for (uint32_t ct = 0; ct < 1000000; ++ct)
		{
			test_no_thread_local();
		}
		result = test_no_thread_local();
	}
	fprintf(stdout, "result 2 is %lu\n", result);
}

void UrlDecode_bench()
{
	{
		const KString encoded("fskfaghskdvbjvfmlksfjsdjdbfjskdh%31%3aaex.x,mfbmn,dfmn;dofioighhlijf;lijgldbnnibsvshviusvhiivsgi%21vyuskvhfv%");
		KString s1;
		KProf p("-kUrlDecode");
		for (uint32_t ct = 0; ct < 100000; ++ct)
		{
			s1 = encoded;
			KProf pp("kUrlDecode");
			kUrlDecode(s1);
			pp.stop();
		}
		if (s1 != "fskfaghskdvbjvfmlksfjsdjdbfjskdh1:aex.x,mfbmn,dfmn;dofioighhlijf;lijgldbnnibsvshviusvhiivsgi!vyuskvhfv%")
		{
			fprintf(stdout, "urldecode generates wrong result: %s\n", s1.c_str());
		}
	}
}

void kprops_bench()
{
//	thread_local_bench();
	kprops();
	UrlDecode_bench();
}


