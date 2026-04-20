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
 //
 */

#pragma once

/// @file kwebpush.h
/// @brief Web Push notifications (RFC 8291 + VAPID)
///
/// KWebPush implements the server side of the W3C Push API. It generates
/// and persists VAPID (Voluntary Application Server Identification) key
/// pairs, stores browser push subscriptions in a pluggable database
/// backend, encrypts notification payloads per RFC 8291 (ECDH +
/// AES-128-GCM), and delivers them via HTTP POST to the push service.
///
/// @par Supported database backends
/// - **KSQLite** — embedded, zero-config, ideal for single-server setups
/// - **KSQL** — any database supported by KSQL (MySQL, PostgreSQL, SQLServer etc.)
/// - **Custom** — implement the KWebPush::DB interface in your own code
///
/// @par Server-side C++ example
/// @code
/// // 1. Create the push manager (SQLite backend, VAPID keys are
/// //    generated automatically on first use and persisted in the DB)
/// KWebPush push("/var/lib/myapp/push.db", "admin@example.com");
///
/// if (push.HasError())
/// {
///     kWarning("push init failed: {}", push.GetLastError());
///     return;
/// }
///
/// // 2. Expose the VAPID public key to the browser (e.g. via REST endpoint)
/// //    GET /api/push/vapid-key
/// REST.SetRawOutput(push.GetPublicKey());
///
/// // 3. Store a subscription received from the browser
/// //    POST /api/push/subscribe  (JSON body)
/// KWebPush::Subscription sub;
/// sub.sUser     = sCurrentUser;
/// sub.sEndpoint = jBody["endpoint"];
/// sub.sP256dh   = jBody["keys"]["p256dh"];
/// sub.sAuth     = jBody["keys"]["auth"];
/// push.Subscribe(std::move(sub));
///
/// // 4. Send a notification
/// KJSON jPayload;
/// jPayload["title"] = "New message";
/// jPayload["body"]  = "You have 3 unread messages";
/// jPayload["url"]   = "/inbox";
/// auto iSent = push.SendToUser(sCurrentUser, jPayload);
/// @endcode
///
/// @par Client-side JavaScript example
/// @code{.js}
/// // --- service-worker.js ---------------------------------------------------
/// self.addEventListener('push', event => {
///   const data = event.data ? event.data.json() : {};
///   event.waitUntil(
///     self.registration.showNotification(data.title || 'Notification', {
///       body: data.body || '',
///       data: { url: data.url || '/' }
///     })
///   );
/// });
///
/// self.addEventListener('notificationclick', event => {
///   event.notification.close();
///   event.waitUntil(clients.openWindow(event.notification.data.url));
/// });
///
/// // --- main.js (in the page) -----------------------------------------------
/// async function setupPush() {
///   const reg    = await navigator.serviceWorker.register('/service-worker.js');
///   const vapid  = await fetch('/api/push/vapid-key').then(r => r.text());
///
///   // Convert base64url VAPID key to Uint8Array
///   const pad = '='.repeat((4 - vapid.length % 4) % 4);
///   const raw = atob(vapid.replace(/-/g, '+').replace(/_/g, '/') + pad);
///   const key = Uint8Array.from(raw, c => c.charCodeAt(0));
///
///   const sub = await reg.pushManager.subscribe({
///     userVisibleOnly: true,
///     applicationServerKey: key
///   });
///
///   // Send subscription to the server
///   await fetch('/api/push/subscribe', {
///     method: 'POST',
///     headers: { 'Content-Type': 'application/json' },
///     body: JSON.stringify(sub)
///   });
/// }
/// @endcode
///
/// @see RFC 8291 — Message Encryption for Web Push
/// @see RFC 8292 — Voluntary Application Server Identification (VAPID)
/// @see https://www.w3.org/TR/push-api/

#include <dekaf2/core/init/kdefinitions.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/strings/kstringview.h>
#include <dekaf2/data/json/kjson.h>
#include <dekaf2/time/clock/ktime.h>
#include <dekaf2/core/errors/kerror.h>
#include <dekaf2/crypto/ec/keckey.h>
#include <mutex>
#include <vector>
#include <memory>

DEKAF2_NAMESPACE_BEGIN

class KSQL;
class KWebClient;

/// @addtogroup web_push
/// @{

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Web Push manager — generates VAPID key pairs, stores browser push
/// subscriptions via a pluggable database backend, encrypts notification
/// payloads per RFC 8291 (ECDH + AES-128-GCM), and delivers them to the
/// push service via HTTP POST.
///
/// The constructor creates tables (if needed), generates or loads VAPID
/// keys, and reports any failure through KErrorBase.  All public methods
/// are guarded — they return safe defaults when initialization failed.
///
/// Thread safety: all public methods are mutex-protected and may be
/// called from any thread.
class DEKAF2_PUBLIC KWebPush : public KErrorBase
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// A single push subscription as returned by the browser's
	/// PushManager.subscribe() call.  The sEndpoint, sP256dh and sAuth
	/// fields come directly from the PushSubscription JSON; sUser is
	/// application-defined.  tCreated and tLastMod are set automatically
	/// by the database backend.
	struct DEKAF2_PUBLIC Subscription
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		KString    sUser;       ///< username of the subscriber
		KString    sEndpoint;   ///< push service URL from PushManager.subscribe()
		KString    sP256dh;     ///< client public key (base64url)
		KString    sAuth;       ///< client auth secret (base64url)
		KString    sUserAgent;  ///< parsed user agent description
		KUnixTime  tCreated;    ///< when the subscription was first created (UTC)
		KUnixTime  tLastMod;    ///< when the subscription was last modified (UTC)
	};

	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// Abstract database backend interface.  Concrete implementations
	/// (KWebPushSQLiteDB, KWebPushKSQLDB) are provided in
	/// web/push/bits/.  Custom backends can be implemented by deriving
	/// from this class and passing an instance to the KWebPush
	/// constructor.
	class DEKAF2_PUBLIC DB
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{

	//------
	public:
	//------

		virtual ~DB() = default;

		/// create the required tables if they don't exist
		virtual bool              CreateTables()                                            = 0;
		/// store a VAPID key/value pair
		virtual bool              StoreVAPIDKey(KStringView sKey, KStringView sValue)       = 0;
		/// load a VAPID key value by name, returns empty string if not found
		virtual KString           LoadVAPIDKey(KStringView sKey)                            = 0;
		/// store or update a push subscription
		virtual bool              StoreSubscription(const Subscription& sub)                = 0;
		/// remove a subscription by its endpoint URL
		virtual bool              RemoveSubscription(KStringView sEndpoint)                 = 0;
		/// remove all subscriptions for a given user
		virtual bool              RemoveUserSubscriptions(KStringView sUser)                = 0;
		/// get subscriptions, optionally filtered by user
		virtual std::vector<Subscription> GetSubscriptions(KStringView sUser = {})          = 0;
		/// returns true if at least one subscription exists
		virtual bool              HasSubscriptions()                                        = 0;
		/// returns the last error message (if any)
		virtual KString           GetLastError() const                                      = 0;

	}; // DB

	/// construct a Web Push manager with a custom database backend
	/// @param DB       database backend (ownership transferred)
	/// @param sContact VAPID contact email (e.g. "admin@example.com" — "mailto:" prefix
	/// is stripped if present and added automatically when building VAPID tokens).
	/// Defaults to "admin@example.org" if empty.
	KWebPush(std::unique_ptr<DB> DB, KString sContact = {});

#if DEKAF2_HAS_SQLITE3
	/// convenience constructor — creates a KSQLite backend at the given path
	/// @param sDatabase path to the SQLite database file
	/// @param sContact VAPID contact email (e.g. "admin@example.com" — "mailto:" prefix
	/// is stripped if present and added automatically when building VAPID tokens).
	/// Defaults to "admin@example.org" if empty.
	KWebPush(KStringViewZ sDatabase, KString sContact = {});
#endif

	/// convenience constructor — creates a KSQL backend with the given connection
	/// @param db        reference to an open KSQL connection (caller manages lifetime)
	/// @param sContact VAPID contact email (e.g. "admin@example.com" — "mailto:" prefix
	/// is stripped if present and added automatically when building VAPID tokens).
	/// Defaults to "admin@example.org" if empty.
	KWebPush(KSQL& db, KString sContact = {});

	/// @returns the VAPID public key in base64url encoding (for the browser)
	DEKAF2_NODISCARD
	const KString& GetPublicKey() const { return m_sPublicKeyB64; }

	/// Store a new push subscription.  If a subscription with the same
	/// endpoint already exists it is updated (upsert).
	/// @param sub the subscription to store
	/// @returns true on success
	bool Subscribe(Subscription sub);

	/// Remove a subscription identified by its push service endpoint URL.
	/// @param sEndpoint the endpoint URL (as returned by the browser)
	/// @returns true on success
	bool Unsubscribe(KStringView sEndpoint);

	/// Remove all subscriptions for the given user.
	/// @param sUser the username
	/// @returns true on success
	bool UnsubscribeUser(KStringView sUser);

	/// Send a push notification.
	/// Expired subscriptions are automatically cleaned up.
	/// @param jPayload  JSON payload
	/// @param sUser if non-empty, only send to subscriptions of this user;
	///              if empty (default), send to all subscriptions.
	/// @returns number of successfully delivered notifications
	std::size_t Send(const KJSON& jPayload, KStringView sUser = {});

	/// @returns true if at least one active subscription exists in the database
	DEKAF2_NODISCARD
	bool HasSubscriptions() const;

	/// List stored subscriptions, optionally filtered by user.
	/// @param sUser if non-empty, only return subscriptions for this user;
	///              if empty (default), return all subscriptions.
	/// @returns vector of Subscription structs (may be empty)
	DEKAF2_NODISCARD
	std::vector<Subscription> ListSubscriptions(KStringView sUser = {}) const;

//------
private:
//------

	/// create tables if they don't exist, generate/load VAPID keys
	bool Init();
	/// generate a new ECDSA P-256 key pair, store in DB
	bool GenerateVAPIDKeys();
	/// load VAPID keys from DB
	bool LoadVAPIDKeys();

	/// send a single push message to one subscription
	/// @returns true on success (HTTP 201)
	bool SendPush(KWebClient& Client, const Subscription& sub, KStringView sPayload);

	/// build a VAPID JWT token for the given audience (origin of the push endpoint)
	DEKAF2_NODISCARD
	KString BuildVAPIDToken(KStringView sAudience) const;

	/// encrypt the payload per RFC 8291 (ECDH + AES-128-GCM)
	/// @returns encrypted payload, empty on error
	DEKAF2_NODISCARD
	KString EncryptPayload(KStringView sPayload, KStringView sP256dhB64, KStringView sAuthB64) const;

	/// normalize the VAPID contact email
	static KString NormalizeContact(KString sContact);

	std::unique_ptr<DB> m_DB;
	KString m_sContact;         ///< VAPID contact URL/email
	KString m_sPublicKeyB64;    ///< VAPID public key (base64url, 65 bytes uncompressed)
	KECKey  m_VAPIDKey;         ///< VAPID EC P-256 key pair

	mutable std::mutex m_Mutex;
	bool m_bReady { false };    ///< true after successful Init()

}; // KWebPush

/// @}

DEKAF2_NAMESPACE_END
