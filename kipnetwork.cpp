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
#include "kstringutils.h"
#include "kctype.h"

DEKAF2_NAMESPACE_BEGIN

//-----------------------------------------------------------------------------
uint8_t detail::KIPNetworkBase::PrefixLengthFromString (KStringView sNetwork, uint8_t iMaxPrefixLength, bool bAcceptSingleHost, KIPError& ec) noexcept
//-----------------------------------------------------------------------------
{
	uint8_t iPrefixLength { 0 };

	if (!ec)
	{
		auto iSlashPos = sNetwork.rfind('/');

		// if size == 0 overflows (without UB) to npos, which is fine
		if (iSlashPos >= sNetwork.size() - 1)
		{
			if (bAcceptSingleHost)
			{
				iPrefixLength = iMaxPrefixLength;
			}
			else
			{
				ec = KIPError("no prefix set");
			}
		}
		else
		{
			for (auto i = iSlashPos + 1; i < sNetwork.size(); ++i)
			{
				auto ch = sNetwork[i];

				if (!KASCII::kIsDigit(ch))
				{
					ec = KIPError("invalid digit in prefix", 445);
					iPrefixLength = 0;
					break;
				}

				iPrefixLength *= 10;
				iPrefixLength += ch - '0';
			}

			if (iPrefixLength > iMaxPrefixLength)
			{
				ec = KIPError("prefix too large", 444);
				iPrefixLength = iMaxPrefixLength;
			}
		}
	}

	return iPrefixLength;

} // detail::KIPNetworkBase::PrefixLengthFromString

//-----------------------------------------------------------------------------
KStringView detail::KIPNetworkBase::AddressStringFromString(KStringView sNetwork, bool bAcceptSingleHost, KIPError& ec) noexcept
//-----------------------------------------------------------------------------
{
	if (!ec)
	{
		auto iSlashPos = sNetwork.rfind('/');

		// if size == 0 overflows (without UB) to npos, which is fine
		if (iSlashPos >= sNetwork.size() - 1)
		{
			if (bAcceptSingleHost)
			{
				return sNetwork;
			}

			ec = KIPError("no prefix set");
		}
		else
		{
			sNetwork.remove_suffix(sNetwork.size() - iSlashPos);

			return sNetwork;
		}
	}

	return {};

} // detail::KIPNetworkBase::AddressStringFromString

//-----------------------------------------------------------------------------
KIPNetwork4::KIPNetwork4(const KIPAddress4& IP, const KIPAddress4& Mask, KIPError& ec) noexcept
//-----------------------------------------------------------------------------
: base(CalcPrefixFromMask(Mask, ec), 32, ec)
, m_IP(IP)
{
}

//-----------------------------------------------------------------------------
KIPNetwork4::KIPNetwork4(const KIPAddress4& IP, const KIPAddress4& Mask)
//-----------------------------------------------------------------------------
: m_IP(IP)
{
	KIPError ec;
	SetPrefixLength(CalcPrefixFromMask(Mask, ec));
	if (ec) throw ec;
}

//-----------------------------------------------------------------------------
KIPNetwork4::KIPNetwork4(KStringView sNetwork, bool bAcceptSingleHost, KIPError& ec) noexcept
//-----------------------------------------------------------------------------
: m_IP(AddressFromString(sNetwork, bAcceptSingleHost, ec))
{
	SetPrefixLength(PrefixLengthFromString(sNetwork, 32, bAcceptSingleHost, ec));

} // KIPNetwork4::KIPNetwork4

//-----------------------------------------------------------------------------
KIPNetwork4::KIPNetwork4(KStringView sNetwork, bool bAcceptSingleHost)
//-----------------------------------------------------------------------------
{
	KIPError ec;
	m_IP = AddressFromString(sNetwork, bAcceptSingleHost, ec);
	SetPrefixLength(PrefixLengthFromString(sNetwork, 32, bAcceptSingleHost, ec));
	if (ec) throw ec;

} // KIPNetwork4::KIPNetwork4


//-----------------------------------------------------------------------------
KIPAddress4 KIPNetwork4::AddressFromString(KStringView sNetwork, bool bAcceptSingleHost, KIPError& ec) noexcept
//-----------------------------------------------------------------------------
{
	if (!ec)
	{
		auto sAddress = AddressStringFromString(sNetwork, bAcceptSingleHost, ec);

		if (!ec)
		{
			return KIPAddress4(sAddress, ec);
		}
	}

	return {};

} // KIPNetwork4::AddressFromString

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
KString KIPNetwork4::ToString () const noexcept
//-----------------------------------------------------------------------------
{
	return kFormat("{}/{}", m_IP, PrefixLength());

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
bool KIPNetwork4::IsSubnetOf(const KIPNetwork6& other) const
//-----------------------------------------------------------------------------
{
	if (other.PrefixLength() >= PrefixLength() + 96)
	{
		return false;
	}

	const KIPNetwork6 net6(*this);

	return net6.IsSubnetOf(other);

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
bool KIPNetwork4::Contains(const KIPAddress6& IP) const noexcept
//-----------------------------------------------------------------------------
{
	KIPError ec;
	KIPAddress4 IP4(IP, ec);

	if (ec)
	{
		return false;
	}

	auto p = Hosts();

	return p.first <= IP4 && p.second > IP4;

} // KIPNetwork4::Contains

//-----------------------------------------------------------------------------
bool KIPNetwork4::Contains(const KIPAddress& IP) const noexcept
//-----------------------------------------------------------------------------
{
	if (IP.Is4()) return Contains(IP.get4());
	if (IP.Is6()) return Contains(IP.get6());
	return false;

} // KIPNetwork4::Contains

//-----------------------------------------------------------------------------
KIPNetwork6::KIPNetwork6(const KIPNetwork4& Net4) noexcept
//-----------------------------------------------------------------------------
: base(Net4.PrefixLength() + 96, 128)
, m_IP(Net4.Address())
{
} // KIPNetwork6::KIPNetwork6

//-----------------------------------------------------------------------------
KIPNetwork6::KIPNetwork6(KStringView sNetwork, bool bAcceptSingleHost, KIPError& ec) noexcept
//-----------------------------------------------------------------------------
: m_IP(AddressFromString(sNetwork, bAcceptSingleHost, ec))
{
	SetPrefixLength(PrefixLengthFromString(sNetwork, 128, bAcceptSingleHost, ec));

} // KIPNetwork6::KIPNetwork6

//-----------------------------------------------------------------------------
KIPNetwork6::KIPNetwork6(KStringView sNetwork, bool bAcceptSingleHost)
//-----------------------------------------------------------------------------
{
	KIPError ec;
	m_IP = AddressFromString(sNetwork, bAcceptSingleHost, ec);
	SetPrefixLength(PrefixLengthFromString(sNetwork, 128, bAcceptSingleHost, ec));
	if (ec) throw ec;

} // KIPNetwork6::KIPNetwork6

//-----------------------------------------------------------------------------
KIPAddress6 KIPNetwork6::AddressFromString(KStringView sNetwork, bool bAcceptSingleHost, KIPError& ec) noexcept
//-----------------------------------------------------------------------------
{
	if (!ec)
	{
		auto sAddress = AddressStringFromString(sNetwork, bAcceptSingleHost, ec);

		if (!ec)
		{
			return KIPAddress6(sAddress, ec);
		}
	}

	return {};

} // KIPNetwork6::AddressFromString

//-----------------------------------------------------------------------------
KString KIPNetwork6::ToString () const noexcept
//-----------------------------------------------------------------------------
{
	return kFormat("{}/{}", m_IP, PrefixLength());

} // KIPNetwork6::ToString

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
bool KIPNetwork6::IsSubnetOf(const KIPNetwork4& other) const
//-----------------------------------------------------------------------------
{
	return IsSubnetOf(KIPNetwork6(other));

} // KIPNetwork6::IsSubnetOf

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

//-----------------------------------------------------------------------------
bool KIPNetwork6::Contains(const KIPAddress4& IP) const noexcept
//-----------------------------------------------------------------------------
{
	KIPAddress6 IP6(IP);

	auto p = Hosts();

	return p.first <= IP6 && p.second > IP6;

} // KIPNetwork4::Contains

//-----------------------------------------------------------------------------
bool KIPNetwork6::Contains(const KIPAddress& IP) const noexcept
//-----------------------------------------------------------------------------
{
	if (IP.Is4()) return Contains(IP.get4());
	if (IP.Is6()) return Contains(IP.get6());
	return false;

} // KIPNetwork4::Contains

//-----------------------------------------------------------------------------
KIPNetwork KIPNetwork::FromString(KStringView sNetwork, bool bAcceptSingleHost, KIPError& ec) noexcept
//-----------------------------------------------------------------------------
{
	KIPNetwork4 n4(sNetwork, bAcceptSingleHost, ec);

	// 444 is our 'prefix too large' error - it does not invalidate the address,
	// and we do not change into IPv6 mode - this was an IPv4 address
	// 445 is our 'invalid digit in prefix' error - same as above
	if (!ec || ec.value() == 444 || ec.value() == 445)
	{
		return KIPNetwork(std::move(n4));
	}

	ec.clear();

	KIPNetwork6 n6(sNetwork, bAcceptSingleHost, ec);

	if (!ec || ec.value() == 444 || ec.value() == 445)
	{
		return KIPNetwork(std::move(n6));
	}

	return KIPNetwork6{};

} // KIPNetwork::FromString

//-----------------------------------------------------------------------------
KIPNetwork KIPNetwork::FromString(KStringView sNetwork, bool bAcceptSingleHost)
//-----------------------------------------------------------------------------
{
	KIPError ec;
	auto Net = FromString(sNetwork, bAcceptSingleHost, ec);
	if (ec) throw ec;
	return Net;

} // KIPNetwork::FromString

//-----------------------------------------------------------------------------
KString KIPNetwork::ToString() const noexcept
//-----------------------------------------------------------------------------
{
	if (Is4()) return m_Net4.ToString();
	if (Is6()) return m_Net6.ToString();
	return {};

} // KIPNetwork::ToString

//-----------------------------------------------------------------------------
bool KIPNetwork::Contains(const KIPAddress4& IP) const noexcept
//-----------------------------------------------------------------------------
{
	if (Is4()) return m_Net4.Contains(IP);
	if (Is6()) return m_Net6.Contains(IP);
	return false;

} // KIPNetwork::Contains

//-----------------------------------------------------------------------------
bool KIPNetwork::Contains(const KIPAddress6& IP) const noexcept
//-----------------------------------------------------------------------------
{
	if (Is4()) return m_Net4.Contains(IP);
	if (Is6()) return m_Net6.Contains(IP);
	return false;

} // KIPNetwork::Contains

//-----------------------------------------------------------------------------
bool KIPNetwork::Contains(const KIPAddress& IP) const noexcept
//-----------------------------------------------------------------------------
{
	if (Is4()) return m_Net4.Contains(IP);
	if (Is6()) return m_Net6.Contains(IP);
	return false;

} // KIPNetwork::Contains

//-----------------------------------------------------------------------------
constexpr bool KIPNetwork::IsHost() const noexcept
//-----------------------------------------------------------------------------
{
	if (Is4()) return m_Net4.IsHost();
	if (Is6()) return m_Net6.IsHost();
	return false;

} // KIPNetwork::IsHost

//-----------------------------------------------------------------------------
bool KIPNetwork::IsSubnetOf(const KIPNetwork& other) const
//-----------------------------------------------------------------------------
{
	if (other.Is4())
	{
		if (Is4()) return get4().IsSubnetOf(other.get4());
		if (Is6()) return get6().IsSubnetOf(other.get4());
	}
	else if (other.Is6())
	{
		if (Is4()) return get4().IsSubnetOf(other.get6());
		if (Is6()) return get6().IsSubnetOf(other.get6());
	}
	return false;

} // KIPNetwork::IsSubnetOf

DEKAF2_NAMESPACE_END
