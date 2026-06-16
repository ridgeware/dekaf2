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

/// @file kapplepush.h
/// @brief Apple Push Notification service (APNs) client

#include <dekaf2/core/init/kdefinitions.h>
#include <dekaf2/core/errors/kerror.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/strings/kstringview.h>
#include <dekaf2/data/json/kjson.h>
#include <dekaf2/time/clock/ktime.h>
#include <dekaf2/time/duration/kduration.h>
#include <dekaf2/crypto/ec/keckey.h>

#include <memory>
#include <mutex>
#include <vector>

DEKAF2_NAMESPACE_BEGIN

class KSQL;

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Apple Push Notification service (APNs) client. Parallel to KWebPush but
/// for iOS / macOS device tokens — uses HTTP/2 (mandatory per Apple), a
/// provider-token JWT signed with ES256 from a `.p8` Auth Key, and JSON
/// payloads in the `aps` envelope format.
///
/// Two delivery channels are supported on Apple platforms:
///
///   - **Regular** notifications (chat messages, contact requests, …) —
///     delivered via standard APNs with the recipient device's user-presented
///     token (`device_token`). The app may be backgrounded or suspended.
///
///   - **VoIP** notifications (incoming call invitations) — delivered via
///     PushKit's separate token (`voip_token`). VoIP pushes wake suspended
///     apps so the iOS shell can immediately report the call to CallKit
///     (Apple-mandated; failure to do so on every VoIP push results in the
///     app being denied VoIP push privileges).
///
/// One subscription record per (user, device) pair. The same device usually
/// has both a regular and a VoIP token; Subscribe() upserts on (user,
/// device_id) so a re-registration replaces both.
///
/// Authentication uses Apple's provider tokens (JWT ES256 signed with the
/// .p8 Auth Key). The token is cached and refreshed automatically before
/// the 60-minute APNs validity window expires.
class DEKAF2_PUBLIC KApplePush : public KErrorBase
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	/// the urgency level for a sent notification (maps to apns-priority header)
	enum eUrgency
	{
		VeryLow,    ///< priority 1 (background, no user interruption)
		Low,        ///< priority 5 (alert displayed but not energy-intensive)
		Normal,     ///< priority 5 (default for regular pushes)
		High        ///< priority 10 (immediate delivery, used for VoIP/calls)
	};

	static KStringViewZ DEKAF2_PUBLIC ToString(eUrgency);

	/// Which APNs topic / channel this notification targets.
	enum class eChannel
	{
		Regular,    ///< standard user-visible push (chat, alerts)
		VoIP        ///< PushKit VoIP push (apns-topic gets ".voip" suffix)
	};

	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	struct DEKAF2_PUBLIC SendOptions
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		SendOptions(eUrgency urgency = eUrgency::Normal,
		            KDuration expiry = chrono::hours(24),
		            eChannel channel = eChannel::Regular)
		: Urgency(urgency), Expiry(expiry), Channel(channel) {}
		eUrgency  Urgency;
		KDuration Expiry;       ///< 0 = drop if not deliverable immediately
		eChannel  Channel;
	};

	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// A device subscription. The same device_id may carry two tokens — one
	/// for standard push, one for VoIP — both registered in a single record.
	struct DEKAF2_PUBLIC Subscription
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		KString    sUser;       ///< username of the subscriber
		KString    sDeviceID;   ///< app-stable device identifier
		KString    sToken;      ///< APNs device token (hex, regular channel)
		KString    sVoIPToken;  ///< PushKit VoIP token (hex, may be empty)
		KString    sUserAgent;  ///< parsed user agent description
		KUnixTime  tCreated;    ///< when first stored (UTC)
		KUnixTime  tLastMod;    ///< when last updated (UTC)
	};

	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// Configuration loaded from a `.p8` Auth Key file and your Apple
	/// Developer account identifiers.
	struct DEKAF2_PUBLIC Config
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		KString sTeamID;        ///< 10-char Apple Team ID
		KString sKeyID;         ///< 10-char Auth Key ID (from the .p8 filename)
		KString sBundleID;      ///< app bundle identifier (e.g. "org.sample.app")
		KString sAuthKeyPEM;    ///< contents of the .p8 file (PEM-encoded ES256 private key)
		bool    bSandbox{false};///< true → api.sandbox.push.apple.com (development builds)
	};

	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// Abstract DB backend, mirroring KWebPush::DB shape.
	class DEKAF2_PUBLIC DB
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{

	//------
	public:
	//------

		virtual ~DB() = default;

		virtual bool                      CreateTables()                                       = 0;
		virtual bool                      StoreSubscription(const Subscription& sub)           = 0;
		virtual bool                      RemoveSubscription(KStringView sUser,
		                                                    KStringView sDeviceID)             = 0;
		virtual bool                      RemoveUserSubscriptions(KStringView sUser)           = 0;
		virtual std::vector<Subscription> GetSubscriptions(KStringView sUser = {})             = 0;
		virtual bool                      HasSubscriptions()                                   = 0;
		virtual KString                   GetLastError() const                                 = 0;

	}; // DB

	/// Construct with a custom DB backend and APNs config.
	KApplePush(std::unique_ptr<DB> DB, Config cfg);

#if DEKAF2_HAS_SQLITE3
	/// Convenience constructor — SQLite backend at the given path.
	KApplePush(KStringViewZ sDatabase, Config cfg);
#endif

	/// Convenience constructor — KSQL backend on an existing connection.
	KApplePush(KSQL& db, Config cfg);

	/// Store / upsert a device subscription. Keyed on (user, device_id).
	bool Subscribe(Subscription sub);

	/// Remove one device subscription.
	bool Unsubscribe(KStringView sUser, KStringView sDeviceID);

	/// Remove all subscriptions for the given user.
	bool UnsubscribeUser(KStringView sUser);

	/// Send a push notification.
	///
	/// @p jPayload is the complete APNs payload — must already contain an
	/// `aps` key. Caller controls the full envelope so custom top-level keys
	/// (used for PushKit / NSE-decryption / call routing) survive verbatim.
	/// A typical regular push:
	///   { "aps": { "alert": { "title": "...", "body": "..." }, "sound": "default" } }
	/// A typical VoIP push:
	///   { "aps": {}, "from": "alice@…", "room": "uuid-…" }
	///
	/// Expired or invalid device tokens are pruned automatically (APNs
	/// returns 410 Unregistered for them).
	///
	/// @param jPayload full APNs JSON payload (must contain `aps`)
	/// @param sUser    if non-empty, only target this user's devices; else all
	/// @param Options  urgency, expiry, channel
	/// @returns number of successfully delivered notifications
	std::size_t Send(const KJSON& jPayload,
	                 KStringView sUser = {},
	                 SendOptions Options = SendOptions{});

	/// True if at least one subscription exists.
	DEKAF2_NODISCARD
	bool HasSubscriptions() const;

	/// List subscriptions, optionally filtered by user.
	DEKAF2_NODISCARD
	std::vector<Subscription> ListSubscriptions(KStringView sUser = {}) const;

	/// True once Init() succeeded — config valid, key loaded, tables present.
	DEKAF2_NODISCARD
	bool IsReady() const { return m_bReady; }

//------
private:
//------

	/// create tables, load the ES256 signing key from the configured PEM
	bool Init();

	/// Build (or refresh) the cached provider-token JWT. Returns by value
	/// (not by reference) so callers can't end up holding a dangling view
	/// into the cached member when another thread refreshes the token mid
	/// network call. JWTs are ~250 bytes — the copy cost is irrelevant next
	/// to the APNs HTTP/2 round-trip.
	KString ProviderToken() const;

	/// POST one notification to APNs. Returns true on 200; updates m_sError otherwise.
	/// On 410 Unregistered the caller should drop the subscription.
	bool SendOne(KStringView sToken, KStringView sPayload,
	             SendOptions Options, int& iHTTPStatus) const;

	/// Compute the apns-topic header value for the given channel.
	KString TopicFor(eChannel ch) const;

	std::unique_ptr<DB> m_DB;
	Config              m_Config;
	KECKey              m_SigningKey;   ///< ES256 private key from sAuthKeyPEM

	mutable std::mutex  m_TokenMutex;
	mutable KString     m_sCachedToken; ///< provider-token JWT (refreshed before expiry)
	mutable KUnixTime   m_tTokenExpiry; ///< when m_sCachedToken stops being valid

	mutable std::mutex  m_Mutex;
	bool                m_bReady { false };

}; // KApplePush

DEKAF2_NAMESPACE_END
