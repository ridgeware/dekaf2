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

// kssod_store.h
//
// SQLite-backed persistence for the kssod sample OpenID Connect provider:
//   * KSSOdUserStore   — implements KOpenIDServer::UserStore   + user management
//   * KSSOdClientStore — implements KOpenIDServer::ClientStore + client management
//
// Both share one SQLite database file (the same file the KSessionSQLiteStore
// uses for the OP login sessions — just different tables). Each operation opens
// a short-lived KSQLite handle (the ktunnel_store pattern); a mutex
// serializes writers, WAL mode lets readers run concurrently.

#pragma once

#include <dekaf2/crypto/auth/kopenidserver.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/strings/kstringview.h>
#include <dekaf2/data/json/kjson.h>
#include <mutex>
#include <vector>

using namespace dekaf2;

/// Create the kssod schema (users + clients tables) and set WAL pragmas.
/// @returns false and fills sError on failure.
bool KSSOdInitDatabase(KString sDatabase, KString& sError);

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// SQLite user directory. Beyond the OIDC UserStore interface (VerifyPassword +
/// GetClaims) it offers the management operations the provider's own UI needs.
class KSSOdUserStore : public KOpenIDServer::UserStore
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
public:
	struct User
	{
		KString sUsername;
		KString sName;
		KString sEmail;
		bool    bAdmin { false };
	};

	/// a user's per-client role assignment
	struct Assignment
	{
		KString sUsername;
		KString sRoles;          ///< space-separated role names
		bool    bAccess { true }; ///< false = suspended: roles are kept but not active
	};

	explicit KSSOdUserStore(KString sDatabase) : m_sDatabase(std::move(sDatabase)) {}

	// --- KOpenIDServer::UserStore ---
	bool VerifyPassword(KStringView sUsername, KStringView sPassword) override;
	bool GetClaims     (KStringView sUsername, KJSON& Claims)         override;
	/// per-client access control + RBAC: deny when the client requires explicit
	/// assignment and the user has none; otherwise allow and return any assigned
	/// roles as { "roles": [...] }.
	bool AuthorizeClientAccess(KStringView sUsername, KStringView sClientID, KJSON& jClientClaims) override;

	// --- user management ---
	bool        AddUser       (KStringView sUsername, KStringView sPassword,
	                           KStringView sName, KStringView sEmail, bool bAdmin);
	bool        ChangePassword(KStringView sUsername, KStringView sNewPassword);
	bool        SetName       (KStringView sUsername, KStringView sName); ///< update the display name
	bool        DeleteUser    (KStringView sUsername);
	bool        IsAdmin       (KStringView sUsername);
	/// promote/demote: set the administrator flag on an existing user. The caller
	/// is responsible for the policy guards (don't demote yourself / the last admin).
	bool        SetAdmin      (KStringView sUsername, bool bAdmin);
	bool        Exists        (KStringView sUsername);
	std::size_t Count         ();
	/// number of administrators — for the "don't remove the last admin" guard
	std::size_t CountAdmins   ();
	std::vector<User> List    ();

	// --- two-factor auth (TOTP) ---
	bool        HasTotp       (KStringView sUsername);             ///< 2FA enrolled?
	KString     GetTotpSecret (KStringView sUsername);             ///< base32 secret, empty if none
	bool        SetTotpSecret (KStringView sUsername, KStringView sSecretBase32); ///< enroll/enable
	bool        ClearTotp     (KStringView sUsername);             ///< disable (also drops backup codes)
	// backup/recovery codes: stored hashed, single-use
	bool        SetBackupCodes(KStringView sUsername, const std::vector<KString>& Hashes); ///< replace the set
	bool        ConsumeBackupCode(KStringView sUsername, KStringView sCodeHash);           ///< verify+delete one
	std::size_t CountBackupCodes (KStringView sUsername);

	// --- email (address, verification, email-OTP-as-2FA) ---
	KString     GetEmail        (KStringView sUsername);                ///< the user's address ('' if none)
	/// change a user's email address. Resets email_verified (a new address is
	/// unconfirmed) and turns off email-as-2FA (its channel changed) — so the new
	/// address must be re-verified before email features apply to it again.
	bool        SetEmail        (KStringView sUsername, KStringView sEmail);
	// --- pending email change (two-phase: the new address is only swapped in once
	//     it is verified, so the old one stays the active recovery anchor meanwhile) ---
	KString     GetPendingEmail (KStringView sUsername);                ///< the requested-but-unconfirmed new address ('' if none)
	bool        SetPendingEmail (KStringView sUsername, KStringView sEmail); ///< stage a change; pass '' to cancel
	bool        ApplyPendingEmail(KStringView sUsername);               ///< swap pending in: email=pending, pending='', verified=1
	bool        RestoreEmail    (KStringView sUsername, KStringView sEmail); ///< revert: email=sEmail, verified=1, pending='' (resets email_otp)
	KString     FindByEmail     (KStringView sEmail);                   ///< username for an address, or '' (for recovery)
	bool        IsEmailVerified (KStringView sUsername);
	bool        SetEmailVerified(KStringView sUsername, bool bVerified);
	bool        HasEmailOtp     (KStringView sUsername);                ///< email used as the second factor?
	bool        SetEmailOtp     (KStringView sUsername, bool bEnabled);
	/// mint a single-use email token (verify/recovery/revert), store its hash with a
	/// purpose-dependent expiry plus an optional payload, and return the plaintext
	/// for the emailed link. The payload carries purpose data (e.g. the prior email
	/// address for a 'revert' token, so a completed change can be undone).
	KString     CreateEmailToken (KStringView sUsername, KStringView sPurpose, KStringView sData = {});
	/// validate+consume a token: returns the username on success (and deletes the
	/// row), or '' if unknown/expired/wrong purpose. If pData is non-null it receives
	/// the stored payload.
	KString     ConsumeEmailToken(KStringView sToken, KStringView sPurpose, KString* pData = nullptr);

	// --- role catalog (the set of roles defined for a client) ---
	std::vector<KString> ListRoles (KStringView sClientID);
	bool        AddRole    (KStringView sClientID, KStringView sRole);
	/// remove a role from the catalog AND strip it from every assignment of this
	/// client (so deleting a role revokes it everywhere it was granted)
	bool        DeleteRole (KStringView sClientID, KStringView sRole);
	/// the client's default role (auto-granted when access is given without an
	/// explicit role), or empty if none is set
	KString     DefaultRole(KStringView sClientID);
	/// mark sRole as the client's single default; an empty sRole clears it
	bool        SetDefaultRole(KStringView sClientID, KStringView sRole);

	// --- assignment management (user <-> client + roles) ---
	/// grant (union) the given roles to the user's assignment for this client and
	/// (re)activate access, creating the membership if it does not exist yet. An
	/// empty role set still grants access and receives the client's default role.
	bool        GrantRoles (KStringView sUsername, KStringView sClientID, const std::vector<KString>& Roles);
	/// the per-user grid editor: set both the access flag and the role set in one
	/// go. Suspending (bAccess=false) KEEPS the roles so re-granting access later
	/// restores them. An active+empty set applies the default role. A no-op call
	/// (no access, no roles, no existing row) does not create an empty row.
	bool        Upsert     (KStringView sUsername, KStringView sClientID, bool bAccess, const std::vector<KString>& Roles);
	std::vector<Assignment> ListAssignments(KStringView sClientID);

	/// every assignment across all clients (for the global users x clients overview)
	struct GlobalAssignment { KString sUsername; KString sClientID; KString sRoles; bool bAccess { true }; };
	std::vector<GlobalAssignment> ListAllAssignments();

private:
	KString    m_sDatabase;
	std::mutex m_Mutex;

}; // KSSOdUserStore

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// SQLite relying-party registry. The list-valued columns (redirect URIs,
/// post-logout URIs, scopes) are stored newline-separated.
class KSSOdClientStore : public KOpenIDServer::ClientStore
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
public:
	/// a registered client plus the kssod-specific access policy (the library's
	/// Client struct intentionally does not carry the require-assignment flag)
	struct ClientInfo
	{
		// the type is spelled fully qualified on purpose
		KOpenIDServer::ClientStore::Client Client;
		bool                               bRequireAssignment { false };
	};

	explicit KSSOdClientStore(KString sDatabase) : m_sDatabase(std::move(sDatabase)) {}

	// --- KOpenIDServer::ClientStore ---
	bool Lookup(KStringView sClientID, Client& Out) override;

	// --- management ---
	bool        AddClient   (const Client& Client, bool bRequireAssignment = false);
	/// update an existing client's settings (the client_id is the key and is not
	/// changed). The secret hash is only touched when bUpdateSecret is true - so a
	/// blank secret field on the edit form keeps the current secret.
	bool        UpdateClient(const Client& Client, bool bRequireAssignment, bool bUpdateSecret);
	bool        DeleteClient(KStringView sClientID);
	bool        Exists      (KStringView sClientID);
	/// fetch a single client plus its require-assignment flag, for the edit form
	bool        LookupInfo  (KStringView sClientID, ClientInfo& Out);
	std::size_t Count       ();
	std::vector<ClientInfo> List();

private:
	KString    m_sDatabase;
	std::mutex m_Mutex;

}; // KSSOdClientStore

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// A tiny key/value settings store (one shared table). kssod uses it to hold the
/// optional outgoing-mail configuration: if no relay is set, every email feature
/// (verification, password recovery, email OTP) is simply unavailable.
class KSSOdSettingsStore
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
public:
	/// the outgoing-mail relay; bConfigured() is false until a URL and From are set
	struct Smtp
	{
		KString sURL;       ///< relay URL, e.g. smtp://mail.example.com:587 or smtps://...:465
		KString sUser;
		KString sPass;
		KString sFrom;      ///< envelope/From address
		KString sFromName;  ///< optional display name
		bool    IsConfigured() const { return !sURL.empty() && !sFrom.empty(); }
	};

	explicit KSSOdSettingsStore(KString sDatabase) : m_sDatabase(std::move(sDatabase)) {}

	KString Get(KStringView sKey);
	bool    Set(KStringView sKey, KStringView sValue);

	Smtp    LoadSmtp();
	bool    SaveSmtp(const Smtp& Config);
	bool    SmtpConfigured() { return LoadSmtp().IsConfigured(); }

	/// security policy: when an email-change takeover is reverted and the change had
	/// already completed, force the user to set a new password (the attacker may have
	/// changed it). Default ON — security over convenience; admins can turn it off for
	/// convenience.
	bool    ForcePwOnRevert()          { return Get("revert_force_pw") != "0"; } // absent/'' -> on
	bool    SetForcePwOnRevert(bool b) { return Set("revert_force_pw", b ? "1" : "0"); }

private:
	KString    m_sDatabase;
	std::mutex m_Mutex;

}; // KSSOdSettingsStore
