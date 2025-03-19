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
#include <dekaf2/kutf8.h>

#ifdef DEKAF2_HAS_ICONV
#include <iconv.h>
#endif
#ifdef DEKAF2_HAS_SIMDUTF
#include "simdutf.h"
#endif
//#define HAVE_TEXTUTILS 1
#ifdef HAVE_TEXTUTILS
#include "textutils/UtfConv.h"
#endif

using namespace dekaf2;

#ifdef DEKAF2_HAS_ICONV
void iconv_to_utf32(const KString& sData, KString& sOutBuffer)
{
	static iconv_t s_cd = iconv_open("UTF-32", "UTF-8");

	auto buf8 = sData.data();
	auto len8 = sData.size();
	auto buf32 = sOutBuffer.data();
	auto len32 = sOutBuffer.size();

	if (!::iconv(s_cd, (char **)&buf8, &len8, &buf32, &len32))
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

		} while (Unicode::IsSurrogate(ch));
		Unicode::ToUTF(ch, sData);
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

		} while (Unicode::IsSurrogate(ch));
		Unicode::ToUTF(ch, sDataShort);
	}

	kPrintLine("Time: {}", kFormTimestamp(KLocalTime::now(), "{:%Y-%m-%d %H:%M:%S}"));
	kPrintLine("long  size: {} bytes, {} codepoints, valid={}", sData.size(),      Unicode::CountUTF(sData),      Unicode::ValidUTF(sData)     );
	kPrintLine("short size: {} bytes, {} codepoints, valid={}", sDataShort.size(), Unicode::CountUTF(sDataShort), Unicode::ValidUTF(sDataShort));

	auto sWide      = Unicode::ConvertUTF<std::u32string>(sData);
	auto sWideShort = Unicode::ConvertUTF<std::u32string>(sDataShort);
#ifdef HAVE_TEXTUTILS
	auto sWide32      = Unicode::ConvertUTF<std::basic_string<UnicodeChar32>>(sData);
	auto sWideShort32 = Unicode::ConvertUTF<std::basic_string<UnicodeChar32>>(sDataShort);
#endif

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
			auto iCount = Unicode::CountUTF(sData.begin(), sData.end());
			if (iCount > 100000) KProf::Force();
		}
	}
	{
		dekaf2::KProf prof("CountUTF short");
		prof.SetMultiplier(40000);
		for (int ct = 0; ct < 40000; ++ct)
		{
			KProf::Force(&sDataShort);
			auto iCount = Unicode::CountUTF(sDataShort.begin(), sDataShort.end());
			if (iCount > 100000) KProf::Force();
		}
	}

	{
		dekaf2::KProf prof("CountUTFMax long");
		prof.SetMultiplier(100);
		for (int ct = 0; ct < 100; ++ct)
		{
			KProf::Force(&sData);
			auto iCount = Unicode::CountUTF(sData, sData.size());
			if (iCount > 100000) KProf::Force();
		}
	}
	{
		dekaf2::KProf prof("CountUTFMax short");
		prof.SetMultiplier(40000);
		for (int ct = 0; ct < 40000; ++ct)
		{
			KProf::Force(&sDataShort);
			auto iCount = Unicode::CountUTF(sDataShort, sDataShort.size());
			if (iCount > 100000) KProf::Force();
		}
	}
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
		dekaf2::KProf prof("ConvertUTF long");
		prof.SetMultiplier(100);
		for (int ct = 0; ct < 100; ++ct)
		{
			KProf::Force(&sData);
			auto sWide = Unicode::ConvertUTF<std::basic_string<char32_t>>(sData);
			if (sWide.size() > 100000) KProf::Force();
		}
	}
	{
		dekaf2::KProf prof("ConvertUTF short");
		prof.SetMultiplier(40000);
		for (int ct = 0; ct < 40000; ++ct)
		{
			KProf::Force(&sDataShort);
			auto sWide = Unicode::ConvertUTF<std::basic_string<char32_t>>(sDataShort);
			if (sWide.size() > 100000) KProf::Force();
		}
	}
#ifdef HAVE_TEXTUTILS
	{
		dekaf2::KProf prof("UTF8ToUTF32 long");
		prof.SetMultiplier(100);
		for (int ct = 0; ct < 100; ++ct)
		{
			KProf::Force(&sData);
			auto sWide = utf_conv::UTF8ToUTF32(sData.str());
			if (sWide.size() > 100000) KProf::Force();
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
#ifdef DEKAF2_HAS_ICONV
	{
		dekaf2::KProf prof("iconv8to32 long");
		prof.SetMultiplier(100);
		for (int ct = 0; ct < 100; ++ct)
		{
			KProf::Force(&sData);
			KString sOut;
			sOut.resize(sData.size());
			iconv_to_utf32(sData, sOut);
			if (sWide.size() > 100000) KProf::Force();
		}
	}
	{
		dekaf2::KProf prof("iconv8to32 short");
		prof.SetMultiplier(40000);
		for (int ct = 0; ct < 40000; ++ct)
		{
			KProf::Force(&sDataShort);
			KString sOut;
			sOut.resize(sDataShort.size());
			iconv_to_utf32(sDataShort, sOut);
			if (sWide.size() > 100000) KProf::Force();
		}
	}
#endif
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
			auto yes = Unicode::ValidUTF(sData);
			if (yes) KProf::Force();
		}
	}
	{
		dekaf2::KProf prof("ValidUTF short");
		prof.SetMultiplier(40000);
		for (int ct = 0; ct < 40000; ++ct)
		{
			KProf::Force(&sDataShort);
			auto yes = Unicode::ValidUTF(sDataShort);
			if (yes) KProf::Force();
		}
	}
	{
		dekaf2::KProf prof("InvalidUTF long");
		prof.SetMultiplier(100);
		for (int ct = 0; ct < 100; ++ct)
		{
			KProf::Force(&sData);
			auto yes = Unicode::InvalidUTF(sData);
			if (yes) KProf::Force();
		}
	}
	{
		dekaf2::KProf prof("InvalidUTF short");
		prof.SetMultiplier(40000);
		for (int ct = 0; ct < 40000; ++ct)
		{
			KProf::Force(&sDataShort);
			auto yes = Unicode::InvalidUTF(sDataShort);
			if (yes) KProf::Force();
		}
	}
	{
		dekaf2::KProf prof("LeftUTF short (30)");
		prof.SetMultiplier(1000);
		for (int ct = 0; ct < 1000; ++ct)
		{
			KProf::Force(&sData);
			auto it = Unicode::LeftUTF(sData.begin(), sData.end(), 30);
			if (it == sData.end()) KProf::Force();
		}
	}

	{
		dekaf2::KProf prof("LeftUTF long (2048)");
		prof.SetMultiplier(100);
		for (int ct = 0; ct < 100; ++ct)
		{
			KProf::Force(&sData);
			auto it = Unicode::LeftUTF(sData.begin(), sData.end(), 2048);
			if (it == sData.end()) KProf::Force();
		}
	}

	{
		dekaf2::KProf prof("LeftUTF large");
		prof.SetMultiplier(100);
		for (int ct = 0; ct < 100; ++ct)
		{
			KProf::Force(&sData);
			auto it = Unicode::LeftUTF(sData.begin(), sData.end(), sData.size());
			if (it == sData.end()) KProf::Force();
		}
	}

	{
		dekaf2::KProf prof("RightUTF large");
		prof.SetMultiplier(100);
		for (int ct = 0; ct < 100; ++ct)
		{
			KProf::Force(&sData);
			auto it = Unicode::RightUTF(sData.begin(), sData.end(), sData.size());
			if (it == sData.end()) KProf::Force();
		}
	}

	{
		dekaf2::KProf prof("LeftUTF copy long (2048)");
		prof.SetMultiplier(100);
		for (int ct = 0; ct < 100; ++ct)
		{
			KProf::Force(&sData);
			KString sString = Unicode::LeftUTF(sData, 2048);
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
			KString sString = Unicode::RightUTF(sData, 2048);
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
			Unicode::codepoint_t cp;
			for (int ict = 0; ict < 1000; ++ict)
			{
				cp = Unicode::Codepoint(it, ie);
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
			Unicode::codepoint_t cp;
			for (int ict = 0; ict < 2 * 1000 * 1000; ++ict)
			{
				cp = Unicode::Codepoint(it, ie);
			}
			if (cp) KProf::Force();
		}
	}

#ifdef DEKAF2_HAS_SIMDUTF
	{
		dekaf2::KProf prof("SIMDUTF32to8 short (50)");
		prof.SetMultiplier(40000);
		for (int ct = 0; ct < 40000; ++ct)
		{
			KProf::Force(&sWideShort);
			KString sUTF8;
			auto size = simdutf::utf8_length_from_utf32(sWideShort.data(), sWideShort.size());
			sUTF8.resize(size);
			auto iWrote = simdutf::convert_utf32_to_utf8(sWideShort.data(), sWideShort.size(), sUTF8.data());
			if (iWrote > 100000) KProf::Force();
		}
	}

	{
		dekaf2::KProf prof("SIMDUTF32to8 large");
		prof.SetMultiplier(100);
		for (int ct = 0; ct < 100; ++ct)
		{
			KProf::Force(&sWide);
			KString sUTF8;
			auto size = simdutf::utf8_length_from_utf32(sWide.data(), sWide.size());
			sUTF8.resize(size);
			auto iWrote = simdutf::convert_utf32_to_utf8(sWide.data(), sWide.size(), sUTF8.data());
			if (iWrote > 100000) KProf::Force();
		}
	}
#endif
	{
		dekaf2::KProf prof("ConvertUTF short (50)");
		prof.SetMultiplier(40000);
		for (int ct = 0; ct < 40000; ++ct)
		{
			KProf::Force(&sWideShort);
			auto sUTF = Unicode::ConvertUTF<KString>(sWideShort);
			if (sUTF[2] == 'a') KProf::Force();
		}
	}

	{
		dekaf2::KProf prof("ConvertUTF large");
		prof.SetMultiplier(100);
		for (int ct = 0; ct < 100; ++ct)
		{
			KProf::Force(&sWide);
			auto sUTF = Unicode::ConvertUTF<KString>(sWide);
			if (sUTF[2] == 'a') KProf::Force();
		}
	}
#ifdef HAVE_TEXTUTILS
	{
		dekaf2::KProf prof("UTF32ToUTF8 short (50)");
		prof.SetMultiplier(40000);
		for (int ct = 0; ct < 40000; ++ct)
		{
			KProf::Force(&sWideShort);
			auto sUTF = utf_conv::UTF32ToUTF8(sWideShort32);
			if (sUTF[2] == 'a') KProf::Force();
		}
	}

	{
		dekaf2::KProf prof("UTF32ToUTF8 large");
		prof.SetMultiplier(100);
		for (int ct = 0; ct < 100; ++ct)
		{
			KProf::Force(&sWide);
			auto sUTF = utf_conv::UTF32ToUTF8(sWide32);
			if (sUTF[2] == 'a') KProf::Force();
		}
	}
#endif
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
			Unicode::ToUpperUTF<KString>(sDataShort, sUTF);
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
			Unicode::ToUpperUTF<KString>(sData, sUTF);
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



