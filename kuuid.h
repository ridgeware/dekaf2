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

#pragma once

/// @file kuuid.h
/// provides support for UUIDs

#include "kdefinitions.h"
#include "kstringview.h"
#include "kstring.h"
#include "ktime.h"
#include "khex.h"
#include "knetworkinterface.h"
#include <array>
#include <ostream>

DEKAF2_NAMESPACE_BEGIN

/// @addtogroup util_id
/// @{

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Creating, parsing, and analyzing UUIDs like a1ae410d-9bc7-478a-b2fa-1266927a1dd7
class DEKAF2_PUBLIC KUUID
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	/// the UUID version, from 1 to 7 - see https://datatracker.ietf.org/doc/html/rfc9562.html for details
	enum Version : uint8_t
	{
		Null        = 0, ///< the empty UUID
		MACTime     = 1, ///< MAC and time based UUID, prefer TimeRandom (7) instead if you want a time based UUID
		MACTimeDCE  = 2, ///< legacy, creation not supported
		NamedMD5    = 3, ///< legacy for namespaced use of name based UUIDs, prefer NamedSHA1 (5) if you want a name based UUID
		Random      = 4, ///< prefer for general use of random UUIDs, has most randomness (122 bits)
		NamedSHA1   = 5, ///< prefer for namespaced use of name based UUIDs
		MACTimeSort = 6, ///< DB index friendly version of MACTime, prefer TimeRandom (7) though
		TimeRandom  = 7, ///< prefer for indexed (DB) use of random UUIDs with timestamp
		Custom      = 8  ///< creation not supported, for custom algorithm
	};

	using UUID = std::array<unsigned char, 16>;

	/// creates a UUID of the chosen version
	/// @param version the chosen version.
	/// MACTime (1), Random (4), MACTimeSort (6), and TimeRandom (7) are guaranteed to be supported by this constructor.
	/// Check with HasVersion() if the implementation has the version requested, and check the
	/// other constructors for named versions.
	explicit    KUUID(Version version = Version::Random) noexcept
	            : m_UUID(Build(version))
	            {}

	/// constexpr construct a UUID from existing storage, implicitly
	constexpr   KUUID(UUID uuid) noexcept
	            : m_UUID(std::move(uuid))
	            {}

	/// Create a named UUID (version 5, NamedSHA1 or version 3, NamedMD5) from a namespace UUID and a name.
	/// Check for predefined namespaces DNS, URL, OID, X500 in namespace KUUID::ns
	/// @param Namespace a UUID for all names in this namespace
	/// @param sName a name as the base of _this_ UUID
	/// @param bForceLegacyMD5 if true will use MD5 digest instead of the default SHA1 digest - defaults to false
	            KUUID(const KUUID& Namespace, KStringView sName, bool bForceLegacyMD5 = false);

	/// read a UUID string representation and decode it to set this UUID value
	constexpr
	explicit    KUUID(KStringView sUUID, bool bStrict = false)
	            : m_UUID(FromString(sUUID, bStrict))
	            {}

	            KUUID(const KUUID&)            = default;
	            KUUID(KUUID&&)                 = default;
	            KUUID& operator=(const KUUID&) = default;
	            KUUID& operator=(KUUID&&)      = default;

	constexpr
	KUUID& operator=(KStringView sUUID) noexcept
	{
		m_UUID = FromString(sUUID, false);
		return *this;
	}

	/// convert the UUID value to a string representation
	/// @param chSeparator the separator char between the UUID fields, \0 == no separator, defaults to '-'
	KString     ToString   (char chSeparator = '-') const;

	/// convert the UUID value to a string representation
	KString     Serialize  () const              { return ToString();     }

	/// convert the UUID value to a string representation
	operator    KString    () const              { return ToString();     }

	/// returns the UUID version as an integer between 0 an 8 (max 15), see RFC9562
	constexpr
	uint8_t     GetVersion () const noexcept     { return m_UUID[6] >> 4; }

	/// returns the UUID variant (the internal data layout) - do not mix this up with the version, the names are misleading.
	/// The variants are 0, 1, 2, 3, with 0 being the very old legacy format, 1 being used by current implementations,
	/// 2 being reserved for MS backward compatibility, and 3 being reserved for future uses
	uint16_t    GetVariant () const noexcept;

	/// returns the MAC address (if the UUID includes the MAC address) as raw bytes
	KMACAddress GetMAC     () const noexcept;

	/// returns the unix time (if the UUID includes the creation time)
	KUnixTime   GetTime    () const noexcept;

	/// check if the UUID is empty
	constexpr
	bool        empty      () const noexcept
	{
		return IsEqual(m_UUID, Nil().m_UUID);
	}

	/// clear the UUID (set to empty value)
	constexpr
	void        clear      ()       noexcept     { *this = Nil();        }

	/// return the UUID in network byte order
	constexpr
	const UUID& GetUUID    () const noexcept     { return m_UUID;        }

	constexpr friend
	bool operator==(const KUUID& a, const KUUID& b) noexcept
	{
		return IsEqual(a.GetUUID(), b.GetUUID());
	}

	constexpr friend
	bool operator< (const KUUID& a, const KUUID& b) noexcept
	{
		auto i = Compare(a.GetUUID(), b.GetUUID());
		return i < 0;
	}

	/// create a UUID of selected version, defaults to Random
	/// @param version the chosen version.
	/// MACTime (1), Random (4), MACTimeSort (6), and TimeRandom (7) are guaranteed to be supported by this method.
	/// Check with HasVersion() if the implementation has the version requested, and check the
	/// other creation method for named versions.
	static
	KUUID Create (Version version = Version::Random) noexcept
	{
		return KUUID(version);
	}

	/// Create a named UUID (version 5, NamedSHA1 or version 3, NamedMD5) from a namespace UUID and a name.
	/// Check for predefined namespaces DNS, URL, OID, X500 in namespace KUUID::ns
	/// @param Namespace a UUID for all names in this namespace
	/// @param sName a name as the base of _this_ UUID
	/// @param bForceLegacyMD5 if true will use MD5 digest instead of the default SHA1 digest - defaults to false
	static
	KUUID Create (const KUUID& Namespace, KStringView sName, bool bForceLegacyMD5 = false) noexcept
	{
		return KUUID(Namespace, sName, bForceLegacyMD5);
	}

	/// parse a UUID from a string representation
	static constexpr
	KUUID Parse (KStringView sUUID) noexcept
	{
		return KUUID(sUUID);
	}

	/// Returns true if this implementation supports CREATING the requested UUID version. This is a static constexpr function.
	static constexpr
	bool HasVersion (Version iVersion) noexcept
	{
		switch (iVersion)
		{
			case KUUID::Version::Null:
			case KUUID::Version::MACTime:
			case KUUID::Version::NamedMD5:
			case KUUID::Version::Random:
			case KUUID::Version::NamedSHA1:
			case KUUID::Version::MACTimeSort:
			case KUUID::Version::TimeRandom:
				return true;

			default:
				break;
		}

		return false;

	} // HasVersion

	static constexpr KUUID Nil() noexcept
	{
		return UUID { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	}

	static constexpr KUUID Max() noexcept
	{
		return UUID { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 };
	}

	/// the prefefined name spaces from RFC4122
	struct ns
	{
		static constexpr UUID DNS  { 0x6b, 0xa7, 0xb8, 0x10, 0x9d, 0xad, 0x11, 0xd1,
		                             0x80, 0xb4, 0x00, 0xc0, 0x4f, 0xd4, 0x30, 0xc8 };
		static constexpr UUID URL  { 0x6b, 0xa7, 0xb8, 0x11, 0x9d, 0xad, 0x11, 0xd1,
		                             0x80, 0xb4, 0x00, 0xc0, 0x4f, 0xd4, 0x30, 0xc8 };
		static constexpr UUID OID  { 0x6b, 0xa7, 0xb8, 0x12, 0x9d, 0xad, 0x11, 0xd1,
		                             0x80, 0xb4, 0x00, 0xc0, 0x4f, 0xd4, 0x30, 0xc8 };
		static constexpr UUID X500 { 0x6b, 0xa7, 0xb8, 0x14, 0x9d, 0xad, 0x11, 0xd1,
		                             0x80, 0xb4, 0x00, 0xc0, 0x4f, 0xd4, 0x30, 0xc8 };
	};

//------
private:
//------

	static constexpr bool IsEqual(const UUID& a, const UUID& b) noexcept
	{
		return a[ 0] == b[ 0] && a[ 1] == b[ 1] && a[ 2] == b[ 2] && a[ 3] == b[ 3] &&
		       a[ 4] == b[ 4] && a[ 5] == b[ 5] && a[ 6] == b[ 6] && a[ 7] == b[ 7] &&
		       a[ 8] == b[ 8] && a[ 9] == b[ 9] && a[10] == b[10] && a[11] == b[11] &&
		       a[12] == b[12] && a[13] == b[13] && a[14] == b[14] && a[15] == b[15];
	}

	static constexpr int16_t Compare(const UUID& a, const UUID& b) noexcept
	{
		for (uint16_t i = 0; i < 16; ++i)
		{
			if (a[i] < b[i]) return -1;
			if (a[i] > b[i]) return  1;
		}
		return 0;
	}

	static UUID Build(Version version) noexcept;

	static UUID FromStringStrict(KStringView sUUID) noexcept;

	static constexpr UUID FromString(KStringView sUUID, bool bStrict) noexcept
	{
		if (bStrict)
		{
			return FromStringStrict(sUUID);
		}

		return kBytesFromHex<UUID>(sUUID, "-");
	}

	/// set the current time into the given uuid as 100s of nanoseconds, either since the unix epoch or 15-Oct-1582,
	/// and either as low or big endian, and make sure the time is unique and monoton
	static inline void SetTimeToNow(UUID& uuid, bool bGregorian, bool bBigEndian) noexcept
	{
		EncodeTime(uuid, GetMonotonicCurrentTime(), bGregorian, bBigEndian);
	}

	/// get the time from the UUID, either since the unix epoch or 15-Oct-1582, and either as low or big endian
	static KUnixTime DecodeTime(const UUID& uuid, bool bGregorian, bool bBigEndian) noexcept;

	/// encode the given timepoint as 100s of nanoseconds, either since the unix epoch or 15-Oct-1582,
	/// and either as low or big endian
	static void EncodeTime(UUID& uuid, KUnixTime tTime, bool bGregorian, bool bBigEndian) noexcept;

	/// set the node ID, either from an interface or purely random
	static inline void SetMAC(UUID& uuid, bool bRandom)
	{
		EncodeMAC(uuid, GetInterfaceMAC(bRandom));
	}

	/// set the node ID, either from an interface or purely random
	static void EncodeMAC(UUID& uuid, const KMACAddress& Mac);

	/// get a MAC address either from a real interface, or randomly generated
	static const KMACAddress& GetInterfaceMAC(bool bRandom) noexcept;

	/// get the current timestamp, unique and monotone
	static KUnixTime GetMonotonicCurrentTime() noexcept;

	UUID m_UUID;

}; // KUUID

DEKAF2_COMPARISON_OPERATORS_WITH_ATTR(constexpr, KUUID)

inline DEKAF2_PUBLIC std::ostream& operator<<(std::ostream& stream, const KUUID& UUID)
{
	auto s = UUID.ToString();
	stream.write(s.data(), s.size());
	return stream;
}


/// @}

DEKAF2_NAMESPACE_END

#if DEKAF2_HAS_INCLUDE("kformat.h")

// kFormat formatters

#include "kformat.h"

namespace DEKAF2_FORMAT_NAMESPACE
{

template <>
struct formatter<DEKAF2_PREFIX KUUID> : formatter<string_view>
{
	template <typename FormatContext>
	auto format(const DEKAF2_PREFIX KUUID& UUID, FormatContext& ctx) const
	{
		return formatter<string_view>::format(UUID.ToString(), ctx);
	}
};

} // end of DEKAF2_FORMAT_NAMESPACE

#endif // of has #include "kformat.h"

#if DEKAF2_HAS_INCLUDE("bits/khash.h")

#include "bits/khash.h"

namespace std {

template<> struct hash<DEKAF2_PREFIX KUUID>
{
	std::size_t operator()(const DEKAF2_PREFIX KUUID& uuid) const noexcept
	{
		return DEKAF2_PREFIX kHash(uuid.GetUUID().data(), uuid.GetUUID().size());
	}
};

} // end of namespace std

#endif // DEKAF2_HAS_INCLUDE("bits/khash.h")
