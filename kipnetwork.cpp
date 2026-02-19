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

#include "kipnetwork.h"

DEKAF2_NAMESPACE_BEGIN

//-----------------------------------------------------------------------------
KIPNetwork4::KIPNetwork4(const KIPAddress4& IP, const KIPAddress4& Mask, KIPError& ec) noexcept
//-----------------------------------------------------------------------------
: m_IP(IP)
, m_iPrefixLength(CalcPrefixFromMask(Mask, ec))
{
}

//-----------------------------------------------------------------------------
KIPNetwork4::KIPNetwork4(const KIPAddress4& IP, const KIPAddress4& Mask)
//-----------------------------------------------------------------------------
: m_IP(IP)
{
	KIPError ec;
	m_iPrefixLength = CalcPrefixFromMask(Mask, ec);
	if (ec) throw ec;
}

//-----------------------------------------------------------------------------
uint8_t KIPNetwork4::CalcPrefixFromMask(const KIPAddress4& Mask, KIPError& ec) noexcept
//-----------------------------------------------------------------------------
{
	uint8_t iPrefixLength { 0 };
	bool    bFinished     { false };
	auto    MaskBytes     { Mask.ToBytes() };

	for (std::size_t i = 0; i < MaskBytes.size(); ++i)
	{
		if (bFinished)
		{
			if (MaskBytes[i])
			{
				ec = KIPError("non-contiguous netmask");
				return 0;
			}
			continue;
		}
		else
		{
			switch (MaskBytes[i])
			{
				case 255:
					iPrefixLength += 8;
					break;

				case 254:
					++iPrefixLength;
					DEKAF2_FALLTHROUGH;
				case 252:
					++iPrefixLength;
					DEKAF2_FALLTHROUGH;
				case 248:
					++iPrefixLength;
					DEKAF2_FALLTHROUGH;
				case 240:
					++iPrefixLength;
					DEKAF2_FALLTHROUGH;
				case 224:
					++iPrefixLength;
					DEKAF2_FALLTHROUGH;
				case 192:
					++iPrefixLength;
					DEKAF2_FALLTHROUGH;
				case 128:
					++iPrefixLength;
					DEKAF2_FALLTHROUGH;
				case 0:
					bFinished = true;
					break;

				default:
					ec = KIPError("non-contiguous netmask");
					return 0;
			}
		}
	}

	return iPrefixLength;

} // KIPNetwork4::CalcPrefixFromMask

//-----------------------------------------------------------------------------
KIPNetwork4::KIPNetwork4(KStringView sNetwork, KIPError& ec) noexcept
//-----------------------------------------------------------------------------
: m_IP(NetworkFromString(sNetwork, ec))
, m_iPrefixLength(PrefixLengthFromString(sNetwork, ec))
{
} // KIPNetwork4::KIPNetwork4

//-----------------------------------------------------------------------------
KIPNetwork4::KIPNetwork4(KStringView sNetwork)
//-----------------------------------------------------------------------------
{
	KIPError ec;
	m_IP = NetworkFromString(sNetwork, ec);
	m_iPrefixLength = PrefixLengthFromString(sNetwork, ec);
	if (ec) throw ec;

} // KIPNetwork4::KIPNetwork4


//-----------------------------------------------------------------------------
KIPAddress4 KIPNetwork4::Netmask() const noexcept
//-----------------------------------------------------------------------------
{
	uint32_t iMaskBits = 0xffffffff;

	if (PrefixLength() == 0)
	{
		iMaskBits = 0;
	}
	else
	{
		iMaskBits <<= (32 - PrefixLength());
	}

	return KIPAddress4(iMaskBits);

} // KIPNetwork4::Netmask

//-----------------------------------------------------------------------------
KIPAddress4 KIPNetwork4::NetworkFromString(KStringView sNetwork, KIPError& ec) noexcept
//-----------------------------------------------------------------------------
{
	if (!ec)
	{
		auto iSlashPos = sNetwork.rfind('/');

		// if size == 0 overflows (without UB) to npos, which is fine
		if (iSlashPos >= sNetwork.size() - 1)
		{
			ec = KIPError("no prefix set");
		}
		else
		{
			sNetwork.remove_suffix(sNetwork.size() - iSlashPos);
			return KIPAddress4(sNetwork, ec);
		}
	}

	return {};

} // KIPNetwork4::NetworkFromString

//-----------------------------------------------------------------------------
uint8_t KIPNetwork4::PrefixLengthFromString (KStringView sNetwork, KIPError& ec) noexcept
//-----------------------------------------------------------------------------
{
	uint8_t iPrefixLength { 0 };

	if (!ec)
	{
		auto iSlashPos = sNetwork.rfind('/');

		// if size == 0 overflows (without UB) to npos, which is fine
		if (iSlashPos >= sNetwork.size() - 1)
		{
			ec = KIPError("no prefix set");
		}
		else
		{
			for (auto i = iSlashPos + 1; i < sNetwork.size(); ++i)
			{
				iPrefixLength *= 10;
				iPrefixLength += sNetwork[i] - '0';
			}

			if (iPrefixLength > 32)
			{
				ec = KIPError("prefix > 32");
				iPrefixLength = 32;
			}
		}
	}

	return iPrefixLength;

} // KIPNetwork4::PrefixLengthFromString

//-----------------------------------------------------------------------------
KString KIPNetwork4::ToString () const noexcept
//-----------------------------------------------------------------------------
{
	return kFormat("{}/{}", m_IP, m_iPrefixLength);

} // KIPNetwork4::ToString

//-----------------------------------------------------------------------------
bool KIPNetwork4::IsSubnetOf(const KIPNetwork4& other) const
//-----------------------------------------------------------------------------
{
	if (other.PrefixLength() >= PrefixLength())
	{
		return false;
	}

	const KIPNetwork4 net(m_IP, other.PrefixLength());

	return other.Canonical() == net.Canonical();

} // KIPNetwork4::IsSubnetOf

//-----------------------------------------------------------------------------
std::pair<KIPAddress4, KIPAddress4> KIPNetwork4::Hosts() const noexcept
//-----------------------------------------------------------------------------
{
	return IsHost()
		? std::pair<KIPAddress4, KIPAddress4>(m_IP, KIPAddress4(m_IP.ToUInt() + 1) )
		: std::pair<KIPAddress4, KIPAddress4>(KIPAddress4(Network().ToUInt() + 1), Broadcast() );

} // KIPNetwork4::Hosts

//-----------------------------------------------------------------------------
bool KIPNetwork4::Contains(const KIPAddress4& IP) const noexcept
//-----------------------------------------------------------------------------
{
	auto p = Hosts();

	return p.first <= IP && p.second > IP;

} // KIPNetwork4::Contains

//-----------------------------------------------------------------------------
KIPAddress6 KIPNetwork6::Network() const noexcept
//-----------------------------------------------------------------------------
{
	auto Bytes(m_IP.ToBytes());

	for (std::size_t i = 0; i < 16; ++i)
	{
		if (PrefixLength() <= i * 8)
		{
			Bytes[i] = 0;
		}
		else if (PrefixLength() < (i + 1) * 8)
		{
			Bytes[i] &= 0xff00 >> (PrefixLength() % 8);
		}
	}

	return KIPAddress6(Bytes, m_IP.Scope());

} // KIPNetwork6::Network

//-----------------------------------------------------------------------------
std::pair<KIPAddress6, KIPAddress6> KIPNetwork6::Hosts() const noexcept
//-----------------------------------------------------------------------------
{
	auto Start (m_IP.ToBytes());
	auto End   (m_IP.ToBytes());

	for (std::size_t i = 0; i < 16; ++i)
	{
		if (PrefixLength() <= i * 8)
		{
			Start[i] = 0;
			End  [i] = 0xff;
		}
		else if (PrefixLength() < (i + 1) * 8)
		{
			Start[i] &= 0xff00 >> (PrefixLength() % 8);
			End  [i] |= 0xff   >> (PrefixLength() % 8);
		}
	}

	return std::pair<KIPAddress6, KIPAddress6>
	(
		KIPAddress6(Start, m_IP.Scope()),
		KIPAddress6(End,   m_IP.Scope()).Inc()
	);

} // KIPNetwork6::Hosts

//-----------------------------------------------------------------------------
bool KIPNetwork6::IsSubnetOf(const KIPNetwork6& other) const
//-----------------------------------------------------------------------------
{
	if (other.PrefixLength() >= PrefixLength())
	{
		return false;
	}

	const KIPNetwork6 net(m_IP, other.PrefixLength());

	return other.Canonical() == net.Canonical();

} // KIPNetwork6::IsSubnetOf

//-----------------------------------------------------------------------------
bool KIPNetwork6::Contains(const KIPAddress6& IP) const noexcept
//-----------------------------------------------------------------------------
{
	auto p = Hosts();

	return p.first <= IP && p.second > IP;

} // KIPNetwork6::Contains


DEKAF2_NAMESPACE_END
