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


/// @file kopenid.h
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/web/url/kurl.h>
#include <dekaf2/data/json/kjson.h>
#include <dekaf2/crypto/rsa/krsakey.h>
#include <dekaf2/crypto/ec/keckey.h>
#include <dekaf2/crypto/ec/ked25519sign.h>
#include <dekaf2/time/duration/ktimer.h>
#include <dekaf2/time/clock/ktime.h>
#include <dekaf2/core/errors/kerror.h>
#include <unordered_map>
#include <vector>
#include <atomic>

DEKAF2_NAMESPACE_BEGIN

/// @addtogroup crypto_auth
/// @{

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// holds all keys from a validated OpenID provider
class DEKAF2_PUBLIC KOpenIDKeys : public KErrorBase
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	KOpenIDKeys () = default;
	/// query all known information about an OpenID provider
	KOpenIDKeys (const KURL& URL);
	/// load keys from a JWK Set JSON object (e.g. for testing or local key sources)
	KOpenIDKeys (const KJSON& jwks);

	/// return a reference to the RSA key that matches the given parameters
	const KRSAKey& GetRSAKey(KStringView sKeyID, KStringView sAlgorithm, KStringView sKeyDigest, KStringView sUseType = "sig") const;

	/// verify a JWT signature using the key identified by sKeyID/sAlgorithm.
	/// Supports RS256/RS384/RS512 (RSA), ES256 (ECDSA P-256), and EdDSA (Ed25519).
	/// @param sKeyID     the "kid" from the JWT header
	/// @param sAlgorithm the "alg" from the JWT header
	/// @param sKeyDigest the "x5t" from the JWT header (may be empty)
	/// @param sData      the signed data (header.payload, base64url-encoded)
	/// @param sSignature the raw decoded signature bytes
	/// @param sUseType   the expected key use (default "sig")
	/// @return true if the signature is valid
	bool VerifySignature(KStringView sKeyID, KStringView sAlgorithm, KStringView sKeyDigest,
	                     KStringView sData,  KStringView sSignature, KStringView sUseType = "sig") const;

	/// are all info valid?
	bool IsValid() const { return !HasError(); }

	bool empty() const { return WebKeys.empty(); }
	std::size_t size() const { return WebKeys.size(); }

//----------
private:
//----------

	static const KRSAKey s_EmptyKey;

	struct DEKAF2_PRIVATE WebKey
	{
		WebKey(const KJSON& parms);

		KRSAKey     RSAKey;
		KECKey      ECKey;
#if DEKAF2_HAS_ED25519
		KEd25519Key Ed25519Key;
#endif
		KString     Algorithm;
		KString     Digest;
		KString     UseType;
	};

	std::unordered_map<KString, WebKey> WebKeys;

	DEKAF2_PRIVATE
	bool Validate(const KJSON& Keys) const;

}; // KOpenIDKeys

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// holds all data from a validated OpenID provider
class DEKAF2_PUBLIC KOpenIDProvider : public KErrorBase
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	KOpenIDProvider () = default;
	/// query all known information about an OpenID provider, check if scope is
	/// provided if not empty
	KOpenIDProvider (KURL URL,
	                 KStringView sScope = KStringView{},
	                 KDuration RefreshInterval = std::chrono::hours(24),
	                 bool bMustSupportScope = true);

	/// are all info valid?
	bool IsValid() const { return !HasError(); }

	struct KeysAndIssuer
	{
		KOpenIDKeys Keys;
		KString     sIssuer;
	};

	const KeysAndIssuer& Get() const { return *m_CurrentKeys->load(std::memory_order_relaxed); }

	void Refresh(KUnixTime Now = KUnixTime::now());

//----------
private:
//----------

	DEKAF2_PRIVATE
	bool Validate(const KJSON& Configuration, const KURL& URL, KStringView sScope) const;

	std::unique_ptr<KeysAndIssuer>               m_Keys;
	std::unique_ptr<KeysAndIssuer>               m_DecayingKeys;
	std::unique_ptr<std::atomic<KeysAndIssuer*>> m_CurrentKeys;

	KString           m_sScope;
	bool              m_bMustSupportScope {true};
	KURL              m_URL;
	KDuration         m_RefreshInterval {};
	/// retry interval used while we have no usable keys yet (e.g. the IdP was
	/// unreachable at startup), so we recover within minutes instead of waiting
	/// for the full refresh interval
	KDuration         m_RetryInterval { chrono::minutes(3) };
	KUnixTime         m_LastRefresh;

}; // KOpenIDProvider

using KOpenIDProviderList = std::vector<KOpenIDProvider>;

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// holds an authentication token and validates it
class DEKAF2_PUBLIC KJWT : public KErrorBase
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	/// default ctor
	KJWT() = default;

	/// construct with a token
	KJWT(KStringView sBase64Token, const KOpenIDProviderList& Providers, KStringView sScope = KStringView{}, KStringView sExpectedAudience = KStringView{}, KStringView sExpectedTokenUse = KStringView{}, KDuration tClockLeeway = chrono::seconds(5))
	{
		Check(sBase64Token, Providers, sScope, sExpectedAudience, sExpectedTokenUse, tClockLeeway);
	}

	/// construct with a token - old version, now deprecated
	KJWT(KStringView sBase64Token, const KOpenIDProviderList& Providers, KStringView sScope, KDuration tClockLeeway)
	: KJWT(sBase64Token, Providers, sScope, KStringView{}, KStringView{}, tClockLeeway)
	{
	}

	/// check a new token
	/// @param sExpectedAudience if non-empty, the token's "aud" claim must contain
	/// it (string or array form, RFC 7519); empty (the default) imposes no audience
	/// constraint, so existing callers are unaffected. See AudienceMatches().
	/// @param sExpectedTokenUse if non-empty, a PRESENT "token_use" claim must equal
	/// it; a token that omits the claim is accepted. This lets an OAuth2 resource
	/// server require "access" and thereby reject an OIDC id_token (token_use=="id")
	/// presented as a bearer access token, without breaking third-party access
	/// tokens that omit token_use. Empty (the default) imposes no constraint.
	bool Check(KStringView sBase64Token, const KOpenIDProviderList& Providers, KStringView sScope = KStringView{}, KStringView sExpectedAudience = KStringView{}, KStringView sExpectedTokenUse = KStringView{}, KDuration tClockLeeway = chrono::seconds(5));

	/// is all info valid?
	bool IsValid() const { return !HasError(); }

	/// get user id ("subject")
	const KString& GetUser() const;

	KJSON Header;
	KJSON Payload;

	/// clear all data
	void clear();

	/// Does a token payload's "aud" (audience) claim satisfy the expected audience?
	/// @param Payload the decoded JWT payload
	/// @param sExpectedAudience the audience the caller requires. An EMPTY value
	/// means "do not constrain the audience" (the default), so callers that pass
	/// no audience - and tokens validated without one - are completely unaffected:
	/// audience binding is opt-in.
	/// Per RFC 7519 "aud" is either a single string or an array of strings; an
	/// array matches if it contains the expected value. Comparison is case-sensitive.
	/// @return true if the audience is acceptable
	DEKAF2_NODISCARD
	static bool AudienceMatches(const KJSON& Payload, KStringView sExpectedAudience);

	/// Does a token payload's "token_use" claim satisfy the expected token use?
	/// @param Payload the decoded JWT payload
	/// @param sExpectedTokenUse the token use the caller requires (e.g. "access" for a
	/// resource server). An EMPTY value means "do not constrain" (the default), so
	/// existing callers are unaffected. A token that OMITS the token_use claim is
	/// accepted (third-party access tokens commonly carry no token_use); only a token
	/// whose token_use is PRESENT and differs is rejected - which blocks presenting an
	/// OIDC id_token (token_use=="id") as a bearer access token. Case-sensitive.
	/// @return true if the token use is acceptable
	DEKAF2_NODISCARD
	static bool TokenUseMatches(const KJSON& Payload, KStringView sExpectedTokenUse);

//----------
private:
//----------

	DEKAF2_PRIVATE
	bool Validate(KStringView sIssuer, KStringView sScope, KStringView sExpectedAudience, KStringView sExpectedTokenUse, KDuration tClockLeeway);
	DEKAF2_PRIVATE
	bool SetError(KStringView sError);
	DEKAF2_PRIVATE
	void ClearJSON();

	bool            m_bSignatureIsValid { false };

}; // KJWT


/// @}

DEKAF2_NAMESPACE_END
