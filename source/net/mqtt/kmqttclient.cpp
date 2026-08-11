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

#include <dekaf2/net/mqtt/kmqttclient.h>
#include <dekaf2/net/mqtt/bits/kmqttcodec.h>
#include <dekaf2/net/tcp/ktcpstream.h>
#include <dekaf2/net/tls/ktlsstream.h>
#include <dekaf2/system/os/ksystem.h>
#include <dekaf2/core/format/kformat.h>
#include <dekaf2/core/logging/klog.h>

#include <algorithm>

DEKAF2_NAMESPACE_BEGIN

namespace {

/// protocol name and level of MQTT 3.1.1, as they go into the CONNECT packet
constexpr KStringViewZ s_sProtocolName  { "MQTT" };
constexpr uint8_t      s_iProtocolLevel { 4 };

/// how long to wait before reconnecting after a lost connection
constexpr auto         s_ReconnectDelay = chrono::seconds(5);

/// the publishing session is quiet by nature; this is only the connect and
/// write timeout, not a silence watchdog
constexpr auto         s_WriteTimeout   = chrono::seconds(10);

} // end of anonymous namespace

//-----------------------------------------------------------------------------
KMQTTClient::KMQTTClient(KURL URL, KString sClientID)
//-----------------------------------------------------------------------------
: m_URL      (std::move(URL))
, m_sClientID(std::move(sClientID))
{
	// mqtt:// and mqtts:// are not in dekaf2's protocol table, so the port has
	// to be filled in here rather than by KURL::DefaultPort()
	if (m_URL.Port.empty())
	{
		m_URL.Port = (m_URL.Protocol.Serialize().starts_with("mqtts")) ? "8883" : "1883";
	}

} // ctor

//-----------------------------------------------------------------------------
KMQTTClient::~KMQTTClient()
//-----------------------------------------------------------------------------
{
	Stop();

} // dtor

//-----------------------------------------------------------------------------
void KMQTTClient::SetCredentials(KString sUser, KString sPassword)
//-----------------------------------------------------------------------------
{
	m_sUser     = std::move(sUser);
	m_sPassword = std::move(sPassword);

} // SetCredentials

//-----------------------------------------------------------------------------
void KMQTTClient::SetSilenceTimeout(KDuration Timeout)
//-----------------------------------------------------------------------------
{
	m_SilenceTimeout = Timeout;

} // SetSilenceTimeout

//-----------------------------------------------------------------------------
void KMQTTClient::SetMaxPacketSize(std::size_t iMaxPacketSize)
//-----------------------------------------------------------------------------
{
	m_iMaxPacketSize = iMaxPacketSize;

} // SetMaxPacketSize

//-----------------------------------------------------------------------------
void KMQTTClient::SetVerifyCerts(bool bYesNo)
//-----------------------------------------------------------------------------
{
	m_bVerifyCerts = bYesNo;

} // SetVerifyCerts

//-----------------------------------------------------------------------------
void KMQTTClient::SetMessageCallback(MessageCallback Callback)
//-----------------------------------------------------------------------------
{
	m_OnMessage = std::move(Callback);

} // SetMessageCallback

//-----------------------------------------------------------------------------
void KMQTTClient::SetStateCallback(StateCallback Callback)
//-----------------------------------------------------------------------------
{
	m_OnState = std::move(Callback);

} // SetStateCallback

//-----------------------------------------------------------------------------
std::unique_ptr<KIOStreamSocket> KMQTTClient::OpenSession(KStringView sSessionID, KDuration Timeout)
//-----------------------------------------------------------------------------
{
	bool bTLS = m_URL.Protocol.Serialize().starts_with("mqtts");

	KTCPEndPoint EndPoint(m_URL.Domain, m_URL.Port);

	KStreamOptions Options(Timeout);

	if (bTLS && m_bVerifyCerts) Options.Set(KStreamOptions::VerifyCert);

	std::unique_ptr<KIOStreamSocket> Stream;

	if (bTLS) Stream = std::make_unique<KTLSStream>(EndPoint, Options);
	else      Stream = std::make_unique<KTCPStream>(EndPoint, Options);

	if (!Stream->Good())
	{
		SetError(kFormat("cannot connect to {}: {}", EndPoint.Serialize(), Stream->GetLastError()));
		return nullptr;
	}

	// --- CONNECT ------------------------------------------------------------

	KString sBody;

	kmqtt::AppendString(sBody, s_sProtocolName);
	sBody += static_cast<char>(s_iProtocolLevel);

	// clean session (bit 1): we hold no state worth resuming at QoS 0
	uint8_t iFlags = 0x02;

	// The standard forbids a password without a user name, and brokers do
	// enforce it — announcing one credential and sending the other is a
	// frequent cause of a bare "not authorized" that looks like a wrong
	// password. A password on its own is therefore dropped rather than sent.
	bool bSendUser     = !m_sUser.empty();
	bool bSendPassword = bSendUser && !m_sPassword.empty();

	if (bSendUser)     iFlags |= 0x80;
	if (bSendPassword) iFlags |= 0x40;

	sBody += static_cast<char>(iFlags);

	// Keep alive 0: the broker must not drop us for being quiet. The reading
	// session is not allowed to write a PINGREQ (its socket belongs to the
	// reader thread alone), and the publishing session has nothing to say
	// between publishes. Death is detected by the silence watchdog on one side
	// and by a failing write on the other.
	sBody += static_cast<char>(0);
	sBody += static_cast<char>(0);

	kmqtt::AppendString(sBody, sSessionID);

	if (bSendUser)     kmqtt::AppendString(sBody, m_sUser);
	if (bSendPassword) kmqtt::AppendString(sBody, m_sPassword);

	if (!kmqtt::WritePacket(*Stream, kmqtt::Connect << 4, sBody))
	{
		SetError("cannot send CONNECT");
		return nullptr;
	}

	// --- CONNACK ------------------------------------------------------------

	uint8_t iFirstByte { 0 };
	KString sResponse;

	if (!kmqtt::ReadPacket(*Stream, iFirstByte, sResponse, m_iMaxPacketSize))
	{
		SetError("no CONNACK from the broker");
		return nullptr;
	}

	if ((iFirstByte >> 4) != kmqtt::ConnAck || sResponse.size() < 2)
	{
		SetError("malformed CONNACK");
		return nullptr;
	}

	uint8_t iReturnCode = static_cast<uint8_t>(sResponse[1]);

	if (iReturnCode != 0)
	{
		// the codes the standard defines; anything else is the broker's own
		KStringView sReason = "refused";

		switch (iReturnCode)
		{
			case 1:  sReason = "protocol version not supported"; break;
			case 2:  sReason = "client id rejected";             break;
			case 3:  sReason = "broker unavailable";             break;
			case 4:  sReason = "bad user name or password";      break;
			case 5:  sReason = "not authorized";                 break;
			default: break;
		}

		SetError(kFormat("broker refused the connection: {}", sReason));
		return nullptr;
	}

	kDebug(1, "session '{}' connected to {}", sSessionID, EndPoint.Serialize());

	return Stream;

} // OpenSession

//-----------------------------------------------------------------------------
bool KMQTTClient::SendSubscribe(KIOStreamSocket& Stream, KStringView sTopicFilter)
//-----------------------------------------------------------------------------
{
	uint16_t iPacketID = m_iPacketID++;

	KString sBody;

	sBody += static_cast<char>((iPacketID >> 8) & 0xFF);
	sBody += static_cast<char>( iPacketID       & 0xFF);
	kmqtt::AppendString(sBody, sTopicFilter);
	sBody += static_cast<char>(0);   // QoS 0

	// the standard prescribes the lower nibble 0b0010 for SUBSCRIBE
	return kmqtt::WritePacket(Stream, (kmqtt::Subscribe << 4) | 0x02, sBody);

} // SendSubscribe

//-----------------------------------------------------------------------------
void KMQTTClient::HandlePacket(uint8_t iFirstByte, const KString& sBody)
//-----------------------------------------------------------------------------
{
	uint8_t iType = iFirstByte >> 4;

	switch (iType)
	{
		case kmqtt::Publish:
		{
			std::size_t iPos { 0 };
			KStringView sTopic;

			if (!kmqtt::ReadString(sBody, iPos, sTopic))
			{
				kDebug(1, "malformed PUBLISH");
				return;
			}

			// a packet identifier only exists above QoS 0, and we never
			// subscribe above it - so whatever follows the topic is payload
			uint8_t iQoS = (iFirstByte >> 1) & 0x03;

			if (iQoS > 0) iPos += 2;

			if (iPos > sBody.size()) return;

			// the payload may legitimately be empty - some brokers use that to
			// say "this thing is gone", so it must not be swallowed here
			KStringView sPayload(sBody.data() + iPos, sBody.size() - iPos);

			if (m_OnMessage) m_OnMessage(sTopic, sPayload);
			break;
		}

		case kmqtt::PingResp:
		case kmqtt::SubAck:
		case kmqtt::UnsubAck:
			// nothing to do: at QoS 0 there is no delivery state to track, and
			// a failed subscription shows up as silence on that topic
			break;

		default:
			kDebug(2, "ignoring packet type {}", iType);
			break;
	}

} // HandlePacket

//-----------------------------------------------------------------------------
void KMQTTClient::Listen()
//-----------------------------------------------------------------------------
{
	while (!m_bStop)
	{
		// the silence timeout is the watchdog: a dekaf2 read timeout closes
		// the socket, which is precisely the wanted reaction to a connection
		// that has gone quiet for longer than any message interval
		m_ReadStream = OpenSession(kFormat("{}-sub", m_sClientID), m_SilenceTimeout);

		if (m_ReadStream)
		{
			// doubles as the dedup set for pending subscriptions handed over
			// while this session runs
			std::vector<KString> SessionTopics = *m_Subscriptions.shared();

			bool bSubscribed = true;

			for (const KString& sTopic : SessionTopics)
			{
				// without this a reconnected client sits there looking healthy
				// and deaf
				if (!SendSubscribe(*m_ReadStream, sTopic))
				{
					SetError(kFormat("cannot subscribe to {}", sTopic));
					bSubscribed = false;
					break;
				}
			}

			// Subscriptions handed over while running are sent here and after
			// every received packet — this is the one thread allowed to write
			// on this socket, and at these points it is between two reads.
			auto DrainPending = [&]() -> bool
			{
				if (m_PendingSubscriptions.shared()->empty()) return true;

				std::vector<KString> Pending;
				Pending.swap(*m_PendingSubscriptions.unique());

				for (KString& sTopic : Pending)
				{
					// already covered by the replay at connect time
					if (std::find(SessionTopics.begin(), SessionTopics.end(), sTopic) != SessionTopics.end()) continue;

					if (!SendSubscribe(*m_ReadStream, sTopic)) return false;

					SessionTopics.push_back(std::move(sTopic));
				}

				return true;
			};

			// a topic subscribed between the snapshot above and now sits in
			// the pending list — send it before declaring the session up
			if (bSubscribed) bSubscribed = DrainPending();

			if (bSubscribed)
			{
				kDebug(1, "listening with {} subscriptions", SessionTopics.size());

				m_bConnected = true;

				if (m_OnState) m_OnState(true);

				uint8_t iFirstByte { 0 };
				KString sBody;

				while (!m_bStop && kmqtt::ReadPacket(*m_ReadStream, iFirstByte, sBody, m_iMaxPacketSize))
				{
					HandlePacket(iFirstByte, sBody);

					if (!DrainPending()) break;
				}

				m_bConnected = false;

				if (m_OnState) m_OnState(false);

				kDebug(1, "disconnected from {}", m_URL.Serialize());
			}
		}

		m_ReadStream.reset();

		// backoff before the next attempt, but stay responsive to Stop()
		for (uint16_t iTenth = 0; iTenth < s_ReconnectDelay.count() * 10 && !m_bStop; ++iTenth)
		{
			kSleep(chrono::milliseconds(100));
		}
	}

} // Listen

//-----------------------------------------------------------------------------
void KMQTTClient::Start()
//-----------------------------------------------------------------------------
{
	Stop();

	m_bStop  = false;
	m_Reader = std::thread(&KMQTTClient::Listen, this);

} // Start

//-----------------------------------------------------------------------------
void KMQTTClient::Stop()
//-----------------------------------------------------------------------------
{
	m_bStop = true;

	// closing the socket is what unblocks a reader sitting in Read(). It is
	// touched from here rather than from the reader itself, which is the one
	// exception to the single thread rule — and a safe one, because a close
	// only cancels, it does not post a new operation.
	if (m_ReadStream) m_ReadStream->Disconnect();

	if (m_Reader.joinable()) m_Reader.join();

	m_ReadStream.reset();

	{
		std::lock_guard<std::mutex> Lock(m_WriteMutex);
		m_WriteStream.reset();
	}

	m_bConnected = false;

} // Stop

//-----------------------------------------------------------------------------
bool KMQTTClient::Subscribe(KString sTopicFilter)
//-----------------------------------------------------------------------------
{
	if (sTopicFilter.empty() || sTopicFilter.size() > kmqtt::MaxStringLength) return false;

	{
		KThreadSafe<std::vector<KString>>::UniqueLocked Subscriptions = m_Subscriptions.unique();

		for (const KString& sKnown : *Subscriptions)
		{
			if (sKnown == sTopicFilter) return true;
		}

		Subscriptions->push_back(sTopicFilter);
	}

	// always handed to the reader thread, whatever the connection state: it
	// deduplicates against the session replay, and a stale entry is cheaper
	// than the race window between the reader's replay snapshot and its
	// connected flag
	m_PendingSubscriptions.unique()->push_back(std::move(sTopicFilter));

	return true;

} // Subscribe

//-----------------------------------------------------------------------------
bool KMQTTClient::Unsubscribe(KStringView sTopicFilter)
//-----------------------------------------------------------------------------
{
	// remove it from the pending list as well - it may not have been sent yet
	{
		KThreadSafe<std::vector<KString>>::UniqueLocked Pending = m_PendingSubscriptions.unique();

		for (std::size_t iIndex = 0; iIndex < Pending->size(); ++iIndex)
		{
			if ((*Pending)[iIndex] == sTopicFilter)
			{
				Pending->erase(Pending->begin() + iIndex);
				break;
			}
		}
	}

	KThreadSafe<std::vector<KString>>::UniqueLocked Subscriptions = m_Subscriptions.unique();

	for (std::size_t iIndex = 0; iIndex < Subscriptions->size(); ++iIndex)
	{
		if ((*Subscriptions)[iIndex] == sTopicFilter)
		{
			Subscriptions->erase(Subscriptions->begin() + iIndex);
			return true;
		}
	}

	return false;

} // Unsubscribe

//-----------------------------------------------------------------------------
bool KMQTTClient::Publish(KStringView sTopic, KStringView sPayload)
//-----------------------------------------------------------------------------
{
	if (sTopic.empty() || sTopic.size() > kmqtt::MaxStringLength) return false;

	std::lock_guard<std::mutex> Lock(m_WriteMutex);

	// opened on first use and re-opened after a failure — this session exists
	// only so that publishing never touches the reader's socket
	if (!m_WriteStream)
	{
		m_WriteStream = OpenSession(kFormat("{}-pub", m_sClientID), s_WriteTimeout);

		if (!m_WriteStream) return false;
	}

	KString sBody;

	kmqtt::AppendString(sBody, sTopic);
	sBody += sPayload;

	// QoS 0, not retained, not a duplicate - all flag bits stay zero
	if (kmqtt::WritePacket(*m_WriteStream, kmqtt::Publish << 4, sBody)) return true;

	// a broken publishing session is dropped so the next call rebuilds it
	SetError("publish failed, dropping the publishing session");
	m_WriteStream.reset();

	return false;

} // Publish

//-----------------------------------------------------------------------------
KString KMQTTClient::LastError() const
//-----------------------------------------------------------------------------
{
	return *m_sLastError.shared();

} // LastError

//-----------------------------------------------------------------------------
void KMQTTClient::SetError(KString sError)
//-----------------------------------------------------------------------------
{
	kDebug(1, sError);

	*m_sLastError.unique() = std::move(sError);

} // SetError

DEKAF2_NAMESPACE_END
