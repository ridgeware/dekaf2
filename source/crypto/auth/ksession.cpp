/*
 //
 // DEKAF(tm): Lighter, Faster, Smarter (tm)
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
 // |/|   WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR           |/|
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

#include <dekaf2/crypto/auth/ksession.h>
#include <dekaf2/crypto/random/krandom.h>
#include <dekaf2/crypto/encoding/kbase64.h>
#include <dekaf2/core/format/kformat.h>
#include <dekaf2/core/init/dekaf2.h>
#include <dekaf2/core/logging/klog.h>

// NOTE: the SQLite- and KSQL-backed convenience constructors are defined
// in the separate translation units ksessionsqlitestore.cpp and
// ksessionksqlstore.cpp respectively. Those TUs are compiled into the
// ksqlite / ksql2 libraries, not into the core dekaf2 library, so pulling
// KSession into an application that only uses the in-memory store does
// not force a link-time dependency on SQLite or KSQL.

DEKAF2_NAMESPACE_BEGIN

//-----------------------------------------------------------------------------
KSession::KSession(std::unique_ptr<Store> store, Config cfg)
//-----------------------------------------------------------------------------
: m_Config(std::move(cfg))
, m_Store(std::move(store))
{
	if (!m_Store)
	{
		SetError("KSession: Store is null");
		return;
	}

	if (!m_Store->Initialize())
	{
		SetError(kFormat("KSession: Store initialization failed: {}", m_Store->GetLastError()));
		return;
	}

	StartPurgeTimer();

} // ctor (custom Store)

//-----------------------------------------------------------------------------
KSession::KSession(std::unique_ptr<Store> store)
//-----------------------------------------------------------------------------
: KSession(std::move(store), Config{})
{
} // ctor (custom Store, default config)

//-----------------------------------------------------------------------------
KSession::~KSession()
//-----------------------------------------------------------------------------
{
	StopPurgeTimer();

} // dtor

//-----------------------------------------------------------------------------
void KSession::SetAuthenticator(Authenticator Auth)
//-----------------------------------------------------------------------------
{
	m_Authenticator.unique().get() = std::move(Auth);

} // SetAuthenticator

//-----------------------------------------------------------------------------
KString KSession::GenerateToken() const
//-----------------------------------------------------------------------------
{
	// kGetRandom is cryptographically secure (backed by OpenSSL's RAND_bytes)
	auto sRaw = kGetRandom(m_Config.iTokenBytes);
	return KBase64Url::Encode(sRaw);

} // GenerateToken

//-----------------------------------------------------------------------------
KString KSession::Login(KStringView sUsername,
                        KStringView sPassword,
                        KStringView sClientIP,
                        KStringView sUserAgent,
                        KStringView sExtra)
//-----------------------------------------------------------------------------
{
	if (sUsername.empty())
	{
		kDebug(2, "empty username rejected");
		return {};
	}

	// snapshot the authenticator so we do not hold the lock across the call
	Authenticator Auth;
	{
		Auth = m_Authenticator.shared().get();
	}

	if (!Auth)
	{
		SetError("KSession::Login: no authenticator configured");
		return {};
	}

	if (!Auth(sUsername, sPassword))
	{
		kDebug(1, "authentication failed for user '{}' from {}", sUsername, sClientIP);
		return {};
	}

	Record Rec;
	Rec.sToken     = GenerateToken();
	Rec.sUsername  = sUsername;
	Rec.tCreated   = KUnixTime::now();
	Rec.tLastSeen  = Rec.tCreated;
	Rec.sClientIP  = sClientIP;
	Rec.sUserAgent = sUserAgent;
	Rec.sExtra     = sExtra;

	if (!m_Store->Create(Rec))
	{
		SetError(kFormat("KSession::Login: Store::Create failed: {}", m_Store->GetLastError()));
		return {};
	}

	kDebug(2, "new session for user '{}' from {}", sUsername, sClientIP);
	return std::move(Rec.sToken);

} // Login

//-----------------------------------------------------------------------------
bool KSession::Validate(KStringView sToken, Record* pOut)
//-----------------------------------------------------------------------------
{
	if (sToken.empty())
	{
		return false;
	}

	Record Rec;

	if (!m_Store->Lookup(sToken, &Rec))
	{
		return false;
	}

	auto tNow = KUnixTime::now();

	// check absolute timeout first — it never moves forward
	if (m_Config.AbsoluteTimeout > KDuration::zero()
		&& tNow - Rec.tCreated > m_Config.AbsoluteTimeout)
	{
		kDebug(2, "session for '{}' exceeded absolute timeout", Rec.sUsername);
		m_Store->Erase(sToken);
		return false;
	}

	// check idle timeout
	if (m_Config.IdleTimeout > KDuration::zero()
		&& tNow - Rec.tLastSeen > m_Config.IdleTimeout)
	{
		kDebug(2, "session for '{}' exceeded idle timeout", Rec.sUsername);
		m_Store->Erase(sToken);
		return false;
	}

	// refresh LastSeen — both in the store and in our local copy
	Rec.tLastSeen = tNow;
	m_Store->Touch(sToken, tNow);

	if (pOut)
	{
		*pOut = std::move(Rec);
	}

	return true;

} // Validate

//-----------------------------------------------------------------------------
bool KSession::Logout(KStringView sToken)
//-----------------------------------------------------------------------------
{
	if (sToken.empty())
	{
		return false;
	}

	return m_Store->Erase(sToken);

} // Logout

//-----------------------------------------------------------------------------
std::size_t KSession::LogoutAllFor(KStringView sUsername)
//-----------------------------------------------------------------------------
{
	if (sUsername.empty())
	{
		return 0;
	}

	return m_Store->EraseAllFor(sUsername);

} // LogoutAllFor

//-----------------------------------------------------------------------------
std::size_t KSession::PurgeExpired()
//-----------------------------------------------------------------------------
{
	auto tNow = KUnixTime::now();

	// cutoff values: sessions whose LastSeen is before tOldestLastSeen are
	// idle-expired; sessions whose Created is before tOldestCreated are
	// absolute-expired. If a timeout is zero (i.e. disabled), set the
	// corresponding cutoff to the epoch so it matches nothing.
	auto tOldestLastSeen = (m_Config.IdleTimeout > KDuration::zero())
		? tNow - m_Config.IdleTimeout
		: KUnixTime{};
	auto tOldestCreated  = (m_Config.AbsoluteTimeout > KDuration::zero())
		? tNow - m_Config.AbsoluteTimeout
		: KUnixTime{};

	auto iPurged = m_Store->PurgeExpired(tOldestLastSeen, tOldestCreated);

	if (iPurged)
	{
		kDebug(2, "purged {} expired sessions", iPurged);
	}

	return iPurged;

} // PurgeExpired

//-----------------------------------------------------------------------------
std::size_t KSession::Count() const
//-----------------------------------------------------------------------------
{
	return m_Store ? m_Store->Count() : 0;

} // Count

//-----------------------------------------------------------------------------
KString KSession::SerializeAttributes(KDuration MaxAge) const
//-----------------------------------------------------------------------------
{
	KString sAttrs;

	if (!m_Config.sCookiePath.empty())
	{
		sAttrs += kFormat("; Path={}", m_Config.sCookiePath);
	}

	if (m_Config.bSecure)
	{
		sAttrs += "; Secure";
	}

	if (m_Config.bHttpOnly)
	{
		sAttrs += "; HttpOnly";
	}

	if (!m_Config.sSameSite.empty())
	{
		sAttrs += kFormat("; SameSite={}", m_Config.sSameSite);
	}

	// Max-Age=0 tells the browser to delete the cookie immediately.
	// For regular session cookies (browser-close ends session) we do not
	// emit a Max-Age at all — but we do emit an absolute Max-Age if the
	// caller requests one (e.g. the cookie's absolute lifetime).
	if (MaxAge > KDuration::zero() || MaxAge < KDuration::zero())
	{
		sAttrs += kFormat("; Max-Age={}", MaxAge.seconds().count());
	}

	return sAttrs;

} // SerializeAttributes

//-----------------------------------------------------------------------------
KString KSession::SerializeSetCookie(KStringView sToken) const
//-----------------------------------------------------------------------------
{
	// Format: name=value; Path=/; Secure; HttpOnly; SameSite=Strict
	// Callers wanting a Max-Age (e.g. absolute timeout) can append manually;
	// the default here is a session cookie (no Max-Age → ends with browser).
	auto sCookie = kFormat("{}={}", m_Config.sCookieName, sToken);
	sCookie += SerializeAttributes();
	return sCookie;

} // SerializeSetCookie

//-----------------------------------------------------------------------------
KString KSession::SerializeExpiryCookie() const
//-----------------------------------------------------------------------------
{
	// Empty value + Max-Age=0 is the standard way to tell the browser to
	// drop the cookie immediately. All other attributes must match what
	// was used at Set-time, otherwise some browsers keep the old cookie.
	auto sCookie = kFormat("{}=", m_Config.sCookieName);
	sCookie += SerializeAttributes();
	sCookie += "; Max-Age=0";
	return sCookie;

} // SerializeExpiryCookie

//-----------------------------------------------------------------------------
void KSession::StartPurgeTimer()
//-----------------------------------------------------------------------------
{
	if (m_Config.PurgeInterval <= KDuration::zero())
	{
		return;
	}

	// schedule periodic purge on Dekaf's shared timer — no extra thread
	m_PurgeTimerID = Dekaf::getInstance().GetTimer().CallEvery(
		m_Config.PurgeInterval,
		[this](KUnixTime) { PurgeExpired(); },
		/*bOwnThread=*/true);

} // StartPurgeTimer

//-----------------------------------------------------------------------------
void KSession::StopPurgeTimer()
//-----------------------------------------------------------------------------
{
	if (m_PurgeTimerID != KTimer::InvalidID)
	{
		Dekaf::getInstance().GetTimer().Cancel(m_PurgeTimerID);
		m_PurgeTimerID = KTimer::InvalidID;
	}

} // StopPurgeTimer

DEKAF2_NAMESPACE_END
