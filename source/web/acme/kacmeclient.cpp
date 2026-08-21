/*
 //
 // DEKAF(tm): Lighter, Faster, Smarter(tm)
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
 // |/|   WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR           |\|
 // |\|   PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS        |\|
 // |/|   OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR          |\|
 // |\|   OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR        |\|
 // |/|   OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE         |/|
 // |\|   SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.            |\|
 // |/|                                                                     |/|
 // |/+---------------------------------------------------------------------+/|
 // |\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/ |
 // +-------------------------------------------------------------------------+
 //
 */

#include <dekaf2/web/acme/kacmeclient.h>
#include <dekaf2/core/format/kformat.h>
#include <dekaf2/core/logging/klog.h>
#include <dekaf2/core/types/kscopeguard.h>
#include <dekaf2/core/types/bits/kunique_deleter.h>
#include <dekaf2/crypto/ec/kecsign.h>
#include <dekaf2/crypto/encoding/kbase64.h>
#include <dekaf2/crypto/hash/kmessagedigest.h>
#include <dekaf2/crypto/rsa/kcsr.h>
#include <dekaf2/crypto/rsa/krsacert.h>
#include <dekaf2/crypto/rsa/krsasign.h>
#include <dekaf2/system/os/ksystem.h>
#include <openssl/evp.h>
#include <openssl/opensslv.h>
#include <openssl/pem.h>

#if OPENSSL_VERSION_NUMBER >= 0x10101000L && !defined(LIBRESSL_VERSION_NUMBER)
	#define DEKAF2_HAS_TLS_ALPN_DISPATCH 1
#else
	#define DEKAF2_HAS_TLS_ALPN_DISPATCH 0
#endif

DEKAF2_NAMESPACE_BEGIN

namespace {

constexpr KStringViewZ AcmeTlsAlpn = "acme-tls/1";
constexpr KStringViewZ AcmeIdOID   = "1.3.6.1.5.5.7.1.31"; // id-pe-acmeIdentifier
constexpr KStringViewZ BadNonce    = "urn:ietf:params:acme:error:badNonce";
constexpr KStringViewZ ReplayNonce = "Replay-Nonce";

//-----------------------------------------------------------------------------
bool IsECPEM(KStringView sPEM)
//-----------------------------------------------------------------------------
{
	KUniquePtr<BIO, ::BIO_free_all> bio(::BIO_new_mem_buf(sPEM.data(), static_cast<int>(sPEM.size())));
	KUniquePtr<EVP_PKEY, ::EVP_PKEY_free> key(::PEM_read_bio_PrivateKey(bio.get(), nullptr, nullptr, nullptr));

	return key && ::EVP_PKEY_base_id(key.get()) == EVP_PKEY_EC;

} // IsECPEM

//-----------------------------------------------------------------------------
// set the algorithm in the protected header and return the JWS signing input
KString BuildSigningInput(KJSON& jProtected, KStringView sAlg, KStringView sPayload)
//-----------------------------------------------------------------------------
{
	jProtected["alg"] = sAlg;

	return kFormat("{}.{}", KBase64Url::Encode(jProtected.dump()), KBase64Url::Encode(sPayload));

} // BuildSigningInput

//-----------------------------------------------------------------------------
// assemble the flattened JSON serialization from signing input and signature
KString FinishJWS(KStringView sSigningInput, KStringView sSignature)
//-----------------------------------------------------------------------------
{
	if (sSignature.empty())
	{
		return {};
	}

	auto iDot = sSigningInput.find('.');

	return KJSON {
		{ "protected", sSigningInput.substr(0, iDot)  },
		{ "payload"  , sSigningInput.substr(iDot + 1) },
		{ "signature", KBase64Url::Encode(sSignature) }
	}.dump();

} // FinishJWS

} // end of anonymous namespace

//-----------------------------------------------------------------------------
KAcmeClient::KAcmeClient()
//-----------------------------------------------------------------------------
: KAcmeClient(Options{})
{
}

//-----------------------------------------------------------------------------
KAcmeClient::KAcmeClient(Options Options)
//-----------------------------------------------------------------------------
: m_Options(std::move(Options))
, m_Challenges(std::make_shared<ChallengeMap>())
, m_HTTPChallenges(std::make_shared<HTTPChallengeMap>())
{
	m_HTTP.SetVerifyCerts(m_Options.bVerifyTLS);
}

//-----------------------------------------------------------------------------
KString KAcmeClient::JWKThumbprint(const KRSAKey& Key)
//-----------------------------------------------------------------------------
{
	auto jJWK = Key.GetPublicJWK("", "RS256");

	if (jJWK.empty())
	{
		return {};
	}

	// RFC 7638: SHA-256 over the canonical JWK - only the key parameters, keys in
	// lexicographic order, no whitespace
	auto sCanonical = kFormat(R"({{"e":"{}","kty":"RSA","n":"{}"}})",
	                          jJWK["e"].String(), jJWK["n"].String());

	return KBase64Url::Encode(KSHA256(sCanonical).Digest());

} // JWKThumbprint

//-----------------------------------------------------------------------------
KString KAcmeClient::JWKThumbprint(const KECKey& Key)
//-----------------------------------------------------------------------------
{
	auto jJWK = Key.GetPublicJWK("", "ES256");

	if (jJWK.empty())
	{
		return {};
	}

	auto sCanonical = kFormat(R"({{"crv":"P-256","kty":"EC","x":"{}","y":"{}"}})",
	                          jJWK["x"].String(), jJWK["y"].String());

	return KBase64Url::Encode(KSHA256(sCanonical).Digest());

} // JWKThumbprint

//-----------------------------------------------------------------------------
KString KAcmeClient::SignJWS(const KRSAKey& Key, KJSON jProtected, KStringView sPayload)
//-----------------------------------------------------------------------------
{
	auto sInput = BuildSigningInput(jProtected, "RS256", sPayload);

	KRSASign Signer(KRSASign::Digest::SHA256);
	Signer.Update(sInput);

	auto sSignature = Signer.Sign(Key);

	if (sSignature.empty())
	{
		kDebug(1, "cannot sign JWS: {}", Signer.Error());
	}

	return FinishJWS(sInput, sSignature);

} // SignJWS

//-----------------------------------------------------------------------------
KString KAcmeClient::SignJWS(const KECKey& Key, KJSON jProtected, KStringView sPayload)
//-----------------------------------------------------------------------------
{
	auto sInput = BuildSigningInput(jProtected, "ES256", sPayload);

	KECSign Signer;

	auto sSignature = Signer.Sign(Key, sInput);

	if (sSignature.empty())
	{
		kDebug(1, "cannot sign JWS: {}", Signer.Error());
	}

	return FinishJWS(sInput, sSignature);

} // SignJWS

//-----------------------------------------------------------------------------
KJSON KAcmeClient::AccountJWK() const
//-----------------------------------------------------------------------------
{
	// the account JWK in a JWS header carries only the key parameters
	if (m_bAccountIsEC)
	{
		auto jFull = m_AccountKeyEC.GetPublicJWK("", "ES256");

		if (jFull.empty())
		{
			return KJSON{};
		}

		return KJSON {
			{ "crv", jFull["crv"].String() },
			{ "kty", jFull["kty"].String() },
			{ "x"  , jFull["x"  ].String() },
			{ "y"  , jFull["y"  ].String() }
		};
	}

	auto jFull = m_AccountKey.GetPublicJWK("", "RS256");

	if (jFull.empty())
	{
		return KJSON{};
	}

	return KJSON {
		{ "e"  , jFull["e"  ].String() },
		{ "kty", jFull["kty"].String() },
		{ "n"  , jFull["n"  ].String() }
	};

} // AccountJWK

//-----------------------------------------------------------------------------
KString KAcmeClient::AccountThumbprint() const
//-----------------------------------------------------------------------------
{
	return m_bAccountIsEC ? JWKThumbprint(m_AccountKeyEC) : JWKThumbprint(m_AccountKey);

} // AccountThumbprint

//-----------------------------------------------------------------------------
KString KAcmeClient::AccountSign(KJSON jProtected, KStringView sPayload) const
//-----------------------------------------------------------------------------
{
	return m_bAccountIsEC ? SignJWS(m_AccountKeyEC, std::move(jProtected), sPayload)
	                      : SignJWS(m_AccountKey  , std::move(jProtected), sPayload);

} // AccountSign

//-----------------------------------------------------------------------------
KString KAcmeClient::AccountPEM()
//-----------------------------------------------------------------------------
{
	return m_bAccountIsEC ? m_AccountKeyEC.GetPEM(true) : m_AccountKey.GetPEM(true);

} // AccountPEM

//-----------------------------------------------------------------------------
bool KAcmeClient::LoadOrCreateAccountKey()
//-----------------------------------------------------------------------------
{
	if (!m_AccountKey.empty() || !m_AccountKeyEC.empty())
	{
		return true;
	}

	if (!m_Options.sAccountKeyPEM.empty())
	{
		// a stored key keeps its type, whatever Options.Keys says
		if (IsECPEM(m_Options.sAccountKeyPEM))
		{
			m_bAccountIsEC = true;

			if (!m_AccountKeyEC.CreateFromPEM(m_Options.sAccountKeyPEM))
			{
				return SetError(kFormat("cannot load account key: {}", m_AccountKeyEC.Error()));
			}
		}
		else if (!m_AccountKey.Create(KStringView(m_Options.sAccountKeyPEM)))
		{
			return SetError(kFormat("cannot load account key: {}", m_AccountKey.Error()));
		}
	}
	else if (m_Options.Keys == KeyType::EC)
	{
		m_bAccountIsEC = true;
		m_AccountKeyEC = KECKey(true);

		if (m_AccountKeyEC.empty())
		{
			return SetError(kFormat("cannot create account key: {}", m_AccountKeyEC.Error()));
		}
	}
	else if (!m_AccountKey.Create(m_Options.iKeyLength))
	{
		return SetError(kFormat("cannot create account key: {}", m_AccountKey.Error()));
	}

	return true;

} // LoadOrCreateAccountKey

//-----------------------------------------------------------------------------
std::shared_ptr<KTLSContext> KAcmeClient::CreateChallengeContext(KStringView sDomain, KStringView sKeyAuthorization, uint16_t iKeyLength)
//-----------------------------------------------------------------------------
{
	// RFC 8737: a self-signed cert with the domain as its single SAN, and a critical
	// acmeIdentifier extension carrying SHA-256 of the key authorization
	KRSAKey Key(iKeyLength);

	if (Key.empty())
	{
		kDebug(1, "cannot create challenge key: {}", Key.Error());
		return nullptr;
	}

	KRSACert Cert(Key, sDomain, "", "", "", chrono::days(7));

	KString sAcmeId("\x04\x20", 2); // a DER octet string of 32 bytes
	sAcmeId += KSHA256(sKeyAuthorization).Digest();

	if (Cert.HasError()
		|| !Cert.AddExtension(AcmeIdOID, sAcmeId, true)
		|| !Cert.Sign(Key))
	{
		kDebug(1, "cannot create challenge cert: {}", Cert.Error());
		return nullptr;
	}

	auto Context = std::make_shared<KTLSContext>(true);

	if (!Context->SetTLSCertificates(Cert.GetPEM(), Key.GetPEM(true))
		|| !Context->SetALPN(AcmeTlsAlpn))
	{
		kDebug(1, "cannot create challenge context: {}", Context->Error());
		return nullptr;
	}

	return Context;

} // CreateChallengeContext

//-----------------------------------------------------------------------------
KTLSContext::SNICallback KAcmeClient::GetChallengeResolver() const
//-----------------------------------------------------------------------------
{
	// the lambda shares ownership of the challenge map, so it stays valid
	// after this client is destroyed
	auto Challenges = m_Challenges;

	return [Challenges](const KTLSContext::ClientHello& Hello) -> std::shared_ptr<KTLSContext>
	{
		if (!Hello.HasALPN(AcmeTlsAlpn) || Hello.sServerName.empty())
		{
			return nullptr;
		}

		auto sName = Hello.sServerName.ToLowerASCII();
		auto Map   = Challenges->shared();
		auto it    = Map->find(sName);

		return (it != Map->end()) ? it->second : nullptr;
	};

} // GetChallengeResolver

//-----------------------------------------------------------------------------
std::function<KString(KStringView)> KAcmeClient::GetHTTPChallengeResolver() const
//-----------------------------------------------------------------------------
{
	// the lambda shares ownership of the challenge map, so it stays valid
	// after this client is destroyed
	auto Challenges = m_HTTPChallenges;

	return [Challenges](KStringView sToken) -> KString
	{
		auto Map = Challenges->shared();
		auto it  = Map->find(KString(sToken));

		return (it != Map->end()) ? it->second : KString{};
	};

} // GetHTTPChallengeResolver

//-----------------------------------------------------------------------------
bool KAcmeClient::AttachTo(KTLSContext& Context) const
//-----------------------------------------------------------------------------
{
#if !DEKAF2_HAS_TLS_ALPN_DISPATCH
	// without ALPN visibility in the SNI dispatch the resolver can never match
	return SetError("tls-alpn-01 needs at least OpenSSL 1.1.1 on the server side");
#else
	return Context.SetSNICallback(GetChallengeResolver());
#endif

} // AttachTo

//-----------------------------------------------------------------------------
KString KAcmeClient::CreateARICertID(KStringView sCertPEM)
//-----------------------------------------------------------------------------
{
	KRSACert Cert(sCertPEM);

	auto sAKI    = Cert.GetAuthorityKeyIdentifier();
	auto sSerial = Cert.GetSerialBytes();

	if (sAKI.empty() || sSerial.empty())
	{
		return {};
	}

	return kFormat("{}.{}", KBase64Url::Encode(sAKI), KBase64Url::Encode(sSerial));

} // CreateARICertID

//-----------------------------------------------------------------------------
KAcmeClient::RenewalInfo KAcmeClient::GetRenewalInfo(KStringView sARICertID)
//-----------------------------------------------------------------------------
{
	RenewalInfo Info;

	if (sARICertID.empty())
	{
		return Info;
	}

	if (!FetchDirectory())
	{
		ClearError(); // ARI is advisory - callers fall back to their local policy
		return Info;
	}

	auto sURL = m_jDirectory["renewalInfo"].String();

	if (sURL.empty())
	{
		kDebug(2, "CA does not support ARI");
		return Info;
	}

	// RFC 9773: an unauthenticated GET
	auto sResponse = m_HTTP.Get(KURL(kFormat("{}/{}", sURL, sARICertID)));

	if (m_HTTP.GetStatusCode() != 200)
	{
		kDebug(2, "no renewal info for {}: HTTP {}", sARICertID, m_HTTP.GetStatusCode());
		return Info;
	}

	KJSON jInfo;
	KString sError;

	if (!kjson::Parse(jInfo, sResponse, sError))
	{
		kDebug(1, "invalid renewal info: {}", sError);
		return Info;
	}

	Info.WindowStart = kParseTimestamp(jInfo["suggestedWindow"]["start"].String());
	Info.WindowEnd   = kParseTimestamp(jInfo["suggestedWindow"]["end"  ].String());

	return Info;

} // GetRenewalInfo

//-----------------------------------------------------------------------------
bool KAcmeClient::FetchDirectory()
//-----------------------------------------------------------------------------
{
	if (!m_jDirectory.empty())
	{
		return true;
	}

	auto sResponse = m_HTTP.Get(KURL(m_Options.sDirectoryURL));

	if (m_HTTP.GetStatusCode() != 200)
	{
		return SetError(kFormat("cannot fetch ACME directory {}: HTTP {}", m_Options.sDirectoryURL, m_HTTP.GetStatusCode()));
	}

	KString sError;

	if (!kjson::Parse(m_jDirectory, sResponse, sError))
	{
		return SetError(kFormat("invalid ACME directory: {}", sError));
	}

	return true;

} // FetchDirectory

//-----------------------------------------------------------------------------
bool KAcmeClient::FetchNonce()
//-----------------------------------------------------------------------------
{
	m_HTTP.HttpRequest(KURL(m_jDirectory["newNonce"].String()), KHTTPMethod::HEAD);

	m_sNonce = m_HTTP.Response.Headers.Get(ReplayNonce);

	if (m_sNonce.empty())
	{
		return SetError("cannot fetch nonce");
	}

	return true;

} // FetchNonce

//-----------------------------------------------------------------------------
KString KAcmeClient::SignedRequest(const KURL& URL, KStringView sPayload, bool bUseKid, KStringView sAccept)
//-----------------------------------------------------------------------------
{
	for (int iAttempt = 0; iAttempt < 3; ++iAttempt)
	{
		if (m_sNonce.empty() && !FetchNonce())
		{
			return {};
		}

		KJSON jProtected {
			{ "nonce", m_sNonce        },
			{ "url"  , URL.Serialize() }
		};

		m_sNonce.clear(); // single use

		if (bUseKid)
		{
			jProtected["kid"] = m_sKid;
		}
		else
		{
			jProtected["jwk"] = AccountJWK();
		}

		auto sBody = AccountSign(std::move(jProtected), sPayload);

		if (sBody.empty())
		{
			SetError("cannot sign request");
			return {};
		}

		if (!sAccept.empty())
		{
			m_HTTP.AddHeader(KHTTPHeader::ACCEPT, sAccept);
		}

		auto sResponse = m_HTTP.Post(URL, sBody, KMIME("application/jose+json"));

		// every response carries a fresh nonce
		m_sNonce = m_HTTP.Response.Headers.Get(ReplayNonce);

		auto iStatus = m_HTTP.GetStatusCode();

		if (iStatus >= 200 && iStatus < 300)
		{
			return sResponse;
		}

		KJSON jProblem;
		KString sIgnored;
		kjson::Parse(jProblem, sResponse, sIgnored);

		if (jProblem["type"] == BadNonce)
		{
			kDebug(2, "bad nonce, retrying");
			continue;
		}

		SetError(kFormat("ACME error at {}: HTTP {}, {} ({})",
		                 URL.Serialize(), iStatus,
		                 jProblem["detail"].String(), jProblem["type"].String()));
		return {};
	}

	SetError("too many badNonce retries");
	return {};

} // SignedRequest

//-----------------------------------------------------------------------------
KJSON KAcmeClient::SignedPost(const KURL& URL, KStringView sPayload, bool bUseKid)
//-----------------------------------------------------------------------------
{
	auto sResponse = SignedRequest(URL, sPayload, bUseKid);

	KJSON jResponse;

	if (!sResponse.empty())
	{
		KString sError;

		if (!kjson::Parse(jResponse, sResponse, sError))
		{
			SetError(kFormat("invalid ACME response: {}", sError));
		}
	}

	return jResponse;

} // SignedPost

//-----------------------------------------------------------------------------
bool KAcmeClient::CreateAccount()
//-----------------------------------------------------------------------------
{
	if (!m_sKid.empty())
	{
		return true;
	}

	if (!LoadOrCreateAccountKey())
	{
		return false;
	}

	KJSON jPayload { { "termsOfServiceAgreed", true } };

	if (!m_Options.sContact.empty())
	{
		jPayload["contact"] = KJSON::array({ m_Options.sContact });
	}

	SignedPost(KURL(m_jDirectory["newAccount"].String()), jPayload.dump(), false);

	if (HasError())
	{
		return false;
	}

	// the account URL doubles as the kid for all further requests
	m_sKid = m_HTTP.Response.Headers.Get(KHTTPHeader::LOCATION);

	if (m_sKid.empty())
	{
		return SetError("no account URL in newAccount response");
	}

	kDebug(2, "ACME account: {}", m_sKid);

	return true;

} // CreateAccount

//-----------------------------------------------------------------------------
KJSON KAcmeClient::PollStatus(const KURL& URL)
//-----------------------------------------------------------------------------
{
	auto Timeout = KUnixTime::now() + m_Options.PollTimeout;

	for (;;)
	{
		auto jStatus = SignedPost(URL, "");

		if (HasError())
		{
			return jStatus;
		}

		auto& sStatus = jStatus["status"].String();

		if (sStatus != "pending" && sStatus != "processing" && sStatus != "ready")
		{
			return jStatus;
		}

		if (KUnixTime::now() > Timeout)
		{
			SetError(kFormat("timeout waiting for {}", URL.Serialize()));
			return jStatus;
		}

		auto Wait        = m_Options.PollInterval;
		auto iRetryAfter = m_HTTP.Response.Headers.Get(KHTTPHeader::RETRY_AFTER).UInt64();

		if (iRetryAfter > 0 && iRetryAfter <= 120)
		{
			Wait = chrono::seconds(iRetryAfter);
		}

		kSleep(Wait);
	}

} // PollStatus

//-----------------------------------------------------------------------------
bool KAcmeClient::DoAuthorization(const KURL& URL)
//-----------------------------------------------------------------------------
{
	auto jAuthz = SignedPost(URL, "");

	if (HasError())
	{
		return false;
	}

	if (jAuthz["status"] == "valid")
	{
		// still authorized from an earlier order
		return true;
	}

	auto sDomain = jAuthz["identifier"]["value"].String();

	const bool  bHttp       = (m_Options.ChallengeType == Challenge::Http01);
	KStringView sWantedType = bHttp ? "http-01" : "tls-alpn-01";

	KJSON jChallenge;

	for (auto& jOffered : jAuthz["challenges"])
	{
		if (jOffered["type"] == sWantedType)
		{
			jChallenge = jOffered;
			break;
		}
	}

	if (jChallenge.empty())
	{
		return SetError(kFormat("no {} challenge offered for {}", sWantedType, sDomain));
	}

	auto sToken       = jChallenge["token"].String();
	auto sKeyAuth     = kFormat("{}.{}", sToken, AccountThumbprint());
	auto sLowerDomain = sDomain.ToLowerASCII();

	// serve the challenge until validation completed, then remove it
	if (bHttp)
	{
		m_HTTPChallenges->unique().get()[sToken] = sKeyAuth;
	}
	else
	{
		auto Challenge = CreateChallengeContext(sDomain, sKeyAuth, m_Options.iKeyLength);

		if (!Challenge)
		{
			return SetError(kFormat("cannot create challenge context for {}", sDomain));
		}

		m_Challenges->unique().get()[sLowerDomain] = std::move(Challenge);
	}

	KAtScopeEnd(
		if (bHttp) m_HTTPChallenges->unique().get().erase(sToken);
		else       m_Challenges->unique().get().erase(sLowerDomain);
	);

	// ask the CA to validate
	SignedPost(KURL(jChallenge["url"].String()), "{}");

	if (HasError())
	{
		return false;
	}

	auto jResult = PollStatus(URL);

	if (HasError())
	{
		return false;
	}

	if (jResult["status"] != "valid")
	{
		// surface the error detail the CA reported for the failing challenge
		KString sDetail;

		for (auto& jFailed : jResult["challenges"])
		{
			sDetail = jFailed["error"]["detail"].String();

			if (!sDetail.empty())
			{
				break;
			}
		}

		return SetError(kFormat("validation for {} failed: {}", sDomain,
		                        sDetail.empty() ? jResult.dump() : sDetail));
	}

	kDebug(2, "{} validated", sDomain);

	return true;

} // DoAuthorization

//-----------------------------------------------------------------------------
KAcmeClient::Certificate KAcmeClient::OrderCertificate(const std::vector<KString>& Domains, KStringView sReplacesCertID)
//-----------------------------------------------------------------------------
{
	Certificate Issued;

	ClearError();

	if (Domains.empty())
	{
		SetError("no domains");
		return Issued;
	}

	for (const auto& sDomain : Domains)
	{
		if (sDomain.starts_with('*'))
		{
			SetError("wildcard domains need the dns-01 challenge, which is not supported");
			return Issued;
		}
	}

	if (!FetchDirectory() || !CreateAccount())
	{
		return Issued;
	}

	// create the order
	KJSON jIdentifiers = KJSON::array();

	for (const auto& sDomain : Domains)
	{
		jIdentifiers.push_back(KJSON { { "type", "dns" }, { "value", sDomain } });
	}

	KJSON jNewOrder { { "identifiers", std::move(jIdentifiers) } };

	if (!m_Options.sProfile.empty())
	{
		jNewOrder["profile"] = m_Options.sProfile;
	}

	if (!sReplacesCertID.empty())
	{
		// link this order to the certificate it renews (RFC 9773)
		jNewOrder["replaces"] = KString(sReplacesCertID);
	}

	auto jOrder = SignedPost(KURL(m_jDirectory["newOrder"].String()), jNewOrder.dump());

	if (HasError() && !sReplacesCertID.empty())
	{
		// a CA may reject the replaces link, e.g. when the certificate was
		// already replaced - the order itself is still fine without it
		kDebug(1, "newOrder with replaces failed ({}), retrying without", Error());
		ClearError();
		jNewOrder.erase("replaces");
		jOrder = SignedPost(KURL(m_jDirectory["newOrder"].String()), jNewOrder.dump());
	}

	if (HasError())
	{
		return Issued;
	}

	// copy now - the header is gone with the next request
	KString sOrderURL = m_HTTP.Response.Headers.Get(KHTTPHeader::LOCATION);

	if (sOrderURL.empty())
	{
		SetError("no order URL in newOrder response");
		return Issued;
	}

	// authorize all domains
	for (auto& jAuthz : jOrder["authorizations"])
	{
		if (!DoAuthorization(KURL(jAuthz.String())))
		{
			return Issued;
		}
	}

	// finalize the order with a CSR for a fresh certificate key
	KCSR    Csr;
	KString sCertKeyPEM;

	if (m_Options.Keys == KeyType::EC)
	{
		KECKey CertKey(true);
		Csr.Create(CertKey, Domains);
		sCertKeyPEM = CertKey.GetPEM(true);
	}
	else
	{
		KRSAKey CertKey(m_Options.iKeyLength);
		Csr.Create(CertKey, Domains);
		sCertKeyPEM = CertKey.GetPEM(true);
	}

	if (Csr.HasError())
	{
		SetError(Csr.CopyLastError());
		return Issued;
	}

	SignedPost(KURL(jOrder["finalize"].String()),
	           KJSON { { "csr", KBase64Url::Encode(Csr.GetDER()) } }.dump());

	if (HasError())
	{
		return Issued;
	}

	// wait for the certificate to be issued
	auto jFinal = PollStatus(KURL(sOrderURL));

	if (HasError())
	{
		return Issued;
	}

	if (jFinal["status"] != "valid")
	{
		SetError(kFormat("order failed: {}", jFinal.dump()));
		return Issued;
	}

	// download the certificate chain
	auto sChain = SignedRequest(KURL(jFinal["certificate"].String()), "", true,
	                            "application/pem-certificate-chain");

	if (HasError())
	{
		return Issued;
	}

	if (!sChain.contains("BEGIN CERTIFICATE"))
	{
		SetError("no certificate in response");
		return Issued;
	}

	Issued.sCertPEM       = std::move(sChain);
	Issued.sKeyPEM        = std::move(sCertKeyPEM);
	Issued.sAccountKeyPEM = AccountPEM();

	kDebug(2, "certificate issued for {}", kJoined(Domains));

	return Issued;

} // OrderCertificate

DEKAF2_NAMESPACE_END
