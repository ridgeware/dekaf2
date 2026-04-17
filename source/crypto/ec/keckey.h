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

/// @file keckey.h
/// @brief Elliptic curve key management for NIST P-256 (secp256r1)
///
/// KECKey provides generation, import/export, and Elliptic Curve
/// Diffie-Hellman (ECDH) key agreement for the NIST P-256 curve
/// (also known as secp256r1 or prime256v1).  It wraps the OpenSSL
/// EVP_PKEY API and supports both OpenSSL 1.x and 3.x.
///
/// @par Key formats
/// - **Raw private key**: 32 bytes (the scalar d)
/// - **Raw public key**: 65 bytes uncompressed (0x04 || x || y),
///   where x and y are the 32-byte big-endian coordinates on the curve
/// - **PEM**: standard PEM-encoded EC private or public key
///
/// @par ECDH key agreement (Diffie-Hellman on elliptic curves)
/// ECDH allows two parties to establish a shared secret over an insecure
/// channel.  Each party generates a key pair and shares only the public
/// key.  Both parties then compute the same shared secret independently:
///
/// @code
///   Alice                              Bob
///   ─────                              ───
///   KECKey Alice(true);                KECKey Bob(true);
///
///   // exchange public keys (e.g. over the network)
///   KString sPubAlice = Alice.GetPublicKeyRaw();
///   KString sPubBob   = Bob.GetPublicKeyRaw();
///
///   // both sides compute the same 32-byte shared secret
///   KString sSecretA = Alice.DeriveSharedSecret(sPubBob);
///   KString sSecretB = Bob.DeriveSharedSecret(sPubAlice);
///   assert(sSecretA == sSecretB);  // always true
/// @endcode
///
/// The shared secret is typically not used directly as an encryption
/// key.  Instead it is fed into a key derivation function like HKDF
/// (see KHKDF) to produce one or more purpose-specific keys:
///
/// @code
///   // derive a 32-byte AES key and a 12-byte nonce from the shared secret
///   KString sAESKey = KHKDF::DeriveKey({}, sSecret, "aes-key", 32);
///   KString sNonce  = KHKDF::DeriveKey({}, sSecret, "nonce",   12);
/// @endcode
///
/// @par Usage in Web Push (RFC 8291)
/// KWebPush uses KECKey internally: the server generates an ephemeral
/// key pair for each push message, performs ECDH with the browser's
/// subscription public key (p256dh), and derives the content encryption
/// key via HKDF — all according to RFC 8291.
///
/// @par Key import examples
/// @code
///   // from PEM
///   KECKey FromPEM(sPrivatePEM);
///
///   // from raw bytes (e.g. stored in a database)
///   KECKey FromRaw;
///   FromRaw.CreateFromRaw(sRaw32Bytes, true);   // private key
///   FromRaw.CreateFromRaw(sRaw65Bytes, false);  // public key
///
///   // export
///   KString sPEM    = Key.GetPEM(true);           // private PEM
///   KString sPubPEM = Key.GetPEM(false);          // public PEM
///   KString sRawPub = Key.GetPublicKeyRaw();      // 65 bytes
/// @endcode
///
/// @note Move-only type — copy construction/assignment is deleted
///       because the class owns an OpenSSL EVP_PKEY handle.
///
/// @see KECSign, KECVerify — ECDSA signatures using the same key type
/// @see KHKDF — key derivation from shared secrets
/// @see KWebPush — Web Push (uses ECDH + HKDF internally)

#include <dekaf2/core/strings/kstringview.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/errors/kerror.h>

struct evp_pkey_st;

DEKAF2_NAMESPACE_BEGIN

/// @addtogroup crypto_ec
/// @{

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Elliptic curve key class for NIST P-256 (secp256r1).
///
/// Supports key generation, PEM and raw import/export, and ECDH shared
/// secret derivation.  All operations report errors through the
/// KErrorBase interface (HasError(), Error()).
///
/// P-256 provides ~128 bits of security and is the mandatory curve for
/// Web Push (RFC 8291), VAPID (RFC 8292), and many TLS cipher suites.
class DEKAF2_PUBLIC KECKey : public KErrorBase
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	/// default ctor (empty key)
	KECKey() = default;

	/// generate a new P-256 key pair
	/// @param bGenerate if true, generates a new key pair immediately
	explicit KECKey(bool bGenerate)
	{
		if (bGenerate) Generate();
	}

	/// construct from a PEM string
	/// @param sPEM the PEM-encoded private or public key
	/// @param sPassword optional password for encrypted PEM keys
	explicit KECKey(KStringView sPEM, KStringViewZ sPassword = KStringViewZ{})
	{
		CreateFromPEM(sPEM, sPassword);
	}

	/// copy construction not allowed (OpenSSL handle ownership)
	KECKey(const KECKey&) = delete;
	/// move construction
	KECKey(KECKey&& other) noexcept;
	/// dtor
	~KECKey() { clear(); }

	/// copy assignment not allowed
	KECKey& operator=(const KECKey&) = delete;
	/// move assignment
	KECKey& operator=(KECKey&& other) noexcept;

	/// generate a new P-256 key pair
	/// @return true on success
	bool Generate();

	/// load a key from a PEM string
	/// @param sPEM the PEM-encoded private or public key
	/// @param sPassword optional password for encrypted PEM keys
	/// @return true on success
	bool CreateFromPEM(KStringView sPEM, KStringViewZ sPassword = KStringViewZ{});

	/// load a key from raw bytes (private key: 32 bytes, public key: 65 bytes uncompressed)
	/// @param sRawKey the raw key bytes
	/// @param bIsPrivateKey true if the raw bytes represent a private key
	/// @return true on success
	bool CreateFromRaw(KStringView sRawKey, bool bIsPrivateKey = false);

	/// test if key is set
	DEKAF2_NODISCARD
	bool empty() const { return !m_EVPPKey; }
	/// reset the key
	void clear();

	/// is this a private key?
	DEKAF2_NODISCARD
	bool IsPrivateKey() const { return m_bIsPrivateKey; }

	/// get the raw private key (32 bytes for P-256)
	/// @return raw private key bytes, empty on error
	DEKAF2_NODISCARD
	KString GetPrivateKeyRaw() const;

	/// get the raw uncompressed public key (65 bytes for P-256: 0x04 || x || y)
	/// @return raw public key bytes, empty on error
	DEKAF2_NODISCARD
	KString GetPublicKeyRaw() const;

	/// get the key as a PEM string
	/// @param bPrivateKey if true, exports the private key; otherwise the public key
	/// @return PEM string, empty on error
	DEKAF2_NODISCARD
	KString GetPEM(bool bPrivateKey = true) const;

	/// perform ECDH key derivation with a peer's public key
	/// @param sPeerPublicRaw the peer's uncompressed public key (65 bytes)
	/// @return the shared secret (32 bytes for P-256), empty on error
	DEKAF2_NODISCARD
	KString DeriveSharedSecret(KStringView sPeerPublicRaw) const;

	/// get the underlying OpenSSL key handle
	DEKAF2_NODISCARD
	evp_pkey_st* GetEVPPKey() const { return m_EVPPKey; }

//------
private:
//------

	evp_pkey_st* m_EVPPKey       { nullptr };
	bool         m_bIsPrivateKey { false   };

}; // KECKey

/// @}

DEKAF2_NAMESPACE_END
