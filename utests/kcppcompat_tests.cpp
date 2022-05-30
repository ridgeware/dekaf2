#include "catch.hpp"
#include <dekaf2/bits/kcppcompat.h>
#include <dekaf2/kstring.h>
#include <dekaf2/kstringview.h>
#include <dekaf2/kformat.h>

using namespace dekaf2;

#ifdef DEKAF2_HAS_CPP_14
namespace {

// =========================== std::apply ==========================

KString EscapeChars(KStringView sInput)
{
	KString sOutput;
	for (auto ch : sInput)
	{
		if (ch == '\\') sOutput += '\\';
		sOutput += ch;
	}
	return sOutput;
}

template<typename T, typename std::enable_if<std::is_constructible<KStringView, T>::value == false, int>::type = 0>
static auto EscapeType(T&& value)
{
	return std::forward<T>(value);
}

template<typename T, typename std::enable_if<std::is_constructible<KStringView, T>::value == true, int>::type = 0>
static auto EscapeType(T&& value)
{
	return EscapeChars (std::forward<T>(value));
}

template<class... Args>
static KString FormatEscaped(KStringView sFormat, Args&&... args)
{
	return std::apply([sFormat](auto&&... args)
	{
		return kFormat(sFormat, std::forward<decltype(args)>(args)...);
	},
	std::make_tuple(EscapeType(args)...));
}

  // =========================== end of std::apply ==========================

} // end of anonymous namespace
#endif

TEST_CASE("KCppCompat")
{
	SECTION("Endianess")
	{
		if (kIsLittleEndian())
		{
			CHECK ( kIsBigEndian() == false );
			uint32_t iVal = 0x89abcdef;
			uint32_t iBE  = iVal;
			kToBigEndian(iBE);
			CHECK ( iBE == 0xefcdab89 );
			kToLittleEndian(iBE);
			CHECK ( iBE == 0xefcdab89 );
			kSwapBytes(iBE);
			CHECK ( iBE == 0x89abcdef );
		}
		else
		{
			CHECK ( kIsBigEndian()    == true  );
			CHECK ( kIsLittleEndian() == false );
			uint32_t iVal = 0x89abcdef;
			uint32_t iBE  = iVal;
			kToLittleEndian(iBE);
			CHECK ( iBE == 0xefcdab89 );
			kToBigEndian(iBE);
			CHECK ( iBE == 0xefcdab89 );
			kSwapBytes(iBE);
			CHECK ( iBE == 0x89abcdef );
		}

		{
			uint8_t iVal = 0x89;
			uint8_t iBE  = iVal;
			kToBigEndian(iBE);
			CHECK ( iBE == 0x89 );
			kToLittleEndian(iBE);
			CHECK ( iBE == 0x89 );
		}
	}

#ifdef DEKAF2_HAS_CPP_14
	SECTION("std::apply")
	{
		KString sResult = FormatEscaped("{} {} {} {}", "this", 7.33, 9, "he\\llo");
		CHECK ( sResult == "this 7.33 9 he\\\\llo" );
	}
#endif
}
