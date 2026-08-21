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

#include <dekaf2/web/acme/kacmemanager.h>
#include <dekaf2/core/format/kformat.h>
#include <dekaf2/core/init/dekaf2.h>
#include <dekaf2/core/logging/klog.h>
#include <dekaf2/core/strings/kstringutils.h>
#include <dekaf2/crypto/rsa/krsacert.h>
#include <dekaf2/system/filesystem/kfilesystem.h>
#include <algorithm>

DEKAF2_NAMESPACE_BEGIN

namespace {

constexpr KStringViewZ AccountFile = "acme-account.pem";
constexpr KStringViewZ CertFile    = "acme-cert.pem";
constexpr KStringViewZ KeyFile     = "acme-privkey.pem";

} // end of anonymous namespace

//-----------------------------------------------------------------------------
KAcmeManager::KAcmeManager(Options Options)
//-----------------------------------------------------------------------------
: m_Options(std::move(Options))
{
	if (m_Options.sStorageDir.empty())
	{
		m_Options.sStorageDir = KRSACert::GetDefaultTLSDirectory();
	}

	// reuse a stored account key unless one was given
	if (m_Options.Acme.sAccountKeyPEM.empty())
	{
		m_Options.Acme.sAccountKeyPEM = kReadAll(StoragePath(AccountFile));
	}

	m_Acme = std::make_unique<KAcmeClient>(m_Options.Acme);
}

//-----------------------------------------------------------------------------
KAcmeManager::~KAcmeManager()
//-----------------------------------------------------------------------------
{
	Stop();
}

//-----------------------------------------------------------------------------
KString KAcmeManager::StoragePath(KStringView sFile) const
//-----------------------------------------------------------------------------
{
	return kFormat("{}{}{}", m_Options.sStorageDir, kDirSep, sFile);

} // StoragePath

//-----------------------------------------------------------------------------
bool KAcmeManager::Start(KTLSContext& Context, bool bBlockUntilIssued)
//-----------------------------------------------------------------------------
{
	Stop();
	ClearError();

	if (m_Options.Domains.empty())
	{
		return SetError("no domains");
	}

	if (!kDirExists(m_Options.sStorageDir) && !kCreateDir(m_Options.sStorageDir, 0700, true))
	{
		return SetError(kFormat("cannot create storage directory {}", m_Options.sStorageDir));
	}

	m_Context = &Context;

	if (m_Options.Acme.ChallengeType == KAcmeClient::Challenge::TlsAlpn01)
	{
		// http-01 needs no SNI dispatch - its responder is wired into an HTTP
		// server on port 80, see GetHTTPChallengeResolver()
		if (!m_Acme->AttachTo(Context))
		{
			return SetError(m_Acme->CopyLastError());
		}
	}

	// a stored certificate bridges the time until the first renewal check
	{
		std::lock_guard<std::mutex> Lock(m_Mutex);
		m_ValidUntil = KUnixTime();
		m_sCertID.clear();
		LoadFromDisk();
	}

	if (bBlockUntilIssued)
	{
		if (!CheckNow())
		{
			return false;
		}
	}
	else
	{
		// run the first check shortly after start, when the server is listening -
		// the CA validates the challenge during the order
		m_iFirstCheckID = Dekaf::getInstance().GetTimer().CallOnce(chrono::seconds(1),
		[this](KUnixTime) { CheckAndRenew(); });
	}

	m_iTimerID = Dekaf::getInstance().GetTimer().CallEvery(m_Options.CheckInterval,
	[this](KUnixTime) { CheckAndRenew(); });

	return true;

} // Start

//-----------------------------------------------------------------------------
void KAcmeManager::Stop()
//-----------------------------------------------------------------------------
{
	// Cancel() waits for callbacks in flight
	if (m_iFirstCheckID != KTimer::InvalidID)
	{
		Dekaf::getInstance().GetTimer().Cancel(m_iFirstCheckID);
		m_iFirstCheckID = KTimer::InvalidID;
	}

	if (m_iTimerID != KTimer::InvalidID)
	{
		Dekaf::getInstance().GetTimer().Cancel(m_iTimerID);
		m_iTimerID = KTimer::InvalidID;
	}

} // Stop

//-----------------------------------------------------------------------------
bool KAcmeManager::CheckNow()
//-----------------------------------------------------------------------------
{
	return CheckAndRenew();

} // CheckNow

//-----------------------------------------------------------------------------
KUnixTime KAcmeManager::ValidUntil() const
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);
	return m_ValidUntil;

} // ValidUntil

//-----------------------------------------------------------------------------
std::function<KString(KStringView)> KAcmeManager::GetHTTPChallengeResolver() const
//-----------------------------------------------------------------------------
{
	return m_Acme->GetHTTPChallengeResolver();

} // GetHTTPChallengeResolver

//-----------------------------------------------------------------------------
bool KAcmeManager::CheckAndRenew()
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);

	bool bRenew = (m_ValidUntil == KUnixTime())
	           || (m_ValidUntil - KUnixTime::now()) <= m_Options.RenewBefore;

	if (!bRenew && !m_sCertID.empty())
	{
		// ask the CA for its suggested renewal window (ARI, RFC 9773) - it can
		// only advance the renewal (e.g. before a revocation), RenewBefore stays
		// the upper bound, and ARI failures simply keep the local policy
		auto Window = m_Acme->GetRenewalInfo(m_sCertID);

		if (Window.IsValid() && KUnixTime::now() >= Window.WindowStart)
		{
			kDebug(1, "CA suggests renewal since {:%F %T}", Window.WindowStart);
			bRenew = true;
		}
	}

	if (!bRenew)
	{
		return true;
	}

	kDebug(2, "ordering certificate for {}", kJoined(m_Options.Domains));

	auto Cert = m_Acme->OrderCertificate(m_Options.Domains, m_sCertID);

	if (!Cert.IsValid())
	{
		kDebug(1, "certificate order failed: {}", m_Acme->Error());
		return SetError(m_Acme->CopyLastError());
	}

	// an issued certificate is worth installing even if storing it fails
	if (!StoreToDisk(Cert))
	{
		kDebug(1, "cannot store certificate: {}", Error());
	}

	return Install(Cert.sCertPEM, Cert.sKeyPEM);

} // CheckAndRenew

//-----------------------------------------------------------------------------
bool KAcmeManager::LoadFromDisk()
//-----------------------------------------------------------------------------
{
	auto sCert = kReadAll(StoragePath(CertFile));
	auto sKey  = kReadAll(StoragePath(KeyFile));

	if (sCert.empty() || sKey.empty())
	{
		return false;
	}

	KRSACert Leaf(sCert);

	if (Leaf.empty() || !Leaf.IsValidNow())
	{
		return false;
	}

	// the stored certificate must cover all managed domains
	auto SANs = Leaf.GetSANs();

	for (auto& sSAN : SANs)
	{
		sSAN.MakeLowerASCII();
	}

	for (const auto& sDomain : m_Options.Domains)
	{
		if (std::find(SANs.begin(), SANs.end(), sDomain.ToLowerASCII()) == SANs.end())
		{
			kDebug(2, "stored certificate does not cover {}", sDomain);
			return false;
		}
	}

	kDebug(2, "installing stored certificate");

	return Install(sCert, sKey);

} // LoadFromDisk

//-----------------------------------------------------------------------------
bool KAcmeManager::WriteFileAtomic(KStringView sFile, KStringView sContent, int iMode) const
//-----------------------------------------------------------------------------
{
	auto sPath = StoragePath(sFile);
	auto sTemp = kFormat("{}.tmp", sPath);

	if (!kWriteFile(sTemp, sContent, iMode))
	{
		return SetError(kFormat("cannot write {}", sTemp));
	}

	if (!kRename(sTemp, sPath))
	{
		return SetError(kFormat("cannot rename {} to {}", sTemp, sPath));
	}

	return true;

} // WriteFileAtomic

//-----------------------------------------------------------------------------
bool KAcmeManager::StoreToDisk(const KAcmeClient::Certificate& Cert)
//-----------------------------------------------------------------------------
{
	return WriteFileAtomic(AccountFile, Cert.sAccountKeyPEM, 0600)
	    && WriteFileAtomic(KeyFile    , Cert.sKeyPEM       , 0600)
	    && WriteFileAtomic(CertFile   , Cert.sCertPEM      , 0644);

} // StoreToDisk

//-----------------------------------------------------------------------------
bool KAcmeManager::Install(KStringView sCertPEM, KStringView sKeyPEM)
//-----------------------------------------------------------------------------
{
	auto NewContext = std::make_shared<KTLSContext>(true);

	if (!NewContext->SetTLSCertificates(sCertPEM, sKeyPEM))
	{
		return SetError(NewContext->CopyLastError());
	}

	// SNI dispatch switches per handshake, so the new certificate is effective
	// with the very next connection
	for (const auto& sDomain : m_Options.Domains)
	{
		if (!m_Context->AddSNIContext(sDomain, NewContext))
		{
			return SetError(m_Context->CopyLastError());
		}
	}

	// also make it the default certificate for clients that do not send SNI
	if (!m_Context->SetTLSCertificates(sCertPEM, sKeyPEM))
	{
		return SetError(m_Context->CopyLastError());
	}

	KRSACert Leaf(sCertPEM);
	m_ValidUntil = Leaf.ValidUntil();
	m_sCertID    = KAcmeClient::CreateARICertID(sCertPEM);

	kDebug(1, "installed certificate for {}, valid until {:%F %T}", kJoined(m_Options.Domains), m_ValidUntil);

	return true;

} // Install

DEKAF2_NAMESPACE_END
