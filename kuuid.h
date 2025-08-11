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

#if DEKAF2_HAS_LIBUUID || DEKAF2_IS_WINDOWS

#include "kstringview.h"
#include "kstring.h"
#include <array>
#include <ostream>


DEKAF2_NAMESPACE_BEGIN

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC KUUID
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	enum Variant { Null, Random, MACTime };

	/// creates a UUID of the chosen variant, defaults to Random
	KUUID(Variant var = Variant::Random);

	/// read a UUID string representation and decode it to set this UUID value
	inline
	KUUID(KStringView sUUID)                            { FromString(sUUID);   }

	KUUID(const KUUID&)            = default;
	KUUID(KUUID&&)                 = default;
	KUUID& operator=(const KUUID&) = default;
	KUUID& operator=(KUUID&&)      = default;
	KUUID& operator=(KStringView sUUID);

	/// convert the UUID value to a string representation
	KString  ToString   () const;
	/// convert the UUID value to a string representation
	KString  Serialize  () const                        { return ToString();   }
	/// convert the UUID value to a string representation
	operator KString    () const                        { return ToString();   }

	/// check if the UUID is empty
	bool     empty      () const;
	/// clear the UUID (set to empty value)
	void     clear      ();

	bool operator==(const KUUID& other) const;
	bool operator!=(const KUUID& other) const           { return !operator==(other); }

	/// create a UUID with of selected variant, defaults to Random
	static KUUID Create (Variant var = Variant::Random) { return KUUID(var);   }
	/// parse a UUID from a string representation
	static KUUID Parse  (KStringView sUUID)             { return KUUID(sUUID); }

//------
private:
//------

	void FromString(KStringView sUUID);

	std::array<unsigned char, 16> m_UUID;

}; // KUUID

inline DEKAF2_PUBLIC std::ostream& operator<<(std::ostream& stream, const KUUID& UUID)
{
	auto s = UUID.ToString();
	stream.write(s.data(), s.size());
	return stream;
}

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

#endif // DEKAF2_HAS_LIBUUID || DEKAF2_IS_WINDOWS
