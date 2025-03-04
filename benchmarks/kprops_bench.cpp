#include <sstream>
#include <iostream>
#include <vector>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <cinttypes>
#include <dekaf2/kprof.h>
#include <dekaf2/kstring.h>
#include <dekaf2/kstringutils.h>
#include <dekaf2/kprops.h>
#include <dekaf2/kurlencode.h>
#include <dekaf2/kassociative.h>

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
			KProf::Force(&string);
			if (kCountChar(string, ':') > 4) KProf::Force();
		}
	}
	{
		KProf pp("wstrings");
		for (uint32_t ct = 0; ct < 1000000; ++ct)
		{
			KProf::Force(&wstring);
			if (kCountChar(wstring, ':') > 4) KProf::Force();
		}
	}
	{
		KProf pp("vectors");
		for (uint32_t ct = 0; ct < 1000000; ++ct)
		{
			KProf::Force(&vector);
			if (kCountChar(vector, ':') > 4) KProf::Force();
		}
	}
	{
		KProf pp("char*");
		for (uint32_t ct = 0; ct < 1000000; ++ct)
		{
			KProf::Force(&string);
			if (kCountChar(string.c_str(), ':') > 4) KProf::Force();
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
			KProf::Force(&ct);
			key2.Format("{} {}", key, ct);
			value2.Format("{} {}", value, ct);
			KProf pp(label1);
			kprops.Add(std::move(key2), std::move(value));
			pp.stop();
			KProf::Force();
		}
		for (uint32_t ct = 0; ct < 1000; ++ct)
		{
			KProf::Force(&ct);
			key2.Format("{} {}", key, ct);
			KProf pp(label2);
			auto val = kprops.Get(key2);
			pp.stop();
			KProf::Force();
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
			KProf::Force(&ct);
			key2.Format("{} {}", key, ct);
			value2.Format("{} {}", value, ct);
			KProf pp(label1);
			kprops.Add(key2, value);
			pp.stop();
			KProf::Force();
		}
		for (uint32_t ct = 0; ct < 1000; ++ct)
		{
			KProf::Force(&ct);
			key2.Format("{} {}", key, ct);
			KProf pp(label2);
			auto val = kprops.Get(key2);
			pp.stop();
			KProf::Force();
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
			KProf::Force(&ct);
			test_thread_local();
		}
		result = test_thread_local();
	}
#ifdef DEKAF2_IS_64_BITS
	fprintf(stdout, "result 1 is %lu\n", result);
#else
	fprintf(stdout, "result 1 is %u\n", result);
#endif
	{
		KProf p("no thread local");
		for (uint32_t ct = 0; ct < 1000000; ++ct)
		{
			KProf::Force(&ct);
			test_no_thread_local();
		}
		result = test_no_thread_local();
	}
#ifdef DEKAF2_IS_64_BITS
	fprintf(stdout, "result 2 is %lu\n", result);
#else
	fprintf(stdout, "result 2 is %u\n", result);
#endif
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
			KProf::Force(&s1);
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

void sets()
{
	{
		KProf ps("-std::set");
		{
			std::set<std::string> map;

			{
				KProf p("set.emplace(std::string)");
				p.SetMultiplier(10000);
				for (uint32_t i = 0; i < 10000; i++)
				{
					KProf::Force(&map);
					map.emplace(std::to_string(i));
					KProf::Force();
				}
			}
			{
				std::string sFind("876");
				KProf p("set.find(std::string)");
				p.SetMultiplier(100000);
				for (uint32_t i = 0; i < 100000; i++)
				{
					KProf::Force(&map);
					if (map.find(sFind) == map.end())
					{
						break;
					}
					KProf::Force();
				}
			}
			{
				KProf p("set.find(std::string literal)");
				p.SetMultiplier(100000);
				for (uint32_t i = 0; i < 100000; i++)
				{
					KProf::Force(&map);
					if (map.find("876") == map.end())
					{
						KProf::Force();
						break;
					}
				}
			}
		}
		{
			std::set<KString> map;

			{
				KProf p("set.emplace(KString)");
				p.SetMultiplier(10000);
				for (uint32_t i = 0; i < 10000; i++)
				{
					KProf::Force(&map);
					map.emplace(KString::to_string(i));
					KProf::Force();
				}
			}
			{
				KString sFind("876");
				KProf p("set.find(KString)");
				p.SetMultiplier(100000);
				for (uint32_t i = 0; i < 100000; i++)
				{
					KProf::Force(&map);
					if (map.find(sFind) == map.end())
					{
						KProf::Force();
						break;
					}
				}
			}
			{
				KProf p("set.find(KString literal)");
				p.SetMultiplier(100000);
				for (uint32_t i = 0; i < 100000; i++)
				{
					KProf::Force(&map);
					if (map.find("876") == map.end())
					{
						KProf::Force();
						break;
					}
				}
			}
		}
	}

	{
		KProf ps("-KSet");
		{
			KSet<std::string> map;

			{
				KProf p("KSet.emplace(std::string)");
				p.SetMultiplier(10000);
				for (uint32_t i = 0; i < 10000; i++)
				{
					KProf::Force(&map);
					map.emplace(std::to_string(i));
					KProf::Force();
				}
			}
			{
				std::string sFind("876");
				KProf p("KSet.find(std::string)");
				p.SetMultiplier(100000);
				for (uint32_t i = 0; i < 100000; i++)
				{
					KProf::Force(&map);
					if (map.find(sFind) == map.end())
					{
						break;
					}
					KProf::Force();
				}
			}
			{
				KProf p("KSet.find(std::string literal)");
				p.SetMultiplier(100000);
				for (uint32_t i = 0; i < 100000; i++)
				{
					KProf::Force(&map);
					if (map.find("876") == map.end())
					{
						KProf::Force();
						break;
					}
				}
			}
		}
		{
			KSet<KString> map;

			{
				KProf p("KSet.emplace(KString)");
				p.SetMultiplier(10000);
				for (uint32_t i = 0; i < 10000; i++)
				{
					KProf::Force(&map);
					map.emplace(KString::to_string(i));
					KProf::Force();
				}
			}
			{
				KString sFind("876");
				KProf p("KSet.find(KString)");
				p.SetMultiplier(100000);
				for (uint32_t i = 0; i < 100000; i++)
				{
					KProf::Force(&map);
					if (map.find(sFind) == map.end())
					{
						KProf::Force();
						break;
					}
				}
			}
			{
				KProf p("KSet.find(KString literal)");
				p.SetMultiplier(100000);
				for (uint32_t i = 0; i < 100000; i++)
				{
					KProf::Force(&map);
					if (map.find("876") == map.end())
					{
						KProf::Force();
						break;
					}
				}
			}
		}
	}

	{
		KProf ps("-std::unordered_set");
		{
			std::unordered_set<std::string> map;

			{
				KProf p("unoredered_set.emplace(std::string)");
				p.SetMultiplier(10000);
				for (uint32_t i = 0; i < 10000; i++)
				{
					KProf::Force(&map);
					map.emplace(std::to_string(i));
					KProf::Force();
				}
			}
			{
				std::string sFind("876");
				KProf p("unoredered_set.find(std::string)");
				p.SetMultiplier(100000);
				for (uint32_t i = 0; i < 100000; i++)
				{
					KProf::Force(&map);
					if (map.find(sFind) == map.end())
					{
						break;
					}
					KProf::Force();
				}
			}
			{
				KProf p("unoredered_set.find(std::string literal)");
				p.SetMultiplier(100000);
				for (uint32_t i = 0; i < 100000; i++)
				{
					KProf::Force(&map);
					if (map.find("876") == map.end())
					{
						KProf::Force();
						break;
					}
				}
			}
		}
		{
			std::unordered_set<KString> map;

			{
				KProf p("unoredered_set.emplace(KString)");
				p.SetMultiplier(10000);
				for (uint32_t i = 0; i < 10000; i++)
				{
					KProf::Force(&map);
					map.emplace(KString::to_string(i));
					KProf::Force();
				}
			}
			{
				KString sFind("876");
				KProf p("unoredered_set.find(KString)");
				p.SetMultiplier(100000);
				for (uint32_t i = 0; i < 100000; i++)
				{
					KProf::Force(&map);
					if (map.find(sFind) == map.end())
					{
						KProf::Force();
						break;
					}
				}
			}
			{
				KProf p("unoredered_set.find(KString literal)");
				p.SetMultiplier(100000);
				for (uint32_t i = 0; i < 100000; i++)
				{
					KProf::Force(&map);
					if (map.find("876") == map.end())
					{
						KProf::Force();
						break;
					}
				}
			}
		}
	}

	{
		KProf ps("-KUnorderedSet");
		{
			KUnorderedSet<std::string> map;

			{
				KProf p("KUnorderedSet.emplace(std::string)");
				p.SetMultiplier(10000);
				for (uint32_t i = 0; i < 10000; i++)
				{
					KProf::Force(&map);
					map.emplace(std::to_string(i));
					KProf::Force();
				}
			}
			{
				std::string sFind("876");
				KProf p("KUnorderedSet.find(std::string)");
				p.SetMultiplier(100000);
				for (uint32_t i = 0; i < 100000; i++)
				{
					KProf::Force(&map);
					if (map.find(sFind) == map.end())
					{
						break;
					}
					KProf::Force();
				}
			}
			{
				KProf p("KUnorderedSet.find(std::string literal)");
				p.SetMultiplier(100000);
				for (uint32_t i = 0; i < 100000; i++)
				{
					KProf::Force(&map);
					if (map.find("876") == map.end())
					{
						KProf::Force();
						break;
					}
				}
			}
		}
		{
			KUnorderedSet<KString> map;

			{
				KProf p("KUnorderedSet.emplace(KString)");
				p.SetMultiplier(10000);
				for (uint32_t i = 0; i < 10000; i++)
				{
					KProf::Force(&map);
					map.emplace(KString::to_string(i));
					KProf::Force();
				}
			}
			{
				KString sFind("876");
				KProf p("KUnorderedSet.find(KString)");
				p.SetMultiplier(100000);
				for (uint32_t i = 0; i < 100000; i++)
				{
					KProf::Force(&map);
					if (map.find(sFind) == map.end())
					{
						KProf::Force();
						break;
					}
				}
			}
			{
				KProf p("KUnorderedSet.find(KString literal)");
				p.SetMultiplier(100000);
				for (uint32_t i = 0; i < 100000; i++)
				{
					KProf::Force(&map);
					if (map.find("876") == map.end())
					{
						KProf::Force();
						break;
					}
				}
			}
		}
	}

}

void maps()
{
	{
		KProf ps("-std::map");
		{
			std::map<std::string, std::string> map;
			std::string sValue("value");

			{
				KProf p("map.emplace(std::string)");
				p.SetMultiplier(10000);
				for (uint32_t i = 0; i < 10000; i++)
				{
					KProf::Force(&map);
					map.emplace(std::to_string(i), sValue);
					KProf::Force();
				}
			}
			{
				std::string sFind("876");
				KProf p("map.find(std::string)");
				p.SetMultiplier(100000);
				for (uint32_t i = 0; i < 100000; i++)
				{
					KProf::Force(&map);
					if (map.find(sFind) == map.end())
					{
						break;
					}
					KProf::Force();
				}
			}
			{
				KProf p("map.find(std::string literal)");
				p.SetMultiplier(100000);
				for (uint32_t i = 0; i < 100000; i++)
				{
					KProf::Force(&map);
					if (map.find("876") == map.end())
					{
						KProf::Force();
						break;
					}
				}
			}
		}
		{
			std::map<KString, KString> map;
			KString sValue("value");

			{
				KProf p("map.emplace(KString)");
				p.SetMultiplier(10000);
				for (uint32_t i = 0; i < 10000; i++)
				{
					KProf::Force(&map);
					map.emplace(KString::to_string(i), sValue);
					KProf::Force();
				}
			}
			{
				KString sFind("876");
				KProf p("map.find(KString)");
				p.SetMultiplier(100000);
				for (uint32_t i = 0; i < 100000; i++)
				{
					KProf::Force(&map);
					if (map.find(sFind) == map.end())
					{
						KProf::Force();
						break;
					}
				}
			}
			{
				KProf p("map.find(KString literal)");
				p.SetMultiplier(100000);
				for (uint32_t i = 0; i < 100000; i++)
				{
					KProf::Force(&map);
					if (map.find("876") == map.end())
					{
						KProf::Force();
						break;
					}
				}
			}
		}
	}

	{
		KProf ps("-KMap");
		{
			KMap<std::string, std::string> map;
			std::string sValue("value");

			{
				KProf p("KMap.emplace(std::string)");
				p.SetMultiplier(10000);
				for (uint32_t i = 0; i < 10000; i++)
				{
					KProf::Force(&map);
					map.emplace(std::to_string(i), sValue);
					KProf::Force();
				}
			}
			{
				std::string sFind("876");
				KProf p("KMap.find(std::string)");
				p.SetMultiplier(100000);
				for (uint32_t i = 0; i < 100000; i++)
				{
					KProf::Force(&map);
					if (map.find(sFind) == map.end())
					{
						break;
					}
					KProf::Force();
				}
			}
			{
				KProf p("KMap.find(std::string literal)");
				p.SetMultiplier(100000);
				for (uint32_t i = 0; i < 100000; i++)
				{
					KProf::Force(&map);
					if (map.find("876") == map.end())
					{
						KProf::Force();
						break;
					}
				}
			}
		}
		{
			KMap<KString, KString> map;
			KString sValue("value");

			{
				KProf p("KMap.emplace(KString)");
				p.SetMultiplier(10000);
				for (uint32_t i = 0; i < 10000; i++)
				{
					KProf::Force(&map);
					map.emplace(KString::to_string(i), sValue);
					KProf::Force();
				}
			}
			{
				KString sFind("876");
				KProf p("KMap.find(KString)");
				p.SetMultiplier(100000);
				for (uint32_t i = 0; i < 100000; i++)
				{
					KProf::Force(&map);
					if (map.find(sFind) == map.end())
					{
						KProf::Force();
						break;
					}
				}
			}
			{
				KProf p("KMap.find(KString literal)");
				p.SetMultiplier(100000);
				for (uint32_t i = 0; i < 100000; i++)
				{
					KProf::Force(&map);
					if (map.find("876") == map.end())
					{
						KProf::Force();
						break;
					}
				}
			}
		}
	}

	{
		KProf ps("-std::unordered_map");
		{
			std::unordered_map<std::string, std::string> map;
			std::string sValue("value");

			{
				KProf p("unordered_map.emplace(std::string)");
				p.SetMultiplier(10000);
				for (uint32_t i = 0; i < 10000; i++)
				{
					KProf::Force(&map);
					map.emplace(std::to_string(i), sValue);
					KProf::Force();
				}
			}
			{
				std::string sFind("876");
				KProf p("unordered_map.find(std::string)");
				p.SetMultiplier(100000);
				for (uint32_t i = 0; i < 100000; i++)
				{
					KProf::Force(&map);
					if (map.find(sFind) == map.end())
					{
						KProf::Force();
						break;
					}
				}
			}
			{
				KProf p("unordered_map.find(std::string literal)");
				p.SetMultiplier(100000);
				for (uint32_t i = 0; i < 100000; i++)
				{
					KProf::Force(&map);
					if (map.find("876") == map.end())
					{
						KProf::Force();
						break;
					}
				}
			}
		}
		{
			std::unordered_map<KString, KString> map;
			KString sValue("value");

			{
				KProf p("unordered_map.emplace(KString)");
				p.SetMultiplier(10000);
				for (uint32_t i = 0; i < 10000; i++)
				{
					KProf::Force(&map);
					map.emplace(KString::to_string(i), sValue);
					KProf::Force();
				}
			}
			{
				KString sFind("876");
				KProf p("unordered_map.find(KString)");
				p.SetMultiplier(100000);
				for (uint32_t i = 0; i < 100000; i++)
				{
					KProf::Force(&map);
					if (map.find(sFind) == map.end())
					{
						KProf::Force();
						break;
					}
				}
			}
			{
				KProf p("unordered_map.find(KString literal)");
				p.SetMultiplier(100000);
				for (uint32_t i = 0; i < 100000; i++)
				{
					KProf::Force(&map);
					if (map.find("876") == map.end())
					{
						KProf::Force();
						break;
					}
				}
			}
		}
	}

	{
		KProf ps("-KUnorderedMap");
		{
			KUnorderedMap<std::string, std::string> map;
			std::string sValue("value");

			{
				KProf p("KUnorderedMap.emplace(std::string)");
				p.SetMultiplier(10000);
				for (uint32_t i = 0; i < 10000; i++)
				{
					KProf::Force(&map);
					map.emplace(std::to_string(i), sValue);
					KProf::Force();
				}
			}
			{
				std::string sFind("876");
				KProf p("KUnorderedMap.find(std::string)");
				p.SetMultiplier(100000);
				for (uint32_t i = 0; i < 100000; i++)
				{
					KProf::Force(&map);
					if (map.find(sFind) == map.end())
					{
						KProf::Force();
						break;
					}
				}
			}
			{
				KProf p("KUnorderedMap.find(std::string literal)");
				p.SetMultiplier(100000);
				for (uint32_t i = 0; i < 100000; i++)
				{
					KProf::Force(&map);
					if (map.find("876") == map.end())
					{
						KProf::Force();
						break;
					}
				}
			}
		}
		{
			KUnorderedMap<KString, KString> map;
			KString sValue("value");

			{
				KProf p("KUnorderedMap.emplace(KString)");
				p.SetMultiplier(10000);
				for (uint32_t i = 0; i < 10000; i++)
				{
					KProf::Force(&map);
					map.emplace(KString::to_string(i), sValue);
					KProf::Force();
				}
			}
			{
				KString sFind("876");
				KProf p("KUnorderedMap.find(KString)");
				p.SetMultiplier(100000);
				for (uint32_t i = 0; i < 100000; i++)
				{
					KProf::Force(&map);
					if (map.find(sFind) == map.end())
					{
						KProf::Force();
						break;
					}
				}
			}
			{
				KProf p("KUnorderedMap.find(KString literal)");
				p.SetMultiplier(100000);
				for (uint32_t i = 0; i < 100000; i++)
				{
					KProf::Force(&map);
					if (map.find("876") == map.end())
					{
						KProf::Force();
						break;
					}
				}
			}
		}
	}

	{
		KProf ps("-KProps unique non sequential");
		{
			KProps<std::string, std::string, false, true> map;
			std::string sValue("value");

			{
				KProf p("KProps.emplace(std::string)");
				p.SetMultiplier(10000);
				for (uint32_t i = 0; i < 10000; i++)
				{
					KProf::Force(&map);
					map.emplace(std::to_string(i), sValue);
					KProf::Force();
				}
			}
			{
				std::string sFind("876");
				KProf p("KProps.find(std::string)");
				p.SetMultiplier(100000);
				for (uint32_t i = 0; i < 100000; i++)
				{
					KProf::Force(&map);
					if (map.find(sFind) == map.end())
					{
						KProf::Force();
						break;
					}
				}
			}
			{
				KProf p("KProps.find(std::string literal)");
				p.SetMultiplier(100000);
				for (uint32_t i = 0; i < 100000; i++)
				{
					KProf::Force(&map);
					if (map.find("876") == map.end())
					{
						KProf::Force();
						break;
					}
				}
			}
		}
		{
			KProps<KString, KString, false, true> map;
			KString sValue("value");

			{
				KProf p("KProps.emplace(KString)");
				p.SetMultiplier(10000);
				for (uint32_t i = 0; i < 10000; i++)
				{
					KProf::Force(&map);
					map.emplace(KString::to_string(i), sValue);
					KProf::Force();
				}
			}
			{
				KString sFind("876");
				KProf p("KProps.find(KString)");
				p.SetMultiplier(100000);
				for (uint32_t i = 0; i < 100000; i++)
				{
					KProf::Force(&map);
					if (map.find(sFind) == map.end())
					{
						KProf::Force();
						break;
					}
				}
			}
			{
				KProf p("KProps.find(KString literal)");
				p.SetMultiplier(100000);
				for (uint32_t i = 0; i < 100000; i++)
				{
					KProf::Force(&map);
					if (map.find("876") == map.end())
					{
						KProf::Force();
						break;
					}
				}
			}
		}
	}

	{
		KProf ps("-KProps unique sequential");
		{
			KProps<std::string, std::string, true, true> map;
			std::string sValue("value");

			{
				KProf p("KPropsS.emplace(std::string)");
				p.SetMultiplier(10000);
				for (uint32_t i = 0; i < 10000; i++)
				{
					KProf::Force(&map);
					map.emplace(std::to_string(i), sValue);
					KProf::Force();
				}
			}
			{
				std::string sFind("876");
				KProf p("KPropsS.find(std::string)");
				p.SetMultiplier(100000);
				for (uint32_t i = 0; i < 100000; i++)
				{
					KProf::Force(&map);
					if (map.find(sFind) == map.end())
					{
						KProf::Force();
						break;
					}
				}
			}
			{
				KProf p("KPropsS.find(std::string literal)");
				p.SetMultiplier(100000);
				for (uint32_t i = 0; i < 100000; i++)
				{
					KProf::Force(&map);
					if (map.find("876") == map.end())
					{
						KProf::Force();
						break;
					}
				}
			}
		}
		{
			KProps<KString, KString, true, true> map;
			KString sValue("value");

			{
				KProf p("KPropsS.emplace(KString)");
				p.SetMultiplier(10000);
				for (uint32_t i = 0; i < 10000; i++)
				{
					KProf::Force(&map);
					map.emplace(KString::to_string(i), sValue);
					KProf::Force();
				}
			}
			{
				KString sFind("876");
				KProf p("KPropsS.find(KString)");
				p.SetMultiplier(100000);
				for (uint32_t i = 0; i < 100000; i++)
				{
					KProf::Force(&map);
					if (map.find(sFind) == map.end())
					{
						KProf::Force();
						break;
					}
				}
			}
			{
				KProf p("KPropsS.find(KString literal)");
				p.SetMultiplier(100000);
				for (uint32_t i = 0; i < 100000; i++)
				{
					KProf::Force(&map);
					if (map.find("876") == map.end())
					{
						KProf::Force();
						break;
					}
				}
			}
		}
	}
}

void kprops_bench()
{
//	thread_local_bench();
	kprops();
	sets();
	maps();
	UrlDecode_bench();
}


