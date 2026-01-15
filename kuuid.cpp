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

#undef DEKAF2_HAS_LIBUUID

#if DEKAF2_HAS_LIBUUID

	#include <uuid/uuid.h>

#elif DEKAF2_IS_WINDOWS

	#include <windows.h>
	#include <rpcdce.h>

namespace {

inline
unsigned char* FromUUID(UUID& uuid)
{
	return static_cast<unsigned char*>(static_cast<void*>(&uuid));
}

inline
UUID* ToUUID(const unsigned char* data)
{
	return static_cast<UUID*>(const_cast<void*>(static_cast<const void*>(data)));
}

inline
UUID* ToUUID(unsigned char* data)
{
	return static_cast<UUID*>(static_cast<void*>(data));
}

} // end of anonymous namespace

#else

	#include "kstringutils.h"
	#include "khex.h"
	#include "ksystem.h"

#endif

DEKAF2_NAMESPACE_BEGIN

//-----------------------------------------------------------------------------
KUUID::KUUID(Variant var)
//-----------------------------------------------------------------------------
{
#if DEKAF2_HAS_LIBUUID

	switch (var)
	{
		case KUUID::Variant::Null:
			clear();
			break;

		case KUUID::Variant::Random:
			::uuid_generate_random(m_UUID.data());
			break;

		case KUUID::Variant::MACTime:
			::uuid_generate_time(m_UUID.data());
			break;
	}

#elif DEKAF2_IS_WINDOWS

	switch (var)
	{
		case KUUID::Variant::Null:
			clear();
			break;

		default:
			::UuidCreate(ToUUID(m_UUID.data()));
			break;
	}

#else

	switch (var)
	{
		case KUUID::Variant::Null:
			clear();
			break;

		default:
			{
				// this generates a relatively weak random number,
				// instead of 128 bits of randomness it only gives
				// ~ 67 bits - but this is really only a last resort
				// code, normally we use one of the real UUID
				// generators
				auto p = m_UUID.data();

				for (uint16_t i = 0; i < 4; ++i)
				{
					uint32_t r = kRandom();
					*p++ = r & 0xff;
					*p++ = (r >>= 8) & 0xff;
					*p++ = (r >>= 8) & 0xff;
					*p++ = (r >>= 8) & 0xff;
				}
			}
			break;
	}

#endif

} // ctor

//-----------------------------------------------------------------------------
void KUUID::FromString(KStringView sUUID)
//-----------------------------------------------------------------------------
{
#if DEKAF2_HAS_LIBUUID

	if (::uuid_parse(sUUID.data(), m_UUID.data()) != 0)
	{
		clear();
	}

#elif DEKAF2_IS_WINDOWS

	if (::UuidFromStringA(const_cast<unsigned char*>(reinterpret_cast<const unsigned char*>(sUUID.data())), ToUUID(m_UUID.data())) != RPC_S_OK)
	{
		clear();
	}

#else

	// a1ae410d-9bc7-478a-b2fa-1266927a1dd7
	if (sUUID.size() != 36  ||
	    sUUID[ 8]    != '-' ||
	    sUUID[13]    != '-' ||
	    sUUID[18]    != '-' ||
	    sUUID[23]    != '-')
	{
		clear();
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

		auto p = m_UUID.begin();

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

			clear();
			break;
		}
	}

#endif

} // FromString

//-----------------------------------------------------------------------------
KUUID& KUUID::operator=(KStringView sUUID)
//-----------------------------------------------------------------------------
{
	FromString(sUUID);
	return *this;
}

//-----------------------------------------------------------------------------
KString KUUID::ToString() const
//-----------------------------------------------------------------------------
{
#if DEKAF2_HAS_LIBUUID

	KString sUUID(36, '\0');
	::uuid_unparse_lower(m_UUID.data(), sUUID.data());
	return sUUID;

#elif DEKAF2_IS_WINDOWS

	RPC_CSTR uuid_str;
	KString sUUID;

	if (UuidToStringA(ToUUID(m_UUID.data()), &uuid_str) == RPC_S_OK)
	{
		sUUID = (char*)uuid_str;
	}

	RpcStringFreeA(&uuid_str);

	return sUUID;

#else

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

#endif

} // ToString

//-----------------------------------------------------------------------------
void KUUID::clear()
//-----------------------------------------------------------------------------
{
#if DEKAF2_HAS_LIBUUID

	::uuid_clear(m_UUID.data());

#elif DEKAF2_IS_WINDOWS

	::UuidCreateNil(ToUUID(m_UUID.data()));

#else

	::memset(m_UUID.data(), 0, m_UUID.size());

#endif

} // clear

//-----------------------------------------------------------------------------
bool KUUID::empty() const
//-----------------------------------------------------------------------------
{
#if DEKAF2_HAS_LIBUUID

	return ::uuid_is_null(m_UUID.data());

#elif DEKAF2_IS_WINDOWS

	RPC_STATUS status;
	return ::UuidIsNil(ToUUID(m_UUID.data()), &status);

#else

	for (auto ch : m_UUID)
	{
		if (ch != '\0')
		{
			return false;
		}
	}

	return true;

#endif
}

//-----------------------------------------------------------------------------
bool KUUID::operator==(const KUUID& other) const
//-----------------------------------------------------------------------------
{
#if DEKAF2_HAS_LIBUUID

	return ::uuid_compare(m_UUID.data(), other.m_UUID.data()) == 0;

#elif DEKAF2_IS_WINDOWS

	RPC_STATUS status;
	return ::UuidEqual(ToUUID(m_UUID.data()), ToUUID(other.m_UUID.data()), &status);

#else

	return m_UUID == other.m_UUID;

#endif
}

DEKAF2_NAMESPACE_END
