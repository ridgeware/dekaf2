
#include <vector>
#include <map>
#include <cinttypes>
#include <dekaf2/kprof.h>
#include <dekaf2/kstring.h>
#include <dekaf2/kstringview.h>
#include <dekaf2/ksplit.h>
#include <dekaf2/kjoin.h>
#include <dekaf2/kprops.h>

using namespace dekaf2;

void ksplit_bench()
{
	dekaf2::KProf ps("-kSplit/kJoin");

	KStringView svFewTokens   = "one,two,three,four,five";
	KStringView svManyTokens  = "a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p,q,r,s,t,u,v,w,x,y,z";
	KStringView svSpaces      = "This is a sentence with several words in it for testing";
	KStringView svKeyValue    = "key1=val1&key2=val2&key3=val3&key4=val4&key5=val5";

	KString sLongCSV;
	for (int i = 0; i < 200; ++i)
	{
		if (i) sLongCSV += ',';
		sLongCSV += "item";
		sLongCSV += std::to_string(i);
	}

	{
		dekaf2::KProf prof("kSplit char 5 tokens");
		prof.SetMultiplier(50000);
		for (int ct = 0; ct < 50000; ++ct)
		{
			std::vector<KStringView> v;
			kSplit(v, svFewTokens, ',');
			KProf::Force(&v);
		}
	}
	{
		dekaf2::KProf prof("kSplit char 26 tokens");
		prof.SetMultiplier(50000);
		for (int ct = 0; ct < 50000; ++ct)
		{
			std::vector<KStringView> v;
			kSplit(v, svManyTokens, ',');
			KProf::Force(&v);
		}
	}
	{
		dekaf2::KProf prof("kSplit space delim");
		prof.SetMultiplier(50000);
		for (int ct = 0; ct < 50000; ++ct)
		{
			std::vector<KStringView> v;
			kSplit(v, svSpaces, ' ');
			KProf::Force(&v);
		}
	}
	{
		dekaf2::KProf prof("kSplit 200 tokens");
		prof.SetMultiplier(5000);
		for (int ct = 0; ct < 5000; ++ct)
		{
			std::vector<KStringView> v;
			kSplit(v, sLongCSV, ',');
			KProf::Force(&v);
		}
	}
	{
		dekaf2::KProf prof("kSplit key=val pairs");
		prof.SetMultiplier(50000);
		for (int ct = 0; ct < 50000; ++ct)
		{
			std::vector<KStringViewPair> v;
			kSplit(v, svKeyValue, "&", "=");
			KProf::Force(&v);
		}
	}
	{
		dekaf2::KProf prof("kSplit into KProps");
		prof.SetMultiplier(50000);
		for (int ct = 0; ct < 50000; ++ct)
		{
			KProps<KStringView, KStringView, true, true> props;
			kSplit(props, svKeyValue, "&", "=");
			KProf::Force(&props);
		}
	}

	// kJoin benchmarks
	std::vector<KStringView> vFew  = { "one", "two", "three", "four", "five" };
	std::vector<KStringView> vMany;
	vMany.reserve(26);
	for (char c = 'a'; c <= 'z'; ++c) vMany.push_back(KStringView(&c, 1));

	std::vector<KStringView> vLong;
	{
		std::vector<KStringView> tmp;
		kSplit(tmp, sLongCSV, ',');
		vLong = std::move(tmp);
	}

	{
		dekaf2::KProf prof("kJoin 5 tokens");
		prof.SetMultiplier(50000);
		for (int ct = 0; ct < 50000; ++ct)
		{
			KString s;
			kJoin(s, vFew, ",");
			KProf::Force(&s);
		}
	}
	{
		dekaf2::KProf prof("kJoin 26 tokens");
		prof.SetMultiplier(50000);
		for (int ct = 0; ct < 50000; ++ct)
		{
			KString s;
			kJoin(s, vMany, ",");
			KProf::Force(&s);
		}
	}
	{
		dekaf2::KProf prof("kJoin 200 tokens");
		prof.SetMultiplier(5000);
		for (int ct = 0; ct < 5000; ++ct)
		{
			KString s;
			kJoin(s, vLong, ",");
			KProf::Force(&s);
		}
	}
}
