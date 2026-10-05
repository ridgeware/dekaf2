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
#include <mutex>
#include <ctime>

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
	/// Does not change this object, so several threads may verify with one key set.
	/// @param sKeyID     the "kid" from the JWT header
	/// @param sAlgorithm the "alg" from the JWT header
	/// @param sKeyDigest the "x5t" from the JWT header (may be empty)
	/// @param sData      the signed data (header.payload, base64url-encoded)
	/// @param sSignature the raw decoded signature bytes
	/// @param sUseType   the expected key use (default "sig")
	/// @param psError    if not null, receives the reason of a failure
	/// @return true if the signature is valid
	bool VerifySignature(KStringView sKeyID, KStringView sAlgorithm, KStringView sKeyDigest,
	                     KStringView sData,  KStringView sSignature, KStringView sUseType = "sig",
	                     KString* psError = nullptr) const;

	/// are all info valid?
	bool IsValid() const { return !HasError(); }

	bool empty() const { return WebKeys.empty(); }
	/// is there a key with this key ID?
	bool contains(KStringView sKeyID) const { return WebKeys.find(sKeyID) != WebKeys.end(); }
	std::size_t size() const { return WebKeys.size(); }

	/// the same keys as another set, as the provider published them?
	bool operator==(const KOpenIDKeys& other) const { return m_sPublished == other.m_sPublished; }
	bool operator!=(const KOpenIDKeys& other) const { return !operator==(other); }

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
	/// the keys as the provider published them, for the comparison
	KString                             m_sPublished;

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
	/// a provider with a fixed set of keys, e.g. from a local source or for tests -
	/// Refresh() leaves it unchanged
	/// @param Keys the keys that sign the tokens of the issuer
	/// @param sIssuer the issuer as the tokens name it in their "iss" claim
	KOpenIDProvider (KOpenIDKeys Keys, KString sIssuer);

	/// are all info valid? This is the state after the last Refresh(), which is not
	/// synchronized with a Refresh() on another thread - KJWT::Check() therefore only
	/// looks at the keys that Get() returns
	bool IsValid() const { return !HasError(); }

	struct KeysAndIssuer
	{
		KOpenIDKeys Keys;
		KString     sIssuer;
	};

	/// the current keys and issuer - safe to call while another thread runs Refresh().
	/// After a key change the previous set stays valid until the next change, at least
	/// an hour later, so use the reference right away and do not keep it. Without
	/// keys, e.g. for a default constructed provider, the set is empty
	const KeysAndIssuer& Get() const
	{
		// acquire pairs with the release in Refresh(): a thread that sees the new
		// pointer also sees the keys it points to
		return m_CurrentKeys ? *m_CurrentKeys->load(std::memory_order_acquire) : s_EmptyKeys;
	}

	/// load the keys again once the refresh interval has passed - call it regularly,
	/// e.g. from a timer. Safe to call from any thread, the readers of Get() need no lock
	void Refresh(KUnixTime Now = KUnixTime::now());

	/// load the keys right now, for a token of this issuer with a key ID that is not
	/// among them - the issuer may have changed its key. Only once an hour for such
	/// tokens, only if the keys in memory are at least an hour old, and not while
	/// another thread asks the provider. Blocks for the duration of the request.
	/// KJWT::Check() calls it and verifies the token again. Safe to call from any thread
	/// @return true if it asked the provider
	bool RefreshForUnknownKey(KUnixTime Now = KUnixTime::now()) const;

	/// load the keys right now if there are none yet, e.g. because the provider was
	/// unreachable at the start. No more often than the retry interval of three minutes,
	/// counting the requests of Refresh() as well, and not while another thread asks the
	/// provider. Blocks for the duration of the request. KJWT::Check() calls it for a
	/// token. Safe to call from any thread
	/// @return true if it asked the provider
	bool RefreshForMissingKeys(KUnixTime Now = KUnixTime::now()) const;

//----------
private:
//----------

	DEKAF2_PRIVATE
	bool Validate(const KJSON& Configuration, const KURL& URL, KStringView sScope) const;

	/// ask the provider for its keys and publish new ones - with m_Request->Mutex held
	DEKAF2_PRIVATE
	void Load(KUnixTime Now) const;
	/// Load() on behalf of a token, if it is not too early
	DEKAF2_PRIVATE
	bool LoadOnDemand(KUnixTime Now, bool bForUnknownKey) const;

	static const KeysAndIssuer s_EmptyKeys;

	/// the lock for the requests to the provider, and when what happened - on the heap
	/// like m_CurrentKeys, which keeps the class movable. The times are seconds since
	/// the epoch, written with the lock held, read also without it
	struct RequestState
	{
		std::mutex               Mutex;
		/// the last request to the provider
		std::atomic<std::time_t> tLastRequest    { 0 };
		/// the last request for a token with an unknown key ID
		std::atomic<std::time_t> tLastUnknownKey { 0 };
		/// the last change of the keys - the previous ones stay in memory until the
		/// next change, which therefore comes an hour later at the earliest
		std::atomic<std::time_t> tLastChange     { 0 };
	};

	// the key cache - Load() changes it from the const RefreshForUnknownKey() as well,
	// with m_Request->Mutex held, while the readers of Get() need no lock
	mutable std::unique_ptr<KeysAndIssuer>       m_Keys;
	mutable std::unique_ptr<KeysAndIssuer>       m_DecayingKeys;
	std::unique_ptr<std::atomic<KeysAndIssuer*>> m_CurrentKeys;
	std::unique_ptr<RequestState>                m_Request;

	KString           m_sScope;
	bool              m_bMustSupportScope {true};
	KURL              m_URL;
	KDuration         m_RefreshInterval {};
	/// retry interval used while we have no usable keys yet (e.g. the IdP was
	/// unreachable at startup), so we recover within minutes instead of waiting
	/// for the full refresh interval
	KDuration         m_RetryInterval { chrono::minutes(3) };

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
	/// A token whose issuer matches a provider that does not know its key ID asks that
	/// provider for new keys and is verified again, see KOpenIDProvider::RefreshForUnknownKey().
	/// A provider without keys asks for them, see KOpenIDProvider::RefreshForMissingKeys().
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
