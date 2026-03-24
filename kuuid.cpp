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
#include "kstringutils.h"
#include "khex.h"
#include "krandom.h"
#include "knetworkinterface.h"
#include <mutex>

DEKAF2_NAMESPACE_BEGIN

//-----------------------------------------------------------------------------
KUUID::UUID KUUID::Build(Version version) noexcept
//-----------------------------------------------------------------------------
{
	//  0                       1
	//  0 1 2 3  4 5  6 7  8 9  0 1 2 3 4 5
	// a1ae410d-9bc7-478a-b2fa-1266927a1dd7

	UUID uuid;

	switch (version)
	{
		default:
		case KUUID::Version::Null:
			uuid.fill(0);
			return uuid;

		case KUUID::Version::MACTime:
			SetTimeToNow(uuid, true, false);
			kGetRandom(uuid.data() + 8, 2);
			SetMAC(uuid, false);
			break;

		case KUUID::Version::Random:
			kGetRandom(uuid.data(), uuid.size());
			break;

		case KUUID::Version::MACTimeSort:
			SetTimeToNow(uuid, true, true);
			kGetRandom(uuid.data() + 8, 2);
			SetMAC(uuid, false);
			break;

		case KUUID::Version::TimeRandom:
			kGetRandom(uuid.data() + 8, uuid.size() - 8);
			SetTimeToNow(uuid, false, true);
			break;
	}

	// set version
	uuid[6] &= 0x0f;
	uuid[6] |= version << 4;

	// set MSB of clk_seq_hi_res to %10
	uuid[8] &= 0x3f;
	uuid[8] |= 0x80;

	return uuid;

} // Build

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
		uuid.fill(0);
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

			uuid.fill(0);
			break;
		}
	}

	return uuid;

} // FromStringStrict

//-----------------------------------------------------------------------------
KString KUUID::ToString(char chSeparator) const
//-----------------------------------------------------------------------------
{
	KString sUUID;
	sUUID.reserve(36);

	auto* p = &m_UUID[0];

	auto Put = [&sUUID, &p, chSeparator](uint16_t iCount)
	{
		if (chSeparator && !sUUID.empty())
		{
			sUUID += chSeparator;
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
	if ((b & 0x80) == 0) return 0; // MSB 1 bit  of b8 used
	if ((b & 0x40) == 0) return 1; // MSB 2 bits of b8 used
	if ((b & 0x20) == 0) return 2; // MSB 3 bits of b8 used
	return 3;                      // MSB 3 bits of b8 used
}

//-----------------------------------------------------------------------------
KMACAddress KUUID::GetMAC() const noexcept
//-----------------------------------------------------------------------------
{
	KMACAddress::MAC Mac {};

	switch (GetVersion())
	{
		case Version::MACTime:
		case Version::MACTimeDCE:
		case Version::MACTimeSort:
			std::copy(m_UUID.begin() + 10, m_UUID.end(), Mac.begin());
			break;
	}

	return Mac;

} // GetMAC

//-----------------------------------------------------------------------------
KUnixTime KUUID::GetTime() const noexcept
//-----------------------------------------------------------------------------
{
	switch (GetVersion())
	{
		case Version::MACTimeSort:
			return DecodeTime(m_UUID, true, true);

		case Version::TimeRandom:
			return DecodeTime(m_UUID, false, true);

		case Version::MACTime:
			return DecodeTime(m_UUID, true, false);
	}

	return {};

} // GetTime

//-----------------------------------------------------------------------------
KUnixTime KUUID::GetMonotonicCurrentTime() noexcept
//-----------------------------------------------------------------------------
{
	// make sure we have monotonic increasing time stamps
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
		else
		{
			tLast = usecs;
		}
	}

	return KUnixTime(usecs);

} // GetMonotonicCurrentTime

//-----------------------------------------------------------------------------
KUnixTime KUUID::DecodeTime(const UUID& uuid, bool bGregorian, bool bBigEndian) noexcept
//-----------------------------------------------------------------------------
{
	uint64_t t60 { 0 };

	if (bBigEndian)
	{
		t60 += (static_cast<uint64_t>(uuid[0]) << 40) +
		       (static_cast<uint64_t>(uuid[1]) << 32) +
		       (static_cast<uint64_t>(uuid[2]) << 24) +
		       (static_cast<uint64_t>(uuid[3]) << 16) +
		       (static_cast<uint64_t>(uuid[4]) <<  8) +
		       (static_cast<uint64_t>(uuid[5]) <<  0);

		if (!bGregorian)
		{
			// For the unix time format in uuid v7 only 48 bits are standardized.
			// While our implementation adds sub-millisecond precision in rand_a
			// (RFC 9562 Section 6.2 Method 3), we do not know if external UUIDs
			// do the same, so we stick with milliseconds resolution on decode.
			return KUnixTime(chrono::milliseconds(t60));
		}

		// gregorian also implies 60 bits, not only 48
		t60 <<= 12;

		t60 += ((static_cast<uint64_t>(uuid[6]) & 0x0f) << 8) +
		        (static_cast<uint64_t>(uuid[7]) << 0);
	}
	else
	{
		kAssert(bGregorian == true, "no support for low endian unix time format");

		t60 += (static_cast<uint64_t>(uuid[0]) << 24) +
		       (static_cast<uint64_t>(uuid[1]) << 16) +
		       (static_cast<uint64_t>(uuid[2]) <<  8) +
		       (static_cast<uint64_t>(uuid[3]) <<  0) +
		       (static_cast<uint64_t>(uuid[4]) << 40) +
		       (static_cast<uint64_t>(uuid[5]) << 32) +
		      ((static_cast<uint64_t>(uuid[6]) & 0x0f) << 56) +
		       (static_cast<uint64_t>(uuid[7]) << 48);
	}

	if (bGregorian)
	{
		// subtract the offset between 15-Oct-1582 and 1-Jan-70
		t60 -= 0x01b21dd213814000;
	}

	return KUnixTime(chrono::microseconds(t60 / 10)
#if DEKAF2_HAS_NANOSECONDS_SYS_CLOCK
	               + chrono::nanoseconds((t60 % 10) * 100)
#endif
	);

} // DecodeTime

//-----------------------------------------------------------------------------
void KUUID::EncodeTime(UUID& uuid, KUnixTime tTime, bool bGregorian, bool bBigEndian) noexcept
//-----------------------------------------------------------------------------
{
	if (!bGregorian)
	{
		// UUIDv7: 48-bit Unix timestamp in milliseconds, big-endian (RFC 9562 Section 5.7)
		// plus 12-bit sub-millisecond fraction in rand_a (RFC 9562 Section 6.2 Method 3)
		auto duration = tTime.time_since_epoch();
		auto millis   = chrono::duration_cast<chrono::milliseconds>(duration);
		uint64_t ms   = millis.count();
		// sub-millisecond remainder in microseconds (0-999), scaled to 12 bits (0-4091)
		auto sub_ms_us  = chrono::duration_cast<chrono::microseconds>(duration - millis).count();
		uint16_t frac12 = static_cast<uint16_t>((sub_ms_us << 12) / 1000);
		uuid[0] = static_cast<unsigned char>(ms >> 40);
		uuid[1] = static_cast<unsigned char>(ms >> 32);
		uuid[2] = static_cast<unsigned char>(ms >> 24);
		uuid[3] = static_cast<unsigned char>(ms >> 16);
		uuid[4] = static_cast<unsigned char>(ms >>  8);
		uuid[5] = static_cast<unsigned char>(ms >>  0);
		uuid[6] = static_cast<unsigned char>(frac12 >> 8); // top 4 bits (version replaces high nibble)
		uuid[7] = static_cast<unsigned char>(frac12 >> 0);
		return;
	}

	chrono::nanoseconds nsecs = chrono::duration_cast<chrono::nanoseconds>(tTime.time_since_epoch());

	// convert to 100s of nanoseconds
	uint64_t nano100 = nsecs.count() / 100;

	// add the offset between 15-Oct-1582 and 1-Jan-70
	nano100 += 0x01b21dd213814000;

	if (bBigEndian)
	{
		// UUIDv6: 60-bit Gregorian timestamp, big-endian (RFC 9562 Section 5.6)
		uuid[0] = static_cast<unsigned char>(nano100 >> 52);
		uuid[1] = static_cast<unsigned char>(nano100 >> 44);
		uuid[2] = static_cast<unsigned char>(nano100 >> 36);
		uuid[3] = static_cast<unsigned char>(nano100 >> 28);
		uuid[4] = static_cast<unsigned char>(nano100 >> 20);
		uuid[5] = static_cast<unsigned char>(nano100 >> 12);
		uuid[6] = static_cast<unsigned char>(nano100 >>  8);
		uuid[7] = static_cast<unsigned char>(nano100 >>  0);
	}
	else
	{
		// UUIDv1: 60-bit Gregorian timestamp, mixed-endian (RFC 9562 Section 5.1)
		uuid[0] = static_cast<unsigned char>(nano100 >> 24);
		uuid[1] = static_cast<unsigned char>(nano100 >> 16);
		uuid[2] = static_cast<unsigned char>(nano100 >>  8);
		uuid[3] = static_cast<unsigned char>(nano100 >>  0);
		uuid[4] = static_cast<unsigned char>(nano100 >> 40);
		uuid[5] = static_cast<unsigned char>(nano100 >> 32);
		uuid[6] = static_cast<unsigned char>(nano100 >> 56);
		uuid[7] = static_cast<unsigned char>(nano100 >> 48);
	}

} // EncodeTime

//-----------------------------------------------------------------------------
void KUUID::EncodeMAC(UUID& uuid, const KMACAddress& Mac)
//-----------------------------------------------------------------------------
{
	std::copy(Mac.ToBytes().begin(), Mac.ToBytes().end(), uuid.begin() + 10);

} // EncodeMAC

//-----------------------------------------------------------------------------
const KMACAddress& KUUID::GetInterfaceMAC(bool bRandom) noexcept
//-----------------------------------------------------------------------------
{
	static KMACAddress s_Mac = [bRandom]() -> KMACAddress
	{
		if (!bRandom)
		{
			auto Interfaces = kGetNetworkInterfaces();
			const KMACAddress* Local { nullptr };

			for (const auto& Interface : Interfaces)
			{
				if (Interface.GetMAC().IsValid())
				{
					if (Interface.GetMAC().IsGlobal())
					{
						return Interface.GetMAC();
					}

					if (!Local)
					{
						Local = &Interface.GetMAC();
					}
				}
			}

			if (Local)
			{
				return *Local;
			}
		}

		// https://www.rfc-editor.org/rfc/rfc9562#section-6.10
		// requires the multicast bit being set for random MAC addresses
		return KMACAddress::Random(true);

	}();

	return s_Mac;

} // GetInterfaceMAC

#ifdef DEKAF2_REPEAT_CONSTEXPR_VARIABLE
constexpr KUUID::UUID KUUID::ns::DNS;
constexpr KUUID::UUID KUUID::ns::URL;
constexpr KUUID::UUID KUUID::ns::OID;
constexpr KUUID::UUID KUUID::ns::X500;
#endif

DEKAF2_NAMESPACE_END
