/*
 //
 // DEKAF(tm): Lighter, Faster, Smarter(tm)
 //
 // Copyright (c) 2018, Ridgeware, Inc.
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

#pragma once

/// @file kcrc.h
/// CRC computations

#include <dekaf2/core/init/kdefinitions.h>
#include <dekaf2/io/streams/kstream.h>
#include <dekaf2/core/strings/kstringview.h>
#include <dekaf2/core/strings/kstring.h>
#include <cstdint>

DEKAF2_NAMESPACE_BEGIN

/// @addtogroup crypto_hash
/// @{

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// CRC-32 (polynomial 0x04C11DB7, reflected, init/xorout 0xFFFFFFFF - the
/// zip/Ethernet CRC).
/// Feed data incrementally via Update()/operator+=()/operator() and read the
/// result with CRC(). The lookup table is a compile-time constant built once in
/// kcrc.cpp. For a CRC of data known at compile time (e.g. turning string
/// literals into integer ids) use the constexpr kCRC32() free function below.
class DEKAF2_PUBLIC KCRC32
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	/// default ctor
	KCRC32() = default;

	/// ctor with a string
	KCRC32(KStringView sInput)
	{
		Update(sInput);
	}

	/// ctor with a stream
	KCRC32(KInStream& InputStream)
	{
		Update(InputStream);
	}

	/// appends a string to the CRC
	bool Update(KStringView sInput);

	/// appends a stream to the CRC
	bool Update(KInStream& InputStream);

	/// appends a string to the CRC
	KCRC32& operator+=(KStringView sInput)
	{
		Update(sInput);
		return *this;
	}

	/// returns the CRC as integer
	uint32_t CRC() const
	{
		return m_iCRC ^ 0xFFFFFFFFu;
	}

	/// appends a string to the CRC
	void operator()(KStringView sInput)
	{
		Update(sInput);
	}

	/// appends a stream to the CRC
	void operator()(KInStream& InputStream)
	{
		Update(InputStream);
	}

	/// returns the CRC as integer
	uint32_t operator()() const
	{
		return CRC();
	}

	/// clears the CRC and prepares for new computation
	void clear()
	{
		m_iCRC = 0xFFFFFFFFu;
	}

//------
protected:
//------

	uint32_t m_iCRC { 0xFFFFFFFFu }; ///< running CRC, before the final xor-out

}; // KCRC32

inline bool operator==(const KCRC32& left, const KCRC32& right) { return left.CRC() == right.CRC(); }
inline bool operator!=(const KCRC32& left, const KCRC32& right) { return !operator==(left, right); }

//-----------------------------------------------------------------------------
/// CRC-32 of data known at compile time, e.g. for turning string literals into
/// integer ids (switch labels, perfect-hash dispatch). Computed bitwise so it
/// needs no lookup table, and yields exactly the same value as KCRC32. At run
/// time prefer KCRC32 (table-driven, ~8x fewer steps) for sizable inputs.
DEKAF2_CONSTEXPR_14 uint32_t kCRC32(KStringView sInput)
//-----------------------------------------------------------------------------
{
	uint32_t iCRC = 0xFFFFFFFFu;
	for (std::size_t i = 0; i < sInput.size(); ++i)
	{
		iCRC ^= static_cast<uint8_t>(sInput[i]);
		for (int k = 0; k < 8; ++k)
		{
			iCRC = (iCRC & 1u) ? ((iCRC >> 1) ^ 0xEDB88320u) : (iCRC >> 1);
		}
	}
	return iCRC ^ 0xFFFFFFFFu;

} // kCRC32

/// @}

DEKAF2_NAMESPACE_END
