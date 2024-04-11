#include "catch.hpp"

#include <dekaf2/kstringview.h>
#include <dekaf2/kstringutils.h>
#include <dekaf2/kjson.h>
#include <dekaf2/kmime.h>
#include <string>
#ifdef DEKAF2_HAS_STD_STRING_VIEW
#include <string_view>
#endif

using namespace dekaf2;

namespace {

const std::string& fstr()
{
	static std::string s { "xyz" };
	return s;
}

#ifdef DEKAF2_HAS_STD_STRING_VIEW
std::string_view fsv()
{
	static std::string_view sv { "xyz" };
	return sv;
}
#endif

const KString& fks()
{
	static KString s { "xyz" };
	return s;
}

KStringView fksv()
{
	static constexpr KStringView s { "xyz" };
	return s;
}

KStringViewZ fksz()
{
	static constexpr KStringViewZ s { "xyz" };
	return s;
}

struct OST
{
	operator const std::string&() const { return m_s; }
	operator std::string&() { return m_s; }
	operator std::string&&() && { return std::move(m_s); }

	std::string m_s { "abcde" };
};

struct OS
{
	OS() = default;
	void fccp(const std::string& s)     { m_s = s; }
	void fcp (std::string& s)           { m_s = s; }
	void fvcp(std::string s)            { m_s = std::move(s); }
	void fref(std::string& s)           { s = m_s; }
	void fmv (std::string&& s)          { m_s = std::move(s); }
	const std::string& str() const      { return m_s;   }
	operator const std::string&() const { return str(); }
	std::string m_s { "pqrst" };
};

#ifdef DEKAF2_HAS_STD_STRING_VIEW
struct OSV
{
	OSV() = default;
	void fccp(const std::string_view& s) { m_s = s; }
	void fcp (std::string_view& s)       { m_s = s; }
	void fref(std::string_view& s)       { s = m_s; }
	void fmv (std::string_view&& s)      { m_s = std::move(s); }
	operator std::string_view() const    { return m_s; }
	std::string_view m_s { "pqrst" };
};
#endif

struct OKS
{
	OKS() = default;
	void fccp(const KString& s)     { m_s = s; }
	void fcp (KString& s)           { m_s = s; }
	void fvcp(KString s)            { m_s = std::move(s); }
	void fref(KString& s)           { s = m_s; }
	void fmv (KString&& s)          { m_s = std::move(s); }
	const KString& strc() const     { return m_s; }
	KString& strr()                 { return m_s; }
	KString  str()                  { return m_s; }
	operator const KString&() const { return m_s; }
	KString m_s { "pqrst" };
};

struct OKSV
{
	OKSV() = default;
	operator KStringView() const { return m_s; }
	static constexpr KStringView m_s { "pqrst" };
};

struct OKSZ
{
	OKSZ() = default;
	operator KStringViewZ() const { return m_s; }
	static constexpr KStringViewZ m_s { "pqrst" };
};

#ifdef DEKAF2_REPEAT_CONSTEXPR_VARIABLE
constexpr KStringView OKSV::m_s;
constexpr KStringViewZ OKSZ::m_s;
#endif

} // end of anonymous namespace

TEST_CASE("StringBalance") {

	SECTION("assignments")
	{
#ifdef DEKAF2_HAS_STD_STRING_VIEW
		std::string_view sv { "abcdefghijk" };
		std::string_view sv2 = sv;
		std::string s(sv);
#else
		std::string s { "abcdefghijk" };
#endif
		std::string s2 = s;
#ifdef DEKAF2_HAS_STD_STRING_VIEW
		s = sv;
		sv = s;
		std::string_view svx(s);
#else
		s = "abcdefghijk";
#endif

		KString ks;
		KStringView ksv;
		KStringViewZ ksz = s;

		ks = fstr();
#ifdef DEKAF2_HAS_STD_STRING_VIEW
		ks = fsv();
#endif
		ks = fks();
		ks = fksv();
		ks = fksz();

		OS   os;
#ifdef DEKAF2_HAS_STD_STRING_VIEW
		OSV  osv;
#endif
		OKS  oks;
		OKSV oksv;
		OKSZ oksz;

		static_assert(!std::is_convertible<const OS&,    KStringView>::value, "convertible");
#ifdef DEKAF2_HAS_STD_STRING_VIEW
		static_assert(!std::is_convertible<const OSV&,   KStringView>::value, "convertible");
#endif
		static_assert(!std::is_convertible<const OKS&,   KStringView>::value, "convertible");
		static_assert( std::is_convertible<const OKSV&,  KStringView>::value, "not convertible");
		static_assert( std::is_convertible<const OKSZ&,  KStringView>::value, "not convertible");
		static_assert( std::is_convertible<const KJSON&, KStringView>::value, "not convertible");
		static_assert( std::is_convertible<const KJSON&, KString    >::value, "not convertible");

#ifdef DEKAF2_HAS_STD_STRING_VIEW
		static_assert( std::is_convertible<const std::string&,      std::string_view>::value, "not convertible");
		static_assert(!std::is_convertible<const std::string_view&, std::string     >::value, "convertible");

		static_assert( std::is_constructible<std::string_view,      const std::string&>::value     , "not constructible");
		static_assert( std::is_constructible<std::string,           const std::string_view&>::value, "constructible");
#endif
		static_assert( std::is_convertible<const KString&,          std::string>::value, "not convertible");
		static_assert( std::is_convertible<const std::string&,      KString>::value    , "not convertible");
		static_assert( std::is_convertible<const std::string&,      KStringView>::value, "not convertible");
		static_assert( std::is_convertible<const KStringView&,      std::string>::value, "not convertible");
#ifdef DEKAF2_HAS_STD_STRING_VIEW
		static_assert( std::is_convertible<const std::string_view&, KStringView     >::value, "not convertible");
		static_assert( std::is_convertible<const KStringView&,      std::string_view>::value, "not convertible");
		static_assert( std::is_convertible<const KString&,          std::string_view>::value, "not convertible");
		static_assert( std::is_convertible<const std::string_view&, KString         >::value, "not convertible");
#endif
		static_assert( std::is_convertible<const KString&,          KStringView>::value, "not convertible");
		static_assert( std::is_convertible<const KStringView&,      KStringView>::value, "not convertible");
		static_assert( std::is_convertible<const KStringViewZ&,     KStringView>::value, "not convertible");

		static_assert( detail::is_kstringview_assignable<const char*,  true>::value, "not assignable");
		static_assert( detail::is_kstringview_assignable<const char*,  true>::value, "not assignable");
		static_assert( detail::is_kstringview_assignable<KStringViewZ, true>::value, "not assignable");
		static_assert( detail::is_kstringview_assignable<KStringViewZ, true>::value, "not assignable");
		static_assert( detail::is_kstringview_assignable<OS,           true>::value, "not assignable");
		{
			bool b;
			KString str = "1234567890";
			std::string s = "123";
#ifdef DEKAF2_HAS_STD_STRING_VIEW
			std::string_view sv = "123";
			b = str.starts_with(sv);
			b = str.ends_with(sv);
			b = str.contains(sv);
			str.replace(0, 2, sv);
			str.replace(0, 2, OSV());
			static_cast<void>(str.find_first_not_of(OSV()));
#endif
			b = str.starts_with(s);
			b = str.ends_with(s);
			b = str.contains(s);
			str.replace(0, 2, s);

			b = str.starts_with("abc");
			b = str.ends_with("abc");
			b = str.contains("abc");
			str.replace(0, 2, "abc");
			
//			str.starts_with(OKS());
			str.replace(0, 2, OKS());
			static_cast<void>(str.find_first_not_of(OKS()));

			if (b) { }
		}

		{
			KMIME mime = KMIME::NONE;
			KMIME mime2 = KMIME::NONE;
			KStringView sv = "abcdef";
			KStringViewZ svz = "abcdef";
			KString str = "abcdef";
			
			if (mime == mime2)
			{
				if (mime == sv)
				{
					if (sv == mime)
					{
					}
				}
				if (mime == str)
				{
					if (str == mime)
					{
					}
				}
				if (mime == svz)
				{
					if (svz == mime)
					{
					}
				}
				if (mime == "abcd")
				{
				}
			}
			sv = mime;
		}

		ks = os;
#ifdef DEKAF2_HAS_STD_STRING_VIEW
		ks = osv;
#endif
		ks = oks;
		ks = oksv;
		ks = oksz;

		{
			KString ks1(os);
#ifdef DEKAF2_HAS_STD_STRING_VIEW
			KString ks2(osv);
#endif
			KString ks3(oks);
			KString ks4(oksv);
			KString ks5(oksz);
		}

#ifdef DEKAF2_HAS_STD_STRING_VIEW
		{
			KStringView ks1(os);
			KStringView ks2(osv);
			KStringView ks3(oks);
			KStringView ks4(oksv);
			KStringView ks5(oksz);
		}
#endif
		{
			OST ost;
			os.fccp(ost);
			os.fcp(ost);
			os.fvcp(ost);
			os.fmv(std::move(ost));
			os.fref(ost);

			KString ks = "123";
			os.fcp(ks);
			CHECK ( os.str() == "123" );
			ks = "333";
			CHECK ( ks == "333" );
			os.fref(ks);
			CHECK ( ks == "123" );
			os.fccp(ks);
			os.fvcp(ks);
			ks = "123456789012345678901234567890";
			os.fmv(std::move(ks));
			CHECK ( ks == "" );
			CHECK ( os.str() == "123456789012345678901234567890" );
			ks = "123456789012345678901234567890";
			os.fvcp(std::move(ks));
			CHECK ( ks == "" );
			CHECK ( os.str() == "123456789012345678901234567890" );
		}
		{
			std::string str;
#ifdef DEKAF2_HAS_FULL_CPP_17
			str = oks.str();
			str = oks.strc();
			str = oks.strr();
#endif
			str = "123456789012345678901234567890";
			oks.fmv(std::move(str));
			CHECK ( str == "" );
			CHECK ( oks.str() == "123456789012345678901234567890" );
			str = "123456789012345678901234567890";
			oks.fvcp(std::move(str));
			CHECK ( str == "" );
			CHECK ( oks.str() == "123456789012345678901234567890" );
//			oks.fref(str);
			oks.fccp(str);
//			oks.fcp(str);
			oks.fvcp(str);
		}

		ks += fstr();
#ifdef DEKAF2_HAS_STD_STRING_VIEW
		ks += fsv();
#endif
		ks += fks();
		ks += fksv();
		ks += fksz();

		ks += os;
#ifdef DEKAF2_HAS_STD_STRING_VIEW
		ks += osv;
#endif
		ks += oks;
		ks += oksv;
		ks += oksz;

		ksv = "abcde";
		ksv = s;
#ifdef DEKAF2_HAS_STD_STRING_VIEW
		ksv = sv;
#endif
		ksv = ks;
		ksv = ksz;

		{
			KString ks    = "abcde";
			std::string s = "123456";
			ks  = s;
			CHECK ( ks == "123456" );
			CHECK ( s  == "123456" );
			s   = "123456789012345678901234567890";
			ks  = std::move(s);
			CHECK ( ks == "123456789012345678901234567890" );
			CHECK ( s  == "" );
			s   = std::move(ks).str();
			CHECK ( s  == "123456789012345678901234567890" );
			CHECK ( ks == "" );
			s   = "12321";
#ifdef DEKAF2_HAS_STD_STRING_VIEW
			ks  = sv;
#endif
			ks  = ksv;
			ks  = ksz;
		}

		{
			KString ks("123456789012345678901234567890");
			std::string s(std::move(ks).str());
			CHECK ( ks == "" );
			CHECK ( s  == "123456789012345678901234567890");
		}
		{
			std::string s("123456789012345678901234567890");
			KString ks(std::move(s));
			CHECK ( s  == "" );
			CHECK ( ks == "123456789012345678901234567890");
		}

		ksz = "abcde";
		ksz = s;
		ksz = ks;

		KString ks2;
		ks2 += s;
#ifdef DEKAF2_HAS_STD_STRING_VIEW
		ks2 += sv;
#endif
		ks2 += ks;
		ks2 += ksv;
		ks2 += ksz;
		ks2 += 'a';
		ks2 += "abc";

		ks2.clear();
		ks2.append(s);
		ks2.append(s, 0, 2);
#ifdef DEKAF2_HAS_STD_STRING_VIEW
		ks2.append(sv);
#endif
		ks2.append(ks);
		ks2.append(ks, 0, 2);
		ks2.append(ksv);
		ks2.append(ksv, 0, 2);
		ks2.append(ksz);
		ks2.append(ksz, 0, 2);
		ks2.append(1, 'a');
		ks2.append("abc");


		ks  = "abcdefghijk";
		ksz = ks;
		ksv = ksz;
		CHECK ( ksv == "abcdefghijk" );
		CHECK ( ksv == ksz );
#ifdef DEKAF2_HAS_STD_STRING_VIEW
		sv = ksv;
		CHECK ( ksv == sv  );
#endif
		s = ksv;
		CHECK ( ksv == s   );

		CHECK ( ks  == ksv );
		CHECK ( ks  == ksz );
		CHECK ( ks  == s   );
#ifdef DEKAF2_HAS_STD_STRING_VIEW
		CHECK ( ks  == sv  );
#endif

		CHECK ( ksz == s   );
#ifdef DEKAF2_HAS_STD_STRING_VIEW
		CHECK ( ksz == sv  );
#endif
		CHECK ( ksz == ks  );
		CHECK ( ksz == ksv );

		CHECK ( s   == s2  );
		CHECK ( s   == ks  );
#ifdef DEKAF2_HAS_STD_STRING_VIEW
		CHECK ( sv  == sv2 );
		CHECK ( sv  == ks  );
		CHECK ( sv  == s   );
		CHECK ( s   == sv  );
#endif

		CHECK_FALSE ( ksv < "abcdefghijk" );
		CHECK_FALSE ( ksv < ksz );
#ifdef DEKAF2_HAS_STD_STRING_VIEW
//		CHECK_FALSE ( ksv < sv  ); // we cannot provide this implicit comparison
		CHECK_FALSE ( s   < sv  );
		CHECK_FALSE ( sv  < s   );
#endif
		CHECK_FALSE ( ksv < s   );

		CHECK_FALSE ( ks  < ksv );
		CHECK_FALSE ( ks  < ksz );
		CHECK_FALSE ( ks  < s   );
#ifdef DEKAF2_HAS_STD_STRING_VIEW
		CHECK_FALSE ( ks  < sv  );
#endif

		CHECK_FALSE ( ksz < s   );
#ifdef DEKAF2_HAS_STD_STRING_VIEW
//		CHECK_FALSE ( ksz < sv  ); // we cannot provide this implicit comparison
#endif
		CHECK_FALSE ( ksz < ks  );
		CHECK_FALSE ( ksz < ksv );

		CHECK_FALSE ( s   < ks  );
#ifdef DEKAF2_HAS_STD_STRING_VIEW
		CHECK_FALSE ( sv  < ks  );
#endif
		// unrelated, but relied upon
		char ch = '/';
		KString strFromChar{ch};
		CHECK ( strFromChar == "/" );
	}
}
