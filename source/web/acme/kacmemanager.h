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

/// @file kacmemanager.h
/// automatic TLS certificate management via ACME

#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/strings/kstringview.h>
#include <dekaf2/core/errors/kerror.h>
#include <dekaf2/net/tls/ktlscontext.h>
#include <dekaf2/time/clock/ktime.h>
#include <dekaf2/time/duration/kduration.h>
#include <dekaf2/time/duration/ktimer.h>
#include <dekaf2/web/acme/kacmeclient.h>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

DEKAF2_NAMESPACE_BEGIN

/// @addtogroup web
/// @{

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Keeps a server's TLS certificate obtained and renewed via ACME: loads a
/// stored certificate at startup, orders one when missing or expiring (through
/// KAcmeClient with the tls-alpn-01 challenge), installs it into the server's
/// KTLSContext without downtime, and checks periodically for renewal.
/// KTCPServer::SetACME() wraps this for the common case.
class DEKAF2_PUBLIC KAcmeManager : public KErrorBase
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	struct Options
	{
		/// the ACME client options (directory URL, contact, poll intervals, ..)
		KAcmeClient::Options Acme;
		/// the domains for the certificate - the first one is also the CN
		std::vector<KString> Domains;
		/// storage directory for account key, certificate and key - empty uses
		/// KRSACert::GetDefaultTLSDirectory()
		KString   sStorageDir;
		/// renew when the certificate has less remaining validity than this
		KDuration RenewBefore   { chrono::days (30) };
		/// interval between renewal checks
		KDuration CheckInterval { chrono::hours(12) };

	}; // Options

	KAcmeManager(Options Options);
	~KAcmeManager();

	KAcmeManager(const KAcmeManager&)            = delete;
	KAcmeManager& operator=(const KAcmeManager&) = delete;

	/// attach to a server TLS context and start certificate management: installs a
	/// stored certificate, orders one when missing or expiring, and keeps it renewed.
	/// @param Context the server context - must outlive this manager
	/// @param bBlockUntilIssued if true, block until a certificate is installed and
	/// fail if none can be obtained - else order and renew in the background. Blocking
	/// requires an already reachable server, as the CA validates during the order.
	bool Start(KTLSContext& Context, bool bBlockUntilIssued = false);

	/// stop the renewal checks - Start() may be called again
	void Stop();

	/// run a renewal check right now (the timer does this periodically)
	/// @returns true if a valid certificate is installed afterwards
	bool CheckNow();

	/// the time until which the installed certificate is valid - epoch if none
	/// is installed yet
	KUnixTime ValidUntil() const;

	/// the http-01 challenge responder for Challenge::Http01 - serve its return value
	/// as text/plain for requests to /.well-known/acme-challenge/(token) on port 80
	std::function<KString(KStringView)> GetHTTPChallengeResolver() const;

//------
private:
//------

	/// install a stored certificate if it exists, is valid, and covers the domains
	DEKAF2_PRIVATE
	bool LoadFromDisk    ();
	DEKAF2_PRIVATE
	bool StoreToDisk     (const KAcmeClient::Certificate& Cert);
	/// install cert and key into the attached context - per SNI dispatch for the
	/// managed domains (effective with the next handshake), and as the default cert
	DEKAF2_PRIVATE
	bool Install         (KStringView sCertPEM, KStringView sKeyPEM);
	/// order a new certificate if none is installed or it is about to expire
	DEKAF2_PRIVATE
	bool CheckAndRenew   ();
	DEKAF2_PRIVATE
	bool WriteFileAtomic (KStringView sFile, KStringView sContent, int iMode) const;
	DEKAF2_PRIVATE
	KString StoragePath  (KStringView sFile) const;

	Options                      m_Options;
	std::unique_ptr<KAcmeClient> m_Acme;
	KTLSContext*                 m_Context       { nullptr };
	KTimer::ID_t                 m_iTimerID      { KTimer::InvalidID };
	KTimer::ID_t                 m_iFirstCheckID { KTimer::InvalidID };
	KUnixTime                    m_ValidUntil;
	KString                      m_sCertID;
	mutable std::mutex           m_Mutex;

}; // KAcmeManager

/// @}

DEKAF2_NAMESPACE_END
