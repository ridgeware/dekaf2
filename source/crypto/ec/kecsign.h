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

/// @file kecsign.h
/// @brief ECDSA signatures with ES256 (P-256 + SHA-256)
///
/// KECSign and KECVerify implement the ES256 digital signature algorithm
/// as specified in RFC 7518 (JSON Web Algorithms).  ES256 combines
/// ECDSA on the NIST P-256 curve with SHA-256 hashing.
///
/// @par How ECDSA works (simplified)
/// -# The signer hashes the message with SHA-256.
/// -# Using the private key and a random nonce, ECDSA produces two
///    32-byte integers (r, s) that form the signature.
/// -# The verifier uses the public key, the original message, and the
///    signature to check validity — without ever needing the private key.
///
/// @par Signature format
/// KECSign returns the **raw r||s** concatenation (exactly 64 bytes),
/// not the DER-encoded ASN.1 format that OpenSSL uses internally.
/// This is the format required by JWS/JWT (RFC 7515) and VAPID
/// (RFC 8292).
///
/// @par Typical usage
/// @code
///   // generate a key pair
///   KECKey Key(true);
///
///   // sign
///   KECSign Signer;
///   KString sSig = Signer.Sign(Key, "data to protect");
///   // sSig is exactly 64 bytes (r: 32 bytes, s: 32 bytes)
///
///   // verify (can use the same key or a public-only key)
///   KECVerify Verifier;
///   bool bOK = Verifier.Verify(Key, "data to protect", sSig);
/// @endcode
///
/// @par Verification with public key only
/// @code
///   // extract public key (e.g. from a PEM or raw bytes)
///   KECKey PubOnly;
///   PubOnly.CreateFromRaw(Key.GetPublicKeyRaw(), false);
///
///   KECVerify Verifier;
///   bool bOK = Verifier.Verify(PubOnly, "data to protect", sSig);
/// @endcode
///
/// @par Usage in VAPID (RFC 8292)
/// KWebPush uses KECSign internally to sign the VAPID JWT token that
/// authenticates the application server to the push service.  The push
/// service verifies the signature using the VAPID public key that the
/// browser received during subscription.
///
/// @see KECKey — the key type used for signing and verification
/// @see KHKDF — often used together with ECDH + ECDSA in protocols
/// @see KWebPush — Web Push (uses ES256 for VAPID tokens)

#include <dekaf2/core/strings/kstringview.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/crypto/ec/keckey.h>

DEKAF2_NAMESPACE_BEGIN

/// @addtogroup crypto_ec
/// @{

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Sign data with ES256 (ECDSA P-256 + SHA-256).
///
/// Produces a 64-byte raw r||s signature suitable for JWS/JWT and VAPID.
/// The signer requires a KECKey that contains a private key.  Errors are
/// reported through the KErrorBase interface.
class DEKAF2_PUBLIC KECSign : public KErrorBase
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	/// sign data with ES256 using the given private key
	/// @param Key the EC private key
	/// @param sData the data to sign
	/// @return raw r||s signature (64 bytes), empty on error
	DEKAF2_NODISCARD
	KString Sign(const KECKey& Key, KStringView sData) const;

}; // KECSign

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Verify an ES256 (ECDSA P-256 + SHA-256) signature.
///
/// Accepts a KECKey with either a private or public key.  The signature
/// must be in raw r||s format (64 bytes).  Returns true only if the
/// signature is cryptographically valid for the given data and key.
class DEKAF2_PUBLIC KECVerify : public KErrorBase
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	/// verify an ES256 signature
	/// @param Key the EC public or private key
	/// @param sData the original data
	/// @param sSignature raw r||s signature (64 bytes)
	/// @return true if the signature is valid
	DEKAF2_NODISCARD
	bool Verify(const KECKey& Key, KStringView sData, KStringView sSignature) const;

}; // KECVerify

/// @}

DEKAF2_NAMESPACE_END
