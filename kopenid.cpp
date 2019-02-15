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
#include <vector>

namespace dekaf2 {

static constexpr KStringViewZ OpenID_Configuration = ".well-known/openid-configuration";

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
		KHTTPClient ProviderKeys;
		kjson::Parse(Keys, ProviderKeys.Get(URL, true /* = bVerifyCerts */ )); // we have to verify the CERT!
		if (!Validate())
		{
			Keys = KJSON{};
		}
	}
	DEKAF2_CATCH (const KJSON::exception& exc)
	{
		SetError(kFormat("OpenID provider keys '{}' returned invalid JSON: {}", URL.Serialize(), exc.what()));
		Keys = KJSON{};
	}

} // ctor

//-----------------------------------------------------------------------------
KString KOpenIDKeys::GetPublicKey(KStringView sAlgorithm, KStringView sKID, KStringView sKeyDigest) const
//-----------------------------------------------------------------------------
{
	KString sKey;

	DEKAF2_TRY
	{
		if (IsValid())
		{
			for (auto& it : Keys["keys"])
			{
				if (it["alg"].get<KStringView>() == sAlgorithm
					&& it["kid"].get<KStringView>() == sKID
					&& it["x5t"].get<KStringView>() == sKeyDigest)
				{
					sKey = it["x5c"];
					break;
				}
			}
		}
	}
	DEKAF2_CATCH (const KJSON::exception& exc)
	{
		SetError(kFormat("invalid JSON: {}", exc.what()));
	}

	return sKey;

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
	if (Configuration["issuer"].get<KString>() != URL.Serialize())
	{
		return SetError(kFormat("issuer ({}) does not match URL ({})",
								Configuration["issuer"].get<KString>(),
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
			if (Validate(URL))
			{
				// only query keys if valid data
				Keys = KOpenIDKeys(KStringView(Configuration["jwks_uri"]));
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
	ClearJSON();
	m_sError = std::move(sError);
	kDebug(1, m_sError);
	return false;

} // SetError

//-----------------------------------------------------------------------------
bool KJWT::Validate(const KOpenIDProvider& Provider) const
//-----------------------------------------------------------------------------
{

	return true;

} // Validate

//-----------------------------------------------------------------------------
KJWT::KJWT(KStringView sBase64Token, const KOpenIDProvider& Provider)
//-----------------------------------------------------------------------------
{
	if (!Provider.IsValid())
	{
		SetError(kFormat("invalid provider: {}", Provider.Error()));
		return;
	}

	std::vector<KStringView> Part;

	if (kSplit(Part, sBase64Token, ".") != 3)
	{
		SetError(kFormat("wrong part count in token string, expected 3 parts, got {}", Part.size()));
		return;
	}

	DEKAF2_TRY
	{
		kjson::Parse(Header, KBase64Url::Decode(Part[0]));
		auto sSignature = KBase64Url::Decode(Part[2]);

		if  (Header["typ"].get<KStringView>() != "JWT")
		{
			SetError("not a JWT header");
			return;
		}
		KString sAlgorithm = Header["alg"];
		KString sKID       = Header["kid"];
		KString sKeyDigest = Header["x5t"];

		// find the public key
		auto sPublicKey = Provider.Keys.GetPublicKey(sAlgorithm, sKID, sKeyDigest);

		if (sPublicKey.empty())
		{
			SetError("no matching public key");
			return;
		}

		if (sAlgorithm == "RS256")
		{
			KRSAVerify_SHA256 RSAVerify(sPublicKey);
			RSAVerify.Update(Part[0]);
			RSAVerify.Update(".");
			RSAVerify.Update(Part[1]);
			if (!RSAVerify.Verify(Part[2]))
			{
				SetError("bad signature");
				return;
			}
		}
		else if (sAlgorithm == "RS384")
		{

		}
		else if (sAlgorithm == "RS512")
		{
		}
		else
		{
			SetError(kFormat("signature algorithm not supported: {}", sAlgorithm));
			return;
		}

		kjson::Parse(Payload, KBase64Url::Decode(Part[1]));

		if (!Validate(Provider))
		{
			return;
		}
	}
	DEKAF2_CATCH (const KJSON::exception& exc)
	{
		SetError(kFormat("invalid JSON: {}", exc.what()));
 	}

} // ctor

} // end of namespace dekaf2


