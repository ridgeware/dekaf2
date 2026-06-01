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

#include <dekaf2/web/push/kapplepush.h>
#include <dekaf2/web/push/bits/kapplepushsqlitedb.h>
// NOTE: deliberately no include of kapplepushksqldb.h here. The KSQL
// convenience constructor lives in kapplepushksqldb.cpp so that kapplepush.o
// has zero references to KSQL symbols — apps that don't link ksql2 can still
// use the SQLite or custom-DB backends without unresolved symbols.
#include <dekaf2/core/logging/klog.h>
#include <dekaf2/core/format/kformat.h>
#include <dekaf2/crypto/encoding/kbase64.h>
#include <dekaf2/crypto/ec/kecsign.h>
#include <dekaf2/http/client/kwebclient.h>
#include <dekaf2/net/util/kstreamoptions.h>
#include <dekaf2/web/url/kurl.h>
#include <dekaf2/util/id/kuuid.h>

DEKAF2_NAMESPACE_BEGIN

//-----------------------------------------------------------------------------
KStringViewZ KApplePush::ToString(eUrgency u)
//-----------------------------------------------------------------------------
{
	// APNs apns-priority header values: 1 (low), 5 (normal), 10 (high).
	// 1 maps to VeryLow/Low (background-friendly).
	switch (u)
	{
		case eUrgency::VeryLow: return "1";
		case eUrgency::Low:     return "1";
		case eUrgency::Normal:  return "5";
		case eUrgency::High:    return "10";
	}
	return "5";

} // ToString

//-----------------------------------------------------------------------------
KApplePush::KApplePush(std::unique_ptr<DB> db, Config cfg)
//-----------------------------------------------------------------------------
: m_DB(std::move(db))
, m_Config(std::move(cfg))
{
	Init();
}

#if DEKAF2_HAS_SQLITE3
//-----------------------------------------------------------------------------
KApplePush::KApplePush(KStringViewZ sDatabase, Config cfg)
//-----------------------------------------------------------------------------
: m_DB(std::make_unique<KApplePushSQLiteDB>(sDatabase))
, m_Config(std::move(cfg))
{
	Init();
}
#endif

// KApplePush::KApplePush(KSQL& db, Config cfg) lives in kapplepushksqldb.cpp
// to keep this TU free of KSQL symbol references.

//-----------------------------------------------------------------------------
bool KApplePush::Init()
//-----------------------------------------------------------------------------
{
	if (!m_DB)
	{
		SetError("no database backend");
		return false;
	}

	if (!m_DB->CreateTables())
	{
		SetError(kFormat("cannot create APNs subscription tables: {}", m_DB->GetLastError()));
		return false;
	}

	// Empty PEM is a deliberate stub-mode: subscriptions still work, but
	// Send() short-circuits to 0. Lets an app deploy the endpoints + DB
	// schema before the .p8 Auth Key is provisioned.
	if (m_Config.sAuthKeyPEM.empty())
	{
		kDebug(1, "APNs: no Auth Key configured — Send() will be a no-op");
		m_bReady = true;
		return true;
	}

	if (m_Config.sTeamID.size() != 10 || m_Config.sKeyID.size() != 10)
	{
		SetError("APNs: Team ID and Key ID must each be 10 characters");
		return false;
	}

	if (m_Config.sBundleID.empty())
	{
		SetError("APNs: bundle_id is required");
		return false;
	}

	if (!m_SigningKey.CreateFromPEM(m_Config.sAuthKeyPEM))
	{
		SetError(kFormat("APNs: cannot parse .p8 Auth Key: {}", m_SigningKey.Error()));
		return false;
	}

	m_bReady = true;
	return true;

} // Init

//-----------------------------------------------------------------------------
bool KApplePush::Subscribe(Subscription sub)
//-----------------------------------------------------------------------------
{
	if (!m_DB) return false;
	auto Now = KUnixTime::now();
	if (sub.tCreated == KUnixTime{}) sub.tCreated = Now;
	sub.tLastMod = Now;

	std::lock_guard<std::mutex> L(m_Mutex);
	return m_DB->StoreSubscription(sub);

} // Subscribe

//-----------------------------------------------------------------------------
bool KApplePush::Unsubscribe(KStringView sUser, KStringView sDeviceID)
//-----------------------------------------------------------------------------
{
	if (!m_DB) return false;
	std::lock_guard<std::mutex> L(m_Mutex);
	return m_DB->RemoveSubscription(sUser, sDeviceID);

} // Unsubscribe

//-----------------------------------------------------------------------------
bool KApplePush::UnsubscribeUser(KStringView sUser)
//-----------------------------------------------------------------------------
{
	if (!m_DB) return false;
	std::lock_guard<std::mutex> L(m_Mutex);
	return m_DB->RemoveUserSubscriptions(sUser);

} // UnsubscribeUser

//-----------------------------------------------------------------------------
bool KApplePush::HasSubscriptions() const
//-----------------------------------------------------------------------------
{
	if (!m_DB) return false;
	std::lock_guard<std::mutex> L(m_Mutex);
	return m_DB->HasSubscriptions();

} // HasSubscriptions

//-----------------------------------------------------------------------------
std::vector<KApplePush::Subscription> KApplePush::ListSubscriptions(KStringView sUser) const
//-----------------------------------------------------------------------------
{
	if (!m_DB) return {};
	std::lock_guard<std::mutex> L(m_Mutex);
	return m_DB->GetSubscriptions(sUser);

} // ListSubscriptions

//-----------------------------------------------------------------------------
KString KApplePush::TopicFor(eChannel ch) const
//-----------------------------------------------------------------------------
{
	// VoIP pushes go to "<bundle>.voip" per Apple's PushKit convention.
	return (ch == eChannel::VoIP)
		? m_Config.sBundleID + ".voip"
		: m_Config.sBundleID;

} // TopicFor

//-----------------------------------------------------------------------------
KString KApplePush::ProviderToken() const
//-----------------------------------------------------------------------------
{
	// APNs provider tokens are JWTs (ES256) signed with the .p8 Auth Key,
	// valid up to 60 minutes. Apple recommends refreshing every ~50 minutes
	// to avoid using a token that expires mid-flight. We cache one token
	// for the whole process; safe to reuse across users and channels.
	std::lock_guard<std::mutex> L(m_TokenMutex);

	auto Now = KUnixTime::now();
	if (!m_sCachedToken.empty() && m_tTokenExpiry > Now + chrono::seconds(60))
	{
		return m_sCachedToken;
	}

	// Build header: { "alg": "ES256", "kid": "<KeyID>" }
	KJSON jHeader {
		{ "alg", "ES256"         },
		{ "kid", m_Config.sKeyID }
	};

	// Build payload: { "iss": "<TeamID>", "iat": <now> }
	KJSON jPayload {
		{ "iss", m_Config.sTeamID                     },
		{ "iat", static_cast<int64_t>(Now.to_time_t())}
	};

	auto sHeaderB64  = KBase64Url::Encode(jHeader.dump());
	auto sPayloadB64 = KBase64Url::Encode(jPayload.dump());

	KString sSigningInput = sHeaderB64;
	sSigningInput += '.';
	sSigningInput += sPayloadB64;

	KECSign Signer;
	KString sRawSig = Signer.Sign(m_SigningKey, sSigningInput);
	if (sRawSig.empty())
	{
		kDebug(1, "APNs: cannot sign provider token: {}", Signer.Error());
		m_sCachedToken.clear();
		return {};
	}

	sSigningInput += '.';
	sSigningInput += KBase64Url::Encode(sRawSig);

	m_sCachedToken = std::move(sSigningInput);
	m_tTokenExpiry = Now + chrono::minutes(55);   // refresh window
	return m_sCachedToken;

} // ProviderToken

//-----------------------------------------------------------------------------
bool KApplePush::SendOne(KStringView sToken, KStringView sPayload,
                         SendOptions Options, int& iHTTPStatus) const
//-----------------------------------------------------------------------------
{
	iHTTPStatus = 0;

	if (sToken.empty()) return false;

	auto sJWT = ProviderToken();   // returned by value — see header comment
	if (sJWT.empty()) return false;

	KStringView sHost = m_Config.bSandbox
		? "https://api.sandbox.push.apple.com"
		: "https://api.push.apple.com";

	KURL url(kFormat("{}/3/device/{}", sHost, sToken));

	// APNs is HTTP/2 only — no HTTP/1 fallback. KHTTPStreamOptions's default
	// "DefaultsForHTTP" allows HTTP/1 fallback; we restrict to HTTP/2.
	KHTTPStreamOptions opts(KStreamOptions::VerifyCert | KStreamOptions::RequestHTTP2);
	KWebClient Client(opts);
	Client.SetTimeout(10);
	Client.AddHeader(KHTTPHeader::AUTHORIZATION, KString("bearer ") + sJWT);
	Client.AddHeader("apns-topic",      TopicFor(Options.Channel));
	Client.AddHeader("apns-priority",   ToString(Options.Urgency));
	Client.AddHeader("apns-push-type",  Options.Channel == eChannel::VoIP ? "voip" : "alert");
	Client.AddHeader("apns-expiration", KString::to_string(
		Options.Expiry.IsZero()
			? 0
			: KUnixTime::now().to_time_t() + Options.Expiry.seconds().count()));
	// uuid-tagged so we can correlate in apple's feedback if needed
	Client.AddHeader("apns-id",         KUUID().ToString());

	auto sResponse = Client.Post(url, sPayload, KMIME::JSON);
	iHTTPStatus    = Client.GetStatusCode();

	if (iHTTPStatus == 200) return true;

	kDebug(1, "APNs send to {}... -> HTTP {}: {}",
	       sToken.Left(16), iHTTPStatus, sResponse.Left(200));
	return false;

} // SendOne

//-----------------------------------------------------------------------------
std::size_t KApplePush::Send(const KJSON& jPayload, KStringView sUser, SendOptions Options)
//-----------------------------------------------------------------------------
{
	if (!m_bReady || m_Config.sAuthKeyPEM.empty()) return 0;
	if (!jPayload.contains("aps"))
	{
		kDebug(1, "APNs Send: payload is missing the required 'aps' key");
		return 0;
	}

	auto Subs = ListSubscriptions(sUser);
	if (Subs.empty()) return 0;

	auto sPayload = jPayload.dump();

	std::size_t iSent = 0;

	for (const auto& sub : Subs)
	{
		auto& sToken = (Options.Channel == eChannel::VoIP) ? sub.sVoIPToken : sub.sToken;
		if (sToken.empty()) continue;

		int iStatus = 0;
		if (SendOne(sToken, sPayload, Options, iStatus))
		{
			++iSent;
			continue;
		}

		// 410 Unregistered: the token is gone. Drop the channel-specific
		// token but keep the subscription record if the other channel is
		// still good (a re-registration in the app will replace it).
		if (iStatus == 410)
		{
			std::lock_guard<std::mutex> L(m_Mutex);
			Subscription updated = sub;

			if (Options.Channel == eChannel::VoIP)
			{
				updated.sVoIPToken.clear();
			}
			else
			{
				updated.sToken.clear();
			}

			if (updated.sToken.empty() && updated.sVoIPToken.empty())
			{
				m_DB->RemoveSubscription(updated.sUser, updated.sDeviceID);
			}
			else
			{
				updated.tLastMod = KUnixTime::now();
				m_DB->StoreSubscription(updated);
			}
		}
		// 403 InvalidProviderToken: signing key changed. Clear our cache so
		// the next attempt rebuilds the JWT (in case admin rotated the key).
		else if (iStatus == 403)
		{
			std::lock_guard<std::mutex> L(m_TokenMutex);
			m_sCachedToken.clear();
		}
	}

	return iSent;

} // Send

DEKAF2_NAMESPACE_END
