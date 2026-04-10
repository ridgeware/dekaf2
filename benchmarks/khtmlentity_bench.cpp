
#include <cinttypes>
#include <dekaf2/time/duration/kprof.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/strings/kstringview.h>
#include <dekaf2/web/html/khtmlentities.h>

using namespace dekaf2;

void khtmlentity_bench()
{
	dekaf2::KProf ps("-KHTMLEntity");

	KStringView svPlain     = "This is a plain text string without any special characters at all";
	KStringView svFewChars  = "Price: 5 < 10 & 20 > 15 with \"quotes\" and 'apostrophes'";
	KStringView svEncoded   = "Price: 5 &lt; 10 &amp; 20 &gt; 15 with &quot;quotes&quot; and &#39;apostrophes&#39;";
	KStringView svHeavy     = "<div class=\"box\">&lt;p&gt;Hello &amp; welcome to the &quot;world&quot; of &lt;html&gt; &amp; &lt;css&gt;&lt;/p&gt;</div>";

	KString sLongPlain(2000, 'x');
	sLongPlain += " <tag> & \"quoted\" text ";

	KString sLongEncoded(2000, 'x');
	sLongEncoded += " &lt;tag&gt; &amp; &quot;quoted&quot; text ";

	{
		dekaf2::KProf prof("EncodeMandatory plain");
		prof.SetMultiplier(50000);
		for (int ct = 0; ct < 50000; ++ct)
		{
			KString s = KHTMLEntity::EncodeMandatory(svPlain);
			KProf::Force(&s);
		}
	}
	{
		dekaf2::KProf prof("EncodeMandatory special");
		prof.SetMultiplier(50000);
		for (int ct = 0; ct < 50000; ++ct)
		{
			KString s = KHTMLEntity::EncodeMandatory(svFewChars);
			KProf::Force(&s);
		}
	}
	{
		dekaf2::KProf prof("EncodeMandatory long");
		prof.SetMultiplier(10000);
		for (int ct = 0; ct < 10000; ++ct)
		{
			KString s = KHTMLEntity::EncodeMandatory(sLongPlain);
			KProf::Force(&s);
		}
	}
	{
		dekaf2::KProf prof("Encode full");
		prof.SetMultiplier(50000);
		for (int ct = 0; ct < 50000; ++ct)
		{
			KString s = KHTMLEntity::Encode(svFewChars);
			KProf::Force(&s);
		}
	}
	{
		dekaf2::KProf prof("Decode entities");
		prof.SetMultiplier(50000);
		for (int ct = 0; ct < 50000; ++ct)
		{
			KString s = KHTMLEntity::Decode(svEncoded);
			KProf::Force(&s);
		}
	}
	{
		dekaf2::KProf prof("Decode heavy");
		prof.SetMultiplier(50000);
		for (int ct = 0; ct < 50000; ++ct)
		{
			KString s = KHTMLEntity::Decode(svHeavy);
			KProf::Force(&s);
		}
	}
	{
		dekaf2::KProf prof("Decode long");
		prof.SetMultiplier(10000);
		for (int ct = 0; ct < 10000; ++ct)
		{
			KString s = KHTMLEntity::Decode(sLongEncoded);
			KProf::Force(&s);
		}
	}
}
