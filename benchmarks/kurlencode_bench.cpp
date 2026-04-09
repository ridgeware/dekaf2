
#include <cinttypes>
#include <dekaf2/kprof.h>
#include <dekaf2/kstring.h>
#include <dekaf2/kstringview.h>
#include <dekaf2/kurlencode.h>

using namespace dekaf2;

void kurlencode_bench()
{
	dekaf2::KProf ps("-kUrlEncode");

	KStringView svPlain   = "hello world this is plain text";
	KStringView svSpecial = "key=hello world&foo=bar/baz?x=1#frag";
	KStringView svUTF8    = "Stra\xc3\x9f" "e nach M\xc3\xbc" "nchen \xc3\xbc\xc3\xb6\xc3\xa4";
	KStringView svHeavy   = "<script>alert('xss');</script>&foo=bar\"baz";

	KString sEncodedPlain   = kUrlEncode(svPlain, URIPart::Query);
	KString sEncodedSpecial = kUrlEncode(svSpecial, URIPart::Query);
	KString sEncodedUTF8    = kUrlEncode(svUTF8, URIPart::Query);

	{
		dekaf2::KProf prof("kUrlEncode plain");
		prof.SetMultiplier(50000);
		for (int ct = 0; ct < 50000; ++ct)
		{
			KString s = kUrlEncode(svPlain, URIPart::Query);
			KProf::Force(&s);
		}
	}
	{
		dekaf2::KProf prof("kUrlEncode special");
		prof.SetMultiplier(50000);
		for (int ct = 0; ct < 50000; ++ct)
		{
			KString s = kUrlEncode(svSpecial, URIPart::Query);
			KProf::Force(&s);
		}
	}
	{
		dekaf2::KProf prof("kUrlEncode UTF8");
		prof.SetMultiplier(50000);
		for (int ct = 0; ct < 50000; ++ct)
		{
			KString s = kUrlEncode(svUTF8, URIPart::Query);
			KProf::Force(&s);
		}
	}
	{
		dekaf2::KProf prof("kUrlEncode heavy");
		prof.SetMultiplier(50000);
		for (int ct = 0; ct < 50000; ++ct)
		{
			KString s = kUrlEncode(svHeavy, URIPart::Query);
			KProf::Force(&s);
		}
	}
	{
		dekaf2::KProf prof("kUrlDecode plain");
		prof.SetMultiplier(50000);
		for (int ct = 0; ct < 50000; ++ct)
		{
			KString s;
			kUrlDecode(KStringView(sEncodedPlain), s);
			KProf::Force(&s);
		}
	}
	{
		dekaf2::KProf prof("kUrlDecode special");
		prof.SetMultiplier(50000);
		for (int ct = 0; ct < 50000; ++ct)
		{
			KString s;
			kUrlDecode(KStringView(sEncodedSpecial), s);
			KProf::Force(&s);
		}
	}
	{
		dekaf2::KProf prof("kUrlDecode UTF8");
		prof.SetMultiplier(50000);
		for (int ct = 0; ct < 50000; ++ct)
		{
			KString s;
			kUrlDecode(KStringView(sEncodedUTF8), s);
			KProf::Force(&s);
		}
	}
	{
		dekaf2::KProf prof("kUrlEncode path");
		prof.SetMultiplier(50000);
		for (int ct = 0; ct < 50000; ++ct)
		{
			KString s = kUrlEncode(svSpecial, URIPart::Path);
			KProf::Force(&s);
		}
	}
}
