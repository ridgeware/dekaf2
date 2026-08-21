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
#include <dekaf2/crypto/encoding/kbase64.h>
#include <dekaf2/crypto/hash/kmessagedigest.h>
#include <dekaf2/crypto/rsa/kcsr.h>
#include <dekaf2/crypto/rsa/krsacert.h>
#include <dekaf2/crypto/rsa/krsasign.h>
#include <dekaf2/system/os/ksystem.h>
#include <openssl/opensslv.h>

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
{
	m_HTTP.SetVerifyCerts(m_Options.bVerifyTLS);
}

//-----------------------------------------------------------------------------
KJSON KAcmeClient::GetPublicJWK(const KRSAKey& Key)
//-----------------------------------------------------------------------------
{
	auto jFull = Key.GetPublicJWK("", "RS256");

	if (jFull.empty())
	{
		return KJSON{};
	}

	// the account JWK in a JWS header carries only the key parameters
	return KJSON {
		{ "e"  , jFull["e"  ].String() },
		{ "kty", jFull["kty"].String() },
		{ "n"  , jFull["n"  ].String() }
	};

} // GetPublicJWK

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
KString KAcmeClient::SignJWS(const KRSAKey& Key, KJSON jProtected, KStringView sPayload)
//-----------------------------------------------------------------------------
{
	jProtected["alg"] = "RS256";

	auto sProtected64 = KBase64Url::Encode(jProtected.dump());
	auto sPayload64   = KBase64Url::Encode(sPayload);

	KRSASign Signer(KRSASign::Digest::SHA256);
	Signer.Update(sProtected64);
	Signer.Update(".");
	Signer.Update(sPayload64);

	auto sSignature = Signer.Sign(Key);

	if (sSignature.empty())
	{
		kDebug(1, "cannot sign JWS: {}", Signer.Error());
		return {};
	}

	return KJSON {
		{ "protected", std::move(sProtected64)        },
		{ "payload"  , std::move(sPayload64)          },
		{ "signature", KBase64Url::Encode(sSignature) }
	}.dump();

} // SignJWS

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
			jProtected["jwk"] = GetPublicJWK(m_AccountKey);
		}

		auto sBody = SignJWS(m_AccountKey, std::move(jProtected), sPayload);

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

	if (m_AccountKey.empty())
	{
		if (!m_Options.sAccountKeyPEM.empty())
		{
			if (!m_AccountKey.Create(KStringView(m_Options.sAccountKeyPEM)))
			{
				return SetError(kFormat("cannot load account key: {}", m_AccountKey.Error()));
			}
		}
		else if (!m_AccountKey.Create(m_Options.iKeyLength))
		{
			return SetError(kFormat("cannot create account key: {}", m_AccountKey.Error()));
		}
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

	KJSON jChallenge;

	for (auto& jOffered : jAuthz["challenges"])
	{
		if (jOffered["type"] == "tls-alpn-01")
		{
			jChallenge = jOffered;
			break;
		}
	}

	if (jChallenge.empty())
	{
		return SetError(kFormat("no tls-alpn-01 challenge offered for {}", sDomain));
	}

	auto sKeyAuth  = kFormat("{}.{}", jChallenge["token"].String(), JWKThumbprint(m_AccountKey));
	auto Challenge = CreateChallengeContext(sDomain, sKeyAuth, m_Options.iKeyLength);

	if (!Challenge)
	{
		return SetError(kFormat("cannot create challenge context for {}", sDomain));
	}

	auto sLowerDomain = sDomain.ToLowerASCII();

	// serve the challenge until validation completed, then remove it
	m_Challenges->unique().get()[sLowerDomain] = std::move(Challenge);

	KAtScopeEnd( m_Challenges->unique().get().erase(sLowerDomain) );

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
KAcmeClient::Certificate KAcmeClient::OrderCertificate(const std::vector<KString>& Domains)
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

	auto jOrder = SignedPost(KURL(m_jDirectory["newOrder"].String()),
	                         KJSON { { "identifiers", std::move(jIdentifiers) } }.dump());

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
	KRSAKey CertKey(m_Options.iKeyLength);
	KCSR    Csr(CertKey, Domains);

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
	Issued.sKeyPEM        = CertKey.GetPEM(true);
	Issued.sAccountKeyPEM = m_AccountKey.GetPEM(true);

	kDebug(2, "certificate issued for {}", kJoined(Domains));

	return Issued;

} // OrderCertificate

DEKAF2_NAMESPACE_END
