/*
 //
 // DEKAF(tm): Lighter, Faster, Smarter (tm)
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
 */

#pragma once

/// @file kbase32.h
/// Base32 (RFC 4648) encoding and decoding

#include <dekaf2/core/init/kdefinitions.h>
#include <dekaf2/core/strings/kstringview.h>
#include <dekaf2/core/strings/kstring.h>

DEKAF2_NAMESPACE_BEGIN

/// @addtogroup crypto_encoding
/// @{

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Base32 as defined in RFC 4648 - the all-caps "A-Z 2-7" alphabet used by TOTP
/// secrets, recovery keys and many provisioning formats. Encoding pads with '='
/// to a multiple of eight characters by default; decoding ignores padding and
/// whitespace and accepts either letter case.
class DEKAF2_PUBLIC KBase32
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	//-----------------------------------------------------------------------------
	/// Encode bytes as RFC 4648 base32.
	/// @param sInput the raw bytes to encode
	/// @param bPadding append '=' padding to a multiple of 8 characters (the spec
	/// default); pass false for the compact, unpadded form authenticator apps use
	DEKAF2_NODISCARD
	static KString Encode(KStringView sInput, bool bPadding = true);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Decode RFC 4648 base32 into raw bytes. Whitespace and '=' padding are
	/// ignored and input is case-insensitive; returns an empty string if a
	/// non-alphabet character is encountered.
	DEKAF2_NODISCARD
	static KString Decode(KStringView sInput);
	//-----------------------------------------------------------------------------

}; // KBase32

/// @}

DEKAF2_NAMESPACE_END
