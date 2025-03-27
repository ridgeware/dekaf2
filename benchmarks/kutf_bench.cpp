#include <sstream>
#include <iostream>
#include <vector>
#include <map>
#include <set>
#include <cinttypes>

#include <dekaf2/kprof.h>
#include <dekaf2/kstring.h>
#include <dekaf2/ksystem.h>
#include <dekaf2/ktime.h>
#include <dekaf2/kwriter.h>
#include <dekaf2/kutf.h>

#ifdef DEKAF2_HAS_ICONV
#include <iconv.h>
#endif
#ifdef DEKAF2_HAS_SIMDUTF
#include "simdutf.h"
#endif
#ifdef DEKAF2_HAS_UTF8CPP
#include <utf8cpp/utf8.h>
#endif
//#define HAVE_TEXTUTILS 1
#ifdef HAVE_TEXTUTILS
#include "textutils/UtfConv.h"
#endif

using namespace dekaf2;

#ifdef DEKAF2_HAS_ICONV
void iconv_to_utf32(const KString& sData, std::u32string& sOutBuffer)
{
	static iconv_t s_cd1 = iconv_open("UTF-32", "UTF-8");

	auto buf8 = sData.data();
	auto len8 = sData.size();
	auto buf32 = sOutBuffer.data();
	auto len32 = sOutBuffer.size();

	if (!::iconv(s_cd1, (char **)&buf8, &len8, (char **)&buf32, &len32))
	{

	}
	else
	{

	}
}

void iconv_to_utf8(const std::u32string& sData, KString& sOutBuffer)
{
	static iconv_t s_cd2 = iconv_open("UTF-8", "UTF-32");

	auto buf32 = sData.data();
	auto len32 = sData.size();
	auto buf8  = sOutBuffer.data();
	auto len8  = sOutBuffer.size();

	if (!::iconv(s_cd2, (char **)&buf32, &len32, &buf8, &len8))
	{

	}
	else
	{

	}
}
#endif

void kutf8_bench()
{
	KString sData;

	for (std::size_t i = 0; i < 2 * 1000 * 1000; ++i)
	{
		uint32_t ch;
		do
		{
			//			ch = kRandom(0, 0x0110000);
			ch = kRandom(0, 0x0800);
			//			ch = kRandom(0, 0x0ff);

		} while (kutf::IsSurrogate(ch));
		kutf::ToUTF(ch, sData);
	}

	KString sDataShort;

	for (std::size_t i = 0; i < 50; ++i)
	{
		uint32_t ch;
		do
		{
			//			ch = kRandom(0, 0x0110000);
			ch = kRandom(0, 0x0800);
			//			ch = kRandom(0, 0x0ff);

		} while (kutf::IsSurrogate(ch));
		kutf::ToUTF(ch, sDataShort);
	}

	kPrintLine("Time: {}", kFormTimestamp(KLocalTime::now(), "{:%Y-%m-%d %H:%M:%S}"));
	kPrintLine("long  size: {} bytes, {} codepoints, valid={}", sData.size(),      kutf::Count(sData),      kutf::Valid(sData)     );
	kPrintLine("short size: {} bytes, {} codepoints, valid={}", sDataShort.size(), kutf::Count(sDataShort), kutf::Valid(sDataShort));

	auto sWide      = kutf::Convert<std::u32string>(sData);
	auto sWideShort = kutf::Convert<std::u32string>(sDataShort);
#ifdef HAVE_TEXTUTILS
	auto sWide32      = kutf::Convert<std::basic_string<UnicodeChar32>>(sData);
	auto sWideShort32 = kutf::Convert<std::basic_string<UnicodeChar32>>(sDataShort);
#endif

	KString sASCII;

	for (std::size_t i = 0; i < 2 * 1000 * 1000; ++i)
	{
		sASCII += kRandom(0, 0x07f);
	}

	kPrintLine("ASCII size: {} bytes, {} codepoints, valid={}", sASCII.size(),     sASCII.size(),     kutf::ValidASCII(sASCII)    );

	// surrogate
	//	sData.insert(0, "\xed\xad\xbf"_ksv);
	//	sWide.insert(0, 1, 0x0DB7FUL);

	dekaf2::KProf ps("-KUTF8");

#ifdef DEKAF2_HAS_SIMDUTF
	{
		dekaf2::KProf prof("simdutf count long");
		prof.SetMultiplier(100);
		for (int ct = 0; ct < 100; ++ct)
		{
			KProf::Force(&sData);
			auto iCount = simdutf::count_utf8(sData.data(), sData.size());
			if (iCount > 100000) KProf::Force();
		}
	}
	{
		dekaf2::KProf prof("simdutf count short");
		prof.SetMultiplier(40000);
		for (int ct = 0; ct < 40000; ++ct)
		{
			KProf::Force(&sDataShort);
			auto iCount = simdutf::count_utf8(sDataShort.data(), sDataShort.size());
			if (iCount > 100000) KProf::Force();
		}
	}
	{
		dekaf2::KProf prof("simdutf count shortened");
		prof.SetMultiplier(40000);
		for (int ct = 0; ct < 40000; ++ct)
		{
			KProf::Force(&sData);
			auto iCount = simdutf::count_utf8(sData.data(), sDataShort.size());
			if (iCount > 100000) KProf::Force();
		}
	}
#endif
	{
		dekaf2::KProf prof("CountUTF long");
		prof.SetMultiplier(100);
		for (int ct = 0; ct < 100; ++ct)
		{
			KProf::Force(&sData);
			auto iCount = kutf::Count(sData.begin(), sData.end());
			if (iCount > 100000) KProf::Force();
		}
	}
	{
		dekaf2::KProf prof("CountUTF short");
		prof.SetMultiplier(40000);
		for (int ct = 0; ct < 40000; ++ct)
		{
			KProf::Force(&sDataShort);
			auto iCount = kutf::Count(sDataShort.begin(), sDataShort.end());
			if (iCount > 100000) KProf::Force();
		}
	}

	{
		dekaf2::KProf prof("CountUTFMax long");
		prof.SetMultiplier(100);
		for (int ct = 0; ct < 100; ++ct)
		{
			KProf::Force(&sData);
			auto iCount = kutf::Count(sData, sData.size());
			if (iCount > 100000) KProf::Force();
		}
	}
	{
		dekaf2::KProf prof("CountUTFMax short");
		prof.SetMultiplier(40000);
		for (int ct = 0; ct < 40000; ++ct)
		{
			KProf::Force(&sDataShort);
			auto iCount = kutf::Count(sDataShort, sDataShort.size());
			if (iCount > 100000) KProf::Force();
		}
	}
#ifdef DEKAF2_HAS_UTF8CPP
	{
		dekaf2::KProf prof("UTF8CPP Count long");
		prof.SetMultiplier(100);
		for (int ct = 0; ct < 100; ++ct)
		{
			KProf::Force(&sData);
			auto iCount = utf8::distance(sData.begin(), sData.end());
			if (iCount > 100000) KProf::Force();
		}
	}
	{
		dekaf2::KProf prof("UTF8CPP Count short");
		prof.SetMultiplier(40000);
		for (int ct = 0; ct < 40000; ++ct)
		{
			KProf::Force(&sDataShort);
			auto iCount = utf8::distance(sDataShort.begin(), sDataShort.end());
			if (iCount > 100000) KProf::Force();
		}
	}
#endif
#ifdef HAVE_TEXTUTILS
	{
		dekaf2::KProf prof("CountUTF8ToUTF32 long");
		prof.SetMultiplier(100);
		for (int ct = 0; ct < 100; ++ct)
		{
			KProf::Force(&sData);
			auto iCount = utf_conv::UTF8ToUTF32(sData.data(), static_cast<unsigned int>(sData.size()), NULL, NULL, 0, NULL, 0);
			if (iCount > 100000) KProf::Force();
		}
	}
	{
		dekaf2::KProf prof("CountUTF8ToUTF32 short");
		prof.SetMultiplier(40000);
		for (int ct = 0; ct < 40000; ++ct)
		{
			KProf::Force(&sDataShort);
			auto iCount = utf_conv::UTF8ToUTF32(sDataShort.data(), static_cast<unsigned int>(sDataShort.size()), NULL, NULL, 0, NULL, 0);
			if (iCount > 100000) KProf::Force();
		}
	}
#endif
#ifdef DEKAF2_HAS_SIMDUTF
	{
		dekaf2::KProf prof("SIMDUTF8to32 long");
		prof.SetMultiplier(100);
		for (int ct = 0; ct < 100; ++ct)
		{
			KProf::Force(&sData);
			auto size = simdutf::utf32_length_from_utf8(sData.data(), sData.size());
			std::basic_string<char32_t> sOut;
			sOut.resize(size);
			auto iWrote = simdutf::convert_utf8_to_utf32(sData.data(), sData.size(), sOut.data());
			if (iWrote > 100000) KProf::Force();
		}
	}
	{
		dekaf2::KProf prof("SIMDUTF8to32 short");
		prof.SetMultiplier(40000);
		for (int ct = 0; ct < 40000; ++ct)
		{
			KProf::Force(&sDataShort);
			auto expected_utf32words = simdutf::utf32_length_from_utf8(sDataShort.data(), sDataShort.size());
			std::basic_string<char32_t> sOut;
			sOut.resize(expected_utf32words);
			auto iWrote = simdutf::convert_utf8_to_utf32(sDataShort.data(), sDataShort.size(), sOut.data());
			if (iWrote > 100000) KProf::Force();
		}
	}
#endif
	{
		dekaf2::KProf prof("ConvertUTF 8 to 32 long");
		prof.SetMultiplier(100);
		for (int ct = 0; ct < 100; ++ct)
		{
			KProf::Force(&sData);
			auto sOut = kutf::Convert<std::basic_string<char32_t>>(sData);
			if (sOut.size() > 100000) KProf::Force();
		}
	}
	{
		dekaf2::KProf prof("ConvertUTF 8 to 32 short");
		prof.SetMultiplier(40000);
		for (int ct = 0; ct < 40000; ++ct)
		{
			KProf::Force(&sDataShort);
			auto sOut = kutf::Convert<std::basic_string<char32_t>>(sDataShort);
			if (sOut.size() > 100000) KProf::Force();
		}
	}
#ifdef HAVE_TEXTUTILS
	{
		dekaf2::KProf prof("UTF8ToUTF32 long");
		prof.SetMultiplier(100);
		for (int ct = 0; ct < 100; ++ct)
		{
			KProf::Force(&sData);
			auto sOut = utf_conv::UTF8ToUTF32(sData.str());
			if (sOut.size() > 100000) KProf::Force();
		}
	}
	{
		dekaf2::KProf prof("UTF8ToUTF32 short");
		prof.SetMultiplier(40000);
		for (int ct = 0; ct < 40000; ++ct)
		{
			KProf::Force(&sDataShort);
			auto sWide = utf_conv::UTF8ToUTF32(sDataShort.str());
			if (sWide.size() > 100000) KProf::Force();
		}
	}
#endif
#ifdef DEKAF2_HAS_UTF8CPP
	{
		dekaf2::KProf prof("UTF8CPP conv 8 to 32 long");
		prof.SetMultiplier(100);
		for (int ct = 0; ct < 100; ++ct)
		{
			KProf::Force(&sData);
			auto sOut = utf8::utf8to32(sData.str());
			if (sOut.size() > 100000) KProf::Force();
		}
	}
	{
		dekaf2::KProf prof("UTF8CPP conv 8 to 32 short");
		prof.SetMultiplier(40000);
		for (int ct = 0; ct < 40000; ++ct)
		{
			KProf::Force(&sDataShort);
			auto sOut = utf8::utf8to32(sDataShort.str());
			if (sOut.size() > 100000) KProf::Force();
		}
	}
#endif
#ifdef DEKAF2_HAS_ICONV
	{
		dekaf2::KProf prof("iconv8to32 long");
		prof.SetMultiplier(100);
		for (int ct = 0; ct < 100; ++ct)
		{
			KProf::Force(&sData);
			std::u32string sOut;
			sOut.resize(sData.size());
			iconv_to_utf32(sData, sOut);
			if (sOut.size() > 100000) KProf::Force();
		}
	}
	{
		dekaf2::KProf prof("iconv8to32 short");
		prof.SetMultiplier(40000);
		for (int ct = 0; ct < 40000; ++ct)
		{
			KProf::Force(&sDataShort);
			std::u32string sOut;
			sOut.resize(sDataShort.size());
			iconv_to_utf32(sDataShort, sOut);
			if (sOut.size() > 100000) KProf::Force();
		}
	}
#endif

#ifdef DEKAF2_HAS_SIMDUTF
	{
		dekaf2::KProf prof("SIMDUTF32to8 long");
		prof.SetMultiplier(100);
		for (int ct = 0; ct < 100; ++ct)
		{
			KProf::Force(&sWide);
			auto size = simdutf::utf8_length_from_utf32(sWide.data(), sWide.size());
			std::string sOut;
			sOut.resize(size);
			auto iWrote = simdutf::convert_utf32_to_utf8(sWide.data(), sWide.size(), sOut.data());
			if (iWrote > 100000) KProf::Force();
		}
	}
	{
		dekaf2::KProf prof("SIMDUTF32to8 short");
		prof.SetMultiplier(40000);
		for (int ct = 0; ct < 40000; ++ct)
		{
			KProf::Force(&sWideShort);
			auto size = simdutf::utf8_length_from_utf32(sWideShort.data(), sWideShort.size());
			std::string sOut;
			sOut.resize(size);
			auto iWrote = simdutf::convert_utf32_to_utf8(sWideShort.data(), sWideShort.size(), sOut.data());
			if (iWrote > 100000) KProf::Force();
		}
	}
#endif
	{
		dekaf2::KProf prof("ConvertUTF 32 to 8 long");
		prof.SetMultiplier(100);
		for (int ct = 0; ct < 100; ++ct)
		{
			KProf::Force(&sWide);
			auto sOut = kutf::Convert<std::string>(sWide);
			if (sOut.size() > 100000) KProf::Force();
		}
	}
	{
		dekaf2::KProf prof("ConvertUTF 32 to 8 short");
		prof.SetMultiplier(40000);
		for (int ct = 0; ct < 40000; ++ct)
		{
			KProf::Force(&sWideShort);
			auto sOut = kutf::Convert<std::string>(sWideShort);
			if (sOut.size() > 100000) KProf::Force();
		}
	}
#ifdef HAVE_TEXTUTILS
	{
		dekaf2::KProf prof("UTF32ToUTF8 long");
		prof.SetMultiplier(100);
		for (int ct = 0; ct < 100; ++ct)
		{
			KProf::Force(&sWide);
			auto sOut = utf_conv::UTF32ToUTF8(sWide32);
			if (sOut.size() > 100000) KProf::Force();
		}
	}
	{
		dekaf2::KProf prof("UTF32ToUTF8 short");
		prof.SetMultiplier(40000);
		for (int ct = 0; ct < 40000; ++ct)
		{
			KProf::Force(&sWideShort);
			auto sOut = utf_conv::UTF32ToUTF8(sWideShort32);
			if (sOut.size() > 100000) KProf::Force();
		}
	}
#endif
#ifdef DEKAF2_HAS_UTF8CPP
	{
		dekaf2::KProf prof("UTF8CPP conv 32 to 8 long");
		prof.SetMultiplier(100);
		for (int ct = 0; ct < 100; ++ct)
		{
			KProf::Force(&sWide);
			auto sOut = utf8::utf32to8(sWide);
			if (sOut.size() > 100000) KProf::Force();
		}
	}
	{
		dekaf2::KProf prof("UTF8CPP conv 32 to 8 short");
		prof.SetMultiplier(40000);
		for (int ct = 0; ct < 40000; ++ct)
		{
			KProf::Force(&sWideShort);
			auto sOut = utf8::utf32to8(sWideShort);
			if (sOut.size() > 100000) KProf::Force();
		}
	}
#endif
#ifdef DEKAF2_HAS_ICONV
	{
		dekaf2::KProf prof("iconv32to8 long");
		prof.SetMultiplier(100);
		for (int ct = 0; ct < 100; ++ct)
		{
			KProf::Force(&sWide);
			KString sOut;
			sOut.resize(sWide.size()*4);
			iconv_to_utf8(sWide, sOut);
			if (sWide.size() > 100000) KProf::Force();
		}
	}
	{
		dekaf2::KProf prof("iconv32to8 short");
		prof.SetMultiplier(40000);
		for (int ct = 0; ct < 40000; ++ct)
		{
			KProf::Force(&sWideShort);
			KString sOut;
			sOut.resize(sWideShort.size() * 4);
			iconv_to_utf8(sWideShort, sOut);
			if (sOut.size() > 100000) KProf::Force();
		}
	}
#endif

	{
		dekaf2::KProf prof("ValidASCII long");
		prof.SetMultiplier(100);
		for (int ct = 0; ct < 100; ++ct)
		{
			KProf::Force(&sASCII);
			auto yes = kutf::ValidASCII(sASCII);
			if (!yes) KProf::Force();
		}
	}

	{
		dekaf2::KProf prof("InvalidASCII long");
		prof.SetMultiplier(100);
		for (int ct = 0; ct < 100; ++ct)
		{
			KProf::Force(&sASCII);
			auto it = kutf::InvalidASCII(sASCII);
			if (!it) KProf::Force();
		}
	}

	{
		dekaf2::KProf prof("No ValidASCII long");
		prof.SetMultiplier(100);
		for (int ct = 0; ct < 100; ++ct)
		{
			KProf::Force(&sData);
			auto yes = kutf::ValidASCII(sData);
			if (yes) KProf::Force();
		}
	}

	{
		dekaf2::KProf prof("No InvalidASCII long");
		prof.SetMultiplier(100);
		for (int ct = 0; ct < 100; ++ct)
		{
			KProf::Force(&sData);
			auto it = kutf::InvalidASCII(sData);
			if (!it) KProf::Force();
		}
	}

#ifdef DEKAF2_HAS_SIMDUTF
	{
		dekaf2::KProf prof("SIMD ValidUTF long");
		prof.SetMultiplier(100);
		for (int ct = 0; ct < 100; ++ct)
		{
			KProf::Force(&sData);
			auto yes = simdutf::validate_utf8(sData.data(), sData.size());
			if (yes) KProf::Force();
		}
	}

	{
		dekaf2::KProf prof("SIMD ValidUTF short");
		prof.SetMultiplier(40000);
		for (int ct = 0; ct < 40000; ++ct)
		{
			KProf::Force(&sDataShort);
			auto yes = simdutf::validate_utf8(sDataShort.data(), sDataShort.size());
			if (yes) KProf::Force();
		}
	}
#endif
	{
		dekaf2::KProf prof("ValidUTF long");
		prof.SetMultiplier(100);
		for (int ct = 0; ct < 100; ++ct)
		{
			KProf::Force(&sData);
			auto yes = kutf::Valid(sData);
			if (yes) KProf::Force();
		}
	}
	{
		dekaf2::KProf prof("ValidUTF short");
		prof.SetMultiplier(40000);
		for (int ct = 0; ct < 40000; ++ct)
		{
			KProf::Force(&sDataShort);
			auto yes = kutf::Valid(sDataShort);
			if (yes) KProf::Force();
		}
	}
#ifdef DEKAF2_HAS_UTF8CPP
	{
		dekaf2::KProf prof("UTF8CPP Valid long");
		prof.SetMultiplier(100);
		for (int ct = 0; ct < 100; ++ct)
		{
			KProf::Force(&sData);
			auto iCount = utf8::is_valid(sData.str());
			if (iCount == false) KProf::Force();
		}
	}
	{
		dekaf2::KProf prof("UTF8CPP Valid short");
		prof.SetMultiplier(40000);
		for (int ct = 0; ct < 40000; ++ct)
		{
			KProf::Force(&sDataShort);
			auto iCount = utf8::is_valid(sDataShort.str());
			if (iCount == false) KProf::Force();
		}
	}
#endif
	{
		dekaf2::KProf prof("InvalidUTF long");
		prof.SetMultiplier(100);
		for (int ct = 0; ct < 100; ++ct)
		{
			KProf::Force(&sData);
			auto yes = kutf::Invalid(sData);
			if (yes) KProf::Force();
		}
	}
	{
		dekaf2::KProf prof("InvalidUTF short");
		prof.SetMultiplier(40000);
		for (int ct = 0; ct < 40000; ++ct)
		{
			KProf::Force(&sDataShort);
			auto yes = kutf::Invalid(sDataShort);
			if (yes) KProf::Force();
		}
	}
	{
		dekaf2::KProf prof("LeftUTF short (30)");
		prof.SetMultiplier(1000);
		for (int ct = 0; ct < 1000; ++ct)
		{
			KProf::Force(&sData);
			auto it = kutf::Left(sData.begin(), sData.end(), 30);
			if (it == sData.end()) KProf::Force();
		}
	}

	{
		dekaf2::KProf prof("LeftUTF long (2048)");
		prof.SetMultiplier(100);
		for (int ct = 0; ct < 100; ++ct)
		{
			KProf::Force(&sData);
			auto it = kutf::Left(sData.begin(), sData.end(), 2048);
			if (it == sData.end()) KProf::Force();
		}
	}

	{
		dekaf2::KProf prof("LeftUTF large");
		prof.SetMultiplier(100);
		for (int ct = 0; ct < 100; ++ct)
		{
			KProf::Force(&sData);
			auto it = kutf::Left(sData.begin(), sData.end(), sData.size());
			if (it == sData.end()) KProf::Force();
		}
	}

	{
		dekaf2::KProf prof("RightUTF large");
		prof.SetMultiplier(100);
		for (int ct = 0; ct < 100; ++ct)
		{
			KProf::Force(&sData);
			auto it = kutf::Right(sData.begin(), sData.end(), sData.size());
			if (it == sData.end()) KProf::Force();
		}
	}

	{
		dekaf2::KProf prof("LeftUTF copy long (2048)");
		prof.SetMultiplier(100);
		for (int ct = 0; ct < 100; ++ct)
		{
			KProf::Force(&sData);
			KString sString = kutf::Left(sData, 2048);
			KProf::Force(&sString);
			KProf::Force();
		}
	}

	{
		dekaf2::KProf prof("Left string copy long (3048)");
		prof.SetMultiplier(100);
		for (int ct = 0; ct < 100; ++ct)
		{
			KProf::Force(&sData);
			KString sString = sData.Left(3048);
			KProf::Force(&sString);
			KProf::Force();
		}
	}

	{
		dekaf2::KProf prof("RightUTF copy long (2048)");
		prof.SetMultiplier(100);
		for (int ct = 0; ct < 100; ++ct)
		{
			KProf::Force(&sData);
			KString sString = kutf::Right(sData, 2048);
			KProf::Force(&sString);
			KProf::Force();
		}
	}

	{
		dekaf2::KProf prof("Right string copy long (2048)");
		prof.SetMultiplier(100);
		for (int ct = 0; ct < 100; ++ct)
		{
			KProf::Force(&sData);
			KString sString = sData.Right(3048);
			KProf::Force(&sString);
			KProf::Force();
		}
	}

	{
		dekaf2::KProf prof("NextCodepoint (1000)");
		prof.SetMultiplier(100);
		for (int ct = 0; ct < 100; ++ct)
		{
			KProf::Force(&sData);
			auto it = sData.begin();
			auto ie = sData.end();
			kutf::codepoint_t cp;
			for (int ict = 0; ict < 1000; ++ict)
			{
				cp = kutf::Codepoint(it, ie);
			}
			if (cp) KProf::Force();
		}
	}

	{
		dekaf2::KProf prof("NextCodepoint (2 mio)");
		prof.SetMultiplier(100);
		for (int ct = 0; ct < 100; ++ct)
		{
			KProf::Force(&sData);
			auto it = sData.begin();
			auto ie = sData.end();
			kutf::codepoint_t cp;
			for (int ict = 0; ict < 2 * 1000 * 1000; ++ict)
			{
				cp = kutf::Codepoint(it, ie);
			}
			if (cp) KProf::Force();
		}
	}

	{
		dekaf2::KProf prof("NextCodepoint (2 mio ASCII)");
		prof.SetMultiplier(100);
		for (int ct = 0; ct < 100; ++ct)
		{
			KProf::Force(&sASCII);
			auto it = sASCII.begin();
			auto ie = sASCII.end();
			kutf::codepoint_t cp;
			for (int ict = 0; ict < 2 * 1000 * 1000; ++ict)
			{
				cp = kutf::Codepoint(it, ie);
			}
			if (cp) KProf::Force();
		}
	}

#ifdef DEKAF2_HAS_SIMDUTF
	{
		dekaf2::KProf prof("ToUpperUTF SIMD short (50)");
		prof.SetMultiplier(40000);
		for (int ct = 0; ct < 40000; ++ct)
		{
			KProf::Force(&sDataShort);
			auto size = simdutf::utf16_length_from_utf8(sDataShort.data(), sDataShort.size());
			std::basic_string<char16_t> sOut;
			sOut.resize(size);
			auto iWrote = simdutf::convert_utf8_to_utf16(sDataShort.data(), sDataShort.size(), sOut.data());
			if (iWrote > 1234234098245) KProf::Force();
			std::transform(sOut.begin(), sOut.end(), sOut.begin(), [](char16_t ch){ return kToUpper(ch); });
			KString sUTF8;
			size = simdutf::utf8_length_from_utf16(sOut.data(), sOut.size());
			sUTF8.resize(size);
			iWrote = simdutf::convert_utf16_to_utf8(sOut.data(), sOut.size(), sUTF8.data());
			if (iWrote > 1234234098245) KProf::Force();
		}
	}

	{
		dekaf2::KProf prof("ToUpperUTF SIMD large");
		prof.SetMultiplier(100);
		for (int ct = 0; ct < 100; ++ct)
		{
			KProf::Force(&sData);
			auto size = simdutf::utf16_length_from_utf8(sData.data(), sData.size());
			std::basic_string<char16_t> sOut;
			sOut.resize(size);
			auto iWrote = simdutf::convert_utf8_to_utf16(sData.data(), sData.size(), sOut.data());
			if (iWrote > 1234234098245) KProf::Force();
			std::transform(sOut.begin(), sOut.end(), sOut.begin(), [](char16_t ch){ return kToUpper(ch); });
			KString sUTF8;
			size = simdutf::utf8_length_from_utf16(sOut.data(), sOut.size());
			sUTF8.resize(size);
			iWrote = simdutf::convert_utf16_to_utf8(sOut.data(), sOut.size(), sUTF8.data());
			if (iWrote > 1234234098245) KProf::Force();
		}
	}
#endif
	{
		dekaf2::KProf prof("ToUpperUTF short (50)");
		prof.SetMultiplier(40000);
		for (int ct = 0; ct < 40000; ++ct)
		{
			KProf::Force(&sDataShort);
			KString sUTF;
			kutf::ToUpper<KString>(sDataShort, sUTF);
			if (sUTF[2] == 'a') KProf::Force();
		}
	}

	{
		dekaf2::KProf prof("ToUpperUTF large");
		prof.SetMultiplier(100);
		for (int ct = 0; ct < 100; ++ct)
		{
			KProf::Force(&sData);
			KString sUTF;
			kutf::ToUpper<KString>(sData, sUTF);
			if (sUTF[2] == 'a') KProf::Force();
		}
	}

	{
		dekaf2::KProf prof("KString::ToUpper short (50)");
		prof.SetMultiplier(40000);
		for (int ct = 0; ct < 40000; ++ct)
		{
			KProf::Force(&sDataShort);
			auto sUTF = sDataShort.ToUpper();
			if (sUTF[2] == 'a') KProf::Force();
		}
	}

	{
		dekaf2::KProf prof("KString::ToUpper large");
		prof.SetMultiplier(100);
		for (int ct = 0; ct < 100; ++ct)
		{
			KProf::Force(&sData);
			auto sUTF = sData.ToUpper();
			if (sUTF[2] == 'a') KProf::Force();
		}
	}

	{
		dekaf2::KProf prof("kutf::ToUpper UTF32 large");
		prof.SetMultiplier(100);
		for (int ct = 0; ct < 100; ++ct)
		{
			KProf::Force(&sWide);
			std::u32string sTarget;
			kutf::ToUpper(sWide, sTarget);
			if (sTarget[2] == 'a') KProf::Force();
		}
	}

	{
		dekaf2::KProf prof("std::towupper UTF32 large");
		prof.SetMultiplier(100);
		for (int ct = 0; ct < 100; ++ct)
		{
			KProf::Force(&sWide);
			std::u32string sTarget;
			sTarget.resize(sWide.size());
			std::transform(sWide.begin(), sWide.end(), sTarget.begin(),[](uint32_t ch){ return std::towupper(ch); });
			if (sTarget[2] == 'a') KProf::Force();
		}
	}

	{
		dekaf2::KProf prof("std::towupper UTF8 large");
		prof.SetMultiplier(100);
		for (int ct = 0; ct < 100; ++ct)
		{
			KProf::Force(&sData);
			std::string sUpperUTF8;
			auto it = sData.begin();
			auto ie = sData.end();
			auto bi = std::back_inserter(sUpperUTF8);
			for (; it != ie; )
			{
				auto cp = std::towupper(kutf::Codepoint(it, ie));
				kutf::ToUTF(cp, bi);
			}
			if (sUpperUTF8[2] == 'a') KProf::Force();
		}
	}

	{
		dekaf2::KProf prof("KString::MakeUpper short (50)");
		prof.SetMultiplier(40000);
		for (int ct = 0; ct < 40000; ++ct)
		{
			KProf::Force(&sDataShort);
			auto sUTF = sDataShort;
			sUTF.MakeUpper();
			if (sUTF[2] == 'a') KProf::Force();
		}
	}

	{
		dekaf2::KProf prof("KString::MakeUpper large");
		prof.SetMultiplier(100);
		for (int ct = 0; ct < 100; ++ct)
		{
			KProf::Force(&sData);
			auto sUTF = sData;
			sUTF.MakeUpper();
			if (sUTF[2] == 'a') KProf::Force();
		}
	}

	{
		dekaf2::KProf prof("KString::ToUpperASCII (50)");
		prof.SetMultiplier(40000);
		for (int ct = 0; ct < 40000; ++ct)
		{
			KProf::Force(&sDataShort);
			auto sUTF = sDataShort.ToUpperASCII();
			if (sUTF[2] == 'a') KProf::Force();
		}
	}

	{
		dekaf2::KProf prof("KString::ToUpperASCII large");
		prof.SetMultiplier(100);
		for (int ct = 0; ct < 100; ++ct)
		{
			KProf::Force(&sData);
			auto sUTF = sData.ToUpperASCII();
			if (sUTF[2] == 'a') KProf::Force();
		}
	}

	{
		dekaf2::KProf prof("KString::MakeUpperASCII short (50)");
		prof.SetMultiplier(40000);
		for (int ct = 0; ct < 40000; ++ct)
		{
			KProf::Force(&sDataShort);
			auto sUTF = sDataShort;
			sUTF.MakeUpperASCII();
			if (sUTF[2] == 'a') KProf::Force();
		}
	}

	{
		dekaf2::KProf prof("KString::MakeUpperASCII large");
		prof.SetMultiplier(100);
		for (int ct = 0; ct < 100; ++ct)
		{
			KProf::Force(&sData);
			auto sUTF = sData;
			sUTF.MakeUpperASCII();
			if (sUTF[2] == 'a') KProf::Force();
		}
	}

	{
		dekaf2::KProf prof("KString::ToUpperLocale (50)");
		prof.SetMultiplier(40000);
		for (int ct = 0; ct < 40000; ++ct)
		{
			KProf::Force(&sDataShort);
			auto sUTF = sDataShort.ToUpperLocale();
			if (sUTF[2] == 'a') KProf::Force();
		}
	}

	{
		dekaf2::KProf prof("KString::ToUpperLocale large");
		prof.SetMultiplier(100);
		for (int ct = 0; ct < 100; ++ct)
		{
			KProf::Force(&sData);
			auto sUTF = sData.ToUpperLocale();
			if (sUTF[2] == 'a') KProf::Force();
		}
	}

	{
		dekaf2::KProf prof("KString::MakeUpperLocale short (50)");
		prof.SetMultiplier(40000);
		for (int ct = 0; ct < 40000; ++ct)
		{
			KProf::Force(&sDataShort);
			auto sUTF = sDataShort;
			sUTF.MakeUpperLocale();
			if (sUTF[2] == 'a') KProf::Force();
		}
	}

	{
		dekaf2::KProf prof("KString::MakeUpperLocale large");
		prof.SetMultiplier(100);
		for (int ct = 0; ct < 100; ++ct)
		{
			KProf::Force(&sData);
			auto sUTF = sData;
			sUTF.MakeUpperLocale();
			if (sUTF[2] == 'a') KProf::Force();
		}
	}

}



