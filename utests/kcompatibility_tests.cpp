#include "catch.hpp"
#include <dekaf2/kcompatibility.h>
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
		return kFormat(KRuntimeFormat(sFormat), std::forward<decltype(args)>(args)...);
	},
	std::make_tuple(EscapeType(args)...));
}

  // =========================== end of std::apply ==========================

} // end of anonymous namespace
#endif

TEST_CASE("KCompatibility")
{
#ifdef DEKAF2_HAS_CPP_14
	SECTION("std::apply")
	{
		KString sResult = FormatEscaped("{} {} {} {}", "this", 7.33, 9, "he\\llo");
		CHECK ( sResult == "this 7.33 9 he\\\\llo" );
	}
#endif
}
