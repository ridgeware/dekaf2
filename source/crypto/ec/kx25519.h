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

/// @file kx25519.h
/// @brief X25519 Diffie-Hellman key exchange (Curve25519)
///
/// KX25519 provides key generation, import/export, and Diffie-Hellman
/// key agreement using the X25519 function (RFC 7748).  X25519 operates
/// on Curve25519, a Montgomery curve designed by Daniel J. Bernstein
/// with fully transparent parameter selection and inherent resistance
/// to timing side-channel attacks.
///
/// @par Key formats
/// - **Private key**: 32 bytes (clamped scalar)
/// - **Public key**: 32 bytes (u-coordinate on Curve25519)
/// - **PEM**: standard PEM-encoded PKCS#8 private or SubjectPublicKeyInfo
///
/// @par X25519 key agreement
/// X25519 allows two parties to establish a shared secret over an insecure
/// channel.  Each party generates a key pair and shares only the public key:
///
/// @code
///   Alice                              Bob
///   ─────                              ───
///   KX25519 Alice(true);               KX25519 Bob(true);
///
///   // exchange 32-byte public keys (e.g. over the network)
///   KString sPubAlice = Alice.GetPublicKeyRaw();
///   KString sPubBob   = Bob.GetPublicKeyRaw();
///
///   // both sides compute the same 32-byte shared secret
///   KString sSecretA = Alice.DeriveSharedSecret(sPubBob);
///   KString sSecretB = Bob.DeriveSharedSecret(sPubAlice);
///   assert(sSecretA == sSecretB);  // always true
/// @endcode
///
/// The shared secret should be fed into a KDF (e.g. KHKDF) to derive
/// purpose-specific keys:
///
/// @code
///   KString sAESKey = KHKDF::DeriveKey({}, sSecret, "aes-key", 32);
///   KString sNonce  = KHKDF::DeriveKey({}, sSecret, "nonce",   12);
/// @endcode
///
/// @par Comparison with KECKey (P-256)
/// | Feature         | KECKey (P-256)       | KX25519 (Curve25519)      |
/// |-----------------|----------------------|---------------------------|
/// | Security level  | ~128 bits            | ~128 bits                 |
/// | Public key size | 65 bytes             | 32 bytes                  |
/// | Private key     | 32 bytes             | 32 bytes                  |
/// | Shared secret   | 32 bytes             | 32 bytes                  |
/// | Side-channel    | implementation-dep.  | resistant by design       |
/// | TLS 1.3         | supported            | preferred (X25519)        |
/// | Web Push        | mandatory (RFC 8291) | not available             |
///
/// @note Requires OpenSSL >= 1.1.0 (EVP_PKEY_X25519).
/// @note Move-only type — copy construction/assignment is deleted.
///
/// @see KEd25519Sign, KEd25519Verify — Ed25519 signatures (same curve family)
/// @see KECKey — P-256 ECDH (for Web Push and legacy protocols)
/// @see KHKDF — key derivation from shared secrets

#include <dekaf2/core/strings/kstringview.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/errors/kerror.h>

#include <openssl/opensslv.h>

// X25519 was introduced in OpenSSL 1.1.0
#if OPENSSL_VERSION_NUMBER >= 0x010100000L
#define DEKAF2_HAS_X25519 1
#endif

#if DEKAF2_HAS_X25519

struct evp_pkey_st;

DEKAF2_NAMESPACE_BEGIN

/// @addtogroup crypto_ec
/// @{

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// X25519 Diffie-Hellman key class (Curve25519, RFC 7748).
///
/// Supports key generation, PEM and raw import/export, and X25519 shared
/// secret derivation.  All operations report errors through the
/// KErrorBase interface (HasError(), Error()).
///
/// Curve25519 provides ~128 bits of security and is the preferred key
/// exchange curve in TLS 1.3, SSH, Signal, and WireGuard.
class DEKAF2_PUBLIC KX25519 : public KErrorBase
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	/// default ctor (empty key)
	KX25519() = default;

	/// generate a new X25519 key pair
	/// @param bGenerate if true, generates a new key pair immediately
	explicit KX25519(bool bGenerate)
	{
		if (bGenerate) Generate();
	}

	/// construct from a PEM string
	/// @param sPEM the PEM-encoded private or public key
	/// @param sPassword optional password for encrypted PEM keys
	explicit KX25519(KStringView sPEM, KStringViewZ sPassword = KStringViewZ{})
	{
		CreateFromPEM(sPEM, sPassword);
	}

	/// copy construction not allowed (OpenSSL handle ownership)
	KX25519(const KX25519&) = delete;
	/// move construction
	KX25519(KX25519&& other) noexcept;
	/// dtor
	~KX25519() { clear(); }

	/// copy assignment not allowed
	KX25519& operator=(const KX25519&) = delete;
	/// move assignment
	KX25519& operator=(KX25519&& other) noexcept;

	/// generate a new X25519 key pair
	/// @return true on success
	bool Generate();

	/// load a key from a PEM string
	/// @param sPEM the PEM-encoded private or public key
	/// @param sPassword optional password for encrypted PEM keys
	/// @return true on success
	bool CreateFromPEM(KStringView sPEM, KStringViewZ sPassword = KStringViewZ{});

	/// load a key from raw bytes (private key: 32 bytes, public key: 32 bytes)
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

	/// get the raw private key (32 bytes)
	/// @return raw private key bytes, empty on error
	DEKAF2_NODISCARD
	KString GetPrivateKeyRaw() const;

	/// get the raw public key (32 bytes)
	/// @return raw public key bytes, empty on error
	DEKAF2_NODISCARD
	KString GetPublicKeyRaw() const;

	/// get the key as a PEM string
	/// @param bPrivateKey if true, exports the private key; otherwise the public key
	/// @return PEM string, empty on error
	DEKAF2_NODISCARD
	KString GetPEM(bool bPrivateKey = true) const;

	/// perform X25519 Diffie-Hellman key agreement with a peer's public key
	/// @param sPeerPublicRaw the peer's public key (32 bytes)
	/// @return the shared secret (32 bytes), empty on error
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

}; // KX25519

/// @}

DEKAF2_NAMESPACE_END

#endif // DEKAF2_HAS_X25519
