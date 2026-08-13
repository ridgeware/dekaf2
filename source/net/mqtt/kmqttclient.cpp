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

/// connect timeout, and the deadline for completing a started packet or
/// answering a PINGREQ - the silence watchdog is a separate, longer clock
constexpr auto         s_ProtocolTimeout = chrono::seconds(10);

/// granularity of the reader loop: the longest wait until queued
/// subscriptions and messages go out
constexpr auto         s_PollInterval    = chrono::milliseconds(100);

/// bound for the publish queue - above it, Publish() refuses
constexpr std::size_t  s_iMaxQueuedMessages = 1024;

} // end of anonymous namespace

//-----------------------------------------------------------------------------
KMQTTClient::KMQTTClient(KURL URL, KString sClientID)
//-----------------------------------------------------------------------------
: m_URL      (std::move(URL))
, m_sClientID(std::move(sClientID))
{
	if (m_URL.Port.empty())
	{
		auto iPort = m_URL.Protocol.DefaultPort();
		// a broker address without scheme defaults to plain mqtt
		m_URL.Port = KString::to_string(iPort ? iPort : 1883);
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
std::unique_ptr<KIOStreamSocket> KMQTTClient::OpenSession()
//-----------------------------------------------------------------------------
{
	bool bTLS = m_URL.Protocol.WrapInTLS();

	KTCPEndPoint EndPoint(m_URL.Domain, m_URL.Port);

	KStreamOptions Options(s_ProtocolTimeout);

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

	// Keep alive 0: the broker must not drop us for being quiet. Death is
	// detected on our side instead: the silence watchdog probes with a
	// PINGREQ and reconnects when the broker stays mute.
	sBody += static_cast<char>(0);
	sBody += static_cast<char>(0);

	kmqtt::AppendString(sBody, m_sClientID);

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

	kDebug(1, "session '{}' connected to {}", m_sClientID, EndPoint.Serialize());

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
bool KMQTTClient::SendUnsubscribe(KIOStreamSocket& Stream, KStringView sTopicFilter)
//-----------------------------------------------------------------------------
{
	uint16_t iPacketID = m_iPacketID++;

	KString sBody;

	sBody += static_cast<char>((iPacketID >> 8) & 0xFF);
	sBody += static_cast<char>( iPacketID       & 0xFF);
	kmqtt::AppendString(sBody, sTopicFilter);

	// the standard prescribes the lower nibble 0b0010 for UNSUBSCRIBE
	return kmqtt::WritePacket(Stream, (kmqtt::Unsubscribe << 4) | 0x02, sBody);

} // SendUnsubscribe

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
		m_Stream = OpenSession();

		if (m_Stream)
		{
			// doubles as the dedup set for subscriptions handed over while
			// this session runs
			std::vector<KString> SessionTopics = *m_Subscriptions.shared();

			bool bSessionGood = true;

			for (const KString& sTopic : SessionTopics)
			{
				// without this a reconnected client sits there looking healthy
				// and deaf
				if (!SendSubscribe(*m_Stream, sTopic))
				{
					SetError(kFormat("cannot subscribe to {}", sTopic));
					bSessionGood = false;
					break;
				}
			}

			// everything the public calls have queued up since the last drain:
			// subscriptions, unsubscriptions, and published messages
			auto DrainQueues = [&]() -> bool
			{
				if (!m_PendingSubscriptions.shared()->empty())
				{
					std::vector<KString> Pending;
					Pending.swap(*m_PendingSubscriptions.unique());

					for (KString& sTopic : Pending)
					{
						// already covered by the replay at connect time
						if (std::find(SessionTopics.begin(), SessionTopics.end(), sTopic) != SessionTopics.end()) continue;

						if (!SendSubscribe(*m_Stream, sTopic)) return false;

						SessionTopics.push_back(std::move(sTopic));
					}
				}

				if (!m_PendingUnsubscribes.shared()->empty())
				{
					std::vector<KString> Pending;
					Pending.swap(*m_PendingUnsubscribes.unique());

					for (const KString& sTopic : Pending)
					{
						SessionTopics.erase(std::remove(SessionTopics.begin(), SessionTopics.end(), sTopic), SessionTopics.end());

						if (!SendUnsubscribe(*m_Stream, sTopic)) return false;
					}
				}

				if (!m_PendingPublishes.shared()->empty())
				{
					std::vector<Message> Pending;
					Pending.swap(*m_PendingPublishes.unique());

					for (const Message& Msg : Pending)
					{
						KString sBody;

						kmqtt::AppendString(sBody, Msg.sTopic);
						sBody += Msg.sPayload;

						// QoS 0, not retained, not a duplicate - all flag bits stay zero
						if (!kmqtt::WritePacket(*m_Stream, kmqtt::Publish << 4, sBody)) return false;
					}
				}

				return true;
			};

			// a topic subscribed between the snapshot above and now sits in
			// the pending list - send it before declaring the session up
			if (bSessionGood) bSessionGood = DrainQueues();

			if (bSessionGood)
			{
				kDebug(1, "listening with {} subscriptions", SessionTopics.size());

				m_bConnected = true;

				if (m_OnState) m_OnState(true);

				// the silence watchdog: probe with a PINGREQ after the silence
				// timeout, reconnect when the broker does not answer either
				KStopTime SinceLastPacket;
				KStopTime SincePing (KStopTime::Halted);
				bool      bPingSent { false };

				uint8_t iFirstByte { 0 };
				KString sBody;

				while (!m_bStop)
				{
					if (m_Stream->IsReadReady(s_PollInterval))
					{
						// the packet is (partially) here - ReadPacket blocks
						// until it is complete, a torn packet dies with the
						// stream timeout
						if (!kmqtt::ReadPacket(*m_Stream, iFirstByte, sBody, m_iMaxPacketSize)) break;

						HandlePacket(iFirstByte, sBody);

						SinceLastPacket.clear();
						bPingSent = false;
					}
					else if (!m_Stream->is_open())
					{
						break;
					}

					if (!DrainQueues()) break;

					if (!bPingSent)
					{
						if (SinceLastPacket.elapsed() > m_SilenceTimeout)
						{
							if (!kmqtt::WritePacket(*m_Stream, kmqtt::PingReq << 4, "")) break;

							bPingSent = true;
							SincePing.clear();
						}
					}
					else if (SincePing.elapsed() > s_ProtocolTimeout)
					{
						SetError("broker does not answer the PINGREQ");
						break;
					}
				}

				// on a controlled stop, hand the queued messages to the wire
				// before hanging up
				if (m_bStop && m_Stream->is_open()) DrainQueues();

				m_bConnected = false;

				if (m_OnState) m_OnState(false);

				kDebug(1, "disconnected from {}", m_URL.Serialize());
			}
		}

		m_Stream.reset();

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

	if (m_Reader.joinable()) m_Reader.join();

	m_Stream.reset();

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

	{
		KThreadSafe<std::vector<KString>>::UniqueLocked Subscriptions = m_Subscriptions.unique();

		bool bFound = false;

		for (std::size_t iIndex = 0; iIndex < Subscriptions->size(); ++iIndex)
		{
			if ((*Subscriptions)[iIndex] == sTopicFilter)
			{
				Subscriptions->erase(Subscriptions->begin() + iIndex);
				bFound = true;
				break;
			}
		}

		if (!bFound) return false;
	}

	// the reader thread sends the UNSUBSCRIBE within the poll interval; a
	// filter this session never subscribed gets one anyway, which is harmless
	m_PendingUnsubscribes.unique()->push_back(KString(sTopicFilter));

	return true;

} // Unsubscribe

//-----------------------------------------------------------------------------
bool KMQTTClient::Publish(KStringView sTopic, KStringView sPayload)
//-----------------------------------------------------------------------------
{
	if (sTopic.empty() || sTopic.size() > kmqtt::MaxStringLength) return false;

	// asynchronous by design: enqueued here, sent by the reader thread within
	// the poll interval - the caller never touches (or waits for) the socket
	KThreadSafe<std::vector<Message>>::UniqueLocked Pending = m_PendingPublishes.unique();

	if (Pending->size() >= s_iMaxQueuedMessages)
	{
		SetError("publish queue is full");
		return false;
	}

	Pending->push_back({ KString(sTopic), KString(sPayload) });

	return true;

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
