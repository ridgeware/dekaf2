/*
//
// DEKAF(tm): Lighter, Faster, Smarter (tm)
//
// Copyright (c) 2017, Ridgeware, Inc.
//
// +-------------------------------------------------------------------------+
// | /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\|
// |/+---------------------------------------------------------------------+/|
// |/|                                                                     |/|
// |\|  ** THIS NOTICE MUST NOT BE REMOVED FROM THE SOURCE CODE MODULE **  |\|
// |/|                                                                     |/|
// |\|   OPEN SOURCE LICENSE                                               |\|
// |/|                                                                     |/|
// |\|   Permission is hereby granted, free of charge, to any person       |\|
// |/|   obtaining a copy of this software and associated                  |/|
// |\|   documentation files (the "Software"), to deal in the              |\|
// |/|   Software without restriction, including without limitation        |/|
// |\|   the rights to use, copy, modify, merge, publish,                  |\|
// |/|   distribute, sublicense, and/or sell copies of the Software,       |/|
// |\|   and to permit persons to whom the Software is furnished to        |\|
// |/|   do so, subject to the following conditions:                       |/|
// |\|                                                                     |\|
// |/|   The above copyright notice and this permission notice shall       |/|
// |\|   be included in all copies or substantial portions of the          |\|
// |/|   Software.                                                         |/|
// |\|                                                                     |\|
// |/|   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY         |/|
// |\|   KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE        |\|
// |/|   WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR           |/|
// |\|   PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS        |\|
// |/|   OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR          |/|
// |\|   OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR        |\|
// |/|   OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE         |/|
// |\|   SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.            |\|
// |/|                                                                     |/|
// |/+---------------------------------------------------------------------+/|
// |\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/ |
// +-------------------------------------------------------------------------+
*/

#include "kstringview.h"   // keep kstringview.h on top of kstringutils.h
#include "kstringutils.h"  // or gcc < 10 produces circular references!
#include "khex.h"
#if DEKAF2_HAS_INCLUDE("kurl.h")
	#include "kurl.h"
#endif
#if DEKAF2_HAS_INCLUDE("kregex.h")
	#include "kregex.h"
#endif

DEKAF2_NAMESPACE_BEGIN

//----------------------------------------------------------------------
std::size_t kReplace(KStringRef& sString,
					 KStringRef::value_type chSearch,
					 KStringRef::value_type chReplace,
					 KStringRef::size_type pos,
					 bool bReplaceAll)
//----------------------------------------------------------------------
{
	KStringRef::size_type iReplaced{0};

	while ((pos = sString.find(chSearch, pos)) != npos)
	{
		sString[pos] = chReplace;
		++pos;
		++iReplaced;

		if (!bReplaceAll)
		{
			break;
		}
	}

	return iReplaced;

} // kReplace

//----------------------------------------------------------------------
std::size_t kReplace(KStringRef& sString,
					 const KStringView sSearch,
					 const KStringView sReplace,
					 KStringRef::size_type pos,
					 bool bReplaceAll)
//----------------------------------------------------------------------
{
	const auto iSize = sString.size();

	if (DEKAF2_UNLIKELY(pos >= iSize))
	{
		return 0;
	}

	const auto iSearchSize = sSearch.size();

	if (DEKAF2_UNLIKELY(sSearch.empty() || iSize - pos < iSearchSize))
	{
		return 0;
	}

	const auto iReplaceSize = sReplace.size();

	if (DEKAF2_UNLIKELY(iSearchSize == 1 && iReplaceSize == 1))
	{
		return kReplace(sString, sSearch.front(), sReplace.front(), pos, bReplaceAll);
	}

	KStringRef::size_type iNumReplacement = 0;
	// use a non-const ref to the first element, as .data() is const with C++ < 17
	KStringRef::value_type* haystack = &sString[pos];
	KStringRef::size_type haystackSize = iSize - pos;

	auto pszFound = static_cast<KStringRef::value_type*>(memmem(haystack, haystackSize, sSearch.data(), iSearchSize));

	if (DEKAF2_LIKELY(pszFound != nullptr))
	{

		if (iReplaceSize <= iSearchSize)
		{
			// execute an in-place substitution

			auto pszTarget = haystack;

			while (pszFound)
			{
				auto untouchedSize = static_cast<KStringRef::size_type>(pszFound - haystack);

				if (pszTarget < haystack)
				{
					std::memmove(pszTarget, haystack, untouchedSize);
				}

				pszTarget += untouchedSize;

				if (DEKAF2_LIKELY(iReplaceSize != 0))
				{
					std::memmove(pszTarget, sReplace.data(), iReplaceSize);
					pszTarget += iReplaceSize;
				}

				haystack = pszFound + iSearchSize;
				haystackSize -= (iSearchSize + untouchedSize);

				pszFound = static_cast<KStringRef::value_type*>(memmem(haystack, haystackSize, sSearch.data(), iSearchSize));

				++iNumReplacement;

				if (DEKAF2_UNLIKELY(bReplaceAll == false))
				{
					break;
				}
			}

			if (DEKAF2_LIKELY(haystackSize > 0))
			{
				std::memmove(pszTarget, haystack, haystackSize);
				pszTarget += haystackSize;
			}

			auto iResultSize = static_cast<KStringRef::size_type>(pszTarget - sString.data());
			sString.resize(iResultSize);

		}
		else
		{
			// execute a copy substitution

			KString sResult;
			// make room for at least one replace without realloc
			sResult.reserve(iSize + iReplaceSize - iSearchSize);

			if (DEKAF2_UNLIKELY(pos))
			{
				// copy the skipped part of the source string
				sResult.append(sString, 0, pos);
			}

			while (pszFound)
			{
				auto untouchedSize = static_cast<KStringRef::size_type>(pszFound - haystack);
				sResult.append(haystack, untouchedSize);
				sResult.append(sReplace.data(), iReplaceSize);

				haystack = pszFound + iSearchSize;
				haystackSize -= (iSearchSize + untouchedSize);

				pszFound = static_cast<KStringRef::value_type*>(memmem(haystack, haystackSize, sSearch.data(), iSearchSize));

				++iNumReplacement;

				if (DEKAF2_UNLIKELY(bReplaceAll == false))
				{
					break;
				}
			}

			sResult.append(haystack, haystackSize);
			sString.swap(sResult);

		}
	}

	return iNumReplacement;

} // kReplace

namespace detail {

//----------------------------------------------------------------------
void kTransform(const char* it, const char* ie, char* out, char(*transformer)(char))
//----------------------------------------------------------------------
{
	for (; std::distance(it, ie) >= 8; )
	{
		*out++ = transformer(*it++);
		*out++ = transformer(*it++);
		*out++ = transformer(*it++);
		*out++ = transformer(*it++);
		*out++ = transformer(*it++);
		*out++ = transformer(*it++);
		*out++ = transformer(*it++);
		*out++ = transformer(*it++);
	}

	for (; it != ie; )
	{
		*out++ = transformer(*it++);
	}
}

//----------------------------------------------------------------------
void kMakeUpperASCII(const char* it, const char* ie, char* out)
//----------------------------------------------------------------------
{
	kTransform(it, ie, out, &KASCII::kToUpper);
}

//----------------------------------------------------------------------
void kMakeLowerASCII(const char* it, const char* ie, char* out)
//----------------------------------------------------------------------
{
	kTransform(it, ie, out, &KASCII::kToLower);
}

namespace {

//----------------------------------------------------------------------
char kToUpperLocale(char ch)
//----------------------------------------------------------------------
{
	return static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
}

//----------------------------------------------------------------------
char kToLowerLocale(char ch)
//----------------------------------------------------------------------
{
	return static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
}

}

//----------------------------------------------------------------------
void kMakeUpperLocale(const char* it, const char* ie, char* out)
//----------------------------------------------------------------------
{
	kTransform(it, ie, out, &kToUpperLocale);
}

//----------------------------------------------------------------------
void kMakeLowerLocale(const char* it, const char* ie, char* out)
//----------------------------------------------------------------------
{
	kTransform(it, ie, out, &kToLowerLocale);
}

} // end of namespace detail

//----------------------------------------------------------------------
KString kToUpper(KStringView sInput)
//----------------------------------------------------------------------
{
	if (kutf::ValidASCII(sInput))
	{
		return kToUpperASCII(sInput);
	}
	else
	{
		KString sUpper;
		kutf::ToUpper(sInput, sUpper);
		return sUpper;
	}
}

//----------------------------------------------------------------------
KString kToLower(KStringView sInput)
//----------------------------------------------------------------------
{
	if (kutf::ValidASCII(sInput))
	{
		return kToLowerASCII(sInput);
	}
	else
	{
		KString sLower;
		kutf::ToLower(sInput, sLower);
		return sLower;
	}
}

//----------------------------------------------------------------------
KString kToUpperASCII(KStringView sInput)
//----------------------------------------------------------------------
{
	KString sTransformed;
	sTransformed.resize(sInput.size());

	auto it  = sInput.begin();
	auto ie  = sInput.end();
	auto out = sTransformed.begin();

	kMakeUpperASCII(it, ie, out);

	return sTransformed;
}

//----------------------------------------------------------------------
KString kToLowerASCII(KStringView sInput)
//----------------------------------------------------------------------
{
	KString sTransformed;
	sTransformed.resize(sInput.size());

	auto it  = sInput.begin();
	auto ie  = sInput.end();
	auto out = sTransformed.begin();

	kMakeLowerASCII(it, ie, out);

	return sTransformed;
}

//----------------------------------------------------------------------
KString kToUpperLocale(KStringView sInput)
//----------------------------------------------------------------------
{
	KString sTransformed;
	sTransformed.resize(sInput.size());

	auto it  = sInput.begin();
	auto ie  = sInput.end();
	auto out = sTransformed.begin();

	kMakeUpperLocale(it, ie, out);

	return sTransformed;
}

//----------------------------------------------------------------------
KString kToLowerLocale(KStringView sInput)
//----------------------------------------------------------------------
{
	KString sTransformed;
	sTransformed.resize(sInput.size());

	auto it  = sInput.begin();
	auto ie  = sInput.end();
	auto out = sTransformed.begin();

	kMakeLowerLocale(it, ie, out);

	return sTransformed;
}

//-----------------------------------------------------------------------------
bool kStrIn (KStringView sNeedle, KStringView sHaystack, char iDelim)
//-----------------------------------------------------------------------------
{
	std::size_t iNeedle = 0;
	std::size_t iHaystack = 0; // Beginning indices
	std::size_t iNsize = sNeedle.size ();
	std::size_t iHsize = sHaystack.size (); // Ending

	while (iHaystack < iHsize)
	{
		iNeedle = 0;

		// Search for matching tokens
		while ( (iNeedle < iNsize)
			   && (iHaystack < iHsize)
			   && (sNeedle[iNeedle] == sHaystack[iHaystack]))
		{
			++iNeedle;
			++iHaystack;
		}

		// If end of needle or haystack at delimiter or end of haystack
		if ((iNeedle >= iNsize) &&
			(iHaystack >= iHsize || (sHaystack[iHaystack] == iDelim)))
		{
			return true;
		}

		// Advance to next delimiter
		while (iHaystack < iHsize && sHaystack[iHaystack] != iDelim)
		{
			++iHaystack;
		}

		// skip delimiter (if present, otherwise we are at end of string)
		++iHaystack;
	}

	return false;

} // kStrIn

//-----------------------------------------------------------------------------
bool kStrIn (const char* sNeedle, const char* sHaystack, char iDelim/*=','*/)
//-----------------------------------------------------------------------------
{
	if (!sHaystack || !sNeedle)
	{
		return false;
	}

	std::size_t iNeedle = 0, iHaystack = 0; // Beginning indices

	while (sHaystack[iHaystack])
	{
		iNeedle = 0;

		// Search for matching tokens
		while (  sNeedle[iNeedle] &&
			   (sNeedle[iNeedle] == sHaystack[iHaystack]))
		{
			++iNeedle;
			++iHaystack;
		}

		// If end of needle or haystack at delimiter or end of haystack
		if ( !sNeedle[iNeedle] &&
			((sHaystack[iHaystack] == iDelim) || !sHaystack[iHaystack]))
		{
			return true;
		}

		// Advance to next delimiter
		while (sHaystack[iHaystack] && sHaystack[iHaystack] != iDelim)
		{
			++iHaystack;
		}

		// Pass by the delimiter if it exists
		iHaystack += (!!sHaystack[iHaystack]);
	}
	return false;

} // kStrIn

//----------------------------------------------------------------------
bool kStrIn (KStringView sNeedle, const char* Haystack[])
//----------------------------------------------------------------------
{
	for (std::size_t ii=0; Haystack[ii] && *(Haystack[ii]); ++ii)
	{
		if (!strncmp(sNeedle.data(), Haystack[ii], sNeedle.size()))
		{
			return (true);
		}
	}

	return (false); // not found

} // kStrIn

//-----------------------------------------------------------------------------
KStringView kFirstWord(const KStringView& sInput)
//-----------------------------------------------------------------------------
{
	KStringView::size_type iPos = 0;
	// loop over all whitespace
	while (iPos < sInput.size() && KASCII::kIsSpace(sInput[iPos])) ++iPos;
	auto iStart = iPos;
	// collect all chars until next whitespace
	while (iPos < sInput.size() && !KASCII::kIsSpace(sInput[iPos])) ++iPos;
	// copy all leading non-whitespace
	return sInput.ToView(iStart, iPos - iStart);

} // kFirstWord


namespace detail {

// table with base36 integer values for lower and upper case ASCII
#ifdef DEKAF2_IS_MSC
const uint8_t LookupBase36[256] =
#else
constexpr uint8_t LookupBase36[256] =
#endif
{
	/* 0x00 */ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	/* 0x10 */ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	/* 0x20 */ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	/* 0x30 */ 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	/* 0x40 */ 0xFF, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
	/* 0x50 */ 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	/* 0x60 */ 0xFF, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
	/* 0x70 */ 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	/* 0x80 */ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	/* 0x90 */ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	/* 0xA0 */ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	/* 0xB0 */ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	/* 0xC0 */ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	/* 0xD0 */ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	/* 0xE0 */ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	/* 0xF0 */ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
};

} // end of namespace detail

//-----------------------------------------------------------------------------
KString kFormString(KStringView sInp, KString::value_type separator, KString::size_type every)
//-----------------------------------------------------------------------------
{
	KString result{sInp};

	if (every > 0)
	{
		// now insert the separator every N digits
		auto last = every;
		auto pos = result.length();
		while (pos > last)
		{
			result.insert(pos-every, 1, separator);
			pos -= every;
		}
	}

	return result;

} // kFormString

//-----------------------------------------------------------------------------
bool kIsBinary(KStringView sBuffer)
//-----------------------------------------------------------------------------
{
	return !kutf::Valid(sBuffer);

} // kIsBinary

//-----------------------------------------------------------------------------
bool kIsInteger(KStringView str, bool bSigned) noexcept
//-----------------------------------------------------------------------------
{
	if (str.empty())
	{
		return false;
	}

	const auto* start = str.data();
	const auto* buf   = start;
	size_t size       = str.size();
	bool bHadDigit { false };

	while (size--)
	{
		if (!KASCII::kIsDigit(*buf++))
		{
			if (!size || buf != start + 1 || ((!bSigned || *start != '-') && *start != '+'))
			{
				return false;
			}
		}
		else
		{
			bHadDigit = true;
		}
	}

	return bHadDigit;

} // kIsInteger

//-----------------------------------------------------------------------------
bool kIsFloat(KStringView str, KStringView::value_type chDecimalSeparator) noexcept
//-----------------------------------------------------------------------------
{
	if (str.empty())
	{
		return false;
	}

	const auto* start = str.data();
	const auto* buf   = start;
	size_t size       = str.size();
	bool bDeciSeen    = false;
	bool bHadDigit    = false;

	while (size--)
	{
		if (!KASCII::kIsDigit(*buf))
		{
			if (bDeciSeen)
			{
				return false;
			}
			else if (chDecimalSeparator == *buf)
			{
				bDeciSeen = true;
			}
			else if (buf != start || (*start != '-' && *start != '+'))
			{
				return false;
			}
		}
		else
		{
			bHadDigit = true;
		}
		++buf;
	}

	// to qualify as a float we need a decimal separator (we do not accept exponential representations)
	return bDeciSeen && bHadDigit;

} // kIsFloat

//-----------------------------------------------------------------------------
bool kIsEmail(KStringView str) noexcept
//-----------------------------------------------------------------------------
{
#if DEKAF2_HAS_INCLUDE("kregex.h")
 	// see https://stackoverflow.com/a/201378
	// but note that we extended the regex to match uppercase ASCII alphabetic characters
	// as well, as that is what RFC5322 and RFC1035 permit (addresses are caseless)
	static constexpr KStringView sRegex =
	R"foo((?:[a-zA-Z0-9!#$%&'*+/=?^_`{|}~-]+(?:\.[a-zA-Z0-9!#$%&'*+/=?^_`{|}~-]+)*|"(?:[\x01-\x08\x0b\x0c\x0e-\x1f\x21\x23-\x5b\x5d-\x7f]|\\[\x01-\x09\x0b\x0c\x0e-\x7f])*")@(?:(?:[a-zA-Z0-9](?:[a-zA-Z0-9-]*[a-zA-Z0-9])?\.)+[a-zA-Z0-9](?:[a-zA-Z0-9-]*[a-zA-Z0-9])?|\[(?:(?:(2(5[0-5]|[0-4][0-9])|1[0-9][0-9]|[1-9]?[0-9]))\.){3}(?:(2(5[0-5]|[0-4][0-9])|1[0-9][0-9]|[1-9]?[0-9])|[a-zA-Z0-9-]*[a-zA-Z0-9]:(?:[\x01-\x08\x0b\x0c\x0e-\x1f\x21-\x5a\x53-\x7f]|\\[\x01-\x09\x0b\x0c\x0e-\x7f])+)\]))foo";

	return KRegex::Matches(str, sRegex);
#else
	return false;
#endif

} // kIsEmail

//-----------------------------------------------------------------------------
bool kIsURL(KStringView str) noexcept
//-----------------------------------------------------------------------------
{
#if DEKAF2_HAS_INCLUDE("kurl.h")
	return KURL(str).IsURL();
#else
	return false;
#endif
}

//-----------------------------------------------------------------------------
void kEscapeForLogging(KStringRef& sLog, KStringView sInput)
//-----------------------------------------------------------------------------
{
	sLog.reserve(sLog.size() + sInput.size());
	
	for (auto ch : sInput)
	{
		auto ctype = KASCII::kCharType(ch);

		if (DEKAF2_UNLIKELY(KASCII::kIsSpace(ctype)))
		{
			switch (ch)
			{
				case ' ':
					sLog += ' ';
					break;

				case '\r':
					sLog += "\\r";
					break;

				case '\n':
					sLog += "\\n";
					break;

				case '\t':
					sLog += "\\t";
					break;

				case '\b':
					sLog += "\\b";
					break;

				case '\v':
					sLog += "\\v";
					break;

				case '\f':
					sLog += "\\f";
					break;
			}
		}
		else if (DEKAF2_UNLIKELY(KASCII::kIsCntrl(ctype)))
		{
			// escape non-printable characters..
			sLog += '\\';
			sLog += 'x';
			kHexAppend(sLog, ch);
		}
		else if (DEKAF2_UNLIKELY(ch == '\\' || ch == '"'))
		{
			sLog += '\\';
			sLog += ch;
		}
		else
		{
			sLog += ch;
		}
	}

} // kEscapeForLogging

#ifndef DEKAF2_KSTRING_IS_STD_STRING
//-----------------------------------------------------------------------------
void kResizeUninitialized(KString& sStr, KString::size_type iNewSize)
//-----------------------------------------------------------------------------
{
#if defined(__cpp_lib_string_resize_and_overwrite_NOTYET)
	// with C++23 we will get the equivalence of what we used to do with FBString:
	// resizing the string buffer uninitialized, with a handler to set its content
	// (which we won't do)
	sStr.str().resize_and_overwrite(iNewSize, [](std::string::pointer buf, std::string::size_type buf_size) noexcept { return buf_size; });
#else
	#ifdef DEKAF2_KSTRING_HAS_ACQUIRE_MALLOCATED
	static constexpr KString::size_type LARGEST_SSO = 23;

	// never do this optimization for SSO strings
	if (iNewSize > LARGEST_SSO)
	{
		auto iSize = sStr.size();

		if (iNewSize > iSize)
		{
			auto iUninitialized = iNewSize - iSize;

			if (!iSize || (iUninitialized / iSize > 2))
			{
				// We can do this trick only with FBString, as std::string does not
				// offer a matching constructor: we malloc enough memory for the full
				// buffer, memcopy the existing string (if any) into it, construct a
				// new fbstring from that partly uninitialized buffer, and swap the string.

				// reserve the buffer
				char* buf = static_cast<char*>(std::malloc(iSize + iUninitialized + 1)); // do not forget the 0-terminator

				if (buf)
				{
					// copy existing string content into
					std::memcpy(buf, sStr.data(), iSize);
					// set 0 at end of buffer
					buf[iSize + iUninitialized] = 0;
					// construct string on the buffer
					KString sNew(buf, iSize + iUninitialized, iSize + iUninitialized + 1, KString::AcquireMallocatedString{});
					// and swap it in for the existing string
					sStr.swap(sNew);
					// that's it
					return;
				}
			}
		}
	}
	#endif

	// fallback to an initialized resize
	sStr.resize(iNewSize);
#endif

} // kResizeUninitialized
#endif

//-----------------------------------------------------------------------------
void kResizeUninitialized(std::string& sStr, std::string::size_type iNewSize)
//-----------------------------------------------------------------------------
{
#ifdef __cpp_lib_string_resize_and_overwrite_NOTYET
	sStr.resize_and_overwrite(iNewSize, [](std::string::pointer buf, std::string::size_type buf_size) noexcept { return buf_size; });
#else
	// fallback to an initialized resize
	sStr.resize(iNewSize);
#endif

} // kResizeUninitialized

//-----------------------------------------------------------------------------
KString kCurlyToStraight(KStringView sInput)
//-----------------------------------------------------------------------------
{
	KString sTransformed;
	sTransformed.reserve(sInput.size());

	kutf::ForEach (sInput, [&sTransformed](kutf::codepoint_t ch)
	{
		// fix "leaning" quotes and doubles to be plain ascii:
		switch (ch)
		{
			// single quotes
			case 0x2018:
			case 0x2019:
			case 0x201A:
			case 0x201B:
			case 0x2039:
			case 0x203A:
				ch = '\'';
				break;

			// double quotes
			case 0x201C:
			case 0x201D:
			case 0x00AB: // «
			case 0x00BB: // »
				ch = '"';
				break;

			// ~
			case 0x223C:
				ch = '~';
				break;

			// space
			case 0x00A0:
			case 0x2000:
			case 0x2001:
			case 0x2002:
			case 0x2003:
			case 0x2004:
			case 0x2005:
			case 0x2006:
			case 0x2007:
			case 0x2008:
				ch = ' ';
				break;

			// hyphen / minus
			case 0x2010:
			case 0x2011:
			case 0x2212:
				ch = '-';
				break;

			default:
				break;
		}

		kutf::ToUTF (ch, sTransformed);

		return true;
	});

	return sTransformed;

} // kCurlyToStraight

//-----------------------------------------------------------------------------
bool kHasUTF8BOM(KStringView sInput)
//-----------------------------------------------------------------------------
{
	return sInput.size() >= 3 &&
	    static_cast<uint8_t>(sInput[0]) == 0xef &&
	    static_cast<uint8_t>(sInput[1]) == 0xbb &&
	    static_cast<uint8_t>(sInput[2]) == 0xbf;

} // kHasUTF8BOM

//-----------------------------------------------------------------------------
bool kHasUTF8BOM(KInStream& InStream, bool bSkipIfExisting)
//-----------------------------------------------------------------------------
{
	uint8_t iMustUnread { 0 };
	bool    bResult { false };

	auto ch = InStream.Read();

	if (ch == 0xef)
	{
		ch = InStream.Read();

		if (ch == 0xbb)
		{
			ch = InStream.Read();

			if (ch == 0xbf)
			{
				if (bSkipIfExisting)
				{
					return true;
				}
				else
				{
					bResult = true;
				}
			}
			iMustUnread = 3;
		}
		else
		{
			iMustUnread = 2;
		}
	}
	else
	{
		iMustUnread = 1;
	}

	if (std::istream::traits_type::eq_int_type(ch, std::istream::traits_type::eof()))
	{
		--iMustUnread;
	}

	if (iMustUnread)
	{
		auto iCouldNotUnread = InStream.UnRead(iMustUnread);

		if (iCouldNotUnread)
		{
			kWarning("could not unread {} of {} characters", iCouldNotUnread, iMustUnread);
		}
	}

	return bResult;

} // kHasUTF8BOM

//-----------------------------------------------------------------------------
KStringView kSkipUTF8BOM(KStringView sInput)
//-----------------------------------------------------------------------------
{
	if (kHasUTF8BOM(sInput))
	{
		sInput.remove_prefix(3);
	}

	return sInput;

} // kSkipUTF8BOM

//-----------------------------------------------------------------------------
KInStream& kSkipUTF8BOM(KInStream& InStream)
//-----------------------------------------------------------------------------
{
	kHasUTF8BOM(InStream, true);
	return InStream;

} // kSkipUTF8BOM

//-----------------------------------------------------------------------------
bool kSkipUTF8BOMInPlace(KStringRef& sInput)
//-----------------------------------------------------------------------------
{
	if (kHasUTF8BOM(sInput))
	{
		sInput.erase(0, 3);
		return true;
	}
	else
	{
		return false;
	}

} // kSkipUTF8BOMInPlace

//-----------------------------------------------------------------------------
KOutStream& kWriteUTF8BOM(KOutStream& Out)
//-----------------------------------------------------------------------------
{
	return Out.Write(kWriteUTF8BOM());
}

//-----------------------------------------------------------------------------
KStringView kWriteUTF8BOM()
//-----------------------------------------------------------------------------
{
	return KStringView { "\xef\xbb\xbf" };
}

namespace detail
{

//-----------------------------------------------------------------------------
KString kFormScaledUnsignedNumber (
	uint64_t    iNumber,
	uint16_t    iPrecision,
	KStringView sSeparator,
	uint16_t    iDivisor,
	uint16_t    iMaxDigits,
	KStringView sUnits,
	bool        bIsNegative
)
//-----------------------------------------------------------------------------
{
	static constexpr uint64_t maxvals[16] {
		9, 9, 99, 999, 9999, 99999, 999999, 9999999, 99999999, 999999999, 9999999999,
		99999999999, 999999999999, 9999999999999, 99999999999999, 999999999999999
	};

	KString sNumber;
	uint16_t    iMagnitude { 0 };
	std::size_t iDivideBy  { 1 };
	uint64_t    iOrigBytes { iNumber };
	uint64_t    iMaxNumber { maxvals[iMaxDigits & 0x0f] };

	while (iNumber > iMaxNumber && iMagnitude < sUnits.size() - 1)
	{
		iNumber   /= iDivisor;
		iDivideBy *= iDivisor;
		++iMagnitude;
	}

	sNumber = kFormat("{}{:.{}f}",
	                  bIsNegative ? "-" : "",
	                  double(iOrigBytes) / iDivideBy,
	                  iPrecision);

	sNumber.remove_suffix(".0");

	if (!sSeparator.empty())
	{
		sNumber += sSeparator;
	}

	sNumber += sUnits[iMagnitude];

	return sNumber;

} // kFormScaledUnsignedNumber

} // end of detail namespace

DEKAF2_NAMESPACE_END
