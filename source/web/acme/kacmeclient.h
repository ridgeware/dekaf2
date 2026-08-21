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

#pragma once

/// @file kacmeclient.h
/// ACME client (RFC 8555) with tls-alpn-01 validation (RFC 8737)

#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/strings/kstringview.h>
#include <dekaf2/core/errors/kerror.h>
#include <dekaf2/crypto/rsa/krsakey.h>
#include <dekaf2/data/json/kjson.h>
#include <dekaf2/http/client/kwebclient.h>
#include <dekaf2/net/tls/ktlscontext.h>
#include <dekaf2/threading/primitives/kthreadsafe.h>
#include <dekaf2/time/duration/kduration.h>
#include <memory>
#include <unordered_map>
#include <vector>

DEKAF2_NAMESPACE_BEGIN

/// @addtogroup web
/// @{

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// ACME client (RFC 8555) that orders TLS certificates from a CA like Let's
/// Encrypt, proving domain ownership with the tls-alpn-01 challenge (RFC 8737).
/// The challenge is served on port 443 through the SNI dispatch of the server's
/// KTLSContext - no port 80, no firewall change, no downtime:
/// @code
///   KAcmeClient Acme({ KAcmeClient::LetsEncryptStaging, "mailto:admin@example.com" });
///   Acme.AttachTo(ServerTLSContext);
///   auto Cert = Acme.OrderCertificate({ "www.example.com" });
///   if (Cert.IsValid()) ServerTLSContext.SetTLSCertificates(Cert.sCertPEM, Cert.sKeyPEM);
/// @endcode
class DEKAF2_PUBLIC KAcmeClient : public KErrorBase
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	static constexpr KStringViewZ LetsEncrypt        = "https://acme-v02.api.letsencrypt.org/directory";
	static constexpr KStringViewZ LetsEncryptStaging = "https://acme-staging-v02.api.letsencrypt.org/directory";

	struct Options
	{
		/// the ACME directory URL, default is Let's Encrypt production
		KString   sDirectoryURL  { LetsEncrypt        };
		/// optional account contact, e.g. "mailto:admin@example.com"
		KString   sContact;
		/// existing account key in PEM format - empty creates a new account key
		KString   sAccountKeyPEM;
		/// key length for newly created account and certificate keys
		uint16_t  iKeyLength     { 2048               };
		/// interval between status polls
		KDuration PollInterval   { chrono::seconds(2) };
		/// maximum wait for a single authorization or order to complete
		KDuration PollTimeout    { chrono::minutes(2) };
		/// verify the TLS certificate of the ACME directory endpoint - disable
		/// for test servers like Pebble with their own test CA
		bool      bVerifyTLS     { true               };
	};

	struct Certificate
	{
		/// returns true if a certificate was issued
		bool IsValid() const { return !sCertPEM.empty() && !sKeyPEM.empty(); }

		/// certificate plus chain in PEM format
		KString sCertPEM;
		/// the certificate's private key in PEM format
		KString sKeyPEM;
		/// the account key in PEM format - persist it to reuse the account
		KString sAccountKeyPEM;

	}; // Certificate

	KAcmeClient();
	KAcmeClient(Options Options);

	/// install the tls-alpn-01 challenge responder as the SNI callback of a server
	/// context. The context must serve port 443 of the ordered domains.
	/// Fails with TLS libraries older than OpenSSL 1.1.1 (no ALPN dispatch).
	bool AttachTo(KTLSContext& Context) const;

	/// the challenge responder as an SNI callback, to combine with an own callback
	/// instead of AttachTo(). Stays valid after this client is destroyed.
	KTLSContext::SNICallback GetChallengeResolver() const;

	/// order a certificate for the given domains, proving ownership with tls-alpn-01
	/// challenges served through the attached context. Blocks until the certificate
	/// is issued or validation failed (check with Certificate::IsValid()).
	/// @param Domains the requested domains - no wildcards (those need dns-01)
	Certificate OrderCertificate(const std::vector<KString>& Domains);

	/// RFC 7638 thumbprint of an RSA key's public JWK (SHA-256, base64url)
	static KString JWKThumbprint(const KRSAKey& Key);

	/// sign a JWS in flattened JSON serialization with RS256
	/// @param Key the signing key
	/// @param jProtected the protected header - alg is set to RS256
	/// @param sPayload the payload, may be empty (POST-as-GET)
	static KString SignJWS(const KRSAKey& Key, KJSON jProtected, KStringView sPayload);

	/// create a server TLS context carrying a tls-alpn-01 challenge certificate
	/// (RFC 8737) for sDomain, with ALPN restricted to acme-tls/1
	/// @param sDomain the domain under validation
	/// @param sKeyAuthorization the challenge's key authorization (token.thumbprint)
	/// @param iKeyLength key length for the challenge certificate's key
	static std::shared_ptr<KTLSContext> CreateChallengeContext(KStringView sDomain, KStringView sKeyAuthorization, uint16_t iKeyLength = 2048);

//------
private:
//------

	using ChallengeMap = KThreadSafe<std::unordered_map<KString, std::shared_ptr<KTLSContext>>>;

	static KJSON GetPublicJWK   (const KRSAKey& Key);

	bool    FetchDirectory ();
	bool    FetchNonce     ();
	bool    CreateAccount  ();
	KString SignedRequest  (const KURL& URL, KStringView sPayload, bool bUseKid = true, KStringView sAccept = KStringView{});
	KJSON   SignedPost     (const KURL& URL, KStringView sPayload, bool bUseKid = true);
	KJSON   PollStatus     (const KURL& URL);
	bool    DoAuthorization(const KURL& URL);

	Options                       m_Options;
	KWebClient                    m_HTTP;
	KRSAKey                       m_AccountKey;
	KJSON                         m_jDirectory;
	KString                       m_sKid;
	KString                       m_sNonce;
	std::shared_ptr<ChallengeMap> m_Challenges;

}; // KAcmeClient

/// @}

DEKAF2_NAMESPACE_END
