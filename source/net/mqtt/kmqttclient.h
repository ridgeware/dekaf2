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
#include <mutex>
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
/// **Two connections to the broker, one per direction.** A dekaf2 socket
/// stream is thread affine: it holds a single io_service and a single error
/// code for both directions, so a read and a write in flight at the same time
/// consume each other's completions and the write is silently lost. Rather
/// than serialising around that, the reading session and the publishing
/// session get a socket each, and each socket is touched by exactly one
/// thread at a time. Once dekaf2 grows per-direction state this collapses
/// back into one connection.
///
/// **No PINGREQ**: both sessions announce keep alive 0, which tells the broker
/// not to drop them for being quiet — the reading session must never write,
/// and the publishing session has nothing to say between publishes. A dead
/// connection is noticed instead by the read timeout (reader side) and by a
/// failing write (publisher side), both of which reconnect.
///
/// **Subscriptions survive a reconnect**: they are remembered and re-sent
/// after every successful connect. Subscribe() while running hands the topic
/// to the reader thread, which sends it between two received packets — the
/// only thread allowed to write on that socket.
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
	/// @param sClientID basis for the two session ids ("<id>-sub" and
	/// "<id>-pub"); it must be unique on the broker, as two clients sharing an
	/// id disconnect each other, which looks exactly like a reconnect loop
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
	/// How long the reading connection may stay silent before it is considered
	/// dead and re-established. This is a watchdog, not a protocol timeout: a
	/// dekaf2 read timeout closes the socket, which is exactly what is wanted
	/// here. Pick it well above the slowest expected message interval.
	/// Default 2 minutes.
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
	/// before Start(); while running it takes effect as soon as the reader
	/// thread comes up for air between two packets.
	bool Subscribe(KString sTopicFilter);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Drop a subscription from the remembered set (including one not yet
	/// sent). It stops arriving after the next reconnect — unsubscribing on a
	/// live session would mean writing on the reader's socket.
	bool Unsubscribe(KStringView sTopicFilter);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Publish a message (QoS 0, not retained). Opens the publishing
	/// connection on first use and re-opens it after a failure.
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
	DEKAF2_PRIVATE std::unique_ptr<KIOStreamSocket> OpenSession(KStringView sSessionID, KDuration Timeout);
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

	/// the subscribing session — reader thread only, never written to from
	/// anywhere else (see the class comment)
	std::unique_ptr<KIOStreamSocket> m_ReadStream;
	/// the publishing session — used by whoever calls Publish(), serialised
	/// on the mutex below, and never read from
	std::unique_ptr<KIOStreamSocket> m_WriteStream;
	std::mutex                 m_WriteMutex;

	std::thread                m_Reader;
	std::atomic<bool>          m_bStop      { false };
	std::atomic<bool>          m_bConnected { false };

	/// remembered so a reconnect can restore them
	KThreadSafe<std::vector<KString>> m_Subscriptions;
	/// handed to the reader thread, which sends them between two packets and
	/// drops duplicates against what this session already subscribed
	KThreadSafe<std::vector<KString>> m_PendingSubscriptions;
	/// SUBSCRIBE needs a packet id even at QoS 0
	std::atomic<uint16_t>      m_iPacketID  { 1 };

	mutable KThreadSafe<KString> m_sLastError;

}; // KMQTTClient

DEKAF2_NAMESPACE_END
