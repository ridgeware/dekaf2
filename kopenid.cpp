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
#include "khttpclient.h"
#include "klog.h"
#include "kbase64.h"
#include "krsasign.h"
#include "dekaf2.h"
#include <vector>

namespace dekaf2 {

static constexpr KStringViewZ OpenID_Configuration = "/.well-known/openid-configuration";

//-----------------------------------------------------------------------------
bool KOpenIDKeys::SetError(KString sError) const
//-----------------------------------------------------------------------------
{
	m_sError = std::move(sError);
	kDebug(1, m_sError);
	return false;

} // SetError

//-----------------------------------------------------------------------------
bool KOpenIDKeys::Validate() const
//-----------------------------------------------------------------------------
{
	if (Keys["keys"].empty())
	{
		return SetError("no keys found");
	}
	return true;

} // Validate

//-----------------------------------------------------------------------------
KOpenIDKeys::KOpenIDKeys (KURL URL)
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
			KHTTPClient ProviderKeys;
			kjson::Parse(Keys, ProviderKeys.Get(URL, true /* = bVerifyCerts */ )); // we have to verify the CERT!
			if (!Validate())
			{
				Keys = KJSON{};
			}
		}
	}
	DEKAF2_CATCH (const KJSON::exception& exc)
	{
		SetError(kFormat("OpenID provider keys '{}' returned invalid JSON: {}", URL.Serialize(), exc.what()));
		Keys = KJSON{};
	}

} // ctor

//-----------------------------------------------------------------------------
KRSAKey KOpenIDKeys::GetKey(KStringView sAlgorithm, KStringView sKeyID, KStringView sKeyDigest) const
//-----------------------------------------------------------------------------
{
	KRSAKey Key;

	DEKAF2_TRY
	{
		if (IsValid())
		{
			for (auto& it : Keys["keys"])
			{
				if (it["alg"] == sAlgorithm
					&& it["kid"] == sKeyID
					&& it["x5t"] == sKeyDigest)
				{
					if (it["kty"] == "RSA")
					{
						Key.Create(it);
						// TODO add cache for the created key
					}
					break;
				}
			}
		}
	}
	DEKAF2_CATCH (const KJSON::exception& exc)
	{
		SetError(kFormat("invalid JSON: {}", exc.what()));
	}

	return Key;

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
bool KOpenIDProvider::Validate(const KURL& URL) const
//-----------------------------------------------------------------------------
{
	if (Configuration["issuer"] != URL.Serialize())
	{
		return SetError(kFormat("issuer ({}) does not match URL ({})",
								Configuration["issuer"].get_ref<const KString&>(),
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
	return true;

} // Validate

//-----------------------------------------------------------------------------
KOpenIDProvider::KOpenIDProvider (KURL URL)
//-----------------------------------------------------------------------------
{
	if (URL.Protocol != url::KProtocol::HTTPS)
	{
		SetError(kFormat("provider URL does not use HTTPS: {}", URL.Serialize()));
	}
	else
	{
		URL.Path = OpenID_Configuration;
		URL.Query.clear();
		URL.Fragment.clear();

		DEKAF2_TRY
		{
			KHTTPClient Provider;
			kjson::Parse(Configuration, Provider.Get(URL, true /* = bVerifyCerts */ )); // we have to verify the CERT!
			// verify accuracy of information
			URL.Path.clear();
			if (Validate(URL))
			{
				// only query keys if valid data
				Keys = KOpenIDKeys(Configuration["jwks_uri"].get_ref<const KString&>());
			}
			else
			{
				Configuration = KJSON{};
			}
		}
		DEKAF2_CATCH (const KJSON::exception& exc)
		{
			SetError(kFormat("OpenID provider '{}' returned invalid JSON: {}", URL.Serialize(), exc.what()));
			Configuration = KJSON{};
		}
	}

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
	m_sError = std::move(sError);
	if (!m_sError.empty())
	{
		ClearJSON();
		kDebug(1, m_sError);
		return false;
	}
	else
	{
		return true;
	}

} // SetError

//-----------------------------------------------------------------------------
bool KJWT::Validate(const KOpenIDProvider& Provider, time_t tClockLeeway)
//-----------------------------------------------------------------------------
{
	if (Payload["iss"] != Provider.Configuration["issuer"])
	{
		return SetError(kFormat("Payload issuer {} does not match Provider issuer {}",
								Payload["iss"].get_ref<const KString&>(),
								Provider.Configuration["issuer"].get_ref<const KString&>()));
	}

	time_t now = Dekaf().GetCurrentTime();

	if (Payload["nbf"] > (now + tClockLeeway))
	{
		return SetError("token not yet valid");
	}

	if (Payload["exp"] < (now - tClockLeeway))
	{
		return SetError("token has expired");
	}

	return true;

} // Validate

//-----------------------------------------------------------------------------
bool KJWT::Check(KStringView sBase64Token, const KOpenIDProviderList& Providers, time_t tClockLeeway)
//-----------------------------------------------------------------------------
{
	sBase64Token.TrimLeft();
	sBase64Token.remove_prefix("Bearer ") || sBase64Token.remove_prefix("bearer ");

	std::vector<KStringView> Part;

	if (kSplit(Part, sBase64Token, ".") != 3)
	{
		return SetError(kFormat("wrong part count in token string, expected 3 parts, got {}", Part.size()));
	}

	DEKAF2_TRY
	{
		kjson::Parse(Header, KBase64Url::Decode(Part[0]));

		if  (Header["typ"] != "JWT")
		{
			return SetError(kFormat("not a JWT header: {}", Header["typ"].get_ref<const KString&>()));
		}

		kjson::Parse(Payload, KBase64Url::Decode(Part[1]));

		const KString& sAlgorithm = Header["alg"];
		const KString& sKeyID     = Header["kid"];
		const KString& sKeyDigest = Header["x5t"];

		for (auto& Provider : Providers)
		{
			if (!Provider.IsValid())
			{
				SetError(kFormat("invalid provider: {}", Provider.Error()));
				// try the next provider ..
				break;
			}

			// find the key
			auto Key = Provider.Keys.GetKey(sAlgorithm, sKeyID, sKeyDigest);

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

			// clear error
			SetError("");

			// exit here if we cannot validate
			return Validate(Provider, tClockLeeway);
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
	 return Payload["sub"].get_ref<const KString&>();

 } // GetUser

} // end of namespace dekaf2


