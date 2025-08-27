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

#if DEKAF2_HAS_LIBUUID || DEKAF2_IS_WINDOWS

#if DEKAF2_HAS_LIBUUID

	#include <uuid/uuid.h>

#elif DEKAF2_IS_WINDOWS

	#include <windows.h>
	#include <rpcdce.h>

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

#endif

DEKAF2_NAMESPACE_BEGIN

//-----------------------------------------------------------------------------
KUUID::KUUID(Variant var)
//-----------------------------------------------------------------------------
{
#if DEKAF2_HAS_LIBUUID

	switch (var)
	{
		case dekaf2::KUUID::Variant::Null:
			clear();
			break;

		case dekaf2::KUUID::Variant::Random:
			::uuid_generate_random(m_UUID.data());
			break;

		case dekaf2::KUUID::Variant::MACTime:
			::uuid_generate_time(m_UUID.data());
			break;
	}

#elif DEKAF2_IS_WINDOWS

	switch (var)
	{
		case dekaf2::KUUID::Variant::Null:
			clear();
			break;

		default:
			::UuidCreate(ToUUID(m_UUID.data()));
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

	dekaf2::KString sUUID(36, '\0');
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

#endif
}

DEKAF2_NAMESPACE_END

#endif // DEKAF2_HAS_LIBUUID || DEKAF2_IS_WINDOWS
