
#include <cinttypes>
#include <dekaf2/kprof.h>
#include <dekaf2/kstring.h>
#include <dekaf2/kstringview.h>
#include <dekaf2/khash.h>
#include <dekaf2/kcrc.h>
#include <dekaf2/kmessagedigest.h>

using namespace dekaf2;

void khash_bench()
{
	dekaf2::KProf ps("-Hash/CRC");

	KString sShort(64, 'A');
	KString sMedium(1024, 'B');
	KString sLarge(65536, 'C');

	{
		dekaf2::KProf prof("KHash FNV 64B");
		prof.SetMultiplier(100000);
		for (int ct = 0; ct < 100000; ++ct)
		{
			KHash h(sShort);
			auto v = h.Hash();
			KProf::Force(&v);
		}
	}
	{
		dekaf2::KProf prof("KHash FNV 1KB");
		prof.SetMultiplier(100000);
		for (int ct = 0; ct < 100000; ++ct)
		{
			KHash h(sMedium);
			auto v = h.Hash();
			KProf::Force(&v);
		}
	}
	{
		dekaf2::KProf prof("KHash FNV 64KB");
		prof.SetMultiplier(5000);
		for (int ct = 0; ct < 5000; ++ct)
		{
			KHash h(sLarge);
			auto v = h.Hash();
			KProf::Force(&v);
		}
	}
	{
		dekaf2::KProf prof("KCRC32 64B");
		prof.SetMultiplier(100000);
		for (int ct = 0; ct < 100000; ++ct)
		{
			KCRC32 crc(sShort);
			auto v = crc.CRC();
			KProf::Force(&v);
		}
	}
	{
		dekaf2::KProf prof("KCRC32 1KB");
		prof.SetMultiplier(100000);
		for (int ct = 0; ct < 100000; ++ct)
		{
			KCRC32 crc(sMedium);
			auto v = crc.CRC();
			KProf::Force(&v);
		}
	}
	{
		dekaf2::KProf prof("KCRC32 64KB");
		prof.SetMultiplier(5000);
		for (int ct = 0; ct < 5000; ++ct)
		{
			KCRC32 crc(sLarge);
			auto v = crc.CRC();
			KProf::Force(&v);
		}
	}
	{
		dekaf2::KProf prof("MD5 64B");
		prof.SetMultiplier(50000);
		for (int ct = 0; ct < 50000; ++ct)
		{
			KMessageDigest md(KMessageDigest::MD5, sShort);
			KString s = md.Digest();
			KProf::Force(&s);
		}
	}
	{
		dekaf2::KProf prof("MD5 1KB");
		prof.SetMultiplier(20000);
		for (int ct = 0; ct < 20000; ++ct)
		{
			KMessageDigest md(KMessageDigest::MD5, sMedium);
			KString s = md.Digest();
			KProf::Force(&s);
		}
	}
	{
		dekaf2::KProf prof("SHA1 64B");
		prof.SetMultiplier(50000);
		for (int ct = 0; ct < 50000; ++ct)
		{
			KMessageDigest md(KMessageDigest::SHA1, sShort);
			KString s = md.Digest();
			KProf::Force(&s);
		}
	}
	{
		dekaf2::KProf prof("SHA256 64B");
		prof.SetMultiplier(50000);
		for (int ct = 0; ct < 50000; ++ct)
		{
			KMessageDigest md(KMessageDigest::SHA256, sShort);
			KString s = md.Digest();
			KProf::Force(&s);
		}
	}
	{
		dekaf2::KProf prof("SHA256 1KB");
		prof.SetMultiplier(20000);
		for (int ct = 0; ct < 20000; ++ct)
		{
			KMessageDigest md(KMessageDigest::SHA256, sMedium);
			KString s = md.Digest();
			KProf::Force(&s);
		}
	}
}
