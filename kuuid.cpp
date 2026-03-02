/*
// DEKAF(tm): Lighter, Faster, Smarter (tm)
//
// Copyright (c) 2025, Ridgeware, Inc.
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

#include "kuuid.h"
#include "kmessagedigest.h"

#if DEKAF2_HAS_LIBUUID
	#include <uuid/uuid.h>
#endif

#include "kstringutils.h"
#include "khex.h"
#include "ksystem.h"
#include <mutex>

DEKAF2_NAMESPACE_BEGIN

//-----------------------------------------------------------------------------
KUUID::KUUID(Version version) noexcept
//-----------------------------------------------------------------------------
{
	switch (version)
	{
		default:
		case KUUID::Version::Null:
			clear();
			break;

#if DEKAF2_HAS_LIBUUID
		case KUUID::Version::MACTime:
			::uuid_generate_time(m_UUID.data());
			break;
#endif

		case KUUID::Version::Random:
			kGetRandom(m_UUID.data(), m_UUID.size());
			// set version to 4
			m_UUID[6] &= 0x0f;
			m_UUID[6] |= Version::Random << 4;
			// set MSB of clk_seq_hi_res to %10
			m_UUID[8] &= 0x3f;
			m_UUID[8] |= 0x80;
			break;

		case KUUID::Version::TimeRandom:
			kGetRandom(m_UUID.data() + 6, m_UUID.size() - 6);
			SetTime(m_UUID);
			// set version to 7
			m_UUID[6] &= 0x0f;
			m_UUID[6] |= Version::TimeRandom << 4;
			// set MSB of clk_seq_hi_res to %10
			m_UUID[8] &= 0x3f;
			m_UUID[8] |= 0x80;
			break;
	}

} // ctor

//-----------------------------------------------------------------------------
KUUID::KUUID(const KUUID& Namespace, KStringView sName, bool bForceLegacyMD5)
//-----------------------------------------------------------------------------
{
	if (!Namespace.empty())
	{
		KMessageDigest Digest(bForceLegacyMD5 ? KMessageDigest::MD5 : KMessageDigest::SHA1);

		Digest.Update(Namespace.GetUUID().data(), Namespace.GetUUID().size());
		Digest.Update(sName);

		auto& sDigest = Digest.Digest();

		if (sDigest.size() >= m_UUID.size())
		{
			// copy the hash into the UUID
			std::copy(sDigest.begin(), sDigest.begin() + m_UUID.size(), m_UUID.begin());
			// set version
			m_UUID[6] &= 0x0f;
			m_UUID[6] |= (bForceLegacyMD5 ? Version::NamedMD5 : Version::NamedSHA1) << 4;
			// set MSB of clk_seq_hi_res to %10
			m_UUID[8] &= 0x3f;
			m_UUID[8] |= 0x80;
			return;
		}
	}

	// failure..
	m_UUID.fill(0);

} // ctor named version

//-----------------------------------------------------------------------------
KUUID::UUID KUUID::FromStringStrict(KStringView sUUID) noexcept
//-----------------------------------------------------------------------------
{
	UUID uuid;

	// a1ae410d-9bc7-478a-b2fa-1266927a1dd7
	if (sUUID.size() != 36  ||
	    sUUID[ 8]    != '-' ||
	    sUUID[13]    != '-' ||
	    sUUID[18]    != '-' ||
	    sUUID[23]    != '-')
	{
		uuid = Empty;
	}
	else
	{
		static constexpr std::array<unsigned char, 16> Digits {
			 0,  2,  4,  6,
			 9, 11,
			14, 16,
			19, 21,
			24, 26, 28, 30, 32, 34
		};

		auto p = uuid.begin();

		for (auto iPos : Digits)
		{
			int b1 = kFromHexChar(sUUID[iPos]);

			if (b1 <= 15)
			{
				int b2 = kFromHexChar(sUUID[iPos+1]);

				if (b2 <= 15)
				{
					*p++ = (b1 << 4) + b2;
					continue;
				}
			}

			uuid = Empty;
			break;
		}
	}

	return uuid;

} // FromStringStrict

//-----------------------------------------------------------------------------
KString KUUID::ToString() const
//-----------------------------------------------------------------------------
{
	KString sUUID;
	sUUID.reserve(36);

	auto* p = m_UUID.begin();

	auto Put = [&sUUID, &p](uint16_t iCount)
	{
		if (!sUUID.empty())
		{
			sUUID += '-';
		}

		while (iCount--)
		{
			kHexAppend(sUUID, static_cast<char>(*p++));
		}
	};

	Put(4);
	Put(2);
	Put(2);
	Put(2);
	Put(6);

	return sUUID;

} // ToString

//-----------------------------------------------------------------------------
uint16_t KUUID::GetVariant() const noexcept
//-----------------------------------------------------------------------------
{
	auto b = m_UUID[8];
	if ((b & 0x80) == 0) return 0; // MSB 1 bit of b8 used
	if ((b & 0x40) == 0) return 1; // MSB 2 bit of b8 used
	if ((b & 0x20) == 0) return 2; // MSB 3 bit of b8 used
	return 3;                      // MSB 3 bit of b8 used
}

//-----------------------------------------------------------------------------
KStringView KUUID::GetMAC() const noexcept
//-----------------------------------------------------------------------------
{
	KStringView sMAC;

	switch (GetVersion())
	{
		case Version::MACTime:
		case Version::MACTimeDCE:
		case Version::MACTimeSort:
			sMAC = KStringView(reinterpret_cast<const char*>(m_UUID.data()) + 10, 6);
			break;
	}

	return sMAC;

} // GetMAC

//-----------------------------------------------------------------------------
KUnixTime KUUID::GetTime() const noexcept
//-----------------------------------------------------------------------------
{
	KUnixTime tTime;

	switch (GetVersion())
	{
		case Version::MACTimeSort:
		case Version::TimeRandom:
		{
			chrono::milliseconds msecs { 0 };
			for (uint16_t i = 0; i < 6; ++i)
			{
				msecs *= 256;
				msecs += chrono::milliseconds(m_UUID[i]);
			}
			tTime = KUnixTime(msecs);
			break;
		}
	}

	return tTime;

} // GetTime

//-----------------------------------------------------------------------------
void KUUID::SetTime(UUID& uuid)
//-----------------------------------------------------------------------------
{
	// insert the current time in milliseconds into the first 6 bytes of the uuid,
	// make sure we have monotonic increasing time stamps
	// to allow for more than 1000 UUIDs per second we use the LSB 4 bit of uuid[6]
	// and the MSB 6 bit of uuid[7] for the full microsecond resolution, and hence
	// 1 million UUIDs per second
	// see https://datatracker.ietf.org/doc/html/rfc9562.html#name-monotonicity-and-counters

	static std::mutex TMutex;
	static chrono::microseconds tLast { 0 };

	chrono::microseconds usecs;

	{
		std::lock_guard<std::mutex> Lock(TMutex);

		usecs = chrono::duration_cast<chrono::microseconds>(KUnixTime::now().time_since_epoch());

		if (usecs <= tLast)
		{
			usecs = ++tLast;
		}
		else if (usecs > tLast)
		{
			tLast = usecs;
		}
	}

	uint64_t iTicks = usecs.count() / 1000;

	for (int16_t i = 5; i >= 0; --i)
	{
		uuid[i] = iTicks & 0xff;
		iTicks /= 256;
	}

	uint16_t iExt = usecs.count() % 1000;

	uuid[6]  = (iExt >> 6);
	uuid[7] &= 0x03;
	uuid[7] |= (iExt << 2) & 0xfc;

} // SetTime

#ifdef DEKAF2_REPEAT_CONSTEXPR_VARIABLE
constexpr KUUID::UUID KUUID::Empty;
constexpr KUUID::UUID KUUID::ns::DNS;
constexpr KUUID::UUID KUUID::ns::URL;
constexpr KUUID::UUID KUUID::ns::OID;
constexpr KUUID::UUID KUUID::ns::X500;
#endif

DEKAF2_NAMESPACE_END
