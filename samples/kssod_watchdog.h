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

// kssod_watchdog.h — the watchdog: turns audit records into admin alerts
//
// Two kinds of signal, both taken from the audit trail (no extra call sites):
//   * alerts  - a throttle tripped, a second factor was guessed at, a valid session
//               failed a password re-check, an SSO grant was denied. One mail per
//               signal and target, then a cooldown; what happens meanwhile is
//               counted and reported in the next mail, never dropped silently.
//   * digest  - configuration changes (users, clients, roles, settings, 2FA off,
//               email changed) summarised once a day.
// Mails go to every administrator with a verified address, through the mail
// spool (SendMail): queued at once, delivered by the spool when the relay
// answers. Without a relay the watchdog stays asleep. It only reports; it never
// blocks anyone - behind a relay every knock looks alike, and an automatic ban
// could just as well lock out the admin.

#pragma once

#include "kssod_store.h"
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/time/clock/ktime.h>
#include <map>
#include <mutex>
#include <vector>

using namespace dekaf2;

/// the mail sender, provided by the application (kssod.cpp)
bool SendMail(const KSSOdSettingsStore::Smtp& Smtp, KStringView sTo,
              KStringView sSubject, KStringView sBody, KString& sError);

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KSSOdWatchdog
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
public:
	/// @param sIssuer the public base URL, used for the links in the mails
	KSSOdWatchdog(KSSOdUserStore& Users, KSSOdSettingsStore& Settings, KSSOdAuditStore& Audit, KString sIssuer);

	/// audit observer: classify a record, queue an alert or add it to the digest
	void Observe(const KSSOdAuditStore::Entry& E);

	/// periodic (about once a minute): flushes the daily digest when it is due
	void Tick(KUnixTime tNow);

	// the rule table (kssod_watchdog.cpp) is spelled in these terms
	enum class Kind { Alert, Digest };

	struct Rule
	{
		KStringView sEvent;     ///< exact event name, or a prefix when it ends in '.'
		KStringView sOutcome;   ///< required outcome, or empty for any
		Kind        eKind;
		KStringView sTitle;     ///< the alert subject
		bool        bKeyByIP;   ///< cooldown per source address (else per user)
	};

private:
	static const Rule* Match(const KSSOdAuditStore::Entry& E);

	struct Cooldown
	{
		KUnixTime   tLastSent;
		std::size_t iSuppressed { 0 };
	};

	void QueueAlert(const Rule& R, const KSSOdAuditStore::Entry& E, KUnixTime tNow);
	void SendDigest();
	/// hand a mail to every administrator over to the spool, and audit that
	void Send(KStringView sKey, KStringView sSubject, KStringView sBody);
	std::vector<KString> Recipients();
	KString AuditLink(KStringView sKey, KStringView sValue, KUnixTime tSince) const;

	KSSOdUserStore&     m_Users;
	KSSOdSettingsStore& m_Settings;
	KSSOdAuditStore&    m_Audit;
	KString             m_sIssuer;

	std::mutex          m_Mutex;
	std::map<KString, Cooldown>         m_Cooldown;   ///< key: title + target
	std::vector<KSSOdAuditStore::Entry> m_Digest;
	KUnixTime           m_tDigestStart;               ///< when the pending digest began
	KUnixTime           m_tDayStart;                  ///< the 24h window of the daily cap
	uint16_t            m_iSentToday   { 0 };
	std::size_t         m_iOverCap     { 0 };         ///< alerts held back by the daily cap

}; // KSSOdWatchdog
