/*
//
// DEKAF(tm): Lighter, Faster, Smarter(tm)
//
// Copyright (c) 2026, Ridgeware, Inc.
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
//
*/

#include "kipaddress.h"
#include "kctype.h"

DEKAF2_NAMESPACE_BEGIN

//-----------------------------------------------------------------------------
bool kIsIPv6Address(KStringView sAddress, bool bNeedsBraces)
//-----------------------------------------------------------------------------
{
	// there is a function kIsValidIPv6() in kresolve.h, but it takes three times
	// longer than this, and requires a zero-terminated string. Also, it would
	// not accept braces.
	if (bNeedsBraces)
	{
		if (!sAddress.remove_prefix('[') || !sAddress.remove_suffix(']')) return false;
	}

	auto     iEnd                = sAddress.size();
	decltype(iEnd) iIndex        = 0;
	decltype(iEnd) iLastStartDigit = 0;
	uint16_t iDigitCount         = 0;
	uint16_t iBlocks             = 0;
	bool     bHadSubst           = false;
	bool     bLastWasColon       = false;
	bool     bLastWasDoubleColon = false;
	bool     bIsStart            = true;

	for (;iIndex < iEnd; ++iIndex)
	{
		auto ch = sAddress[iIndex];

		if (ch == '.')
		{
			// an ipv6 address may have the last two values replaced by an ipv4 address
			if (iBlocks > 7) return false;
			if (!bHadSubst && iBlocks != 7) return false;
			return kIsIPv4Address(sAddress.ToView(iLastStartDigit));
		}

		if (ch == ':')
		{
			if (bLastWasColon)
			{
				if (bHadSubst)
				{
					// only one substitution by :: is permitted
					return false;
				}
				// a :: counts at least for one block
				if (++iBlocks > 8) return false;
				bLastWasDoubleColon = true;
				bHadSubst = true;
			}

			bLastWasColon = true;
			iDigitCount   = 0;
			continue;
		}

		if (bLastWasColon)
		{
			if (bIsStart && !bHadSubst) return false;
			if (++iBlocks > 8) return false;
			bIsStart = false;
			iLastStartDigit	= iIndex;
		}

		if (bIsStart)
		{
			++iBlocks; // = 1
			bIsStart = false;
			iLastStartDigit	= iIndex;
		}

		if (!KASCII::kIsXDigit(ch)) return false;
		if (++iDigitCount > 4     ) return false;

		bLastWasColon = false;
		bLastWasDoubleColon = false;
	}

	if (iBlocks == 0) return false;
	if (iBlocks < 8 && !bHadSubst) return false;
	if (bLastWasColon && !bLastWasDoubleColon) return false;

	return true;

} // kIsIPv6Address

//-----------------------------------------------------------------------------
bool kIsIPv4Address(KStringView sAddress)
//-----------------------------------------------------------------------------
{
	// there is a function kIsValidIPv4() in kresolve.h, but it takes three times
	// longer than this, and requires a zero-terminated string
	uint16_t iDigit              = 0;
	uint16_t iBlocks             = 0;
	bool     bLastWasDot         = true;

	for (auto ch : sAddress)
	{
		if (ch == '.')
		{
			if (bLastWasDot) return false;

			bLastWasDot = true;
			iDigit      = 0;
			continue;
		}

		if (bLastWasDot)
		{
			bLastWasDot = false;
			if (++iBlocks > 4) return false;
		}

		if (!KASCII::kIsDigit(ch)) return false;
		iDigit *= 10;
		iDigit += ch - '0';
		if (iDigit > 255) return false;
	}

	if (iBlocks != 4)  return false;
	if (bLastWasDot)  return false;

	return true;

} // kIsIPv4Address

//-----------------------------------------------------------------------------
bool kIsPrivateIP(KStringView sIP, bool bExcludeDocker)
//-----------------------------------------------------------------------------
{
	bool bIsV6 = false;

	if (kIsIPv6Address(sIP, sIP.front() == '['))
	{
		if (sIP.front() == '[')
		{
			sIP.remove_suffix(1);
			sIP.remove_prefix(1);
		}

		if (sIP.starts_with("::ffff:") && sIP.rfind(':') == 6)
		{
			// ::ffff:192.168.1.1
			sIP.remove_prefix(7);
		}
		else
		{
			bIsV6 = true;
		}
	}

	kDebug(2, sIP);

	if (bIsV6)
	{
		if (sIP == "::1") return true;
		if (sIP.starts_with("fd")) return true;
		if (sIP.starts_with("fe80::")) return true;
	}
	else
	{
		if (kIsIPv4Address(sIP))
		{
			if (sIP.starts_with("127.")) return true;
			if (sIP.starts_with("192.168.")) return true;
			if (sIP.starts_with("172."))
			{
				// docker normally sits at 172.*.*.* with its internal network
				// and translates external IP addresses into just that one
				if (bExcludeDocker) return false;
				sIP.remove_prefix(4);
				sIP.remove_suffix(sIP.size() - sIP.find('.'));
				auto iIP = sIP.UInt16();
				return (iIP >= 16 && iIP <= 31);
			}
			if (sIP.starts_with("10.")) return true;
		}
	}

	return false;

} // kIsPrivateIP

DEKAF2_NAMESPACE_END
