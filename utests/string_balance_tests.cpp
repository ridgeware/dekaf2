#include "catch.hpp"

#include <dekaf2/kstringview.h>
#include <dekaf2/kstringutils.h>
#include <dekaf2/kjson.h>
#include <string>
#ifdef DEKAF2_HAS_STD_STRING_VIEW
#include <string_view>
#endif

using namespace dekaf2;

namespace {

const std::string& fs()
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

struct OS
{
	OS() = default;
	operator const std::string&() const { return m_s; }
	std::string m_s { "pqrst" };
};

#ifdef DEKAF2_HAS_STD_STRING_VIEW
struct OSV
{
	OSV() = default;
	operator std::string_view() const { return m_s; }
	static constexpr std::string_view m_s { "pqrst" };
};
#endif

struct OKS
{
	OKS() = default;
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

} // end of anonymous namespace

TEST_CASE("StringBalance") {

	SECTION("assignments")
	{
#ifdef DEKAF2_HAS_STD_STRING_VIEW
		std::string_view sv { "abcdefghijk" };
		std::string_view sv2 = sv;
#endif
		std::string s(sv);
		std::string s2 = s;
#ifdef DEKAF2_HAS_STD_STRING_VIEW
		s = sv;
		sv = s;
		std::string_view svx(s);
#else
		s = "abcdefghijk";
#endif
		s.substr(0,2);

		KString ks;
		KStringView ksv;
		KStringViewZ ksz = s;

		ks = fs();
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
		static_assert(!std::is_convertible<const OSV&,   KStringView>::value, "convertible");
		static_assert(!std::is_convertible<const OKS&,   KStringView>::value, "convertible");
		static_assert( std::is_convertible<const OKSV&,  KStringView>::value, "not convertible");
		static_assert( std::is_convertible<const OKSZ&,  KStringView>::value, "not convertible");
		static_assert( std::is_convertible<const KJSON&, KStringView>::value, "not convertible");
		static_assert( std::is_convertible<const KJSON&, KString    >::value, "not convertible");

		static_assert( std::is_convertible<const std::string&,      std::string_view>::value, "not convertible");
		static_assert(!std::is_convertible<const std::string_view&, std::string     >::value, "convertible");

		static_assert( std::is_constructible<std::string_view,      const std::string&>::value     , "not constructible");
		static_assert( std::is_constructible<std::string,           const std::string_view&>::value, "constructible");

		static_assert( std::is_convertible<const std::string&,      KStringView>::value, "not convertible");
		static_assert( std::is_convertible<const std::string_view&, KStringView>::value, "not convertible");
		static_assert( std::is_convertible<const KString&,          KStringView>::value, "not convertible");
		static_assert( std::is_convertible<const KStringView&,      KStringView>::value, "not convertible");
		static_assert( std::is_convertible<const KStringViewZ&,     KStringView>::value, "not convertible");

//		ks = os;  // we cannot provide this implicit conversion
#ifdef DEKAF2_HAS_STD_STRING_VIEW
//		ks = osv; // we cannot provide this implicit conversion
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

		ks += fs();
#ifdef DEKAF2_HAS_STD_STRING_VIEW
		ks += fsv();
#endif
		ks += fks();
		ks += fksv();
		ks += fksz();

//		ks += os;  // we cannot provide this implicit conversion
#ifdef DEKAF2_HAS_STD_STRING_VIEW
//		ks += osv; // we cannot provide this implicit conversion
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

		ks  = "abcde";
		ks  = s;
#ifdef DEKAF2_HAS_STD_STRING_VIEW
		ks  = sv;
#endif
		ks  = ksv;
		ks  = ksz;

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


		CHECK ( ksv == "abcdefghijk" );
		CHECK ( ksv == ksz );
#ifdef DEKAF2_HAS_STD_STRING_VIEW
//		CHECK ( ksv == sv  ); // we cannot provide this implicit comparison
#endif
		CHECK ( ksv == s   );

		CHECK ( ks  == ksv );
		CHECK ( ks  == ksz );
		CHECK ( ks  == s   );
#ifdef DEKAF2_HAS_STD_STRING_VIEW
		CHECK ( ks  == sv  );
#endif

		CHECK ( ksz == s   );
#ifdef DEKAF2_HAS_STD_STRING_VIEW
//		CHECK ( ksz == sv  ); // we cannot provide this implicit comparison
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
	}
}
