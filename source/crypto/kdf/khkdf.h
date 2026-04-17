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
 //
 */

#pragma once

/// @file khkdf.h
/// @brief HKDF key derivation (RFC 5869)
///
/// KHKDF implements the HMAC-based Extract-and-Expand Key Derivation
/// Function as specified in RFC 5869.  HKDF is the standard way to
/// derive cryptographically strong keying material from an input
/// secret (e.g. an ECDH shared secret or a password hash).
///
/// @par Why HKDF?
/// Raw shared secrets (e.g. from ECDH) should never be used directly
/// as encryption keys.  They may have biased bits or be longer/shorter
/// than the cipher requires.  HKDF extracts entropy from the input
/// and expands it into keys of any desired length, with strong
/// cryptographic separation between different derived keys.
///
/// @par The two phases
/// HKDF operates in two stages:
/// -# **Extract**: condenses the input keying material (IKM) and an
///    optional salt into a fixed-size pseudorandom key (PRK) using
///    HMAC.  This step concentrates the entropy.
/// -# **Expand**: derives one or more output keys of arbitrary length
///    from the PRK, using an optional context string (info) for
///    domain separation.
///
/// The convenience method DeriveKey() combines both steps.
///
/// @par Typical usage with ECDH
/// @code
///   // Alice and Bob derive a shared secret via ECDH
///   KECKey Alice(true), Bob(true);
///   KString sSecret = Alice.DeriveSharedSecret(Bob.GetPublicKeyRaw());
///
///   // derive separate keys for encryption and authentication
///   KString sEncKey  = KHKDF::DeriveKey({}, sSecret, "enc",  32);  // 256-bit AES key
///   KString sAuthKey = KHKDF::DeriveKey({}, sSecret, "auth", 32);  // 256-bit HMAC key
///   KString sNonce   = KHKDF::DeriveKey({}, sSecret, "nonce", 12); // 96-bit GCM nonce
/// @endcode
///
/// @par Domain separation via the info parameter
/// The info string allows deriving multiple independent keys from the
/// same secret.  As long as the info strings differ, the derived keys
/// are cryptographically independent:
/// @code
///   auto sKey1 = KHKDF::DeriveKey(sSalt, sIKM, "purpose-a", 32);
///   auto sKey2 = KHKDF::DeriveKey(sSalt, sIKM, "purpose-b", 32);
///   // sKey1 != sKey2 (with overwhelming probability)
/// @endcode
///
/// @par Salt
/// The salt is optional but recommended.  It adds an extra layer of
/// randomness.  If omitted (empty), a string of hash-length zero
/// bytes is used per RFC 5869 Section 2.2.
///
/// @par Usage in Web Push (RFC 8291)
/// KWebPush uses KHKDF internally to derive the content encryption key
/// (CEK) and nonce for AES-128-GCM from the ECDH shared secret, the
/// auth secret, and protocol-specific info strings.
///
/// @see KECKey::DeriveSharedSecret() — produces the ECDH shared secret
/// @see RFC 5869 — HMAC-based Extract-and-Expand Key Derivation Function
/// @see RFC 8291 — Message Encryption for Web Push (uses HKDF)

#include <dekaf2/core/init/kdefinitions.h>
#include <dekaf2/core/strings/kstringview.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/crypto/hash/bits/kdigest.h>

DEKAF2_NAMESPACE_BEGIN

/// @addtogroup crypto_kdf
/// @{

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// HMAC-based Key Derivation Function (RFC 5869).
///
/// All methods are static — KHKDF carries no state and can be used
/// without instantiation.  The default hash algorithm is SHA-256;
/// all algorithms supported by KDigest can be used.
///
/// @note The class inherits from KDigest solely to access the Digest
///       enum and helper methods, not for polymorphism.
class DEKAF2_PUBLIC KHKDF : public KDigest
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	/// HKDF-Extract: derive a pseudorandom key from input keying material
	/// @param sSalt  optional salt value (if empty, a hash-length zero block is used)
	/// @param sIKM   input keying material
	/// @param digest hash algorithm to use (default SHA256)
	/// @return the pseudorandom key (PRK), hash-length bytes
	DEKAF2_NODISCARD
	static KString Extract(KStringView sSalt, KStringView sIKM, enum Digest digest = SHA256);

	/// HKDF-Expand: expand a pseudorandom key to the desired length
	/// @param sPRK    pseudorandom key (from Extract)
	/// @param sInfo   optional context/application-specific info
	/// @param iLength desired output length in bytes
	/// @param digest  hash algorithm to use (default SHA256)
	/// @return the output keying material (OKM), iLength bytes
	DEKAF2_NODISCARD
	static KString Expand(KStringView sPRK, KStringView sInfo, std::size_t iLength, enum Digest digest = SHA256);

	/// Combined Extract-then-Expand — the most common entry point.
	/// Equivalent to `Expand(Extract(sSalt, sIKM, digest), sInfo, iLength, digest)`.
	/// @param sSalt   optional salt (if empty, hash-length zero block is used)
	/// @param sIKM    input keying material (e.g. ECDH shared secret)
	/// @param sInfo   optional context/application-specific info for domain separation
	/// @param iLength desired output length in bytes (up to 255 * hash length)
	/// @param digest  hash algorithm to use (default SHA256)
	/// @return the derived key, iLength bytes
	DEKAF2_NODISCARD
	static KString DeriveKey(KStringView sSalt, KStringView sIKM, KStringView sInfo, std::size_t iLength, enum Digest digest = SHA256);

}; // KHKDF

/// @}

DEKAF2_NAMESPACE_END
