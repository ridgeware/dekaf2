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
#include <dekaf2/crypto/ec/keckey.h>
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

	enum class Challenge
	{
		TlsAlpn01, ///< validation on port 443 through the SNI dispatch, see AttachTo()
		Http01     ///< validation on port 80, see GetHTTPChallengeResolver()
	};

	enum class KeyType
	{
		RSA,       ///< RSA keys (iKeyLength bits), JWS with RS256
		EC         ///< P-256 keys, JWS with ES256 - smaller certs, faster handshakes
	};

	struct Options
	{
		/// the ACME directory URL, default is Let's Encrypt production
		KString   sDirectoryURL  { LetsEncrypt        };
		/// optional account contact, e.g. "mailto:admin@example.com"
		KString   sContact;
		/// existing account key in PEM format - empty creates a new account key
		KString   sAccountKeyPEM;
		/// certificate profile to request (CA specific, e.g. "shortlived") -
		/// empty lets the CA choose
		KString   sProfile;
		/// the challenge type proving domain ownership, default tls-alpn-01
		Challenge ChallengeType  { Challenge::TlsAlpn01 };
		/// the type of newly created account and certificate keys, default RSA.
		/// A stored account key keeps the type it was created with.
		KeyType   Keys           { KeyType::RSA         };
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

	struct RenewalInfo
	{
		/// returns true if the CA supplied a renewal window
		bool IsValid() const { return WindowStart != KUnixTime() && WindowEnd != KUnixTime(); }

		/// the CA's suggested renewal window (RFC 9773)
		KUnixTime WindowStart;
		KUnixTime WindowEnd;

	}; // RenewalInfo

	KAcmeClient();
	KAcmeClient(Options Options);

	/// install the tls-alpn-01 challenge responder as the SNI callback of a server
	/// context. The context must serve port 443 of the ordered domains.
	/// Fails with TLS libraries older than OpenSSL 1.1.1 (no ALPN dispatch).
	bool AttachTo(KTLSContext& Context) const;

	/// the challenge responder as an SNI callback, to combine with an own callback
	/// instead of AttachTo(). Stays valid after this client is destroyed.
	KTLSContext::SNICallback GetChallengeResolver() const;

	/// the http-01 challenge responder for Challenge::Http01: returns the response
	/// body for a request to /.well-known/acme-challenge/(token), or an empty string
	/// if no such challenge is pending. Serve it as text/plain on port 80 of the
	/// ordered domains. Stays valid after this client is destroyed.
	std::function<KString(KStringView)> GetHTTPChallengeResolver() const;

	/// order a certificate for the given domains, proving ownership with tls-alpn-01
	/// challenges served through the attached context. Blocks until the certificate
	/// is issued or validation failed (check with Certificate::IsValid()).
	/// @param Domains the requested domains - no wildcards (those need dns-01)
	/// @param sReplacesCertID the ARI certificate ID (see CreateARICertID()) of the
	/// certificate this order replaces - links the renewal for the CA (RFC 9773),
	/// which typically relaxes rate limits. Retried without on rejection.
	Certificate OrderCertificate(const std::vector<KString>& Domains, KStringView sReplacesCertID = KStringView{});

	/// fetch the CA's suggested renewal window for a certificate (RFC 9773). A CA
	/// without ARI support and fetch failures return an invalid RenewalInfo but set
	/// no error - the caller falls back to its local renewal policy.
	/// @param sARICertID the certificate ID from CreateARICertID()
	RenewalInfo GetRenewalInfo(KStringView sARICertID);

	/// compute the ARI certificate ID (RFC 9773) of a PEM certificate:
	/// base64url(AKI keyIdentifier) "." base64url(serial bytes).
	/// @returns the ID, or an empty string if the cert has no Authority Key Identifier
	static KString CreateARICertID(KStringView sCertPEM);

	/// RFC 7638 thumbprint of an RSA key's public JWK (SHA-256, base64url)
	static KString JWKThumbprint(const KRSAKey& Key);

	/// RFC 7638 thumbprint of a P-256 key's public JWK (SHA-256, base64url)
	static KString JWKThumbprint(const KECKey& Key);

	/// sign a JWS in flattened JSON serialization with RS256
	/// @param Key the signing key
	/// @param jProtected the protected header - alg is set to RS256
	/// @param sPayload the payload, may be empty (POST-as-GET)
	static KString SignJWS(const KRSAKey& Key, KJSON jProtected, KStringView sPayload);

	/// sign a JWS in flattened JSON serialization with ES256
	static KString SignJWS(const KECKey& Key, KJSON jProtected, KStringView sPayload);

	/// create a server TLS context carrying a tls-alpn-01 challenge certificate
	/// (RFC 8737) for sDomain, with ALPN restricted to acme-tls/1
	/// @param sDomain the domain under validation
	/// @param sKeyAuthorization the challenge's key authorization (token.thumbprint)
	/// @param iKeyLength key length for the challenge certificate's key
	static std::shared_ptr<KTLSContext> CreateChallengeContext(KStringView sDomain, KStringView sKeyAuthorization, uint16_t iKeyLength = 2048);

//------
private:
//------

	using ChallengeMap     = KThreadSafe<std::unordered_map<KString, std::shared_ptr<KTLSContext>>>;
	using HTTPChallengeMap = KThreadSafe<std::unordered_map<KString, KString>>;

	// the account key operations, dispatching on the key type
	DEKAF2_PRIVATE
	bool    LoadOrCreateAccountKey();
	DEKAF2_PRIVATE
	KJSON   AccountJWK        () const;
	DEKAF2_PRIVATE
	KString AccountThumbprint () const;
	DEKAF2_PRIVATE
	KString AccountSign       (KJSON jProtected, KStringView sPayload) const;
	DEKAF2_PRIVATE
	KString AccountPEM        ();

	bool    FetchDirectory ();
	bool    FetchNonce     ();
	bool    CreateAccount  ();
	KString SignedRequest  (const KURL& URL, KStringView sPayload, bool bUseKid = true, KStringView sAccept = KStringView{});
	KJSON   SignedPost     (const KURL& URL, KStringView sPayload, bool bUseKid = true);
	KJSON   PollStatus     (const KURL& URL);
	bool    DoAuthorization(const KURL& URL);

	Options                           m_Options;
	KWebClient                        m_HTTP;
	KRSAKey                           m_AccountKey;
	KECKey                            m_AccountKeyEC;
	KJSON                             m_jDirectory;
	KString                           m_sKid;
	KString                           m_sNonce;
	std::shared_ptr<ChallengeMap>     m_Challenges;
	std::shared_ptr<HTTPChallengeMap> m_HTTPChallenges;
	bool                              m_bAccountIsEC { false };

}; // KAcmeClient

/// @}

DEKAF2_NAMESPACE_END
