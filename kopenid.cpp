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

#include "kopenid.h"
#include "krestclient.h"
#include "klog.h"
#include "kbase64.h"
#include "krsasign.h"
#include <vector>

DEKAF2_NAMESPACE_BEGIN

static constexpr KStringViewZ OpenID_Configuration = "/.well-known/openid-configuration";
static constexpr int DEFAULT_OPENID_TCP_TIMEOUT = 30;

const KRSAKey KOpenIDKeys::s_EmptyKey;

//-----------------------------------------------------------------------------
KOpenIDKeys::WebKey::WebKey(const KJSON& parms)
//-----------------------------------------------------------------------------
: RSAKey    ( parms                             )
, Algorithm ( kjson::GetStringRef(parms, "alg") )
, Digest    ( kjson::GetStringRef(parms, "x5t") )
, UseType   ( kjson::GetStringRef(parms, "use") )
{
}

//-----------------------------------------------------------------------------
bool KOpenIDKeys::SetError(KString sError) const
//-----------------------------------------------------------------------------
{
	m_sError = std::move(sError);
	kDebug(1, m_sError);
	return false;

} // SetError

//-----------------------------------------------------------------------------
bool KOpenIDKeys::Validate(const KJSON& Keys) const
//-----------------------------------------------------------------------------
{
	if (Keys["keys"].empty())
	{
		return SetError("no keys found");
	}
	return true;

} // Validate

//-----------------------------------------------------------------------------
KOpenIDKeys::KOpenIDKeys (const KURL& URL)
//-----------------------------------------------------------------------------
{
	DEKAF2_TRY
	{
		if (URL.Protocol != url::KProtocol::HTTPS)
		{
			SetError(kFormat("key URL does not use HTTPS: {}", URL.Serialize()));
		}
		else
		{
			KJsonRestClient ProviderKeys(URL, true); // we have to verify the CERT!
			ProviderKeys.SetTimeout(DEFAULT_OPENID_TCP_TIMEOUT);
			auto Keys = ProviderKeys.Get("").Request();

			if (Validate(Keys))
			{
				for (auto& it : Keys["keys"])
				{
					// only add RSA keys to our Key store
					if (it["kty"] == "RSA")
					{
						// construct RSA key and store it by its key ID
#ifdef DEKAF2_WRAPPED_KJSON
						auto p = WebKeys.emplace(std::move(it["kid"].String()), WebKey(it));
#else
						auto p = WebKeys.emplace(std::move(it["kid"].get_ref<KString&>()), WebKey(it));
#endif
						if (p.second)
						{
							kDebug(2, "inserted {} {} key with ID {}",
								   p.first->second.Algorithm,
								   p.first->second.UseType,
								   p.first->first);
						}
					}
				}
			}
		}
	}
	DEKAF2_CATCH (const KHTTPError& exc)
	{
		SetError(kFormat("{}: {}", URL.Serialize(), exc.message()));
	}
	DEKAF2_CATCH (const KJSON::exception& exc)
	{
		SetError(kFormat("OpenID provider keys '{}' returned invalid JSON: {}", URL.Serialize(), exc.what()));
	}

} // ctor

//-----------------------------------------------------------------------------
const KRSAKey& KOpenIDKeys::GetRSAKey(KStringView sKeyID, KStringView sAlgorithm, KStringView sKeyDigest, KStringView sUseType) const
//-----------------------------------------------------------------------------
{
	// search the key ID in our key storage
	auto it = WebKeys.find(sKeyID);

	if (it != WebKeys.end()                &&
		it->second.Algorithm == sAlgorithm &&
		it->second.Digest    == sKeyDigest &&
		it->second.UseType   == sUseType)
	{
		return it->second.RSAKey;
	}

	return s_EmptyKey;

} // GetPublicKey

//-----------------------------------------------------------------------------
bool KOpenIDProvider::SetError(KString sError) const
//-----------------------------------------------------------------------------
{
	m_sError = std::move(sError);
	kDebug(1, m_sError);
	return false;

} // SetError

//-----------------------------------------------------------------------------
bool KOpenIDProvider::Validate(const KJSON& Configuration, const KURL& URL, KStringView sScope) const
//-----------------------------------------------------------------------------
{
	kDebug(3, Configuration.dump(1, '\t'));

	// gcc 8 needs the explicit get()..
#ifdef DEKAF2_WRAPPED_KJSON
	if (Configuration["issuer"].String() != URL.Serialize())
#else
	if (Configuration["issuer"].get_ref<const KString&>() != URL.Serialize())
#endif
	{
		return SetError(kFormat("issuer ({}) does not match URL ({})",
#ifdef DEKAF2_WRAPPED_KJSON
								Configuration["issuer"].String(),
#else
								Configuration["issuer"].get_ref<const KString&>(),
#endif
								URL.Serialize()));
	}
	if (Configuration["authorization_endpoint"].empty())
	{
		return SetError("no authorization endpoint");
	}
	if (Configuration["jwks_uri"].empty())
	{
		return SetError("no JWK URI");
	}
	if (Configuration["response_types_supported"].empty())
	{
		return SetError("no response types");
	}
	if (Configuration["subject_types_supported"].empty())
	{
		return SetError("no subject types");
	}
	if (Configuration["id_token_signing_alg_values_supported"].empty())
	{
		return SetError("no token signing algorithms");
	}
	if (!sScope.empty())
	{
		const auto& Scopes = Configuration["scopes_supported"];
		if (Scopes.empty())
		{
			return SetError("no scopes supported");
		}
		if (!kjson::Contains(Scopes, sScope))
		{
			return SetError(kFormat("scope '{}' not supported", sScope));
		}
	}
	return true;

} // Validate

//-----------------------------------------------------------------------------
void KOpenIDProvider::Refresh(KTimer::Timepoint Now)
//-----------------------------------------------------------------------------
{
	if (Now < m_LastRefresh + m_RefreshInterval)
	{
		return;
	}

	m_LastRefresh = Now;

	if (m_URL.Protocol != url::KProtocol::HTTPS)
	{
		SetError(kFormat("provider URL does not use HTTPS, but has to: {}", m_URL.Serialize()));
		m_RefreshInterval = m_RefreshInterval.zero();
	}
	else
	{
		DEKAF2_TRY
		{
			kDebug(2, "polling OpenID provider {} for keys", m_URL);
			KJsonRestClient Provider(m_URL, true); // we have to verify the CERT!
			Provider.SetTimeout(DEFAULT_OPENID_TCP_TIMEOUT);
			auto Configuration = Provider.Get(OpenID_Configuration).Request();

			// verify accuracy of information
			if (Validate(Configuration, m_URL, m_sScope))
			{
				// only query keys if valid data
				auto Keys = KOpenIDKeys(kjson::GetStringRef(Configuration, "jwks_uri"));

				if (!Keys.empty())
				{
					m_DecayingKeys = std::move(m_Keys);
					auto& sIssuer  = kjson::GetStringRef(Configuration, "issuer");
					m_Keys         = std::make_unique<KeysAndIssuer>(KeysAndIssuer { std::move(Keys), sIssuer } );
					kDebug(2, "got {} valid keys from provider {}", m_Keys->Keys.size(), m_URL);
				}
				else
				{
					kDebug(1, "got no keys when polling provider {}", m_URL);
				}
			}
		}
		DEKAF2_CATCH (const KHTTPError& exc)
		{
			if (m_CurrentKeys && IsValid())
			{
				// there was an issue with the http connection to the SSO provider,
				// but as we have valid keys from a former connection just keep these
				kDebug(1, "cannot refresh SSO keys for {}, will continue to use old keys: {}", m_URL.Serialize(), exc.message());
				return;
			}

			SetError(kFormat("{}: {}", m_URL.Serialize(), exc.message()));
			// continue to create invalid empty keys
		}
		DEKAF2_CATCH (const KJSON::exception& exc)
		{
			// we protect Validate() with this catch
			if (m_CurrentKeys && IsValid())
			{
				// there was an issue with the json response from the SSO provider,
				// but as we have valid keys from a former connection just keep these
				kDebug(1, "cannot refresh SSO keys for {}, will continue to use old keys: {}", m_URL.Serialize(), exc.what());
				return;
			}

			SetError(kFormat("OpenID provider '{}' returned invalid JSON: {}", m_URL.Serialize(), exc.what()));
			// continue to create invalid empty keys
		}
	}

	if (!m_Keys)
	{
		m_Keys = std::make_unique<KeysAndIssuer>();
	}

	if (!m_CurrentKeys)
	{
		m_CurrentKeys = std::make_unique<std::atomic<KeysAndIssuer*>>(m_Keys.get());
	}
	else
	{
		// atomically switch to new keys
		m_CurrentKeys.get()->store(m_Keys.get(), std::memory_order_relaxed);
	}

} // Refresh

//-----------------------------------------------------------------------------
KOpenIDProvider::KOpenIDProvider (KURL URL, KStringView sScope, KTimer::Interval RefreshInterval)
//-----------------------------------------------------------------------------
: m_sScope(sScope)
, m_URL(std::move(URL))
, m_RefreshInterval(RefreshInterval)
{
	m_URL.Query.clear();
	m_URL.Fragment.clear();

	if (m_RefreshInterval < std::chrono::hours(1))
	{
		// we need a refresh interval of at least an hour to make sure all users have finished
		// evaluating against an old set of Keys, as we do lockless access on the Keys and only
		// move them out to a single decaying stage (which means that after one refresh interval,
		// access on them is UB)
		kDebug(1, "refresh interval lower than one hour, changing to hourly, daily is recommended");
		m_RefreshInterval = std::chrono::hours(1);
	}

	Refresh();

} // ctor

//-----------------------------------------------------------------------------
void KJWT::ClearJSON()
//-----------------------------------------------------------------------------
{
	Header = KJSON{};
	Payload = KJSON{};

} // ClearJSON

//-----------------------------------------------------------------------------
bool KJWT::SetError(KString sError)
//-----------------------------------------------------------------------------
{
	if (!sError.empty())
	{
		m_sError = kFormat("{}sub {}: {}", m_bSignatureIsValid ? "" : "bad sig for ", GetUser(), sError);

		ClearJSON();
		kDebug(1, m_sError);
		return false;
	}

	return true;

} // SetError

//-----------------------------------------------------------------------------
bool KJWT::Validate(KStringView sIssuer, KStringView sScope, KDuration tClockLeeway)
//-----------------------------------------------------------------------------
{
	kDebug(3, Payload.dump(1, '\t'));

	// gcc 8 needs the explicit get()..
#ifdef DEKAF2_WRAPPED_KJSON
	if (Payload["iss"].String() != sIssuer)
#else
	if (Payload["iss"].get_ref<const KString&>() != sIssuer)
#endif
	{
		return SetError(kFormat("Payload issuer {} does not match Provider issuer {}",
#ifdef DEKAF2_WRAPPED_KJSON
								Payload["iss"].String(),
#else
								Payload["iss"].get_ref<const KString&>(),
#endif
								sIssuer));
	}

	if (!sScope.empty())
	{
		const auto& Scopes = Payload["scope"];
		if (Scopes.empty())
		{
			return SetError("no scopes supported");
		}

		bool bScopeIsValid = false;
		auto ScopeList     = sScope.Split (",");
		for (auto sScopeName : ScopeList)
		{
			if (kjson::Contains(Scopes, sScopeName))
			{
				bScopeIsValid = true;
				break;
			}
		}

		if (!bScopeIsValid)
		{
			KString sScopes;
			for (const auto& Scope : Scopes)
			{
				if (!sScopes.empty())
				{
					sScopes += ',';
				}
#ifdef DEKAF2_WRAPPED_KJSON
				sScopes += Scope.String();
#else
				sScopes += Scope.get_ref<const KString&>();
#endif
			}
			return SetError(kFormat("scope '{}' not supported, known scopes: '{}'", sScope, sScopes));
		}
	}

	auto now = Dekaf::getInstance().GetCurrentTime();

	auto tNBF = KUnixTime::from_time_t(kjson::GetInt(Payload, "nbf"));

	if (tNBF > (now + tClockLeeway))
	{
		return SetError(kFormat("token will be valid in {} seconds", (tNBF - now).seconds().count()));
	}

	KUnixTime tExp = KUnixTime::from_time_t(kjson::GetInt(Payload, "exp"));

	if (tExp < (now - tClockLeeway))
	{
		return SetError(kFormat("token has expired {} seconds ago", (now - tExp).seconds().count()));
	}

	return true;

} // Validate

//-----------------------------------------------------------------------------
bool KJWT::Check(KStringView sBase64Token, const KOpenIDProviderList& Providers, KStringView sScope, KDuration tClockLeeway)
//-----------------------------------------------------------------------------
{
	m_bSignatureIsValid = false;

	sBase64Token.TrimLeft();
	sBase64Token.remove_prefix("Bearer ") || sBase64Token.remove_prefix("bearer ");

	auto Part = sBase64Token.Split('.');

	if (Part.size() != 3)
	{
		return SetError(kFormat("wrong part count in token string, expected 3 parts, got {}: {}", Part.size(), sBase64Token));
	}

	DEKAF2_TRY
	{
		kjson::Parse(Header, KBase64Url::Decode(Part[0]));

		if  (Header["typ"] != "JWT")
		{
#ifdef DEKAF2_WRAPPED_KJSON
			return SetError(kFormat("not a JWT header: {}", Header["typ"].String()));
#else
			return SetError(kFormat("not a JWT header: {}", Header["typ"].get_ref<const KString&>()));
#endif
		}

		kjson::Parse(Payload, KBase64Url::Decode(Part[1]));

#ifdef DEKAF2_WRAPPED_KJSON
		const KString& sAlgorithm = Header["alg"].String();
		const KString& sKeyID     = Header["kid"].String();
		const KString& sKeyDigest = Header["x5t"].String();
#else
		const KString& sAlgorithm = Header["alg"].get_ref<const KString&>();
		const KString& sKeyID     = Header["kid"].get_ref<const KString&>();
		const KString& sKeyDigest = Header["x5t"].get_ref<const KString&>();
#endif
		for (auto& Provider : Providers)
		{
			if (!Provider.IsValid())
			{
				SetError(kFormat("invalid provider: {}", Provider.Error()));
				// try the next provider ..
				break;
			}

			// get access on the atomic storage
			auto& KeysAndIssuer = Provider.Get();

			// find the key
			const auto& Key = KeysAndIssuer.Keys.GetRSAKey(sKeyID, sAlgorithm, sKeyDigest, "sig");

			if (Key.empty())
			{
				SetError("no matching key");
				// try the next provider ..
				break;
			}

			KRSAVerify::ALGORITHM Algorithm = KRSAVerify::NONE;

			if (sAlgorithm == "RS256")
			{
				Algorithm = KRSAVerify::SHA256;
			}
			else if (sAlgorithm == "RS384")
			{
				Algorithm = KRSAVerify::SHA384;
			}
			else if (sAlgorithm == "RS512")
			{
				Algorithm = KRSAVerify::SHA512;
			}
			else
			{
				// exit here if we do not support the key's algorithm
				return SetError(kFormat("signature algorithm not supported: {}", sAlgorithm));
			}

			KRSAVerify Verifier(Algorithm);

			Verifier.Update(Part[0]);
			Verifier.Update(".");
			Verifier.Update(Part[1]);

			if (!Verifier.Verify(Key, KBase64Url::Decode(Part[2])))
			{
				// exit here if the key is not matching the signature
				return SetError("bad signature");
			}

			// mark that the token itself is from the right issuer (so that we could
			// e.g. read the sub field or any other)
			m_bSignatureIsValid = true;

			// clear error
			SetError("");

			// exit here if we cannot validate
			return Validate(KeysAndIssuer.sIssuer, sScope, tClockLeeway);
		}
	}
	DEKAF2_CATCH (const KJSON::exception& exc)
	{
		SetError(kFormat("invalid JSON: {}", exc.what()));
 	}

	return false;

} // Check

//-----------------------------------------------------------------------------
const KString& KJWT::GetUser() const
//-----------------------------------------------------------------------------
{
	const auto& sMaybe = kjson::GetStringRef(Payload, "sub");

	if (!m_bSignatureIsValid)
	{
		static KString s_sEmpty;

		if (sMaybe.size() < 40)
		{
			for (auto ch : sMaybe)
			{
				if (!KASCII::kIsAlNum(ch))
				{
					if (ch != '@' && ch != '.' && ch != '_' && ch != '-')
					{
						return s_sEmpty;
					}
				}
			}
		}
	}

	return sMaybe;

} // GetUser

//-----------------------------------------------------------------------------
void KJWT::clear()
//-----------------------------------------------------------------------------
{
	*this = KJWT();

} // clear


#ifndef DEKAF2_IS_WINDOWS
static_assert(std::is_nothrow_move_constructible<KOpenIDKeys>::value,
			  "KOpenIDKeys is intended to be nothrow move constructible, but is not!");

// gcc 8.3.1 does not support atomics ptrs noexcept move construction, so we drop this
// test in case..
static_assert(!std::is_nothrow_move_constructible<std::atomic<void*>>::value ||
			  std::is_nothrow_move_constructible<KOpenIDProvider>::value,
			  "KOpenIDProvider is intended to be nothrow move constructible, but is not!");
#endif

static_assert(std::is_nothrow_move_constructible<KJWT>::value,
			  "KJWT is intended to be nothrow move constructible, but is not!");

DEKAF2_NAMESPACE_END
