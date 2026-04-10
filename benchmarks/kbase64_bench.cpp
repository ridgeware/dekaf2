
#include <cinttypes>
#include <dekaf2/time/duration/kprof.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/strings/kstringview.h>
#include <dekaf2/crypto/encoding/kbase64.h>

using namespace dekaf2;

void kbase64_bench()
{
	dekaf2::KProf ps("-KBase64");

	KString sShort(64, 'A');
	KString sMedium(1024, 'B');
	KString sLarge(16384, 'C');

	KString sShortB64  = KBase64::Encode(sShort, false);
	KString sMediumB64 = KBase64::Encode(sMedium, false);
	KString sLargeB64  = KBase64::Encode(sLarge, false);

	KString sShortUrl  = KBase64Url::Encode(sShort);
	KString sMediumUrl = KBase64Url::Encode(sMedium);
	KString sLargeUrl  = KBase64Url::Encode(sLarge);

	{
		dekaf2::KProf prof("Base64 encode 64B");
		prof.SetMultiplier(50000);
		for (int ct = 0; ct < 50000; ++ct)
		{
			KString s = KBase64::Encode(sShort, false);
			KProf::Force(&s);
		}
	}
	{
		dekaf2::KProf prof("Base64 encode 1KB");
		prof.SetMultiplier(20000);
		for (int ct = 0; ct < 20000; ++ct)
		{
			KString s = KBase64::Encode(sMedium, false);
			KProf::Force(&s);
		}
	}
	{
		dekaf2::KProf prof("Base64 encode 16KB");
		prof.SetMultiplier(5000);
		for (int ct = 0; ct < 5000; ++ct)
		{
			KString s = KBase64::Encode(sLarge, false);
			KProf::Force(&s);
		}
	}
	{
		dekaf2::KProf prof("Base64 decode 64B");
		prof.SetMultiplier(50000);
		for (int ct = 0; ct < 50000; ++ct)
		{
			KString s = KBase64::Decode(sShortB64);
			KProf::Force(&s);
		}
	}
	{
		dekaf2::KProf prof("Base64 decode 1KB");
		prof.SetMultiplier(20000);
		for (int ct = 0; ct < 20000; ++ct)
		{
			KString s = KBase64::Decode(sMediumB64);
			KProf::Force(&s);
		}
	}
	{
		dekaf2::KProf prof("Base64 decode 16KB");
		prof.SetMultiplier(5000);
		for (int ct = 0; ct < 5000; ++ct)
		{
			KString s = KBase64::Decode(sLargeB64);
			KProf::Force(&s);
		}
	}
	{
		dekaf2::KProf prof("Base64Url encode 64B");
		prof.SetMultiplier(50000);
		for (int ct = 0; ct < 50000; ++ct)
		{
			KString s = KBase64Url::Encode(sShort);
			KProf::Force(&s);
		}
	}
	{
		dekaf2::KProf prof("Base64Url decode 64B");
		prof.SetMultiplier(50000);
		for (int ct = 0; ct < 50000; ++ct)
		{
			KString s = KBase64Url::Decode(sShortUrl);
			KProf::Force(&s);
		}
	}
	{
		dekaf2::KProf prof("Base64Url encode 1KB");
		prof.SetMultiplier(20000);
		for (int ct = 0; ct < 20000; ++ct)
		{
			KString s = KBase64Url::Encode(sMedium);
			KProf::Force(&s);
		}
	}
	{
		dekaf2::KProf prof("Base64Url decode 1KB");
		prof.SetMultiplier(20000);
		for (int ct = 0; ct < 20000; ++ct)
		{
			KString s = KBase64Url::Decode(sMediumUrl);
			KProf::Force(&s);
		}
	}
}
