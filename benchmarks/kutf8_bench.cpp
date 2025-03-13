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

#include <iconv.h>
#ifdef DEKAF2_HAS_SIMDUTF
#include "simdutf.h"
#endif
#undef HAVE_TEXTUTILS
#ifdef HAVE_TEXTUTILS
#include "textutils/UtfConv.h"
#endif

using namespace dekaf2;

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

void kutf8_bench()
{
	KString sData;

	for (std::size_t i = 0; i < 2 * 1000 * 1000; ++i)
	{
		uint32_t ch;
		do
		{
			ch = kRandom(0, 0x0110000);
//			ch = kRandom(0, 0x0800);

		} while (Unicode::IsSurrogate(ch));
		Unicode::ToUTF8(ch, sData);
	}
	KOut.FormatLine("Time: {}", kFormTimestamp(KLocalTime::now(), "{:%Y-%m-%d %H:%M:%S}"));
	KOut.FormatLine("string size: {} bytes, {} codepoints, valid={}", sData.size(), Unicode::CountUTF8(sData), Unicode::ValidUTF8(sData));

	auto sWide     = Unicode::FromUTF8(sData);

	// surrogate
//	sData.insert(0, "\xed\xad\xbf"_ksv);
//	sWide.insert(0, 1, 0x0DB7FUL);

	auto sWide1000 = sWide.substr(0, 1000);

	dekaf2::KProf ps("-KUTF8");

	{
		dekaf2::KProf prof("strlen");
		prof.SetMultiplier(100);
		for (int ct = 0; ct < 100; ++ct)
		{
			uint64_t iSum = 0;
			KProf::Force(&sData);
			for (auto ch : sData) iSum += ch;
			if (iSum > 100000) KProf::Force();
		}
	}
#ifdef DEKAF2_HAS_SIMDUTF
	{
		dekaf2::KProf prof("simdutf count");
		prof.SetMultiplier(100);
		for (int ct = 0; ct < 100; ++ct)
		{
			KProf::Force(&sData);
			auto iCount = simdutf::count_utf8(sData.data(), sData.size());
			if (iCount > 100000) KProf::Force();
		}
	}
#endif
	{
		dekaf2::KProf prof("LazyCountUTF8");
		prof.SetMultiplier(100);
		for (int ct = 0; ct < 100; ++ct)
		{
			KProf::Force(&sData);
			auto iCount = Unicode::LazyCountUTF8(sData.begin(), sData.end());
			if (iCount > 100000) KProf::Force();
		}
	}

	{
		dekaf2::KProf prof("CountUTF8");
		prof.SetMultiplier(100);
		for (int ct = 0; ct < 100; ++ct)
		{
			KProf::Force(&sData);
			auto iCount = Unicode::CountUTF8(sData);
			if (iCount > 100000) KProf::Force();
		}
	}
#ifdef HAVE_TEXTUTILS
	{
		dekaf2::KProf prof("CountUTF8ToUTF32");
		prof.SetMultiplier(100);
		for (int ct = 0; ct < 100; ++ct)
		{
			KProf::Force(&sData);
			auto iCount = utf_conv::UTF8ToUTF32(sData.data(), static_cast<unsigned int>(sData.size()), NULL, NULL, 0, NULL, 0);
			if (iCount > 100000) KProf::Force();
		}
	}
#endif
#ifdef DEKAF2_HAS_SIMDUTF
	{
		dekaf2::KProf prof("SIMDUTF8to32");
		prof.SetMultiplier(100);
		for (int ct = 0; ct < 100; ++ct)
		{
			KProf::Force(&sData);
			auto expected_utf32words = simdutf::utf32_length_from_utf8(sData.data(), sData.size());
			std::basic_string<char32_t> sOut;
			sOut.resize(expected_utf32words);
			auto iWrote = simdutf::convert_utf8_to_utf32(sData.data(), sData.size(), sOut.data());
			if (iWrote > 100000) KProf::Force();
		}
	}
#endif
	{
		dekaf2::KProf prof("FromUTF8");
		prof.SetMultiplier(100);
		for (int ct = 0; ct < 100; ++ct)
		{
			KProf::Force(&sData);
			auto sWide = Unicode::FromUTF8<std::basic_string<char32_t>>(sData);
			if (sWide.size() > 100000) KProf::Force();
		}
	}
#ifdef HAVE_TEXTUTILS
	{
		dekaf2::KProf prof("UTF8ToUTF32");
		prof.SetMultiplier(100);
		for (int ct = 0; ct < 100; ++ct)
		{
			KProf::Force(&sData);
			auto sWide = utf_conv::UTF8ToUTF32(sData.str());
			if (sWide.size() > 100000) KProf::Force();
		}
	}
#endif
	{
		dekaf2::KProf prof("iconv8to32");
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
		dekaf2::KProf prof("ValidUTF8");
		prof.SetMultiplier(100);
		for (int ct = 0; ct < 100; ++ct)
		{
			KProf::Force(&sData);
			auto yes = Unicode::ValidUTF8(sData);
			if (yes) KProf::Force();
		}
	}

	{
		dekaf2::KProf prof("LeftUTF8 short (30)");
		prof.SetMultiplier(1000);
		for (int ct = 0; ct < 1000; ++ct)
		{
			KProf::Force(&sData);
			auto it = Unicode::LeftUTF8(sData.begin(), sData.end(), 30);
			if (it == sData.end()) KProf::Force();
		}
	}

	{
		dekaf2::KProf prof("LeftUTF8 long (2048)");
		prof.SetMultiplier(100);
		for (int ct = 0; ct < 100; ++ct)
		{
			KProf::Force(&sData);
			auto it = Unicode::LeftUTF8(sData.begin(), sData.end(), 2048);
			if (it == sData.end()) KProf::Force();
		}
	}

	{
		dekaf2::KProf prof("LeftUTF8 large (999999)");
		prof.SetMultiplier(100);
		for (int ct = 0; ct < 100; ++ct)
		{
			KProf::Force(&sData);
			auto it = Unicode::LeftUTF8(sData.begin(), sData.end(), 999999);
			if (it == sData.end()) KProf::Force();
		}
	}

	{
		dekaf2::KProf prof("RightUTF8 large (999999)");
		prof.SetMultiplier(100);
		for (int ct = 0; ct < 100; ++ct)
		{
			KProf::Force(&sData);
			auto it = Unicode::RightUTF8(sData.begin(), sData.end(), 999999);
			if (it == sData.end()) KProf::Force();
		}
	}

	{
		dekaf2::KProf prof("LeftUTF8 copy long (2048)");
		prof.SetMultiplier(100);
		for (int ct = 0; ct < 100; ++ct)
		{
			KProf::Force(&sData);
			KString sString = Unicode::LeftUTF8(sData, 2048);
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
		dekaf2::KProf prof("RightUTF8 copy long (2048)");
		prof.SetMultiplier(100);
		for (int ct = 0; ct < 100; ++ct)
		{
			KProf::Force(&sData);
			KString sString = Unicode::RightUTF8(sData, 2048);
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
		dekaf2::KProf prof("NextCodepointFromUTF8 (1000)");
		prof.SetMultiplier(100);
		for (int ct = 0; ct < 100; ++ct)
		{
			KProf::Force(&sData);
			auto it = sData.begin();
			auto ie = sData.end();
			Unicode::codepoint_t cp;
			for (int ict = 0; ict < 1000; ++ict)
			{
				cp = Unicode::CodepointFromUTF8(it, ie);
			}
			if (cp) KProf::Force();
		}
	}

	{
		dekaf2::KProf prof("NextCodepointFromUTF8 (2 mio)");
		prof.SetMultiplier(100);
		for (int ct = 0; ct < 100; ++ct)
		{
			KProf::Force(&sData);
			auto it = sData.begin();
			auto ie = sData.end();
			Unicode::codepoint_t cp;
			for (int ict = 0; ict < 2 * 1000 * 1000; ++ict)
			{
				cp = Unicode::CodepointFromUTF8(it, ie);
			}
			if (cp) KProf::Force();
		}
	}

	{
		dekaf2::KProf prof("ToUTF8 short (1000)");
		prof.SetMultiplier(100);
		for (int ct = 0; ct < 100; ++ct)
		{
			KProf::Force(&sData);
			auto sUTF = Unicode::ToUTF8<KString>(sWide1000);
			if (sUTF[2] == 'a') KProf::Force();
		}
	}

	{
		dekaf2::KProf prof("ToUTF8 large");
		prof.SetMultiplier(100);
		for (int ct = 0; ct < 100; ++ct)
		{
			KProf::Force(&sData);
			auto sUTF = Unicode::ToUTF8<KString>(sWide);
			if (sUTF[2] == 'a') KProf::Force();
		}
	}

}



