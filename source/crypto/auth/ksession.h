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

#pragma once

/// @file ksession.h
/// Cookie-based session management with pluggable storage backend.
/// Credential verification is fully delegated to the application via an
/// Authenticator callback, so KSession knows nothing about how passwords
/// are stored or hashed (typically the application pairs this with
/// KBCrypt). KSession itself is responsible only for:
///   - generating cryptographically random session tokens
///   - persisting session records via a pluggable Store
///   - enforcing idle / absolute timeouts
///   - serializing properly-attributed Set-Cookie header values

#include <dekaf2/core/init/kdefinitions.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/strings/kstringview.h>
#include <dekaf2/core/errors/kerror.h>
#include <dekaf2/time/clock/ktime.h>
#include <dekaf2/time/duration/kduration.h>
#include <dekaf2/time/duration/ktimer.h>
#include <dekaf2/threading/primitives/kthreadsafe.h>
#include <functional>
#include <memory>
#include <vector>

DEKAF2_NAMESPACE_BEGIN

class KSQL;

/// @addtogroup crypto_auth
/// @{

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Cookie-session manager with pluggable storage backend.
///
/// @par Supported storage backends
/// - **In-memory** (KSessionMemoryStore)  — volatile, process-local
/// - **KSQLite**   (KSessionSQLiteStore)  — embedded, zero-config
/// - **KSQL**      (KSessionKSQLStore)    — any database supported by KSQL
/// - **Custom**    — derive from KSession::Store
///
/// Thread safety: all public methods are thread-safe. The Store
/// implementations provide their own mutex protection.
///
/// Typical usage (ktunnel-style admin UI):
///
/// @code
/// auto bcrypt = std::make_shared<KBCrypt>(chrono::milliseconds(100));
///
/// KSession::Config cfg;
/// cfg.sCookieName     = "__Host-ktunnel";
/// cfg.IdleTimeout     = chrono::minutes(30);
/// cfg.AbsoluteTimeout = chrono::hours(8);
///
/// KSession Session(sDBPath, cfg);
/// Session.SetAuthenticator([bcrypt, &MyUserDB](KStringView sUser, KStringView sPass)
/// {
///     auto sHash = MyUserDB.LookupPasswordHash(sUser);
///     return !sHash.empty() && bcrypt->ValidatePassword(sPass, sHash);
/// });
///
/// // in a route handler:
/// auto oToken = Session.Login("alice", "secret", HTTP.GetRemoteIP());
/// if (oToken) HTTP.Response.Headers.Add(KHTTPHeader::SET_COOKIE,
///                                       Session.SerializeSetCookie(*oToken));
/// @endcode
class DEKAF2_PUBLIC KSession : public KErrorBase
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// One persisted session record. Produced by Login(), consumed by
	/// Validate(). The Extra field is a free-form JSON-string slot for
	/// application-specific session state (role, tenant id, ...).
	struct DEKAF2_PUBLIC Record
	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		KString    sToken;        ///< primary key, base64url-encoded random bytes
		KString    sUsername;     ///< the logged-in user
		KUnixTime  tCreated;      ///< session creation time (UTC)
		KUnixTime  tLastSeen;     ///< last activity (UTC), refreshed by Validate()
		KString    sClientIP;     ///< optional: client IP at login time
		KString    sUserAgent;    ///< optional: user-agent at login time
		KString    sExtra;        ///< application-specific opaque data (JSON recommended)
	};

	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// Abstract storage backend interface. Concrete implementations
	/// (KSessionMemoryStore, KSessionSQLiteStore, KSessionKSQLStore) are
	/// provided in crypto/auth/bits/. Custom backends can be implemented
	/// by deriving from this class and passing an instance to the
	/// KSession constructor.
	class DEKAF2_PUBLIC Store
	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{

	//------
	public:
	//------

		virtual ~Store() = default;

		/// create any tables/state required by this backend
		virtual bool        Initialize   ()                                                = 0;

		/// insert a new session record; returns false on duplicate token or storage error
		virtual bool        Create       (const Record& Rec)                               = 0;

		/// look up a session by token; on success fills *pOut and returns true
		virtual bool        Lookup       (KStringView sToken, Record* pOut)                = 0;

		/// update the LastSeen timestamp for a token
		virtual bool        Touch        (KStringView sToken, KUnixTime tLastSeen)         = 0;

		/// erase a single session
		virtual bool        Erase        (KStringView sToken)                              = 0;

		/// erase all sessions belonging to the given user; returns count erased
		virtual std::size_t EraseAllFor  (KStringView sUsername)                           = 0;

		/// erase all sessions that are either idle-expired or absolute-expired
		/// @param tOldestLastSeen sessions with LastSeen before this are idle-expired
		/// @param tOldestCreated  sessions with Created  before this are absolute-expired
		/// @returns count of erased rows
		virtual std::size_t PurgeExpired (KUnixTime tOldestLastSeen, KUnixTime tOldestCreated) = 0;

		/// @returns total number of live sessions
		virtual std::size_t Count        () const                                          = 0;

		/// @returns last error message (if any)
		virtual KString     GetLastError () const                                          = 0;

	}; // Store

	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// KSession configuration. Defaults are safe for production over TLS.
	/// For plain-HTTP local development: drop the "__Host-" prefix from
	/// sCookieName and set bSecure=false.
	struct DEKAF2_PUBLIC Config
	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		/// cookie name. The "__Host-" prefix is strongly recommended in
		/// production: it forces Secure, Path=/ and forbids a Domain
		/// attribute — which defeats subdomain cookie tossing attacks.
		KString     sCookieName      { "__Host-session" };
		/// cookie Path attribute (must be "/" when using __Host- prefix)
		KString     sCookiePath      { "/"              };
		/// cookie SameSite attribute — "Strict", "Lax" or "None"
		KString     sSameSite        { "Strict"         };
		/// set the Secure attribute — required for HTTPS, required for __Host-
		/// prefix. Leave true even behind a TLS-terminating reverse proxy;
		/// the browser only looks at its address-bar URL.
		bool        bSecure          { true             };
		/// set the HttpOnly attribute — blocks JS access to the cookie
		bool        bHttpOnly        { true             };
		/// sessions idle for longer than this are invalidated on next access
		/// and purged by the background job
		KDuration   IdleTimeout      { chrono::minutes(30) };
		/// absolute maximum session lifetime (counted from login), regardless
		/// of activity — protects against indefinite re-extension by bots
		KDuration   AbsoluteTimeout  { chrono::hours(8)    };
		/// interval of the background purge job. Set to a negative duration
		/// to disable the timer; purge will still run implicitly on Validate()
		KDuration   PurgeInterval    { chrono::minutes(5)  };
		/// raw random bytes used for the token — gets base64url-encoded, so the
		/// resulting string is ~4/3 of this value. 32 bytes = 256 bits of entropy.
		std::size_t iTokenBytes      { 32 };
	};

	/// Authenticator signature: return true if (sUsername, sPassword) is a
	/// valid credential pair. Called by Login() at login time only.
	using Authenticator = std::function<bool(KStringView sUsername, KStringView sPassword)>;

	/// Construct with a custom storage backend.
	/// @param Store  storage backend (ownership transferred)
	/// @param cfg    session configuration; defaults are production-safe over TLS
	KSession(std::unique_ptr<Store> Store, Config cfg);
	KSession(std::unique_ptr<Store> Store);

#if DEKAF2_HAS_SQLITE3
	/// Convenience constructor: creates a KSessionSQLiteStore at the given path.
	/// @param sDatabase filesystem path to the SQLite database file
	/// @param cfg       session configuration
	KSession(KStringViewZ sDatabase, Config cfg);
	KSession(KStringViewZ sDatabase);
#endif

	/// Convenience constructor: creates a KSessionKSQLStore for the given KSQL connection.
	/// @param db  reference to an open KSQL connection (caller manages lifetime)
	/// @param cfg session configuration
	KSession(KSQL& db, Config cfg);
	KSession(KSQL& db);

	/// Destructor — stops the background purge timer if running.
	~KSession();

	KSession(const KSession&)            = delete;
	KSession(KSession&&)                 = delete;
	KSession& operator=(const KSession&) = delete;
	KSession& operator=(KSession&&)      = delete;

	/// Install the credential verifier. The callback is invoked from
	/// Login() only. It is safe to call SetAuthenticator at runtime.
	void SetAuthenticator(Authenticator Auth);

	/// Validate credentials and create a new session on success.
	/// @param sUsername  the user to log in
	/// @param sPassword  the submitted password (verified via the Authenticator)
	/// @param sClientIP  optional: client IP at login time, stored for audit
	/// @param sUserAgent optional: user-agent at login time, stored for audit
	/// @param sExtra     optional: application-specific opaque payload (JSON recommended)
	/// @returns the new session token on success, empty string on authentication failure
	DEKAF2_NODISCARD
	KString Login (KStringView sUsername, KStringView sPassword,
	               KStringView sClientIP  = {},
	               KStringView sUserAgent = {},
	               KStringView sExtra     = {});

	/// Validate an existing session token. On success, refreshes
	/// LastSeen (both in the Store and in *pOut if given).
	/// @param sToken the token received from the client's cookie
	/// @param pOut   if non-null and validation succeeds, receives the session record
	/// @returns true if the token is valid and not expired
	DEKAF2_NODISCARD
	bool Validate (KStringView sToken, Record* pOut = nullptr);

	/// Explicitly invalidate a session (e.g. on logout).
	/// @returns true if a session was found and erased
	bool Logout (KStringView sToken);

	/// Invalidate every session for the given user (e.g. after password change).
	/// @returns count of sessions erased
	std::size_t LogoutAllFor (KStringView sUsername);

	/// Manually trigger the expired-session purge. Called automatically
	/// by the background timer if PurgeInterval is positive.
	/// @returns count of sessions erased
	std::size_t PurgeExpired ();

	/// @returns total number of currently tracked sessions
	DEKAF2_NODISCARD
	std::size_t Count () const;

	/// Serialize a complete Set-Cookie header value for the given token.
	/// The returned string is ready to be passed to
	/// @ref KHTTPResponseHeaders::Add(KHTTPHeader::SET_COOKIE, ...) or
	/// to @ref KRESTServer::SetCookie (name+value+options form).
	/// @param sToken the session token to embed in the cookie
	DEKAF2_NODISCARD
	KString SerializeSetCookie (KStringView sToken) const;

	/// Serialize a Set-Cookie header value that tells the browser to
	/// delete the session cookie immediately (Max-Age=0 with empty value).
	/// Use this on logout.
	DEKAF2_NODISCARD
	KString SerializeExpiryCookie () const;

	/// @returns the configured cookie name (so callers know which
	/// request cookie to read via @ref KRESTServer::GetCookie).
	DEKAF2_NODISCARD
	const KString& GetCookieName () const { return m_Config.sCookieName; }

	/// @returns the active configuration
	DEKAF2_NODISCARD
	const Config& GetConfig () const { return m_Config; }

//------
private:
//------

	/// Generate a cryptographically random, base64url-encoded token of
	/// length corresponding to m_Config.iTokenBytes raw random bytes.
	KString GenerateToken() const;

	/// Compose the attribute tail ("; HttpOnly; Secure; ...") from the Config.
	KString SerializeAttributes(KDuration MaxAge = KDuration::zero()) const;

	/// Start the periodic purge timer (if configured)
	void StartPurgeTimer();

	/// Cancel the periodic purge timer
	void StopPurgeTimer();

	Config                      m_Config;
	std::unique_ptr<Store>      m_Store;
	KThreadSafe<Authenticator>  m_Authenticator;
	KTimer::ID_t                m_PurgeTimerID { KTimer::InvalidID };

}; // KSession

/// @}

DEKAF2_NAMESPACE_END
