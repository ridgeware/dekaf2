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

/// @file ked25519sign.h
/// @brief Ed25519 digital signatures (RFC 8032)
///
/// KEd25519Sign and KEd25519Verify implement the Ed25519 signature scheme
/// as specified in RFC 8032 (Edwards-Curve Digital Signature Algorithm).
/// Ed25519 uses Curve25519 in twisted Edwards form with SHA-512 as the
/// internal hash — the hash is built into the algorithm and cannot be
/// changed.
///
/// @par How Ed25519 works (simplified)
/// -# The signer computes a deterministic nonce from the private key
///    and message (no external randomness needed — unlike ECDSA).
/// -# Using the nonce and private key, Ed25519 produces a 64-byte
///    signature (R || S), where R is a curve point and S is a scalar.
/// -# The verifier uses the public key, the original message, and the
///    signature to check validity.
///
/// @par Signature format
/// Ed25519 signatures are always exactly 64 bytes in raw format.
/// Unlike ECDSA (KECSign), there is no DER encoding involved.
///
/// @par Key type
/// Ed25519 uses its own key type (KEd25519Key) rather than KECKey
/// or KX25519, because OpenSSL treats EVP_PKEY_ED25519 as a distinct
/// key type from both EVP_PKEY_EC and EVP_PKEY_X25519.
///
/// @par Typical usage
/// @code
///   // generate an Ed25519 key pair
///   KEd25519Key Key(true);
///
///   // sign
///   KEd25519Sign Signer;
///   KString sSig = Signer.Sign(Key, "data to protect");
///   // sSig is exactly 64 bytes
///
///   // verify (can use the same key or a public-only key)
///   KEd25519Verify Verifier;
///   bool bOK = Verifier.Verify(Key, "data to protect", sSig);
/// @endcode
///
/// @par Comparison with KECSign (ES256)
/// | Feature           | KECSign (ES256)        | KEd25519Sign            |
/// |-------------------|------------------------|-------------------------|
/// | Curve             | P-256 (NIST)           | Curve25519 (Bernstein)  |
/// | Hash              | SHA-256 (separate)     | SHA-512 (built-in)      |
/// | Signature size    | 64 bytes (r‖s)         | 64 bytes (R‖S)          |
/// | Deterministic     | no (random nonce)      | yes (RFC 6979-like)     |
/// | Side-channel      | implementation-dep.    | resistant by design     |
/// | JWS/JWT algorithm | ES256                  | EdDSA                   |
///
/// @note Requires OpenSSL >= 1.1.1 (EVP_PKEY_ED25519).
/// @note KEd25519Key is move-only.
///
/// @see KX25519 — X25519 key exchange (same curve family)
/// @see KECSign — ECDSA ES256 signatures (P-256)

#include <dekaf2/core/strings/kstringview.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/errors/kerror.h>

#include <openssl/opensslv.h>

// Ed25519 was introduced in OpenSSL 1.1.1. Our implementation also relies on
// EVP_PKEY_new_raw_private_key / EVP_PKEY_new_raw_public_key /
// EVP_PKEY_get_raw_private_key / EVP_PKEY_get_raw_public_key, which LibreSSL
// only gained (for EVP_PKEY_ED25519) in LibreSSL 3.7.0 (Dec 2022, see the
// 3.7.0 release notes). Earlier LibreSSL releases advertise a compatible
// OPENSSL_VERSION_NUMBER but lack these accessors and must be excluded.
#if OPENSSL_VERSION_NUMBER >= 0x010101000L \
 && (!defined(LIBRESSL_VERSION_NUMBER) || LIBRESSL_VERSION_NUMBER >= 0x3070000fL)
#define DEKAF2_HAS_ED25519 1
#endif

#if DEKAF2_HAS_ED25519

struct evp_pkey_st;

DEKAF2_NAMESPACE_BEGIN

/// @addtogroup crypto_ec
/// @{

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Ed25519 key class (RFC 8032).
///
/// Supports key generation, PEM and raw import/export.
/// Private key: 32 bytes (seed), Public key: 32 bytes.
/// All operations report errors through KErrorBase.
class DEKAF2_PUBLIC KEd25519Key : public KErrorBase
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	/// default ctor (empty key)
	KEd25519Key() = default;

	/// generate a new Ed25519 key pair
	/// @param bGenerate if true, generates a new key pair immediately
	explicit KEd25519Key(bool bGenerate)
	{
		if (bGenerate) Generate();
	}

	/// construct from a PEM string
	/// @param sPEM the PEM-encoded private or public key
	/// @param sPassword optional password for encrypted PEM keys
	explicit KEd25519Key(KStringView sPEM, KStringViewZ sPassword = KStringViewZ{})
	{
		CreateFromPEM(sPEM, sPassword);
	}

	/// copy construction not allowed (OpenSSL handle ownership)
	KEd25519Key(const KEd25519Key&) = delete;
	/// move construction
	KEd25519Key(KEd25519Key&& other) noexcept;
	/// dtor
	~KEd25519Key() { clear(); }

	/// copy assignment not allowed
	KEd25519Key& operator=(const KEd25519Key&) = delete;
	/// move assignment
	KEd25519Key& operator=(KEd25519Key&& other) noexcept;

	/// generate a new Ed25519 key pair
	/// @return true on success
	bool Generate();

	/// load a key from a PEM string
	/// @param sPEM the PEM-encoded private or public key
	/// @param sPassword optional password for encrypted PEM keys
	/// @return true on success
	bool CreateFromPEM(KStringView sPEM, KStringViewZ sPassword = KStringViewZ{});

	/// load a key from raw bytes (private key: 32 bytes seed, public key: 32 bytes)
	/// @param sRawKey the raw key bytes
	/// @param bIsPrivateKey true if the raw bytes represent a private key (seed)
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

	/// get the raw private key (32 bytes seed)
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

	/// get the underlying OpenSSL key handle
	DEKAF2_NODISCARD
	evp_pkey_st* GetEVPPKey() const { return m_EVPPKey; }

//------
private:
//------

	evp_pkey_st* m_EVPPKey       { nullptr };
	bool         m_bIsPrivateKey { false   };

}; // KEd25519Key

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Sign data with Ed25519 (RFC 8032).
///
/// Produces a 64-byte signature.  Ed25519 is deterministic — signing the
/// same message with the same key always produces the same signature.
/// The signer requires a KEd25519Key that contains a private key.
class DEKAF2_PUBLIC KEd25519Sign : public KErrorBase
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	/// sign data with Ed25519 using the given private key
	/// @param Key the Ed25519 private key
	/// @param sData the data to sign
	/// @return raw signature (64 bytes), empty on error
	DEKAF2_NODISCARD
	KString Sign(const KEd25519Key& Key, KStringView sData) const;

}; // KEd25519Sign

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Verify an Ed25519 (RFC 8032) signature.
///
/// Accepts a KEd25519Key with either a private or public key.  The
/// signature must be exactly 64 bytes.  Returns true only if the
/// signature is cryptographically valid for the given data and key.
class DEKAF2_PUBLIC KEd25519Verify : public KErrorBase
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	/// verify an Ed25519 signature
	/// @param Key the Ed25519 public or private key
	/// @param sData the original data
	/// @param sSignature raw signature (64 bytes)
	/// @return true if the signature is valid
	DEKAF2_NODISCARD
	bool Verify(const KEd25519Key& Key, KStringView sData, KStringView sSignature) const;

}; // KEd25519Verify

/// @}

DEKAF2_NAMESPACE_END

#endif // DEKAF2_HAS_ED25519
