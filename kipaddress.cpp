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
#include "kstringutils.h"

DEKAF2_NAMESPACE_BEGIN

//-----------------------------------------------------------------------------
KIPAddress4::BytesT KIPAddress4::FromString(KStringView sAddress, KIPError& ec) noexcept
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
		IP = BytesT { 0, 0, 0, 0};
	}

	return IP;

} // KIPAddress4::FromString

//-----------------------------------------------------------------------------
KIPAddress4::BytesT KIPAddress4::FromString(KStringView sAddress)
//-----------------------------------------------------------------------------
{
	KIPError ec;
	auto Bytes = FromString(sAddress, ec);
	if (ec) throw ec;
	return Bytes;

} // KIPAddress4::FromString

//-----------------------------------------------------------------------------
KIPAddress4::BytesT KIPAddress4::FromIPv6(const KIPAddress6& IPv6)
//-----------------------------------------------------------------------------
{
	KIPError ec;
	auto IP = FromIPv6(IPv6, ec);
	if (ec) throw ec;
	return IP;

} // KIPAddress4::FromIPv6

//-----------------------------------------------------------------------------
KString KIPAddress4::ToString () const noexcept
//-----------------------------------------------------------------------------
{
	return kFormat("{}.{}.{}.{}", m_IP[0], m_IP[1], m_IP[2], m_IP[3]);

} // KIPAddress4::ToString

//-----------------------------------------------------------------------------
KIPAddress4& KIPAddress4::Dec() noexcept
//-----------------------------------------------------------------------------
{
	for (int i = 3; i >= 0; --i)
	{
		if (m_IP[i] > 0)
		{
			--m_IP[i];
			break;
		}

		m_IP[i] = 0xff;
	}

	return *this;

} // KIPAddress4::Dec

//-----------------------------------------------------------------------------
KIPAddress4& KIPAddress4::Inc() noexcept
//-----------------------------------------------------------------------------
{
	for (int i = 3; i >= 0; --i)
	{
		if (m_IP[i] < 0xff)
		{
			++m_IP[i];
			break;
		}

		m_IP[i] = 0;
	}

	return *this;

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
KIPAddress6::BytesT KIPAddress6::FromString(KStringView sAddress, KIPError& ec) noexcept
//-----------------------------------------------------------------------------
{
#define DEKAF2_DEBUG_KIPADDRESS 0
#if DEKAF2_DEBUG_KIPADDRESS
	// to debug, set IP to pattern - should have no effect:
	BytesT IP { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x12 };
#else
	BytesT IP;
#endif

	constexpr BytesT Empty { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

	if (sAddress.remove_prefix('[') && !sAddress.remove_suffix(']'))
	{
		ec = KIPError("KIPAddress6: unpaired []");
		return Empty;
	}

	// the shortest possible IPv6 address is ::0 etc,
	// the max (without network) is 45 with IPv4 mapping
	// (0000:0000:0000:0000:0000:ffff:123.111.100.143)
	if (sAddress.size() > 45 || sAddress.size() < 3)
	{
		if (sAddress != "::")
		{
			ec = KIPError("KIPAddress6: invalid size");
		}
		return Empty;
	}

	constexpr uint8_t iInvalid { 255 };

	uint16_t iWord               { 0 };
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
				ec = KIPError("KIPAddress6: more than two consecutive colons");
				return Empty;
			}

			if (iNibbleCount > 0)
			{
				if (iBlock > 7)
				{
					ec = KIPError("KIPAddress6: invalid block count");
					return Empty;
				}
				// we have a value to store
				IP[iBlock * 2    ] = static_cast<uint8_t>(iWord >> 8);
				IP[iBlock * 2 + 1] = static_cast<uint8_t>(iWord);
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
					return Empty;
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
				return Empty;
			}

			auto iStartv4 = iIndex;

			// we can only have (hex)digits or colons before the current iIndex
			while (iStartv4 > 0 && sAddress[iStartv4 - 1] != ':')
			{
				--iStartv4;
			}

			KIPAddress4 IPv4(sAddress.Mid(iStartv4));

			if (IPv4.IsUnspecified())
			{
				ec = KIPError("KIPAddress6: invalid mapped IPv4");
				return Empty;
			}

			IP[iBlock * 2    ] = IPv4.m_IP[0];
			IP[iBlock * 2 + 1] = IPv4.m_IP[1];
			++iBlock;
			IP[iBlock * 2    ] = IPv4.m_IP[2];
			IP[iBlock * 2 + 1] = IPv4.m_IP[3];
			++iBlock;

			// the whole string was sent to KIPAddressv4
			iNibbleCount = 0;
			break;
		}
		else
		{
			if (iBlock > 7)
			{
				ec = KIPError("KIPAddress6: invalid block count");
				return Empty;
			}

			auto iNibble = kFromHexChar(ch);

			if (iNibble > 15 || ++iNibbleCount > 4)
			{
				ec = KIPError("KIPAddress6: invalid hex");
				return Empty;
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
			return Empty;
		}
		// fill last word
		IP[iBlock * 2    ] = static_cast<uint8_t>(iWord >> 8);
		IP[iBlock * 2 + 1] = static_cast<uint8_t>(iWord);
		++iBlock;
	}

	if (    iBlock  > 8
		|| (iBlock == 8 && iDoubleColonBlock != iInvalid)
		|| (iBlock  < 8 && iDoubleColonBlock == iInvalid)
		||  iLastColons == 1)
	{
		ec = KIPError("KIPAddress6: invalid block count");
		return Empty;
	}

	if (iDoubleColonBlock != iInvalid)
	{
		// move the blocks after the double colon, coming from the end
		auto iGap = 8 - iBlock;
		auto iMin = iDoubleColonBlock + iGap;

		for (uint8_t i = 7; i >= iMin; --i)
		{
			IP[i * 2]     = IP[(i - iGap) * 2];
			IP[i * 2 + 1] = IP[(i - iGap) * 2 + 1];
		}

		// and zeroize all gaps that were not yet touched
		for (auto i = iDoubleColonBlock; i < iMin; ++i)
		{
			IP[i * 2]     = 0;
			IP[i * 2 + 1] = 0;
		}
	}

	return IP;

} // KIPAddress6::FromString

//-----------------------------------------------------------------------------
KIPAddress6::BytesT KIPAddress6::FromString(KStringView sAddress)
//-----------------------------------------------------------------------------
{
	KIPError ec;
	auto Bytes = FromString(sAddress, ec);
	if (ec) throw ec;
	return Bytes;

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
		return KIPAddress(a4);
	}

	ec = KIPError();

	KIPAddress6 a6(sAddress, ec);

	if (!ec)
	{
		return KIPAddress(a6);
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
		 if (Is4()) m_v4.Dec();
	else if (Is6()) m_v6.Dec();
	return *this;

} // KIPAddress::Dec

//-----------------------------------------------------------------------------
KIPAddress& KIPAddress::Inc() noexcept
//-----------------------------------------------------------------------------
{
	     if (Is4()) m_v4.Inc();
	else if (Is6()) m_v6.Inc();
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

DEKAF2_NAMESPACE_END
