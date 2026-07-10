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

// kssod_store.cpp — SQLite persistence for the kssod sample (see kssod_store.h)

#include "kssod_store.h"

#include <dekaf2/data/sql/ksqlite.h>
#include <dekaf2/crypto/auth/kbcrypt.h>
#include <dekaf2/crypto/hash/kmessagedigest.h>
#include <dekaf2/crypto/random/krandom.h>
#include <dekaf2/time/clock/ktime.h>
#include <dekaf2/core/format/kformat.h>
#include <algorithm>

using namespace dekaf2;

namespace {

//-----------------------------------------------------------------------------
// the client's default role on an already-open db handle (no locking) - used by
// the grant/set methods which already hold the mutex and a db connection
KString DefaultRoleOn(KSQLite& db, KStringView sClientID)
//-----------------------------------------------------------------------------
{
	return db.SingleStringQuery("select role from kssod_roles where client_id=?1 and is_default<>0 limit 1", sClientID);
}

} // anonymous namespace

//-----------------------------------------------------------------------------
bool KSSOdInitDatabase(KString sDatabase, KString& sError)
//-----------------------------------------------------------------------------
{
	KSQLite db(sDatabase, KSQLite::Mode::READWRITECREATE);
	if (!db.IsOpen())
	{
		sError = kFormat("cannot open '{}'", sDatabase);
		return false;
	}

	// WAL: concurrent readers while a writer holds the lock; idempotent.
	db.ExecSQL("pragma journal_mode=WAL");
	db.ExecSQL("pragma synchronous=NORMAL");

	static constexpr KStringView s_sDDL[] =
	{
		"create table if not exists kssod_users ("
		"  username       text    primary key,"
		"  pw_hash        text    not null,"
		"  name           text    not null default '',"
		"  email          text    not null default '',"
		"  is_admin       integer not null default 0,"
		"  totp_secret    text    not null default '',"   // base32; empty = TOTP off
		"  email_verified integer not null default 0,"    // address confirmed via emailed link
		"  email_otp      integer not null default 0,"    // email used as the second factor
		"  pending_email  text    not null default '',"   // requested-but-unconfirmed new address (pending-change model)
		"  created_utc    integer not null default 0"
		")",

		// single-use recovery codes (hashed), a fallback for a lost authenticator
		"create table if not exists kssod_backup_codes ("
		"  username  text not null,"
		"  code_hash text not null"
		")",

		// single-use email tokens (hashed) for address verification and password
		// recovery; purpose-dependent expiry, deleted on use
		"create table if not exists kssod_email_tokens ("
		"  token_hash  text    not null,"
		"  username    text    not null,"
		"  purpose     text    not null,"   // 'verify' | 'recovery' | 'revert'
		"  data        text    not null default '',"   // purpose payload (e.g. the prior email for a 'revert' token)
		"  expires_utc integer not null default 0,"
		"  primary key (token_hash)"
		")",

		// generic key/value settings (currently just the optional SMTP relay)
		"create table if not exists kssod_settings ("
		"  key   text primary key,"
		"  value text not null default ''"
		")",

		"create table if not exists kssod_clients ("
		"  client_id          text    primary key,"
		"  secret_hash        text    not null default '',"
		"  redirect_uris      text    not null default '',"   // newline-separated
		"  postlogout_uris    text    not null default '',"   // newline-separated
		"  scopes             text    not null default '',"   // newline-separated
		"  is_public          integer not null default 0,"
		"  require_assignment integer not null default 0,"     // only assigned users may use this client
		"  force_login        integer not null default 0,"     // re-auth policy: always prompt, never silent SSO
		"  max_auth_age       integer not null default 0,"     // re-auth policy: force fresh login when OP session older than this many seconds (0 = no limit)
		"  created_utc        integer not null default 0"
		")",

		// per-client user assignment + RBAC: a row grants <username> access to
		// <client_id> with the given (space-separated) roles. For clients with
		// require_assignment=1, the absence of a row denies access.
		"create table if not exists kssod_assignments ("
		"  username    text    not null,"
		"  client_id   text    not null,"
		"  roles       text    not null default '',"
		"  access      integer not null default 1,"   // 0 = suspended: roles kept but inactive
		"  primary key (username, client_id)"
		")",

		// the set of roles defined for a client (the catalog the admin picks from
		// when assigning users). Deleting a row here also strips the role from
		// every assignment of the client (done in code, see DeleteRole).
		"create table if not exists kssod_roles ("
		"  client_id  text    not null,"
		"  role       text    not null,"
		"  is_default integer not null default 0,"   // at most one per client; auto-granted on access
		"  primary key (client_id, role)"
		")",

		// migrations: bring a database created by an older kssod up to date.
		// "create table if not exists" never alters an existing table, so columns
		// added after the first release must be patched in here. These are
		// best-effort (see the loop): they error on a fresh DB where the column
		// already exists, which is expected and ignored.
		"alter table kssod_clients add column require_assignment integer not null default 0",
		"alter table kssod_clients add column force_login integer not null default 0",
		"alter table kssod_clients add column max_auth_age integer not null default 0",
		"alter table kssod_roles add column is_default integer not null default 0",
		"alter table kssod_assignments add column access integer not null default 1",
		"alter table kssod_users add column totp_secret text not null default ''",
		"alter table kssod_users add column email_verified integer not null default 0",
		"alter table kssod_users add column email_otp integer not null default 0",
		"alter table kssod_users add column pending_email text not null default ''",
		"alter table kssod_email_tokens add column data text not null default ''",
	};

	for (const auto sSQL : s_sDDL)
	{
		auto Result = db.ExecSQL(sSQL);
		if (!Result)
		{
			// migration statements are best-effort: an "alter table ... add column"
			// fails on a fresh DB (the column is already in the create) or on a
			// re-run (already migrated) - neither is fatal.
			if (sSQL.starts_with("alter table"))
			{
				continue;
			}
			sError = kFormat("schema init failed: {}: {}", sSQL, Result.Error());
			return false;
		}
	}

	return true;

} // KSSOdInitDatabase

// =============================================================================
// KSSOdUserStore
// =============================================================================

//-----------------------------------------------------------------------------
bool KSSOdUserStore::VerifyPassword(KStringView sUsername, KStringView sPassword)
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);

	KSQLite db(m_sDatabase, KSQLite::Mode::READONLY);
	if (!db.IsOpen()) return false;

	KString sHash = db.SingleStringQuery("select pw_hash from kssod_users where username=?1", sUsername);
	if (sHash.empty()) return false;

	return KBCrypt().ValidatePassword(KString(sPassword), sHash);

} // VerifyPassword

//-----------------------------------------------------------------------------
bool KSSOdUserStore::GetClaims(KStringView sUsername, KJSON& Claims)
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);

	KSQLite db(m_sDatabase, KSQLite::Mode::READONLY);
	if (!db.IsOpen()) return false;

	auto Query = db.ExecQuery("select name, email from kssod_users where username=?1", sUsername);
	if (!Query.Next()) return false;

	auto& Row = Query.GetRow();
	Claims = {
		{ "preferred_username", sUsername           },
		{ "name",               Row.Col(1).String() },
		{ "email",              Row.Col(2).String() }
	};
	return true;

} // GetClaims

//-----------------------------------------------------------------------------
bool KSSOdUserStore::AddUser(KStringView sUsername, KStringView sPassword,
                             KStringView sName, KStringView sEmail, bool bAdmin)
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);

	KSQLite db(m_sDatabase, KSQLite::Mode::READWRITECREATE);
	if (!db.IsOpen()) return false;

	KString sHash = KBCrypt().GenerateHash(KString(sPassword));

	return static_cast<bool>(db.ExecSQL(
		"insert into kssod_users (username, pw_hash, name, email, is_admin, created_utc) "
		"values (?1, ?2, ?3, ?4, ?5, ?6)",
		sUsername, sHash, sName, sEmail,
		static_cast<int64_t>(bAdmin ? 1 : 0),
		static_cast<int64_t>(KUnixTime::now().to_time_t())));

} // AddUser

//-----------------------------------------------------------------------------
bool KSSOdUserStore::ChangePassword(KStringView sUsername, KStringView sNewPassword)
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);

	KSQLite db(m_sDatabase, KSQLite::Mode::READWRITECREATE);
	if (!db.IsOpen()) return false;

	KString sHash = KBCrypt().GenerateHash(KString(sNewPassword));

	return static_cast<bool>(db.ExecSQL("update kssod_users set pw_hash=?1 where username=?2", sHash, sUsername));

} // ChangePassword

//-----------------------------------------------------------------------------
bool KSSOdUserStore::SetName(KStringView sUsername, KStringView sName)
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);

	KSQLite db(m_sDatabase, KSQLite::Mode::READWRITECREATE);
	if (!db.IsOpen()) return false;

	return static_cast<bool>(db.ExecSQL("update kssod_users set name=?1 where username=?2", sName, sUsername));

} // SetName

//-----------------------------------------------------------------------------
bool KSSOdUserStore::DeleteUser(KStringView sUsername)
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);

	KSQLite db(m_sDatabase, KSQLite::Mode::READWRITECREATE);
	if (!db.IsOpen()) return false;

	// cascade: deleting a user also removes their per-client assignments (and the
	// roles granted there), 2FA backup codes and any pending email tokens
	for (KStringView sSQL : { "delete from kssod_assignments  where username=?1",
	                          "delete from kssod_backup_codes where username=?1",
	                          "delete from kssod_email_tokens where username=?1",
	                          "delete from kssod_users        where username=?1" })
	{
		db.ExecSQL(sSQL, sUsername);
	}
	return true;

} // DeleteUser

//-----------------------------------------------------------------------------
bool KSSOdUserStore::IsAdmin(KStringView sUsername)
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);

	KSQLite db(m_sDatabase, KSQLite::Mode::READONLY);
	if (!db.IsOpen()) return false;

	return db.SingleIntQuery("select is_admin from kssod_users where username=?1", sUsername) != 0;

} // IsAdmin

//-----------------------------------------------------------------------------
bool KSSOdUserStore::Exists(KStringView sUsername)
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);

	KSQLite db(m_sDatabase, KSQLite::Mode::READONLY);
	if (!db.IsOpen()) return false;

	return db.SingleIntQuery("select 1 from kssod_users where username=?1", sUsername) != 0;

} // Exists

//-----------------------------------------------------------------------------
std::size_t KSSOdUserStore::Count()
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);

	KSQLite db(m_sDatabase, KSQLite::Mode::READONLY);
	if (!db.IsOpen()) return 0;

	return static_cast<std::size_t>(db.SingleIntQuery("select count(*) from kssod_users"));

} // Count

//-----------------------------------------------------------------------------
bool KSSOdUserStore::SetAdmin(KStringView sUsername, bool bAdmin)
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);

	KSQLite db(m_sDatabase, KSQLite::Mode::READWRITECREATE);
	if (!db.IsOpen()) return false;

	return static_cast<bool>(db.ExecSQL("update kssod_users set is_admin=?1 where username=?2",
	                                    static_cast<int64_t>(bAdmin ? 1 : 0), sUsername));

} // SetAdmin

//-----------------------------------------------------------------------------
std::size_t KSSOdUserStore::CountAdmins()
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);

	KSQLite db(m_sDatabase, KSQLite::Mode::READONLY);
	if (!db.IsOpen()) return 0;

	return static_cast<std::size_t>(db.SingleIntQuery("select count(*) from kssod_users where is_admin<>0"));

} // CountAdmins

//-----------------------------------------------------------------------------
std::vector<KSSOdUserStore::User> KSSOdUserStore::List()
//-----------------------------------------------------------------------------
{
	std::vector<User> Out;

	std::lock_guard<std::mutex> Lock(m_Mutex);

	KSQLite db(m_sDatabase, KSQLite::Mode::READONLY);
	if (!db.IsOpen()) return Out;

	for (auto& Row : db.ExecQuery("select username, name, email, is_admin from kssod_users order by username asc"))
	{
		User u;
		u.sUsername = Row.Col(1).String();
		u.sName     = Row.Col(2).String();
		u.sEmail    = Row.Col(3).String();
		u.bAdmin    = Row.Col(4).Int64() != 0;
		Out.push_back(std::move(u));
	}
	return Out;

} // List

//-----------------------------------------------------------------------------
bool KSSOdUserStore::HasTotp(KStringView sUsername)
//-----------------------------------------------------------------------------
{
	return !GetTotpSecret(sUsername).empty();

} // HasTotp

//-----------------------------------------------------------------------------
KString KSSOdUserStore::GetTotpSecret(KStringView sUsername)
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);

	KSQLite db(m_sDatabase, KSQLite::Mode::READONLY);
	if (!db.IsOpen()) return KString{};

	return db.SingleStringQuery("select totp_secret from kssod_users where username=?1", sUsername);

} // GetTotpSecret

//-----------------------------------------------------------------------------
bool KSSOdUserStore::SetTotpSecret(KStringView sUsername, KStringView sSecretBase32)
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);

	KSQLite db(m_sDatabase, KSQLite::Mode::READWRITECREATE);
	if (!db.IsOpen()) return false;

	return static_cast<bool>(db.ExecSQL("update kssod_users set totp_secret=?1 where username=?2",
	                                    sSecretBase32, sUsername));

} // SetTotpSecret

//-----------------------------------------------------------------------------
bool KSSOdUserStore::ClearTotp(KStringView sUsername)
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);

	KSQLite db(m_sDatabase, KSQLite::Mode::READWRITECREATE);
	if (!db.IsOpen()) return false;

	// disabling 2FA drops the secret AND the now-useless backup codes
	for (KStringView sSQL : { "update kssod_users set totp_secret='' where username=?1",
	                          "delete from kssod_backup_codes        where username=?1" })
	{
		db.ExecSQL(sSQL, sUsername);
	}
	return true;

} // ClearTotp

//-----------------------------------------------------------------------------
bool KSSOdUserStore::SetBackupCodes(KStringView sUsername, const std::vector<KString>& Hashes)
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);

	KSQLite db(m_sDatabase, KSQLite::Mode::READWRITECREATE);
	if (!db.IsOpen()) return false;

	// replace the set: drop the old codes, then insert the new batch
	db.ExecSQL("delete from kssod_backup_codes where username=?1", sUsername);
	for (const auto& sHash : Hashes)
	{
		db.ExecSQL("insert into kssod_backup_codes (username, code_hash) values (?1, ?2)", sUsername, sHash);
	}
	return true;

} // SetBackupCodes

//-----------------------------------------------------------------------------
bool KSSOdUserStore::ConsumeBackupCode(KStringView sUsername, KStringView sCodeHash)
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);

	KSQLite db(m_sDatabase, KSQLite::Mode::READWRITECREATE);
	if (!db.IsOpen()) return false;

	// must exist before we delete it: a code is valid exactly once
	if (db.SingleIntQuery("select 1 from kssod_backup_codes where username=?1 and code_hash=?2",
	                      sUsername, sCodeHash) == 0)
	{
		return false;
	}
	db.ExecSQL("delete from kssod_backup_codes where username=?1 and code_hash=?2", sUsername, sCodeHash);
	return true;

} // ConsumeBackupCode

//-----------------------------------------------------------------------------
std::size_t KSSOdUserStore::CountBackupCodes(KStringView sUsername)
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);

	KSQLite db(m_sDatabase, KSQLite::Mode::READONLY);
	if (!db.IsOpen()) return 0;

	return static_cast<std::size_t>(db.SingleIntQuery("select count(*) from kssod_backup_codes where username=?1", sUsername));

} // CountBackupCodes

//-----------------------------------------------------------------------------
KString KSSOdUserStore::GetEmail(KStringView sUsername)
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);

	KSQLite db(m_sDatabase, KSQLite::Mode::READONLY);
	if (!db.IsOpen()) return KString{};

	return db.SingleStringQuery("select email from kssod_users where username=?1", sUsername);

} // GetEmail

//-----------------------------------------------------------------------------
bool KSSOdUserStore::SetEmail(KStringView sUsername, KStringView sEmail)
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);

	KSQLite db(m_sDatabase, KSQLite::Mode::READWRITECREATE);
	if (!db.IsOpen()) return false;

	// a changed address is unconfirmed, and email-as-2FA depended on the old one —
	// reset both so the new address must be re-verified before email features apply
	return static_cast<bool>(db.ExecSQL("update kssod_users set email=?1, email_verified=0, email_otp=0 where username=?2",
	                                    sEmail, sUsername));

} // SetEmail

//-----------------------------------------------------------------------------
KString KSSOdUserStore::GetPendingEmail(KStringView sUsername)
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);

	KSQLite db(m_sDatabase, KSQLite::Mode::READONLY);
	if (!db.IsOpen()) return KString{};

	return db.SingleStringQuery("select pending_email from kssod_users where username=?1", sUsername);

} // GetPendingEmail

//-----------------------------------------------------------------------------
bool KSSOdUserStore::SetPendingEmail(KStringView sUsername, KStringView sEmail)
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);

	KSQLite db(m_sDatabase, KSQLite::Mode::READWRITECREATE);
	if (!db.IsOpen()) return false;

	return static_cast<bool>(db.ExecSQL("update kssod_users set pending_email=?1 where username=?2",
	                                    sEmail, sUsername));

} // SetPendingEmail

//-----------------------------------------------------------------------------
bool KSSOdUserStore::ApplyPendingEmail(KStringView sUsername)
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);

	KSQLite db(m_sDatabase, KSQLite::Mode::READWRITECREATE);
	if (!db.IsOpen()) return false;

	// swap the pending address in and mark it verified. email_otp survives: the very
	// link that triggers this proves control of the new address, so email-2FA may
	// keep targeting it. No-op if there is no pending change.
	return static_cast<bool>(db.ExecSQL("update kssod_users set email=pending_email, pending_email='', email_verified=1 "
	                                    "where username=?1 and pending_email<>''", sUsername));

} // ApplyPendingEmail

//-----------------------------------------------------------------------------
bool KSSOdUserStore::RestoreEmail(KStringView sUsername, KStringView sEmail)
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);

	KSQLite db(m_sDatabase, KSQLite::Mode::READWRITECREATE);
	if (!db.IsOpen()) return false;

	// revert: restore the prior (verified) address, drop any pending change, and turn
	// email-2FA off — the attacker controlled the new mailbox in the interim
	return static_cast<bool>(db.ExecSQL("update kssod_users set email=?1, email_verified=1, pending_email='', email_otp=0 "
	                                    "where username=?2", sEmail, sUsername));

} // RestoreEmail

//-----------------------------------------------------------------------------
KString KSSOdUserStore::FindByEmail(KStringView sEmail)
//-----------------------------------------------------------------------------
{
	if (sEmail.empty()) return KString{}; // never match users without an address

	std::lock_guard<std::mutex> Lock(m_Mutex);

	KSQLite db(m_sDatabase, KSQLite::Mode::READONLY);
	if (!db.IsOpen()) return KString{};

	// case-insensitive: addresses are compared without regard to letter case
	return db.SingleStringQuery("select username from kssod_users where lower(email)=lower(?1)", sEmail);

} // FindByEmail

//-----------------------------------------------------------------------------
bool KSSOdUserStore::IsEmailVerified(KStringView sUsername)
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);

	KSQLite db(m_sDatabase, KSQLite::Mode::READONLY);
	if (!db.IsOpen()) return false;

	return db.SingleIntQuery("select email_verified from kssod_users where username=?1", sUsername) != 0;

} // IsEmailVerified

//-----------------------------------------------------------------------------
bool KSSOdUserStore::SetEmailVerified(KStringView sUsername, bool bVerified)
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);

	KSQLite db(m_sDatabase, KSQLite::Mode::READWRITECREATE);
	if (!db.IsOpen()) return false;

	return static_cast<bool>(db.ExecSQL("update kssod_users set email_verified=?1 where username=?2",
	                                    static_cast<int64_t>(bVerified ? 1 : 0), sUsername));

} // SetEmailVerified

//-----------------------------------------------------------------------------
bool KSSOdUserStore::HasEmailOtp(KStringView sUsername)
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);

	KSQLite db(m_sDatabase, KSQLite::Mode::READONLY);
	if (!db.IsOpen()) return false;

	return db.SingleIntQuery("select email_otp from kssod_users where username=?1", sUsername) != 0;

} // HasEmailOtp

//-----------------------------------------------------------------------------
bool KSSOdUserStore::SetEmailOtp(KStringView sUsername, bool bEnabled)
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);

	KSQLite db(m_sDatabase, KSQLite::Mode::READWRITECREATE);
	if (!db.IsOpen()) return false;

	return static_cast<bool>(db.ExecSQL("update kssod_users set email_otp=?1 where username=?2",
	                                    static_cast<int64_t>(bEnabled ? 1 : 0), sUsername));

} // SetEmailOtp

//-----------------------------------------------------------------------------
KString KSSOdUserStore::CreateEmailToken(KStringView sUsername, KStringView sPurpose, KStringView sData)
//-----------------------------------------------------------------------------
{
	// recovery links are short-lived; verification links may sit in an inbox longer;
	// a revert link (the security net) gets the longest window.
	// we do not use the std::chrono namespace here because pre-C++17 did not
	// know std::chrono::days() - but dekaf2::chrono does.
	KDuration iTTL = (sPurpose == "recovery") ? chrono::hours(1)
	               : (sPurpose == "revert")   ? chrono::days(7)
	               :                            chrono::days(1);

	// the plaintext goes only into the emailed link; we keep just its hash
	KString sToken = KSHA256(kGetRandom(32)).HexDigest();
	KString sHash  = KSHA256(sToken).HexDigest();

	std::lock_guard<std::mutex> Lock(m_Mutex);

	KSQLite db(m_sDatabase, KSQLite::Mode::READWRITECREATE);
	if (!db.IsOpen()) return KString{};

	// at most one live token per user+purpose: drop any earlier one first
	db.ExecSQL("delete from kssod_email_tokens where username=?1 and purpose=?2", sUsername, sPurpose);

	return db.ExecSQL("insert into kssod_email_tokens (token_hash, username, purpose, data, expires_utc) "
	                  "values (?1, ?2, ?3, ?4, ?5)",
	                  sHash, sUsername, sPurpose, sData,
	                  static_cast<int64_t>((KUnixTime::now() + iTTL).to_time_t())) ? sToken : KString{};

} // CreateEmailToken

//-----------------------------------------------------------------------------
KString KSSOdUserStore::ConsumeEmailToken(KStringView sToken, KStringView sPurpose, KString* pData)
//-----------------------------------------------------------------------------
{
	if (sToken.empty()) return KString{};

	KString sHash = KSHA256(sToken).HexDigest();

	std::lock_guard<std::mutex> Lock(m_Mutex);

	KSQLite db(m_sDatabase, KSQLite::Mode::READWRITECREATE);
	if (!db.IsOpen()) return KString{};

	KString sUsername;
	KString sData;
	int64_t iExpires = 0;
	{
		auto Query = db.ExecQuery("select username, data, expires_utc from kssod_email_tokens "
		                          "where token_hash=?1 and purpose=?2", sHash, sPurpose);
		if (!Query.Next()) return KString{};
		auto& Row = Query.GetRow();
		sUsername = Row.Col(1).String();
		sData     = Row.Col(2).String();
		iExpires  = Row.Col(3).Int64();
	}

	// single-use: the row is gone whether or not it had expired
	db.ExecSQL("delete from kssod_email_tokens where token_hash=?1", sHash);

	if (KUnixTime::now().to_time_t() > iExpires) return KString{}; // expired
	if (pData) *pData = std::move(sData);
	return sUsername;

} // ConsumeEmailToken

//-----------------------------------------------------------------------------
bool KSSOdUserStore::AuthorizeClientAccess(KStringView sUsername, KStringView sClientID, KJSON& jClientClaims)
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);

	KSQLite db(m_sDatabase, KSQLite::Mode::READONLY);
	if (!db.IsOpen()) return false;

	// does this client require explicit assignment?
	bool bRequireAssignment = db.SingleIntQuery("select require_assignment from kssod_clients where client_id=?1",
	                                            sClientID) != 0;

	// is there an ACTIVE assignment row (access=1), and what roles? A suspended
	// row (access=0) keeps its roles but counts as no assignment.
	bool    bActive = false;
	KString sRoles;
	{
		auto Query = db.ExecQuery("select roles, access from kssod_assignments where username=?1 and client_id=?2",
		                          sUsername, sClientID);
		if (Query.Next())
		{
			auto& Row = Query.GetRow();
			if (Row.Col(2).Int64() != 0) { bActive = true; sRoles = Row.Col(1).String(); }
		}
	}

	if (bRequireAssignment && !bActive)
	{
		return false; // restricted client, no active assignment -> deny
	}

	// allowed; expose roles ONLY for an active assignment (a suspended user, or a
	// user with no row on an open client, gets access but no roles in the token)
	std::vector<KString> Roles = sRoles.Split<std::vector<KString>>(' ');
	if (!Roles.empty())
	{
		KJSON jRoles = KJSON::array();
		for (const auto& sRole : Roles) jRoles.push_back(sRole);
		jClientClaims = { { "roles", std::move(jRoles) } };
	}
	return true;

} // AuthorizeClientAccess

//-----------------------------------------------------------------------------
std::vector<KString> KSSOdUserStore::ListRoles(KStringView sClientID)
//-----------------------------------------------------------------------------
{
	std::vector<KString> Out;

	std::lock_guard<std::mutex> Lock(m_Mutex);

	KSQLite db(m_sDatabase, KSQLite::Mode::READONLY);
	if (!db.IsOpen()) return Out;

	for (auto& Row : db.ExecQuery("select role from kssod_roles where client_id=?1 order by role asc", sClientID))
	{
		Out.push_back(Row.Col(1).String());
	}
	return Out;

} // ListRoles

//-----------------------------------------------------------------------------
bool KSSOdUserStore::AddRole(KStringView sClientID, KStringView sRole)
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);

	KSQLite db(m_sDatabase, KSQLite::Mode::READWRITECREATE);
	if (!db.IsOpen()) return false;

	return static_cast<bool>(db.ExecSQL("insert or ignore into kssod_roles (client_id, role) values (?1, ?2)",
	                                    sClientID, sRole));

} // AddRole

//-----------------------------------------------------------------------------
bool KSSOdUserStore::DeleteRole(KStringView sClientID, KStringView sRole)
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);

	KSQLite db(m_sDatabase, KSQLite::Mode::READWRITECREATE);
	if (!db.IsOpen()) return false;

	// 1) collect the assignments that grant this role (read fully before writing)
	std::vector<std::pair<KString, KString>> Rewrites; // username -> new role string
	{
		for (auto& Row : db.ExecQuery("select username, roles from kssod_assignments where client_id=?1", sClientID))
		{
			KString sUser = Row.Col(1).String();
			std::vector<KString> Roles = Row.Col(2).String().Split<std::vector<KString>>(' ');
			std::vector<KString> Kept;
			for (const auto& r : Roles) { if (r != sRole) Kept.push_back(r); }
			if (Kept.size() != Roles.size())
			{
				KString sKept; sKept.Join(Kept, " ");
				Rewrites.emplace_back(std::move(sUser), std::move(sKept));
			}
		}
	}

	// 2) strip the role from those assignments
	for (const auto& Rewrite : Rewrites)
	{
		db.ExecSQL("update kssod_assignments set roles=?1 where username=?2 and client_id=?3",
		           Rewrite.second, Rewrite.first, sClientID);
	}

	// 3) remove from the catalog
	return static_cast<bool>(db.ExecSQL("delete from kssod_roles where client_id=?1 and role=?2", sClientID, sRole));

} // DeleteRole

//-----------------------------------------------------------------------------
bool KSSOdUserStore::GrantRoles(KStringView sUsername, KStringView sClientID, const std::vector<KString>& Roles)
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);

	KSQLite db(m_sDatabase, KSQLite::Mode::READWRITECREATE);
	if (!db.IsOpen()) return false;

	// read the user's current roles for this client (if any)
	std::vector<KString> Merged;
	{
		auto Query = db.ExecQuery("select roles from kssod_assignments where username=?1 and client_id=?2",
		                          sUsername, sClientID);
		if (Query.Next()) Merged = Query.GetRow().Col(1).String().Split<std::vector<KString>>(' ');
	}

	// union in the newly granted roles
	for (const auto& sRole : Roles)
	{
		if (std::find(Merged.begin(), Merged.end(), sRole) == Merged.end()) Merged.push_back(sRole);
	}

	// access without any explicit role gets the client's default role, if set
	if (Merged.empty())
	{
		KString sDefault = DefaultRoleOn(db, sClientID);
		if (!sDefault.empty()) Merged.push_back(std::move(sDefault));
	}

	KString sRoles; sRoles.Join(Merged, " ");
	// granting always (re)activates access
	return static_cast<bool>(db.ExecSQL("insert or replace into kssod_assignments (username, client_id, roles, access) values (?1, ?2, ?3, 1)",
	                                    sUsername, sClientID, sRoles));

} // GrantRoles

//-----------------------------------------------------------------------------
bool KSSOdUserStore::Upsert(KStringView sUsername, KStringView sClientID, bool bAccess, const std::vector<KString>& Roles)
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);

	KSQLite db(m_sDatabase, KSQLite::Mode::READWRITECREATE);
	if (!db.IsOpen()) return false;

	// no access AND no roles -> there is nothing left to remember, so drop the
	// association entirely (symmetric to granting access, which creates it).
	// A SUSPENDED row is only kept when it still carries roles to restore later.
	if (!bAccess && Roles.empty())
	{
		return static_cast<bool>(db.ExecSQL("delete from kssod_assignments where username=?1 and client_id=?2",
		                                    sUsername, sClientID));
	}

	std::vector<KString> Final = Roles;
	// only an ACTIVE membership with no explicit role gets the default role
	if (bAccess && Final.empty())
	{
		KString sDefault = DefaultRoleOn(db, sClientID);
		if (!sDefault.empty()) Final.push_back(std::move(sDefault));
	}

	KString sRoles; sRoles.Join(Final, " ");
	return static_cast<bool>(db.ExecSQL("insert or replace into kssod_assignments (username, client_id, roles, access) values (?1, ?2, ?3, ?4)",
	                                    sUsername, sClientID, sRoles, static_cast<int64_t>(bAccess ? 1 : 0)));

} // Upsert

//-----------------------------------------------------------------------------
KString KSSOdUserStore::DefaultRole(KStringView sClientID)
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);

	KSQLite db(m_sDatabase, KSQLite::Mode::READONLY);
	if (!db.IsOpen()) return {};

	return DefaultRoleOn(db, sClientID);

} // DefaultRole

//-----------------------------------------------------------------------------
bool KSSOdUserStore::SetDefaultRole(KStringView sClientID, KStringView sRole)
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);

	KSQLite db(m_sDatabase, KSQLite::Mode::READWRITECREATE);
	if (!db.IsOpen()) return false;

	// clear the current default for this client...
	db.ExecSQL("update kssod_roles set is_default=0 where client_id=?1", sClientID);

	// ...and set the new one (empty sRole just clears it)
	if (!sRole.empty())
	{
		return static_cast<bool>(db.ExecSQL("update kssod_roles set is_default=1 where client_id=?1 and role=?2",
		                                    sClientID, sRole));
	}
	return true;

} // SetDefaultRole

//-----------------------------------------------------------------------------
std::vector<KSSOdUserStore::GlobalAssignment> KSSOdUserStore::ListAllAssignments()
//-----------------------------------------------------------------------------
{
	std::vector<GlobalAssignment> Out;

	std::lock_guard<std::mutex> Lock(m_Mutex);

	KSQLite db(m_sDatabase, KSQLite::Mode::READONLY);
	if (!db.IsOpen()) return Out;

	for (auto& Row : db.ExecQuery("select username, client_id, roles, access from kssod_assignments"))
	{
		GlobalAssignment a;
		a.sUsername = Row.Col(1).String();
		a.sClientID = Row.Col(2).String();
		a.sRoles    = Row.Col(3).String();
		a.bAccess   = Row.Col(4).Int64() != 0;
		Out.push_back(std::move(a));
	}
	return Out;

} // ListAllAssignments

//-----------------------------------------------------------------------------
std::vector<KSSOdUserStore::Assignment> KSSOdUserStore::ListAssignments(KStringView sClientID)
//-----------------------------------------------------------------------------
{
	std::vector<Assignment> Out;

	std::lock_guard<std::mutex> Lock(m_Mutex);

	KSQLite db(m_sDatabase, KSQLite::Mode::READONLY);
	if (!db.IsOpen()) return Out;

	for (auto& Row : db.ExecQuery("select username, roles, access from kssod_assignments where client_id=?1 order by username asc",
	                              sClientID))
	{
		Assignment a;
		a.sUsername = Row.Col(1).String();
		a.sRoles    = Row.Col(2).String();
		a.bAccess   = Row.Col(3).Int64() != 0;
		Out.push_back(std::move(a));
	}
	return Out;

} // ListAssignments

// =============================================================================
// KSSOdClientStore
// =============================================================================

//-----------------------------------------------------------------------------
bool KSSOdClientStore::Lookup(KStringView sClientID, Client& Out)
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);

	KSQLite db(m_sDatabase, KSQLite::Mode::READONLY);
	if (!db.IsOpen()) return false;

	auto Query = db.ExecQuery(
		"select client_id, secret_hash, redirect_uris, postlogout_uris, scopes, is_public, force_login, max_auth_age "
		"from kssod_clients where client_id=?1", sClientID);
	if (!Query.Next()) return false;

	auto& Row = Query.GetRow();
	Out.sClientID              = Row.Col(1).String();
	Out.sClientSecretHash      = Row.Col(2).String();
	Out.RedirectURIs           = Row.Col(3).String().Split<std::vector<KString>>('\n');
	Out.PostLogoutRedirectURIs = Row.Col(4).String().Split<std::vector<KString>>('\n');
	Out.Scopes                 = Row.Col(5).String().Split<std::vector<KString>>('\n');
	Out.bPublic                = Row.Col(6).Int64() != 0;
	Out.bForceLogin            = Row.Col(7).Int64() != 0;
	Out.MaxAuthAge             = KDuration(chrono::seconds(Row.Col(8).Int64())); // stored as seconds
	return true;

} // Lookup

//-----------------------------------------------------------------------------
bool KSSOdClientStore::AddClient(const Client& Client, bool bRequireAssignment)
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);

	KSQLite db(m_sDatabase, KSQLite::Mode::READWRITECREATE);
	if (!db.IsOpen()) return false;

	KString sRedirect;   sRedirect.Join(Client.RedirectURIs, "\n");
	KString sPostLogout; sPostLogout.Join(Client.PostLogoutRedirectURIs, "\n");
	KString sScopes;     sScopes.Join(Client.Scopes, "\n");

	return static_cast<bool>(db.ExecSQL(
		"insert into kssod_clients (client_id, secret_hash, redirect_uris, postlogout_uris, scopes, is_public, require_assignment, force_login, max_auth_age, created_utc) "
		"values (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10)",
		Client.sClientID,
		Client.sClientSecretHash,
		sRedirect,
		sPostLogout,
		sScopes,
		static_cast<int64_t>(Client.bPublic ? 1 : 0),
		static_cast<int64_t>(bRequireAssignment ? 1 : 0),
		static_cast<int64_t>(Client.bForceLogin ? 1 : 0),
		static_cast<int64_t>(Client.MaxAuthAge.seconds().count()), // KDuration -> seconds at the SQLite boundary
		static_cast<int64_t>(KUnixTime::now().to_time_t())));

} // AddClient

//-----------------------------------------------------------------------------
bool KSSOdClientStore::UpdateClient(const Client& Client, bool bRequireAssignment, bool bUpdateSecret)
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);

	KSQLite db(m_sDatabase, KSQLite::Mode::READWRITECREATE);
	if (!db.IsOpen()) return false;

	KString sRedirect;   sRedirect.Join(Client.RedirectURIs, "\n");
	KString sPostLogout; sPostLogout.Join(Client.PostLogoutRedirectURIs, "\n");
	KString sScopes;     sScopes.Join(Client.Scopes, "\n");

	// two variants: keep the existing secret, or replace it. client_id is the key.
	if (bUpdateSecret)
	{
		return static_cast<bool>(db.ExecSQL(
			"update kssod_clients set secret_hash=?2, redirect_uris=?3, postlogout_uris=?4, "
			"scopes=?5, is_public=?6, require_assignment=?7, force_login=?8, max_auth_age=?9 where client_id=?1",
			Client.sClientID,
			Client.sClientSecretHash,
			sRedirect,
			sPostLogout,
			sScopes,
			static_cast<int64_t>(Client.bPublic ? 1 : 0),
			static_cast<int64_t>(bRequireAssignment ? 1 : 0),
			static_cast<int64_t>(Client.bForceLogin ? 1 : 0),
			static_cast<int64_t>(Client.MaxAuthAge.seconds().count())));
	}

	return static_cast<bool>(db.ExecSQL(
		"update kssod_clients set redirect_uris=?2, postlogout_uris=?3, scopes=?4, "
		"is_public=?5, require_assignment=?6, force_login=?7, max_auth_age=?8 where client_id=?1",
		Client.sClientID,
		sRedirect,
		sPostLogout,
		sScopes,
		static_cast<int64_t>(Client.bPublic ? 1 : 0),
		static_cast<int64_t>(bRequireAssignment ? 1 : 0),
		static_cast<int64_t>(Client.bForceLogin ? 1 : 0),
		static_cast<int64_t>(Client.MaxAuthAge.seconds().count())));

} // UpdateClient

//-----------------------------------------------------------------------------
bool KSSOdClientStore::LookupInfo(KStringView sClientID, ClientInfo& Out)
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);

	KSQLite db(m_sDatabase, KSQLite::Mode::READONLY);
	if (!db.IsOpen()) return false;

	auto Query = db.ExecQuery(
		"select client_id, secret_hash, redirect_uris, postlogout_uris, scopes, is_public, require_assignment, force_login, max_auth_age "
		"from kssod_clients where client_id=?1", sClientID);
	if (!Query.Next()) return false;

	auto& Row = Query.GetRow();
	Out.Client.sClientID              = Row.Col(1).String();
	Out.Client.sClientSecretHash      = Row.Col(2).String();
	Out.Client.RedirectURIs           = Row.Col(3).String().Split<std::vector<KString>>('\n');
	Out.Client.PostLogoutRedirectURIs = Row.Col(4).String().Split<std::vector<KString>>('\n');
	Out.Client.Scopes                 = Row.Col(5).String().Split<std::vector<KString>>('\n');
	Out.Client.bPublic                = Row.Col(6).Int64() != 0;
	Out.bRequireAssignment            = Row.Col(7).Int64() != 0;
	Out.Client.bForceLogin            = Row.Col(8).Int64() != 0;
	Out.Client.MaxAuthAge             = KDuration(chrono::seconds(Row.Col(9).Int64()));
	return true;

} // LookupInfo

//-----------------------------------------------------------------------------
bool KSSOdClientStore::DeleteClient(KStringView sClientID)
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);

	KSQLite db(m_sDatabase, KSQLite::Mode::READWRITECREATE);
	if (!db.IsOpen()) return false;

	// cascade: deleting an app removes everything tied to it - its role catalog
	// and every user assignment to it - so no orphaned rows are left behind
	for (KStringView sSQL : { "delete from kssod_assignments where client_id=?1",
	                          "delete from kssod_roles       where client_id=?1",
	                          "delete from kssod_clients     where client_id=?1" })
	{
		db.ExecSQL(sSQL, sClientID);
	}
	return true;

} // DeleteClient

//-----------------------------------------------------------------------------
bool KSSOdClientStore::Exists(KStringView sClientID)
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);

	KSQLite db(m_sDatabase, KSQLite::Mode::READONLY);
	if (!db.IsOpen()) return false;

	return db.SingleIntQuery("select 1 from kssod_clients where client_id=?1", sClientID) != 0;

} // Exists

//-----------------------------------------------------------------------------
std::size_t KSSOdClientStore::Count()
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);

	KSQLite db(m_sDatabase, KSQLite::Mode::READONLY);
	if (!db.IsOpen()) return 0;

	return static_cast<std::size_t>(db.SingleIntQuery("select count(*) from kssod_clients"));

} // Count

//-----------------------------------------------------------------------------
std::vector<KSSOdClientStore::ClientInfo> KSSOdClientStore::List()
//-----------------------------------------------------------------------------
{
	std::vector<ClientInfo> Out;

	std::lock_guard<std::mutex> Lock(m_Mutex);

	KSQLite db(m_sDatabase, KSQLite::Mode::READONLY);
	if (!db.IsOpen()) return Out;

	for (auto& Row : db.ExecQuery(
		"select client_id, secret_hash, redirect_uris, postlogout_uris, scopes, is_public, require_assignment "
		"from kssod_clients order by client_id asc"))
	{
		ClientInfo Info;
		Info.Client.sClientID              = Row.Col(1).String();
		Info.Client.sClientSecretHash      = Row.Col(2).String();
		Info.Client.RedirectURIs           = Row.Col(3).String().Split<std::vector<KString>>('\n');
		Info.Client.PostLogoutRedirectURIs = Row.Col(4).String().Split<std::vector<KString>>('\n');
		Info.Client.Scopes                 = Row.Col(5).String().Split<std::vector<KString>>('\n');
		Info.Client.bPublic                = Row.Col(6).Int64() != 0;
		Info.bRequireAssignment            = Row.Col(7).Int64() != 0;
		Out.push_back(std::move(Info));
	}
	return Out;

} // List

// =============================================================================
//  KSSOdSettingsStore
// =============================================================================

//-----------------------------------------------------------------------------
KString KSSOdSettingsStore::Get(KStringView sKey)
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);

	KSQLite db(m_sDatabase, KSQLite::Mode::READONLY);
	if (!db.IsOpen()) return KString{};

	return db.SingleStringQuery("select value from kssod_settings where key=?1", sKey);

} // Get

//-----------------------------------------------------------------------------
bool KSSOdSettingsStore::Set(KStringView sKey, KStringView sValue)
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);

	KSQLite db(m_sDatabase, KSQLite::Mode::READWRITECREATE);
	if (!db.IsOpen()) return false;

	return static_cast<bool>(db.ExecSQL("insert or replace into kssod_settings (key, value) values (?1, ?2)",
	                                    sKey, sValue));

} // Set

//-----------------------------------------------------------------------------
KSSOdSettingsStore::Smtp KSSOdSettingsStore::LoadSmtp()
//-----------------------------------------------------------------------------
{
	Smtp Config;
	Config.sURL      = Get("smtp_url");
	Config.sUser     = Get("smtp_user");
	Config.sPass     = Get("smtp_pass");
	Config.sFrom     = Get("smtp_from");
	Config.sFromName = Get("smtp_fromname");
	return Config;

} // LoadSmtp

//-----------------------------------------------------------------------------
bool KSSOdSettingsStore::SaveSmtp(const Smtp& Config)
//-----------------------------------------------------------------------------
{
	return Set("smtp_url",      Config.sURL)
	    && Set("smtp_user",     Config.sUser)
	    && Set("smtp_pass",     Config.sPass)
	    && Set("smtp_from",     Config.sFrom)
	    && Set("smtp_fromname", Config.sFromName);

} // SaveSmtp
