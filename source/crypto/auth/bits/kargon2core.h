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

/// @file kargon2core.h
/// the Argon2 functions of RFC 9106, computed without a crypto library

#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/strings/kstringview.h>

DEKAF2_NAMESPACE_BEGIN

namespace detail {

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// The Argon2 functions of RFC 9106 (version 0x13), computed without a crypto
/// library - the core of KArgon2.
class DEKAF2_PUBLIC KArgon2Core
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	//-----------------------------------------------------------------------------
	/// computes the tag of a password
	/// @param iVariant 0 for Argon2d, 1 for Argon2i, 2 for Argon2id
	/// @param sPassword the password
	/// @param sSalt the salt, at least 8 bytes
	/// @param sSecret an optional secret that is no part of the hash string
	/// @param sAssociatedData optional data that is bound to the tag
	/// @param iMemoryKiB the memory to fill in KiB, at least 8 times the parallelism
	/// @param iIterations the count of passes over the memory, at least 1
	/// @param iParallelism the count of lanes, from 1 to 16777215 - they are computed one after the other
	/// @param iTagLength the length of the tag in bytes, at least 4
	/// @return the tag, or an empty string for parameters outside of these limits
	/// and when the memory cannot be allocated
	static KString Derive(uint32_t    iVariant,
	                      KStringView sPassword,
	                      KStringView sSalt,
	                      KStringView sSecret,
	                      KStringView sAssociatedData,
	                      uint32_t    iMemoryKiB,
	                      uint32_t    iIterations,
	                      uint32_t    iParallelism,
	                      std::size_t iTagLength);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// BLAKE2b of RFC 7693 without a key
	/// @param sInput the data to hash
	/// @param iDigestLength the length of the digest in bytes, from 1 to 64
	/// @return the digest, or an empty string for another digest length
	static KString Blake2b(KStringView sInput, std::size_t iDigestLength);
	//-----------------------------------------------------------------------------

}; // KArgon2Core

} // end of namespace detail

DEKAF2_NAMESPACE_END
