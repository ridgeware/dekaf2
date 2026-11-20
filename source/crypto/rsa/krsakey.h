/*
 //
 // DEKAF(tm): Lighter, Faster, Smarter(tm)
 //
 // Copyright (c) 2019, Ridgeware, Inc.
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

/// @file krsakey.h
/// RSA key conversions

#include <dekaf2/core/strings/kstringview.h>
#include <dekaf2/data/json/kjson.h>
#include <dekaf2/core/errors/kerror.h>

struct evp_pkey_st;

DEKAF2_NAMESPACE_BEGIN

/// @addtogroup crypto_rsa
/// @{

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// RSA key class to transform formats or create a new key, holds either a private/public or a public key
class DEKAF2_PUBLIC KRSAKey : public KErrorBase
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// Parameter struct to create an RSA key
	struct Parameters
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		/// constructs the parameter set from a json node
		Parameters(const KJSON& json);

		KStringView n;
		KStringView e;
		KStringView d;
		KStringView p;
		KStringView q;
		KStringView dp;
		KStringView dq;
		KStringView qi;
	};

	/// default ctor
	KRSAKey() = default;
	/// construct a new key with the given keylen - choose values from 1024 to 16384, probably as power of 2
	KRSAKey(uint16_t iKeylen)
	{
		Create(iKeylen);
	}
	/// construct the key from parameters
	KRSAKey(const Parameters& parms)
	{
		Create(parms);
	}
	/// construct the key from JSON
	KRSAKey(const KJSON& json)
	{
		Create(json);
	}
	/// construct the key from a PEM string
	/// @param sPEMKey the string containing the PEM key
	/// @param sPassword the password to read the PEM key, defaults to no password
	KRSAKey(KStringView sPEMKey, KStringViewZ sPassword = KStringViewZ{})
	{
		Create(sPEMKey, sPassword);
	}
	/// copy construction
	KRSAKey(const KRSAKey&) = delete;
	/// move construction
	KRSAKey(KRSAKey&& other) noexcept;
	// dtor
	~KRSAKey()
	{
		clear();
	}

	/// copy assignment
	KRSAKey& operator=(const KRSAKey&) = delete;
	/// move assignment
	KRSAKey& operator=(KRSAKey&& other) noexcept;

	/// reset the key
	void clear();
	/// test if key is set
	bool empty() const { return !m_EVPPKey; }
	/// create a new key with len keylen - choose values from 1024 to 16384, probably as power of 2
	bool Create(uint16_t iKeylen);
	/// create the key from parameters
	bool Create(const Parameters& parms);
	/// create the key from JSON
	bool Create(const KJSON& json)
	{
		return Create(Parameters(json));
	}
	/// create the key from a JWK (JSON Web Key, RFC 7518 section 6.3):
	/// { "kty":"RSA", "n":..., "e":... } is a public key, with the additional
	/// member "d" (and the CRT members "p", "q", "dp", "dq", "qi", if the
	/// key has them) it is a private key. All other members ("kid", "alg",
	/// "use", ..) are ignored. This reads the same members as
	/// Create(const KJSON&), but insists on the key type "RSA". The parts of
	/// a private key are not checked against each other.
	/// @param jJWK the JWK object
	/// @return true on success
	bool CreateFromJWK(const KJSON& jJWK);
	/// create a public key from its two numbers, as COSE keys have them
	/// @param sModulus the modulus n in bytes, most significant byte first
	/// @param sExponent the public exponent e in bytes, most significant byte first
	/// @return true on success
	bool CreateFromRaw(KStringView sModulus, KStringView sExponent);
	/// get the key from a PEM string
	/// @param sPEMKey the string containing the PEM key
	/// @param sPassword the password to read the PEM key, defaults to no password
	/// @return true if the key could be read, false otherwise
	bool Create(KStringView sPEMKey, KStringViewZ sPassword = KStringViewZ{});
	/// get the key, may return nullptr in case of error or default construction
	evp_pkey_st* GetEVPPKey() const;
	/// is this a private key?
	bool IsPrivateKey() const { return m_bIsPrivateKey; }
	/// get either the public or private key as a PEM string
	KString GetPEM(bool bPrivateKey, KStringView sPassword = KStringView{});
	/// load key from file (PEM format)
	bool Load(KStringViewZ sFilename, KStringViewZ sPassword = KStringViewZ{});
	/// save key to file (PEM format)
	bool Save(KStringViewZ sFilename, bool bPrivateKey, KStringView sPassword = KStringView{});
	/// Export the public key as a JWK (JSON Web Key) with nothing but the key
	/// parameters: { "kty":"RSA", "n":..., "e":... }. This is the form for
	/// the "jwk" of a JWS header, for the input of a JWK thumbprint
	/// (RFC 7638), or for a key exchanged with WebCrypto. Modulus and
	/// exponent are base64url-encoded big-endian.
	/// @return the JWK object, or an empty KJSON if no key is set
	DEKAF2_NODISCARD
	KJSON GetPublicJWK() const;
	/// The JWK thumbprint of the public key (RFC 7638): the base64url-encoded
	/// SHA-256 of the canonical form of GetPublicJWK(). It identifies the key,
	/// and serves as a key id that changes with the key.
	/// @return the thumbprint, or an empty string if no key is set
	DEKAF2_NODISCARD
	KString GetJWKThumbprint() const;
	/// Export the public key as a JWK (JSON Web Key) suitable for a JWKS
	/// document: { "kty":"RSA", "n":..., "e":..., "kid":..., "alg":..., "use":"sig" }.
	/// Modulus and exponent are base64url-encoded big-endian (JWK spec, the
	/// inverse of Create(Parameters)/Create(json)). The result feeds an OpenID
	/// provider's jwks_uri and is consumable by KOpenIDKeys / KJWT.
	/// @param sKid the key id to advertise (the JWT header "kid")
	/// @param sAlg the JWS algorithm, defaults to "RS256"
	/// @return the JWK object, or an empty KJSON if no key is set
	DEKAF2_NODISCARD
	KJSON GetPublicJWK(KStringView sKid, KStringView sAlg = "RS256") const;
	/// Export the private key as a JWK (JSON Web Key): the key parameters of
	/// GetPublicJWK() plus the private exponent "d" and, if the key has all
	/// of them, the CRT members "p", "q", "dp", "dq" and "qi" (RFC 7518
	/// section 6.3.2).
	/// @return the JWK object, or an empty KJSON if this is not a private key
	DEKAF2_NODISCARD
	KJSON GetPrivateJWK() const;

//------
private:
//------

	evp_pkey_st* m_EVPPKey       { nullptr }; // is a EVP_PKEY
	bool         m_bIsPrivateKey { false   };

}; // KRSAKey


/// @}

DEKAF2_NAMESPACE_END
