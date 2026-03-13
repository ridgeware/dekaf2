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
#include "kcompatibility.h"
#include "kctype.h"
#include "kstringutils.h"

#ifdef DEKAF2_IS_WINDOWS
	#include <iphlpapi.h>
	#ifndef IF_NAMESIZE
		#define IF_NAMESIZE 40
	#endif
#else
	#include <net/if.h>
#endif

DEKAF2_NAMESPACE_BEGIN

//-----------------------------------------------------------------------------
KIPAddress4::Bytes4 KIPAddress4::FromString(KStringView sAddress, KIPError& ec) noexcept
//-----------------------------------------------------------------------------
{
	BytesT   IP;
	uint16_t iByte       {     0 };
	uint16_t iBlocks     {     0 };
	bool     bLastWasDot {  true };

	for (auto ch : sAddress)
	{
		if (ch == '.')
		{
			if (bLastWasDot) break;
			bLastWasDot   = true;
			IP[iBlocks-1] = iByte;
			iByte         = 0;
			continue;
		}

		if (bLastWasDot)
		{
			bLastWasDot = false;
			if (++iBlocks > 4) break;
		}

		if (!KASCII::kIsDigit(ch)) break;
		iByte *= 10;
		iByte += ch - '0';
		if (iByte > 255) break;
	}

	if (iBlocks == 4 && iByte <= 255 && !bLastWasDot)
	{
		IP[iBlocks-1] = iByte;
	}
	else
	{
		ec = KIPError("KIPAddress4: invalid IPv4 address");
		IP = Bytes4 { 0, 0, 0, 0};
	}

	return IP;

} // KIPAddress4::FromString

//-----------------------------------------------------------------------------
KIPAddress4::Bytes4 KIPAddress4::FromString(KStringView sAddress)
//-----------------------------------------------------------------------------
{
	KIPError ec;
	auto Bytes = FromString(sAddress, ec);
	if (ec) throw ec;
	return Bytes;

} // KIPAddress4::FromString

//-----------------------------------------------------------------------------
KString KIPAddress4::ToString (const value_type* IP) noexcept
//-----------------------------------------------------------------------------
{
	return kFormat("{}.{}.{}.{}", IP[0], IP[1], IP[2], IP[3]);

} // KIPAddress4::ToString

//-----------------------------------------------------------------------------
KIPAddress4::Bytes4 KIPAddress4::FromIPv6(const KIPAddress6& IPv6)
//-----------------------------------------------------------------------------
{
	KIPError ec;
	auto IP = FromIPv6(IPv6, ec);
	if (ec) throw ec;
	return IP;

} // KIPAddress4::FromIPv6

//-----------------------------------------------------------------------------
void KIPAddress4::Dec(value_type* IP) noexcept
//-----------------------------------------------------------------------------
{
	for (int i = 3; i >= 0; --i)
	{
		if (IP[i] > 0)
		{
			--IP[i];
			break;
		}

		IP[i] = 0xff;
	}

} // KIPAddress4::Dec

//-----------------------------------------------------------------------------
void KIPAddress4::Inc(value_type* IP) noexcept
//-----------------------------------------------------------------------------
{
	for (int i = 3; i >= 0; --i)
	{
		if (IP[i] < 0xff)
		{
			++IP[i];
			break;
		}

		IP[i] = 0;
	}

} // KIPAddress4::Inc

//-----------------------------------------------------------------------------
KIPAddress4 KIPAddress4::operator++(int) noexcept
//-----------------------------------------------------------------------------
{
	auto tmp(*this);
	++*this;
	return tmp;
}

//-----------------------------------------------------------------------------
KIPAddress4 KIPAddress4::operator--(int) noexcept
//-----------------------------------------------------------------------------
{
	auto tmp(*this);
	--*this;
	return tmp;
}

//-----------------------------------------------------------------------------
KIPAddress4 KIPAddress4::Prev() const noexcept
//-----------------------------------------------------------------------------
{
	auto IP = *this;
	IP.Dec();
	return IP;

} // KIPAddress4::Prev

//-----------------------------------------------------------------------------
KIPAddress4 KIPAddress4::Next() const noexcept
//-----------------------------------------------------------------------------
{
	auto IP = *this;
	IP.Inc();
	return IP;

} // KIPAddress4::Inc

//-----------------------------------------------------------------------------
/// resolve a zone string to a numeric scope ID
KIPAddress6::ScopeT KIPAddress6::ResolveScope(KStringView sZone) noexcept
//-----------------------------------------------------------------------------
{
	if (sZone.empty())
	{
		return 0;
	}

	// try numeric scope ID first
	if (KASCII::kIsDigit(sZone.front()))
	{
		KIPAddress6::ScopeT iScope { 0 };

		for (auto ch : sZone)
		{
			if (!KASCII::kIsDigit(ch)) return 0;
			iScope = iScope * 10 + (ch - '0');
		}

		return iScope;
	}

	// resolve interface name to index
	// if_nametoindex needs a null-terminated string - sZone is a view into a
	// potentially larger string, so copy it
	std::array<char, IF_NAMESIZE + 1> buf;
	strcpy_n(buf.data(), sZone.data(), buf.size());

	return static_cast<KIPAddress6::ScopeT>(::if_nametoindex(buf.data()));

} // ResolveScope

//-----------------------------------------------------------------------------
KIPAddress6::Parsed6 KIPAddress6::FromString(KStringView sAddress, KIPError& ec) noexcept
//-----------------------------------------------------------------------------
{
	Parsed6 parsed;

#define DEKAF2_DEBUG_KIPADDRESS 0
#if DEKAF2_DEBUG_KIPADDRESS
	// to debug, set IP to pattern - should have no effect:
	parsed.IP = BytesT { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x12 };
#endif

	if (sAddress.remove_prefix('[') && !sAddress.remove_suffix(']'))
	{
		ec = KIPError("KIPAddress6: unpaired []");
		parsed.IP = Empty;
		return parsed;
	}

	// the shortest possible IPv6 address is :: (2 chars),
	// the max with scope suffix is well below 255 bytes
	if (sAddress.size() > 255 || sAddress.size() < 2)
	{
		if (sAddress != "::")
		{
			ec = KIPError("KIPAddress6: invalid size");
		}
		parsed.IP = Empty;
		return parsed;
	}

	constexpr uint8_t iInvalid { 255 };

	uint_fast16_t iWord               { 0 };
	uint_fast8_t  iNibbleCount        { 0 };
	uint_fast8_t  iIndex              { 0 };
	uint_fast8_t  iEnd                { static_cast<uint_fast8_t>(sAddress.size()) };
	uint_fast8_t  iBlock              { 0 };
	uint_fast8_t  iDoubleColonBlock   { iInvalid };
	uint_fast8_t  iLastColons         { 0 };

	for (;;)
	{
		auto ch = sAddress[iIndex];

		if (ch == ':')
		{
			if (++iLastColons > 2)
			{
				ec = KIPError("KIPAddress6: more than two consecutive colons");
				parsed.IP = Empty;
				return parsed;
			}

			if (iNibbleCount > 0)
			{
				if (iBlock > 7)
				{
					ec = KIPError("KIPAddress6: invalid block count");
					parsed.IP = Empty;
					return parsed;
				}
				// we have a value to store
				parsed.IP[iBlock * 2    ] = static_cast<uint8_t>(iWord >> 8);
				parsed.IP[iBlock * 2 + 1] = static_cast<uint8_t>(iWord);
				++iBlock;
				iWord = 0;
				iNibbleCount = 0;
			}

			if (iLastColons == 2)
			{
				if (iDoubleColonBlock != iInvalid || iBlock > 7) // set to > 6 to suppress 1:..6::x
				{
					// only one substitution by :: is permitted
					// a :: counts at least for one block
					ec = KIPError("KIPAddress6: more than one ::");
					parsed.IP = Empty;
					return parsed;
				}
				// mark the block at which the double colon emerged
				iDoubleColonBlock = iBlock;
			}
		}
		else if (ch == '.')
		{
			// an ipv6 address may have the last two values replaced by an ipv4 address
			if (!iNibbleCount     ||
				 iNibbleCount > 3 ||
				 iBlock > 6       ||
				(iDoubleColonBlock == iInvalid && iBlock != 6))
			{
				ec = KIPError("KIPAddress6: invalid mapped IPv4");
				parsed.IP = Empty;
				return parsed;
			}

			auto iStartv4 = iIndex;

			// we can only have (hex)digits or colons before the current iIndex
			while (iStartv4 > 0 && sAddress[iStartv4 - 1] != ':')
			{
				--iStartv4;
			}

			KIPAddress4 IPv4(sAddress.Mid(iStartv4), ec);

			if (ec)
			{
				ec = KIPError("KIPAddress6: invalid mapped IPv4");
				parsed.IP = Empty;
				return parsed;
			}

			parsed.IP[iBlock * 2    ] = IPv4.m_IP[0];
			parsed.IP[iBlock * 2 + 1] = IPv4.m_IP[1];
			++iBlock;
			parsed.IP[iBlock * 2    ] = IPv4.m_IP[2];
			parsed.IP[iBlock * 2 + 1] = IPv4.m_IP[3];
			++iBlock;

			// the whole string was sent to KIPAddressv4
			iNibbleCount = 0;
			break;
		}
		else if (ch == '%')
		{
			// scope suffix per RFC 9844 - stop parsing the address here
			// and resolve the interface name
			parsed.Scope = ResolveScope(sAddress.ToView(++iIndex));
			break;
		}
		else
		{
			if (iBlock == 0 && iNibbleCount == 0 && iLastColons == 1)
			{
				ec = KIPError("KIPAddress6: leading :");
				parsed.IP = Empty;
				return parsed;
			}
			else if (iBlock > 7)
			{
				ec = KIPError("KIPAddress6: invalid block count");
				parsed.IP = Empty;
				return parsed;
			}

			auto iNibble = kFromHexChar(ch);

			if (iNibble > 15 || ++iNibbleCount > 4)
			{
				ec = KIPError("KIPAddress6: invalid hex");
				parsed.IP = Empty;
				return parsed;
			}

			iWord <<= 4;
			iWord += iNibble;

			iLastColons = 0;
		}

		// end of string reached?
		if (++iIndex >= iEnd) break;

	} // for (;;)

	if (iNibbleCount)
	{
		if (iBlock > 7)
		{
			ec = KIPError("KIPAddress6: invalid block count");
			parsed.IP = Empty;
			return parsed;
		}
		// fill last word
		parsed.IP[iBlock * 2    ] = static_cast<uint8_t>(iWord >> 8);
		parsed.IP[iBlock * 2 + 1] = static_cast<uint8_t>(iWord);
		++iBlock;
	}

	if (    iBlock  > 8
		|| (iBlock == 8 && iDoubleColonBlock != iInvalid)
		|| (iBlock  < 8 && iDoubleColonBlock == iInvalid)
		||  iLastColons == 1)
	{
		ec = KIPError("KIPAddress6: invalid block count");
		parsed.IP = Empty;
		return parsed;
	}

	if (iDoubleColonBlock != iInvalid)
	{
		// move the blocks after the double colon, coming from the end
		auto iGap = 8 - iBlock;
		auto iMin = iDoubleColonBlock + iGap;

		for (uint8_t i = 7; i >= iMin; --i)
		{
			parsed.IP[i * 2]     = parsed.IP[(i - iGap) * 2];
			parsed.IP[i * 2 + 1] = parsed.IP[(i - iGap) * 2 + 1];
		}

		// and zeroize all gaps that were not yet touched
		for (auto i = iDoubleColonBlock; i < iMin; ++i)
		{
			parsed.IP[i * 2]     = 0;
			parsed.IP[i * 2 + 1] = 0;
		}
	}

	return parsed;

} // KIPAddress6::FromString

//-----------------------------------------------------------------------------
KIPAddress6::Parsed6 KIPAddress6::FromString(KStringView sAddress)
//-----------------------------------------------------------------------------
{
	KIPError ec;
	auto parsed = FromString(sAddress, ec);
	if (ec) throw ec;
	return parsed;

} // KIPAddress6::FromString

//-----------------------------------------------------------------------------
KString KIPAddress6::ToString (bool bWithBraces, bool bUnabridged) const noexcept
//-----------------------------------------------------------------------------
{
	KString sIP;

	if (bWithBraces)
	{
		sIP = '[';
	}

	if (bUnabridged)
	{
		sIP += kFormat("{:02x}{:02x}:{:02x}{:02x}:{:02x}{:02x}:{:02x}{:02x}:"
					   "{:02x}{:02x}:{:02x}{:02x}:{:02x}{:02x}:{:02x}{:02x}",
						m_IP[0],  m_IP[1],  m_IP[2],  m_IP[3],
						m_IP[4],  m_IP[5],  m_IP[6],  m_IP[7],
						m_IP[8],  m_IP[9],  m_IP[10], m_IP[11],
						m_IP[12], m_IP[13], m_IP[14], m_IP[15]);
	}
	else
	{
		if (IsV4Mapped())
		{
			KIPAddress4 IPv4( KIPAddress4::BytesT { m_IP[12], m_IP[13], m_IP[14], m_IP[15] } );
			sIP += "::ffff:";
			sIP += IPv4.ToString();
		}
		else
		{
			uint8_t iStart { 255 };
			uint8_t iSize  {   0 };

			uint8_t iCurStart { 255 };
			uint8_t iCurSize  {   0 };
			uint8_t iPos      {   0 };

			// find the longest sequence of 0000
			for (auto it = m_IP.begin(); it != m_IP.end(); ++iPos, ++it)
			{
				if (*it++ == 0 && *it == 0)
				{
					if (iCurStart == 255)
					{
						iCurStart = iPos;
						iCurSize  = 1;
					}
					else
					{
						++iCurSize;
					}
				}
				else
				{
					if (iCurStart != 255)
					{
						if (iCurSize > iSize)
						{
							iStart = iCurStart;
							iSize  = iCurSize;
						}
					}
					iCurStart = 255;
				}
			}

			// the last iteration
			if (iCurStart != 255)
			{
				if (iCurSize > iSize)
				{
					iStart = iCurStart;
					iSize  = iCurSize;
				}
			}

			if (iStart == 7 && iSize == 1)
			{
				iStart = 255;
			}

			bool bNeedColon { false };

			for (auto it = m_IP.begin(); it != m_IP.end(); ++it)
			{
				if (iStart == 0 && iSize > 0)
				{
					++it;

					if (!--iSize)
					{
						sIP += "::";
					}

					bNeedColon = false;
				}
				else
				{
					--iStart;

					if (bNeedColon)
					{
						sIP += ':';
					}

					uint16_t i = *it << 8;
					i += *++it;
					sIP += kIntToString(i, 16);
					bNeedColon = true;
				}
			}
		}
	}

	if (m_Scope)
	{
		sIP += '%';
		sIP += kIntToString(m_Scope);
	}

	if (bWithBraces)
	{
		sIP += ']';
	}

	return sIP;

} // KIPAddress6::ToString

//-----------------------------------------------------------------------------
KIPAddress6& KIPAddress6::Dec() noexcept
//-----------------------------------------------------------------------------
{
	for (int i = 15; i >= 0; --i)
	{
		if (m_IP[i] > 0)
		{
			--m_IP[i];
			break;
		}

		m_IP[i] = 0xff;
	}

	return *this;

} // KIPAddress6::Dec

//-----------------------------------------------------------------------------
KIPAddress6& KIPAddress6::Inc() noexcept
//-----------------------------------------------------------------------------
{
	for (int i = 15; i >= 0; --i)
	{
		if (m_IP[i] < 0xff)
		{
			++m_IP[i];
			break;
		}

		m_IP[i] = 0;
	}

	return *this;

} // KIPAddress6::Inc

//-----------------------------------------------------------------------------
KIPAddress6 KIPAddress6::operator++(int) noexcept
//-----------------------------------------------------------------------------
{
	auto tmp(*this);
	++*this;
	return tmp;
}

//-----------------------------------------------------------------------------
KIPAddress6 KIPAddress6::operator--(int) noexcept
//-----------------------------------------------------------------------------
{
	auto tmp(*this);
	--*this;
	return tmp;
}

//-----------------------------------------------------------------------------
KIPAddress6 KIPAddress6::Prev() const noexcept
//-----------------------------------------------------------------------------
{
	auto IP = *this;
	IP.Dec();
	return IP;

} // KIPAddress6::Prev

//-----------------------------------------------------------------------------
KIPAddress6 KIPAddress6::Next() const noexcept
//-----------------------------------------------------------------------------
{
	auto IP = *this;
	IP.Inc();
	return IP;

} // KIPAddress6::Next

//-----------------------------------------------------------------------------
KIPAddress KIPAddress::FromString(KStringView sAddress, KIPError& ec) noexcept
//-----------------------------------------------------------------------------
{
	KIPAddress4 a4(sAddress, ec);

	if (!ec)
	{
		return KIPAddress(std::move(a4));
	}

	ec = KIPError();

	KIPAddress6 a6(sAddress, ec);

	if (!ec)
	{
		return KIPAddress(std::move(a6));
	}

	return {};

} // KIPAddress::FromString

//-----------------------------------------------------------------------------
KIPAddress KIPAddress::FromString(KStringView sAddress)
//-----------------------------------------------------------------------------
{
	KIPError ec;
	auto IP = FromString(sAddress, ec);
	if (ec) throw ec;
	return IP;

} // KIPAddress::FromString

//-----------------------------------------------------------------------------
KIPAddress& KIPAddress::Dec() noexcept
//-----------------------------------------------------------------------------
{
	     if (Is4()) KIPAddress4::Dec(GetValuePtr4());
	else if (Is6()) m_IP.Dec();
	return *this;

} // KIPAddress::Dec

//-----------------------------------------------------------------------------
KIPAddress& KIPAddress::Inc() noexcept
//-----------------------------------------------------------------------------
{
	     if (Is4()) KIPAddress4::Inc(GetValuePtr4());
	else if (Is6()) m_IP.Inc();
	return *this;

} // KIPAddress::Inc

//-----------------------------------------------------------------------------
KIPAddress KIPAddress::operator++(int) noexcept
//-----------------------------------------------------------------------------
{
	auto tmp(*this);
	++*this;
	return tmp;
}

//-----------------------------------------------------------------------------
KIPAddress KIPAddress::operator--(int) noexcept
//-----------------------------------------------------------------------------
{
	auto tmp(*this);
	--*this;
	return tmp;
}

//-----------------------------------------------------------------------------
KIPAddress KIPAddress::Prev() const noexcept
//-----------------------------------------------------------------------------
{
	auto IP = *this;
	IP.Dec();
	return IP;

} // KIPAddress6::Prev

//-----------------------------------------------------------------------------
KIPAddress KIPAddress::Next() const noexcept
//-----------------------------------------------------------------------------
{
	auto IP = *this;
	IP.Inc();
	return IP;

} // KIPAddress::Next

//-----------------------------------------------------------------------------
bool kIsIPv6Address(KStringView sAddress, bool bNeedsBraces)
//-----------------------------------------------------------------------------
{
	if (bNeedsBraces && (!sAddress.remove_prefix('[') || !sAddress.remove_suffix(']')))
	{
		return false;
	}

	// the shortest possible IPv6 address is ::0 etc,
	// the max (without network) is 45 with IPv4 mapping
	// (0000:0000:0000:0000:0000:ffff:123.111.100.143)
	if (sAddress.size() > 45 || sAddress.size() < 3)
	{
		// "::" is a "valid" address - all naught
		return sAddress == "::";
	}

	constexpr uint8_t iInvalid { 255 };

	uint8_t  iNibbleCount        { 0 };
	uint8_t  iIndex              { 0 };
	uint8_t  iEnd                { static_cast<uint8_t>(sAddress.size()) };
	uint8_t  iBlock              { 0 };
	uint8_t  iDoubleColonBlock   { iInvalid };
	uint8_t  iLastColons         { 0 };

	for (;;)
	{
		auto ch = sAddress[iIndex];

		if (ch == ':')
		{
			if (++iLastColons > 2)
			{
				return false;
			}

			if (iNibbleCount > 0)
			{
				if (iBlock > 7)
				{
					return false;
				}
				// we have a value to store
				++iBlock;
				iNibbleCount = 0;
			}

			if (iLastColons == 2)
			{
				if (iDoubleColonBlock != iInvalid || iBlock > 7) // set to > 6 to suppress 1:..6::x
				{
					// only one substitution by :: is permitted
					// a :: counts at least for one block
					return false;
				}
				// mark the block at which the double colon emerged
				iDoubleColonBlock = iBlock;
			}
		}
		else if (ch == '.')
		{
			// an ipv6 address may have the last two values replaced by an ipv4 address
			if (!iNibbleCount     ||
				iNibbleCount > 3 ||
				iBlock > 6       ||
				(iDoubleColonBlock == iInvalid && iBlock != 6))
			{
				return false;
			}

			auto iStartv4 = iIndex;

			// we can only have (hex)digits or colons before the current iIndex
			while (iStartv4 > 0 && sAddress[iStartv4 - 1] != ':')
			{
				--iStartv4;
			}

			if (!kIsIPv4Address(sAddress.Mid(iStartv4)))
			{
				return false;
			}

			++iBlock;
			++iBlock;

			// the whole string was sent to KIPAddressv4
			iNibbleCount = 0;
			break;
		}
		else
		{
			if (iBlock > 7)
			{
				return false;
			}

			auto iNibble = kFromHexChar(ch);

			if (iNibble > 15 || ++iNibbleCount > 4)
			{
				return false;
			}

			iLastColons = 0;
		}

		// end of string reached?
		if (++iIndex >= iEnd) break;

	} // for (;;)

	if (iNibbleCount)
	{
		if (iBlock > 7)
		{
			return false;
		}
		// fill last word
		++iBlock;
	}

	if (    iBlock  > 8
		|| (iBlock == 8 && iDoubleColonBlock != iInvalid)
		|| (iBlock  < 8 && iDoubleColonBlock == iInvalid)
		||  iLastColons == 1)
	{
		return false;
	}

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

#if DEKAF2_REPEAT_CONSTEXPR_VARIABLE
constexpr KIPAddress6::BytesT KIPAddress6::Empty;
#endif

DEKAF2_NAMESPACE_END
