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
 // |/|   OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR          |/|
 // |\|   OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR        |\|
 // |/|   OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE         |/|
 // |\|   SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.            |\|
 // |/|                                                                     |/|
 // |/+---------------------------------------------------------------------+/|
 // |\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/ |
 // +-------------------------------------------------------------------------+
 */

// kssod_watchdog.cpp — see kssod_watchdog.h

#include "kssod_watchdog.h"
#include <dekaf2/core/format/kformat.h>
#include <dekaf2/crypto/encoding/kencode.h>   // KEncode::URL
#include <dekaf2/util/mail/kmail.h>
#include <dekaf2/core/logging/klog.h>

namespace {

// the rule table: first match wins. Alerts name what happened in the subject;
// digest rules only need to say "worth a line in tomorrow's summary".
constexpr KSSOdWatchdog::Rule s_Rules[] =
{
	// --- alerts: someone is working on an account -----------------------------
	{ "auth.password",  "throttled",      KSSOdWatchdog::Kind::Alert,  "sign-in throttled: too many wrong passwords"         , false },
	{ "auth.2fa",       "too_many",       KSSOdWatchdog::Kind::Alert,  "second factor guessed at - the password is known"    , false },
	{ "account.reauth", "wrong_password", KSSOdWatchdog::Kind::Alert,  "valid session failed a password re-check - hijacked?", false },
	{ "sso.code",       "denied",         KSSOdWatchdog::Kind::Alert,  "SSO access denied"                                   , false },
	{ "sso.token",      "denied",         KSSOdWatchdog::Kind::Alert,  "SSO token refused"                                   , false },
	{ "sso.refresh",    "denied",         KSSOdWatchdog::Kind::Alert,  "SSO refresh refused"                                 , false },
	{ "auth.recovery",  "throttled",      KSSOdWatchdog::Kind::Alert,  "password recovery throttled"                         , true  },
	// --- digest: the configuration moved ---------------------------------------
	{ "admin.",         "",               KSSOdWatchdog::Kind::Digest, "", false },
	{ "account.2fa.totp",  "disabled",    KSSOdWatchdog::Kind::Digest, "", false },
	{ "account.2fa.email", "off",         KSSOdWatchdog::Kind::Digest, "", false },
	{ "account.2fa.backup_codes", "",     KSSOdWatchdog::Kind::Digest, "", false },
	{ "account.email.changed",  "",       KSSOdWatchdog::Kind::Digest, "", false },
	{ "account.password", "ok",           KSSOdWatchdog::Kind::Digest, "", false },
	{ "auth.recovery",  "password_reset", KSSOdWatchdog::Kind::Digest, "", false },
};

constexpr KDuration s_DigestInterval = std::chrono::hours(24);

KString Line(const KSSOdAuditStore::Entry& E)
{
	return kFormat("{}  {} {}  actor={} subject={} ip={}{}{}",
	               E.sTime, E.sEvent, E.sOutcome,
	               E.sActor.empty() ? "-" : E.sActor, E.sSubject.empty() ? "-" : E.sSubject,
	               E.sIP.empty() ? "-" : E.sIP,
	               E.sDetails.empty() ? "" : "  ", E.sDetails);
}

} // anonymous namespace

//-----------------------------------------------------------------------------
KSSOdWatchdog::KSSOdWatchdog(KSSOdUserStore& Users, KSSOdSettingsStore& Settings, KSSOdAuditStore& Audit, KString sIssuer)
//-----------------------------------------------------------------------------
: m_Users(Users)
, m_Settings(Settings)
, m_Audit(Audit)
, m_sIssuer(std::move(sIssuer))
, m_tDayStart(KUnixTime::now())
{
} // ctor

//-----------------------------------------------------------------------------
const KSSOdWatchdog::Rule* KSSOdWatchdog::Match(const KSSOdAuditStore::Entry& E)
//-----------------------------------------------------------------------------
{
	for (const auto& R : s_Rules)
	{
		bool bEvent = R.sEvent.back() == '.' ? E.sEvent.starts_with(R.sEvent) : E.sEvent == R.sEvent;
		if (bEvent && (R.sOutcome.empty() || R.sOutcome == E.sOutcome)) return &R;
	}
	return nullptr;

} // Match

//-----------------------------------------------------------------------------
void KSSOdWatchdog::Observe(const KSSOdAuditStore::Entry& E)
//-----------------------------------------------------------------------------
{
	// our own records must not feed back into us
	if (E.sEvent.starts_with("watchdog.")) return;

	const Rule* R = Match(E);
	if (!R) return;

	KUnixTime tNow = KUnixTime::now();

	if (R->eKind == Kind::Alert)
	{
		QueueAlert(*R, E, tNow);
	}
	else
	{
		std::lock_guard<std::mutex> Lock(m_Mutex);
		if (m_Digest.empty()) m_tDigestStart = tNow;
		m_Digest.push_back(E);
	}

} // Observe

//-----------------------------------------------------------------------------
KString KSSOdWatchdog::AuditLink(KStringView sKey, KStringView sValue, KUnixTime tSince) const
//-----------------------------------------------------------------------------
{
	return kFormat("{}/admin/audit?{}={}&since={}", m_sIssuer, sKey, KEncode::URL(sValue),
	               kFormTimestamp(KUTCTime(tSince), "{:%Y-%m-%dT%H:%M:%SZ}"));

} // AuditLink

//-----------------------------------------------------------------------------
void KSSOdWatchdog::QueueAlert(const Rule& R, const KSSOdAuditStore::Entry& E, KUnixTime tNow)
//-----------------------------------------------------------------------------
{
	auto Config = m_Settings.LoadAlerts();
	if (!Config.bEnabled) return;

	// the target: whom (or where from) this is about
	KString sTarget = R.bKeyByIP ? E.sIP : (E.sActor.empty() ? E.sSubject : E.sActor);
	KString sKey    = kFormat("{}|{}", R.sTitle, sTarget);

	std::unique_lock<std::mutex> Lock(m_Mutex);

	// the daily window
	if (tNow - m_tDayStart >= std::chrono::hours(24))
	{
		m_tDayStart  = tNow;
		m_iSentToday = 0;
		m_iOverCap   = 0;
	}

	auto& Cool = m_Cooldown[sKey];
	bool  bFirst = (Cool.tLastSent == KUnixTime{});
	if (!bFirst && tNow - Cool.tLastSent < Config.Cooldown)
	{
		++Cool.iSuppressed;   // reported with the next mail for this key
		return;
	}
	if (m_iSentToday >= Config.iDailyMax)
	{
		++Cool.iSuppressed;
		++m_iOverCap;
		return;
	}

	KString sSubject = kFormat("[kssod] {}: {}", R.sTitle, sTarget);
	KString sBody    = kFormat("kssod watchdog\n\n{}\n\n{}\n", R.sTitle, Line(E));
	if (!E.sUA.empty()) sBody += kFormat("user agent: {}\n", E.sUA);
	if (Cool.iSuppressed)
	{
		sBody += kFormat("\n{} further occurrence(s) since the last mail about this were not reported separately.\n",
		                 Cool.iSuppressed);
	}
	if (m_iOverCap)
	{
		sBody += kFormat("{} alert(s) of other kinds were held back today by the daily cap of {}.\n",
		                 m_iOverCap, Config.iDailyMax);
	}
	KUnixTime tSince = tNow - std::chrono::hours(24);
	sBody += kFormat("\nAudit trail:\n  {}\n", AuditLink(R.bKeyByIP ? "ip" : "user", sTarget, tSince));
	if (!E.sIP.empty() && !R.bKeyByIP)
	{
		sBody += kFormat("  {}\n", AuditLink("ip", E.sIP, tSince));
	}
	sBody += kFormat("\nNo further mail about this for {} hours; the count will be in the next one.\n",
	                 Config.Cooldown.hours().count());

	Cool.tLastSent   = tNow;
	Cool.iSuppressed = 0;
	++m_iSentToday;

	// Send() only hands the mail to the spool, which returns at once - safe on
	// the request thread, and the mutex is not needed for it
	Lock.unlock();
	Send(sKey, sSubject, sBody);

} // QueueAlert

//-----------------------------------------------------------------------------
void KSSOdWatchdog::SendDigest()
//-----------------------------------------------------------------------------
{
	std::vector<KSSOdAuditStore::Entry> Entries;
	KUnixTime tStart;
	{
		std::lock_guard<std::mutex> Lock(m_Mutex);
		Entries.swap(m_Digest);
		tStart = m_tDigestStart;
	}
	KString sSubject = kFormat("[kssod] daily digest: {} configuration change(s)", Entries.size());
	KString sBody    = kFormat("kssod watchdog - changes since {}\n\n",
	                           kFormTimestamp(KUTCTime(tStart), "{:%Y-%m-%d %H:%M} UTC"));
	for (const auto& E : Entries) { sBody += Line(E); sBody += '\n'; }
	sBody += kFormat("\nAudit trail:\n  {}\n", AuditLink("event", "admin.", tStart));
	Send("digest", sSubject, sBody);

} // SendDigest

//-----------------------------------------------------------------------------
void KSSOdWatchdog::Tick(KUnixTime tNow)
//-----------------------------------------------------------------------------
{
	bool bDue = false;
	{
		std::lock_guard<std::mutex> Lock(m_Mutex);
		if (!m_Digest.empty() && tNow - m_tDigestStart >= s_DigestInterval)
		{
			if (m_Settings.LoadAlerts().bDigest) bDue = true;
			else                                 m_Digest.clear();
		}
	}
	if (bDue) SendDigest();

} // Tick

//-----------------------------------------------------------------------------
std::vector<KString> KSSOdWatchdog::Recipients()
//-----------------------------------------------------------------------------
{
	std::vector<KString> Out;
	for (const auto& U : m_Users.List())
	{
		if (U.bAdmin && !U.sEmail.empty() && m_Users.IsEmailVerified(U.sUsername)) Out.push_back(U.sEmail);
	}
	return Out;

} // Recipients

//-----------------------------------------------------------------------------
void KSSOdWatchdog::Send(KStringView sKey, KStringView sSubject, KStringView sBody)
//-----------------------------------------------------------------------------
{
	auto Smtp = m_Settings.LoadSmtp();
	if (!Smtp.IsConfigured())
	{
		kDebug(1, "watchdog: no mail relay, dropping '{}'", sSubject);
		return;
	}
	auto To = Recipients();
	if (To.empty())
	{
		m_Audit.Write("watchdog.mail", "no_recipient", "", sKey, "", "", {{ "subject", sSubject }});
		return;
	}

	// SendMail hands over to the spool and returns at once; "queued" is all we can
	// vouch for here - delivery failures are the spool's to log and retry
	KString sQueued, sRefused, sErr;
	for (const auto& sTo : To)
	{
		if (SendMail(Smtp, sTo, sSubject, sBody, sErr)) { if (!sQueued.empty())  sQueued  += ' '; sQueued  += sTo; }
		else                                             { if (!sRefused.empty()) sRefused += ' '; sRefused += sTo; }
	}

	KJSON Details = {{ "subject", sSubject }};
	if (!sQueued.empty())  Details["queued_to"] = sQueued;
	if (!sRefused.empty()) { Details["refused"] = sRefused; Details["error"] = sErr; }
	m_Audit.Write("watchdog.mail", sRefused.empty() ? "queued" : (sQueued.empty() ? "refused" : "partial"),
	              "", sKey, "", "", Details);

} // Send
