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

#pragma once

/// @file kmqttclient.h
/// MQTT 3.1.1 client, QoS 0

#include <dekaf2/core/init/kdefinitions.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/strings/kstringview.h>
#include <dekaf2/web/url/kurl.h>
#include <dekaf2/time/duration/kduration.h>
#include <dekaf2/threading/primitives/kthreadsafe.h>

#include <atomic>
#include <functional>
#include <memory>
#include <thread>
#include <vector>

DEKAF2_NAMESPACE_BEGIN

class KIOStreamSocket;

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// MQTT 3.1.1 client for subscribing to and publishing telemetry.
///
/// **QoS 0 only, deliberately.** Higher service levels bring the
/// PUBACK/PUBREC/PUBREL/PUBCOMP state machines with their retransmission and
/// duplicate handling, which doubles the client for no gain here: telemetry
/// arrives again within seconds, so a lost packet costs nothing, and typical
/// publishers send at QoS 0 anyway.
///
/// **One connection, owned by the reader thread.** dekaf2 socket streams
/// tolerate only one thread at a time, so the reader owns the socket alone:
/// between received
/// packets it multiplexes over IsReadReady() and sends whatever the public
/// calls have queued up meanwhile — subscriptions, unsubscriptions, and
/// published messages. The public calls never touch the socket, they enqueue
/// and return.
///
/// **Publish() is asynchronous**: it queues the message, and the reader
/// sends it within the poll interval (a tenth of a second). At QoS 0 a
/// message without a connection has no delivery promise anyway - what is
/// still queued when the connection breaks is dropped with it.
///
/// **Keep alive**: the connection announces keep alive 0, so the broker does
/// not drop it for being quiet. A silence watchdog probes with PINGREQ once
/// nothing was received for the silence timeout, and reconnects only when
/// the broker stays mute - a quiet broker costs a ping every few minutes,
/// not a reconnect.
///
/// **Subscriptions survive a reconnect**: they are remembered and re-sent
/// after every successful connect. Subscribe() and Unsubscribe() while
/// running take effect within the poll interval.
///
/// Callbacks run on the reader thread: keep them short and thread safe.
class DEKAF2_PUBLIC KMQTTClient
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	/// called for every message on a subscribed topic — on the reader thread
	using MessageCallback = std::function<void(KStringView sTopic, KStringView sPayload)>;
	/// called after connecting (true, subscriptions are in place again) and
	/// after losing the connection (false)
	using StateCallback   = std::function<void(bool bConnected)>;

	//-----------------------------------------------------------------------------
	/// @param URL broker address, mqtt:// (plain) or mqtts:// (TLS); the default
	/// ports are 1883 and 8883
	/// @param sClientID the session id; it must be unique on the broker, as
	/// two clients sharing an id disconnect each other, which looks exactly
	/// like a reconnect loop
	KMQTTClient(KURL URL, KString sClientID);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	~KMQTTClient();
	//-----------------------------------------------------------------------------

	KMQTTClient(const KMQTTClient&) = delete;
	KMQTTClient& operator=(const KMQTTClient&) = delete;

	//-----------------------------------------------------------------------------
	/// optional broker credentials, to be set before Start()
	void SetCredentials(KString sUser, KString sPassword);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// How long the connection may stay silent before the watchdog probes it
	/// with a PINGREQ; the connection is re-established only when the broker
	/// does not answer either. Default 2 minutes.
	void SetSilenceTimeout(KDuration Timeout);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Packets with a body above this size are treated as a broken connection —
	/// there is no way to resync on the stream after refusing a body. Protects
	/// against a broken or hostile broker. Default 4 MB.
	void SetMaxPacketSize(std::size_t iMaxPacketSize);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// verify the broker certificate on mqtts:// (default true)
	void SetVerifyCerts(bool bYesNo);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	void SetMessageCallback(MessageCallback Callback);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	void SetStateCallback(StateCallback Callback);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Subscribe to a topic filter (`+` matches one level, `#` the rest and
	/// only at the end). Remembered for the next reconnect. May be called
	/// before Start(); while running it takes effect within the poll
	/// interval.
	bool Subscribe(KString sTopicFilter);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Drop a subscription from the remembered set (including one not yet
	/// sent). On a live session the reader sends an UNSUBSCRIBE within the
	/// poll interval.
	bool Unsubscribe(KStringView sTopicFilter);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Publish a message (QoS 0, not retained). Asynchronous: the message is
	/// queued and leaves within the poll interval once the connection is up.
	/// @return false for an unusable topic or a full queue - and true means
	/// queued, not delivered: at QoS 0 there is no delivery promise
	bool Publish(KStringView sTopic, KStringView sPayload);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// start the reader thread; calling it while running restarts it
	void Start();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// stop the reader thread and drop both connections. Idempotent.
	void Stop();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// is the reading session up? (the publishing one is opened on demand)
	bool IsConnected() const { return m_bConnected; }
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// human readable last error, empty when all is well
	KString LastError() const;
	//-----------------------------------------------------------------------------

//----------
private:
//----------

	//-----------------------------------------------------------------------------
	/// open a socket to the broker (TLS for mqtts://) and run the MQTT
	/// handshake on it. @return the connected stream, or nullptr with the
	/// error recorded
	DEKAF2_PRIVATE std::unique_ptr<KIOStreamSocket> OpenSession();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// reader thread: connect, subscribe, read packets until the connection
	/// breaks, retry
	DEKAF2_PRIVATE void Listen();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// send SUBSCRIBE for one filter on the reader's stream — reader thread only
	DEKAF2_PRIVATE bool SendSubscribe(KIOStreamSocket& Stream, KStringView sTopicFilter);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// send UNSUBSCRIBE for one filter on the reader's stream — reader thread only
	DEKAF2_PRIVATE bool SendUnsubscribe(KIOStreamSocket& Stream, KStringView sTopicFilter);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// dispatch one received packet to the callbacks
	DEKAF2_PRIVATE void HandlePacket(uint8_t iFirstByte, const KString& sBody);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	DEKAF2_PRIVATE void SetError(KString sError);
	//-----------------------------------------------------------------------------

	KURL                       m_URL;
	KString                    m_sClientID;
	KString                    m_sUser;
	KString                    m_sPassword;
	KDuration                  m_SilenceTimeout { chrono::minutes(2) };
	std::size_t                m_iMaxPacketSize { 4 * 1024 * 1024 };
	bool                       m_bVerifyCerts   { true };

	MessageCallback            m_OnMessage;
	StateCallback              m_OnState;

	/// a queued message, waiting for the reader thread to send it
	struct Message
	{
		KString sTopic;
		KString sPayload;
	};

	/// the broker connection — reader thread only, never touched from
	/// anywhere else (see the class comment)
	std::unique_ptr<KIOStreamSocket> m_Stream;

	std::thread                m_Reader;
	std::atomic<bool>          m_bStop      { false };
	std::atomic<bool>          m_bConnected { false };

	/// remembered so a reconnect can restore them
	KThreadSafe<std::vector<KString>> m_Subscriptions;
	/// handed to the reader thread, which sends them between two packets and
	/// drops duplicates against what this session already subscribed
	KThreadSafe<std::vector<KString>> m_PendingSubscriptions;
	/// unsubscriptions for the reader thread
	KThreadSafe<std::vector<KString>> m_PendingUnsubscribes;
	/// published messages for the reader thread
	KThreadSafe<std::vector<Message>> m_PendingPublishes;
	/// SUBSCRIBE and UNSUBSCRIBE need a packet id even at QoS 0
	std::atomic<uint16_t>      m_iPacketID  { 1 };

	mutable KThreadSafe<KString> m_sLastError;

}; // KMQTTClient

DEKAF2_NAMESPACE_END
