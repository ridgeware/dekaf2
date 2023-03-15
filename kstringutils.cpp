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

#include "kstringutils.h"
#include "kurl.h"
#include "dekaf2.h"
#include "kregex.h"
#include "kencode.h"

namespace dekaf2
{

//-----------------------------------------------------------------------------
bool kStrIn (const char* sNeedle, const char* sHaystack, char iDelim/*=','*/)
//-----------------------------------------------------------------------------
{
	if (!sHaystack || !sNeedle)
	{
		return false;
	}

	size_t iNeedle = 0, iHaystack = 0; // Beginning indices

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

namespace detail {

// table with base36 integer values for lower and upper case ASCII
#ifdef _MSC_VER
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
KString kFormString(KStringView sInp, typename KString::value_type separator, typename KString::size_type every)
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
	return !Unicode::ValidUTF8(sBuffer);

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
 	// see https://stackoverflow.com/a/201378
	// but note that we extended the regex to match uppercase ASCII alphabetic characters
	// as well, as that is what RFC5322 and RFC1035 permit (addresses are caseless)
	static constexpr KStringView sRegex =
	R"foo((?:[a-zA-Z0-9!#$%&'*+/=?^_`{|}~-]+(?:\.[a-zA-Z0-9!#$%&'*+/=?^_`{|}~-]+)*|"(?:[\x01-\x08\x0b\x0c\x0e-\x1f\x21\x23-\x5b\x5d-\x7f]|\\[\x01-\x09\x0b\x0c\x0e-\x7f])*")@(?:(?:[a-zA-Z0-9](?:[a-zA-Z0-9-]*[a-zA-Z0-9])?\.)+[a-zA-Z0-9](?:[a-zA-Z0-9-]*[a-zA-Z0-9])?|\[(?:(?:(2(5[0-5]|[0-4][0-9])|1[0-9][0-9]|[1-9]?[0-9]))\.){3}(?:(2(5[0-5]|[0-4][0-9])|1[0-9][0-9]|[1-9]?[0-9])|[a-zA-Z0-9-]*[a-zA-Z0-9]:(?:[\x01-\x08\x0b\x0c\x0e-\x1f\x21-\x5a\x53-\x7f]|\\[\x01-\x09\x0b\x0c\x0e-\x7f])+)\]))foo";

	return KRegex::Matches(str, sRegex);

} // kIsEmail

//-----------------------------------------------------------------------------
bool kIsURL(KStringView str) noexcept
//-----------------------------------------------------------------------------
{
	return KURL(str).IsURL();
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
			KEncode::HexAppend(sLog, ch);
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

//-----------------------------------------------------------------------------
KFindSetOfChars::KFindSetOfChars(KStringView sNeedles)
//-----------------------------------------------------------------------------
{
	for (auto c : sNeedles)
	{
		m_table[static_cast<unsigned char>(c)] = true;
	}

} // ctor

//-----------------------------------------------------------------------------
KFindSetOfChars::size_type KFindSetOfChars::find_first_in_impl(KStringView sHaystack, bool bNot)
//-----------------------------------------------------------------------------
{
	auto it = std::find_if(sHaystack.begin(),
						   sHaystack.end(),
						   [this, bNot](const char c)
	{
		return m_table[static_cast<unsigned char>(c)] != bNot;
	});

	return (it == sHaystack.end()) ? KStringView::npos : static_cast<std::size_t>(it - sHaystack.begin());

} // find_first_in_impl

//-----------------------------------------------------------------------------
KFindSetOfChars::size_type KFindSetOfChars::find_last_in_impl(KStringView sHaystack, bool bNot)
//-----------------------------------------------------------------------------
{
	auto it = std::find_if(sHaystack.rbegin(),
						   sHaystack.rend(),
						   [this, bNot](const char c)
	{
		return m_table[static_cast<unsigned char>(c)] != bNot;
	});

	return (it == sHaystack.rend()) ? KStringView::npos : static_cast<std::size_t>((it.base() - 1) - sHaystack.begin());

} // find_last_in_impl

//-----------------------------------------------------------------------------
void kResizeUninitialized(KString& sStr, KString::size_type iNewSize)
//-----------------------------------------------------------------------------
{
#if defined(__cpp_lib_string_resize_and_overwrite_NOTYET) && !defined(DEKAF2_KSTRING_HAS_ACQUIRE_MALLOCATED)
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

} // end of namespace dekaf2

