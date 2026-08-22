/*
 //
 // DEKAF(tm): Lighter, Faster, Smarter (tm)
 //
 // Copyright (c) 2025, Ridgeware, Inc.
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

// The control and data streams between the tunnel endpoints are multiplexed
// by a simple protocol over a single stream.
//
// This protocol extends the websocket protocol with the ability to add a detailed
// message type, and the multiplex channel ID. Additionally, all websocket payload
// data can be encrypted with aes256-GCM and a session key, which allows to
// even use unsecure websockets and protect the exchanged data against eavesdropping
// and tampering. This payload encryption can also be used if the TLS certificate
// store can not be trusted.
//
// - the payload data length and other details of the transport is encoded in
//   the standard websocket header, a value of 4 is substracted from the payload
//   length to account for the embedded ktunnel protocol header, which consists of:
// - an 8 bit message type
// - the message type is followed by a 24 bit channel ID, in network order
//   (MSB first)
// - channel ID 0 is the control channel
// - channel IDs from 1 upward are used for end-to-end tcp connections
//
// Message types are:
// - Login, Control, Connect, Data, Pause, Resume, Disconnect, Ping, Pong
// - Login messages bear the endpoint node name and its secret as "Basic name:secret",
//   where name:secret is encoded in base64, much like in basic auth. There
//   may be other schemes by future implementations.
// - Ping/Pong messages are used to keep the tunnel alive when there is no
//   other communication, and to check for its health and for the health of
//   specific channels. The websocket ping is not used. Pings always start
//   from the connecting end, as that is the side that has to re-establish
//   a lost connection (but would be supported the other way round as well).
//   A ping can contain a payload, which has to be echoed in the answer pong.
// - Control messages are as of now unused, and would only be valid at channel 0.
// - Connect messages contain a string with the endpoint address to connect
//   to, either as domain:port or ipv4:port or [ipv6]:port, note the [] around
//   the ipv6 address part
// - Data messages contain the payload data in either direction.
// - Disconnect messages indicate a disconnect from one side to the other
// - Pause and Resume messages implement a flow control per individual data
//   channel, telling the respective other end of the channel to pause or
//   resume transmission through the tunnel.
//
// The tunnel connection can transport up to 16 million tunneled streams.

#include <dekaf2/net/util/ktunnel.h>
#include <dekaf2/core/errors/kerror.h>
#include <dekaf2/net/tcp/ktcpstream.h>
#include <dekaf2/core/init/dekaf2.h>
#include <dekaf2/containers/sequential/kbuffer.h>
#include <dekaf2/core/types/kscopeguard.h>
#include <dekaf2/crypto/encoding/kencode.h>
#include <dekaf2/crypto/random/krandom.h>
#include <dekaf2/system/os/ksystem.h>
#include <dekaf2/threading/execution/kthreads.h>
#if DEKAF2_HAS_ED25519 && DEKAF2_HAS_X25519
	#include <dekaf2/crypto/ec/kx25519.h>           // ephemeral DH for v2 handshake
	#include <dekaf2/crypto/kdf/khkdf.h>            // HKDF-SHA256 for session-key derivation
	#include <dekaf2/crypto/hash/kmessagedigest.h>  // KSHA256 for FormatFingerprint
#endif

DEKAF2_NAMESPACE_BEGIN

//-----------------------------------------------------------------------------
KTunnel::Message::Message(Type _type, std::size_t iChannel, KString sMessage)
//-----------------------------------------------------------------------------
: Frame(std::move(sMessage), true)
{
	SetType(_type);
	SetChannel(iChannel);
}

//-----------------------------------------------------------------------------
KStringView KTunnel::Message::PrintType() const
//-----------------------------------------------------------------------------
{
	switch (GetType())
	{
		case Login:       return "Login";
		case Helo:        return "Helo";
		case Ping:        return "Ping";
		case Pong:        return "Pong";
		case Idle:        return "Idle";
		case Control:     return "Control";
		case Connect:     return "Connect";
		case Data:        return "Data";
		case Pause:       return "Pause";
		case Resume:      return "Resume";
		case Disconnect:  return "Disconnect";
		case OpenRepl:    return "OpenRepl";
		case OpenShell:   return "OpenShell";
		case ShellResize: return "ShellResize";
		case None:        return "None";
	}

	return "Invalid";

} // Message::PrintType

//-----------------------------------------------------------------------------
KStringView KTunnel::Message::PrintData() const
//-----------------------------------------------------------------------------
{
	switch (GetType())
	{
		case Login:
		case Helo:
		case Ping:
		case Pong:
		case Idle:
		case Control:
		case Connect:
		case Pause:
		case Resume:
		case Disconnect:
		case OpenRepl:
		case OpenShell:
		case ShellResize:
			if (!kIsBinary(GetMessage()))
			{
				return GetMessage();
			}
			return "";

		case None:
			return "";

		case Data:
		{
			KStringView sStart = GetMessage().LeftUTF8(20);

			if (!kIsBinary(sStart))
			{
				return sStart;
			}

			return "";
		}
	}

	return "";

} // Message::PrintData

//-----------------------------------------------------------------------------
KString KTunnel::Message::Debug() const
//-----------------------------------------------------------------------------
{
	return kFormat("[{}]: {} {} chars: {}", GetChannel(), PrintType(), size(), PrintData());
}

namespace {

//-----------------------------------------------------------------------------
// describe an unexpected handshake frame: type byte, payload size, and a short
// hex (plus text, if printable) sample of the payload - enough to tell a
// foreign protocol, a version mismatch, or a scanner apart in the event log
KString DescribeUnexpectedFrame(const KTunnel::Message& Frame)
//-----------------------------------------------------------------------------
{
	auto sSample = KStringView(Frame.GetMessage()).Left(16);

	KString sInfo = kFormat("type byte {}, {} payload bytes",
	                        static_cast<unsigned>(Frame.GetType()), Frame.size());

	if (!sSample.empty())
	{
		sInfo += kFormat(", starts with 0x{}", KEncode::Hex(sSample));

		if (!kIsBinary(sSample))
		{
			sInfo += kFormat(" ('{}')", sSample);
		}
	}

	return sInfo;

} // DescribeUnexpectedFrame

} // end of anonymous namespace

//-----------------------------------------------------------------------------
void KTunnel::Message::SetChannel(std::size_t iChannel)
//-----------------------------------------------------------------------------
{
	if (iChannel > MaxChannel())
	{
		Throw(kFormat("channel too high: {:x}, max: {:x}", iChannel, MaxChannel()));
	}

	auto it = m_Preamble.begin();
	*++it = (iChannel >> 16) & 0xff;
	*++it = (iChannel >>  8) & 0xff;
	*++it =  iChannel        & 0xff;

} // SetChannel

//-----------------------------------------------------------------------------
std::size_t KTunnel::Message::GetChannel() const
//-----------------------------------------------------------------------------
{
	std::size_t iChannel;
	auto it = m_Preamble.begin();

	// read as unsigned - char is signed on most platforms, and any byte above
	// 0x7f would else be sign extended into a huge channel number
	iChannel  = static_cast<unsigned char>(*++it);
	iChannel *= 256;
	iChannel += static_cast<unsigned char>(*++it);
	iChannel *= 256;
	iChannel += static_cast<unsigned char>(*++it);

	return iChannel;

} // GetChannel

//-----------------------------------------------------------------------------
std::size_t KTunnel::Message::GetPreambleSize() const
//-----------------------------------------------------------------------------
{
	return m_Preamble.size();
}

//-----------------------------------------------------------------------------
char* KTunnel::Message::GetPreambleBuf() const
//-----------------------------------------------------------------------------
{
	return m_Preamble.data();
}

//-----------------------------------------------------------------------------
bool KTunnel::Message::Encode(KStringView sInput, KString& sEncoded)
//-----------------------------------------------------------------------------
{
	if (!m_Cipher)
	{
		kDebug(1, "no cipher");
		return false;
	}

	if (m_Cipher->GetDirection() != KBlockCipher::Direction::Encrypt)
	{
		kDebug(1, "wrong direction");
		return false;
	}

	if (!m_Cipher->SetOutput(sEncoded) ||
		!m_Cipher->Add(sInput)         ||
		!m_Cipher->Finalize())
	{
		return false;
	}

	return true;

} // Encode

//-----------------------------------------------------------------------------
bool KTunnel::Message::Decode(KStringView sEncoded, KString& sDecoded)
//-----------------------------------------------------------------------------
{
	if (!m_Cipher)
	{
		kDebug(1, "no cipher");
		return false;
	}

	if (m_Cipher->GetDirection() != KBlockCipher::Direction::Decrypt)
	{
		kDebug(1, "wrong direction");
		return false;
	}

	if (!m_Cipher->SetOutput(sDecoded) ||
		!m_Cipher->Add(sEncoded)       ||
		!m_Cipher->Finalize())
	{
		return false;
	}

	return true;

} // Decode

//-----------------------------------------------------------------------------
void KTunnel::Message::Read(KIOStreamSocket& Stream, KBlockCipher* Decryptor)
//-----------------------------------------------------------------------------
{
	clear();

	if (Decryptor)
	{
		m_Cipher = Decryptor;
		SetHaveEncoder(true);
	}

	if (!Frame::Read(Stream, false))
	{
		Throw(kFormat("[{}]: cannot read from {}", GetChannel(), Stream.GetEndPointAddress()));
	}

} // Read

//-----------------------------------------------------------------------------
void KTunnel::Message::Write(KIOStreamSocket& Stream, bool bMask, KBlockCipher* Encryptor)
//-----------------------------------------------------------------------------
{
	// Valid range: None (0) .. highest defined type. When adding new
	// Message::Type values, bump the upper bound accordingly.
	if (GetType() > ShellResize)
	{
		Throw("invalid type");
	}

	if (Encryptor)
	{
		m_Cipher = Encryptor;
		SetHaveEncoder(true);
	}

	if (!Frame::Write(Stream, bMask))
	{
		Throw(kFormat("[{}]: cannot write to {}", GetChannel(), Stream.GetEndPointAddress()));
	}

	if (bMask)
	{
		// the frame is now toast because of the websocket xoring, better clear it right away
		clear();
	}

} // Write

//-----------------------------------------------------------------------------
bool KTunnel::Message::Decrypt(KBlockCipher* Decryptor)
//-----------------------------------------------------------------------------
{
	if (Decryptor)
	{
		m_Cipher = Decryptor;
		SetHaveEncoder(true);
	}

	return TryDecode();

} // Decrypt

//-----------------------------------------------------------------------------
void KTunnel::Message::clear()
//-----------------------------------------------------------------------------
{
	*this = Message();
}

//-----------------------------------------------------------------------------
void KTunnel::Message::Throw(KString sError, KStringView sFunction) const
//-----------------------------------------------------------------------------
{
	KLog::getInstance().debug_fun(1, sFunction, sError);
	throw KError(sError);

} // Throw

//-----------------------------------------------------------------------------
void KTunnel::Connection::Resume()
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_TunnelMutex);

	if (!m_bPaused)
	{
		// return right here, do not send notifications
		return;
	}

	// resume
	m_bPaused = false;

	// wake a Pump() that stopped polling for readability while paused
	m_Wakeup.Wake();

} // Resume

//-----------------------------------------------------------------------------
void KTunnel::Connection::Pump()
//-----------------------------------------------------------------------------
{
	if (!m_DirectStream || !m_Tunnel)
	{
		return;
	}

	// name this thread for debugging tools like ps, top, or gdb
	kSetThreadName("tunnelpump");

	try
	{
		KReservedBuffer<KDefaultCopyBufSize - 12> Data;

		// the outbound message currently going to the direct stream, and how
		// much of it already went out - a partial write is normal here
		Message     ToDirect;
		bool        bHaveOutbound { false };
		std::size_t iWritten      { 0 };

		// no I/O in either direction for this long tears the connection down -
		// the same deadline the stream timeout used to enforce on its own
		auto      IdleTimeout = m_DirectStream->GetTimeout();
		KStopTime Idle;

		for (;!m_bQuit;)
		{
			// somebody closed the direct stream on us - same reasoning as in
			// KTunnel::Run(), do not poll a descriptor that is gone
			if (m_DirectStream->IsDisconnecting()) break;

			// ---- pick up the next outbound message ---------------------------

			if (!bHaveOutbound)
			{
				bool bDisconnect { false };

				{
					std::unique_lock<std::mutex> Lock(m_QueueMutex);

					while (!m_MessageQueue.empty())
					{
						auto& Msg = m_MessageQueue.front();

						if (Msg.GetType() == Message::Disconnect)
						{
							kDebug(3, "[{}]: got disconnect frame for {}", GetID(), m_DirectStream->GetEndPointAddress());
							bDisconnect = true;
							m_MessageQueue.pop();
							break;
						}

						if (Msg.GetType() == Message::Data)
						{
							ToDirect      = std::move(Msg);
							bHaveOutbound = true;
							iWritten      = 0;
							m_MessageQueue.pop();
							break;
						}

						// nothing we would write to the direct stream
						m_MessageQueue.pop();
					}

					// check if we had formerly sent a Pause frame, and we have again room
					// in the output queue
					if (m_bRXPaused && m_MessageQueue.size() < MaxMessageQueueSize() / 2)
					{
						m_bRXPaused = false;
						Lock.unlock();
						// request to resume from the other end of the tunnel
						m_Tunnel(Message(Message::Resume, GetID()));
					}
				}

				if (bDisconnect)
				{
					Disconnect();
					break;
				}
			}

			// ---- what can we do right now, without waiting? -------------------

			// Both directions are asked separately and with a zero timeout: a
			// combined CheckIfReady() would report POLLIN alone as soon as any
			// layer holds readable bytes (streambuf, or an undrained TLS record),
			// and the write side would starve exactly under load.
			bool bCanWrite = bHaveOutbound && m_DirectStream->IsWriteReady(KDuration(), false);
			// while paused we must not read - the other tunnel end asked us to
			// stop feeding it
			bool bCanRead  = !IsPaused()   && m_DirectStream->IsReadReady (KDuration(), false);

			bool bDidIO { false };

			// ---- write whatever the stream takes right now --------------------

			if (bCanWrite)
			{
				const auto& sPayload = ToDirect.GetMessage();

				auto iWrote = m_DirectStream->direct_write_some(sPayload.data() + iWritten,
				                                               sPayload.size() - iWritten);

				if (iWrote < 0)
				{
					kDebug(2, "[{}]: cannot write to {}", GetID(), m_DirectStream->GetEndPointAddress());
					break;
				}

				if (iWrote > 0)
				{
					bDidIO    = true;
					iWritten += static_cast<std::size_t>(iWrote);
					m_iBytesToDirect.fetch_add(static_cast<uint64_t>(iWrote), std::memory_order_relaxed);

					kDebug(3, "[{}]: {}: wrote {} of {} chars", GetID(),
					       m_DirectStream->GetEndPointAddress(), iWritten, sPayload.size());

					if (iWritten >= sPayload.size())
					{
						// this one is out, the next round picks up the next
						bHaveOutbound = false;
						iWritten      = 0;
					}
				}
			}

			// ---- read and forward into the tunnel ----------------------------

			if (bCanRead)
			{
				auto iRead = m_DirectStream->direct_read_some(Data.data(), Data.capacity());

				if (iRead <= 0)
				{
					// disconnected
					kDebug(2, "[{}]: disconnected from {}", GetID(), m_DirectStream->GetEndPointAddress());
					break;
				}

				bDidIO = true;
				Data.resize(iRead);
				m_iBytesFromDirect.fetch_add(static_cast<uint64_t>(iRead), std::memory_order_relaxed);
				kDebug(3, "[{}]: {}: read {} chars", GetID(), m_DirectStream->GetEndPointAddress(), iRead);
				m_Tunnel(Message(Message::Data, m_iID, KStringView(Data.data(), Data.size())));
				Data.reset();
			}

			if (bDidIO)
			{
				// there was work - look for more before going to sleep
				Idle.clear();
				continue;
			}

			// ---- neither direction can proceed - wait ------------------------

			short iWhat { POLLHUP };

			if (!IsPaused())   iWhat |= POLLIN;
			if (bHaveOutbound) iWhat |= POLLOUT;

			struct pollfd fds[2];
			std::size_t   iCount { 1 };

			fds[0].fd      = m_DirectStream->GetNativeSocket();
			fds[0].events  = iWhat;
			fds[0].revents = 0;

			if (m_Wakeup.IsValid())
			{
				fds[1].fd      = m_Wakeup.GetFD();
				fds[1].events  = POLLIN;
				fds[1].revents = 0;
				iCount         = 2;
			}

			// without a working interruptor (Windows) nobody can wake us, so we
			// wait in short slices to keep the outbound latency low
			auto Timeout = m_Wakeup.IsValid() ? IdleTimeout : KDuration(chrono::milliseconds(20));

			auto iResult = kPoll(KSpan<pollfd>(fds, iCount), Timeout);

			if (iResult < 0)
			{
				kDebug(2, "[{}]: poll error for {}", GetID(), m_DirectStream->GetEndPointAddress());
				break;
			}

			if (iResult > 0 && iCount == 2 && fds[1].revents)
			{
				// woken from another thread (queued data, resume, disconnect) -
				// drain the interruptor and re-evaluate at the top
				m_Wakeup.Clear();
				continue;
			}

			if (iResult > 0 && (fds[0].revents & (POLLHUP | POLLERR | POLLNVAL)) != 0)
			{
				kDebug(2, "[{}]: got disconnected from {}", GetID(), m_DirectStream->GetEndPointAddress());
				break;
			}

			if (iResult == 0 && Idle.elapsed() >= IdleTimeout)
			{
				kDebug(2, "[{}]: timeout from {}", GetID(), m_DirectStream->GetEndPointAddress());
				break;
			}
		}

		m_Tunnel(Message(Message::Disconnect, m_iID));
	}
	catch (const std::exception& ex)
	{
		kDebug(1, ex.what());
	}

} // Connection::Pump

//-----------------------------------------------------------------------------
void KTunnel::Connection::SetTarget(KTCPEndPoint Target)
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_QueueMutex);
	m_Target = std::move(Target);
}

//-----------------------------------------------------------------------------
KTCPEndPoint KTunnel::Connection::GetTarget() const
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_QueueMutex);
	return m_Target;
}

//-----------------------------------------------------------------------------
void KTunnel::Connection::SetPeer(KString sPeer)
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_QueueMutex);
	m_sPeer = std::move(sPeer);
}

//-----------------------------------------------------------------------------
KString KTunnel::Connection::GetPeer() const
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_QueueMutex);
	return m_sPeer;
}

//-----------------------------------------------------------------------------
KString KTunnel::Connection::GetDisconnectReason() const
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_QueueMutex);
	return m_sDisconnectReason;
}

//-----------------------------------------------------------------------------
KString KTunnel::Connection::WaitForDisconnectReason(KDuration Timeout) const
//-----------------------------------------------------------------------------
{
	std::unique_lock<std::mutex> Lock(m_QueueMutex);

	// SendData() notifies m_FreshData on every arriving frame, including
	// the Disconnect that carries the reason
	m_FreshData.wait_for(Lock, Timeout.duration(), [this]
	{
		return !m_sDisconnectReason.empty();
	});

	return m_sDisconnectReason;
}

//-----------------------------------------------------------------------------
KTunnel::Connection::Activity KTunnel::Connection::WaitForActivity(KDuration Timeout) const
//-----------------------------------------------------------------------------
{
	std::unique_lock<std::mutex> Lock(m_QueueMutex);

	auto bActivity = m_FreshData.wait_for(Lock, Timeout.duration(), [this]
	{
		return !m_MessageQueue.empty() || m_bQuit.load(std::memory_order_relaxed);
	});

	if (!bActivity)
	{
		return Activity::Timeout;
	}

	if (!m_MessageQueue.empty())
	{
		// only Data and Disconnect frames are queued on a connection
		return m_MessageQueue.front().GetType() == Message::Disconnect
			? Activity::Closed
			: Activity::Data;
	}

	// m_bQuit triggered the wakeup
	return Activity::Closed;

} // Connection::WaitForActivity

//-----------------------------------------------------------------------------
void KTunnel::Connection::SendData(Message&& FromTunnel)
//-----------------------------------------------------------------------------
{
	KStopTime Stop;
	{
		std::lock_guard<std::mutex> Lock(m_QueueMutex);

		// an optional Disconnect payload carries the peer's error (e.g.
		// "cannot connect to target"). Capture it right here at frame
		// arrival: the pump may already have ended when it comes in
		// (a downstream that closed first), and a WaitForDisconnectReason
		// caller sits on m_FreshData for exactly this
		if (FromTunnel.GetType() == Message::Disconnect && !FromTunnel.GetMessage().empty())
		{
			m_sDisconnectReason = FromTunnel.GetMessage();
		}

		m_MessageQueue.push(std::move(FromTunnel));

		kDebug(3, "[{}]: queued {} chars, took {}", GetID(), m_MessageQueue.back().size(), Stop.elapsedAndClear());

		if (!m_bRXPaused && m_MessageQueue.size() >= MaxMessageQueueSize())
		{
			m_Tunnel(Message(Message::Pause, GetID()));
			m_bRXPaused = true;
			kDebug(3, "[{}]: requested pause, took {}", GetID(), Stop.elapsedAndClear());
		}

		// for consumers that wait on the queue (ReadData on REPL channels)
		m_FreshData.notify_one();

	}

	// and for a Pump() that sits in its poll
	m_Wakeup.Wake();

	kDebug(3, "[{}]: notified the pump, took {}", GetID(), Stop.elapsed());

} // Connection::SendData

//-----------------------------------------------------------------------------
void KTunnel::Connection::Disconnect()
//-----------------------------------------------------------------------------
{
	m_bQuit = true;

	m_FreshData.notify_all();
	m_Wakeup.Wake();

	if (m_DirectStream)
	{
		m_DirectStream->Disconnect();
	}
}

//-----------------------------------------------------------------------------
bool KTunnel::Connection::ReadData(KString& sOut)
//-----------------------------------------------------------------------------
{
	// Consumer-side of the message queue for channels that do NOT have
	// a DirectStream (OpenRepl / future OpenShell). Mirrors the relevant
	// parts of PumpFromTunnel(): drain one frame, handle Disconnect as
	// "stream closed", and mirror the Resume-on-drain handshake so TX
	// pause is lifted once the queue drops below half-full.
	for (;;)
	{
		std::unique_lock<std::mutex> Lock(m_QueueMutex);

		// wait for data or shutdown
		m_FreshData.wait(Lock, [this]
		{
			return !m_MessageQueue.empty() || m_bQuit.load(std::memory_order_relaxed);
		});

		if (m_MessageQueue.empty())
		{
			// m_bQuit triggered the wakeup
			return false;
		}

		auto msg = std::move(m_MessageQueue.front());
		m_MessageQueue.pop();

		// symmetric with PumpFromTunnel: if we had previously asked the
		// other side to pause and we now have room again, resume it
		if (m_bRXPaused && m_MessageQueue.size() < MaxMessageQueueSize() / 2)
		{
			m_Tunnel(Message(Message::Resume, GetID()));
			m_bRXPaused = false;
		}

		// release the queue lock before returning / sending Disconnect
		Lock.unlock();

		if (msg.GetType() == Message::Data)
		{
			// GetMessage returns const& to the payload; msg is a local
			// that's about to go out of scope, so a copy into sOut is
			// acceptable.
			sOut = msg.GetMessage();
			return true;
		}

		if (msg.GetType() == Message::Disconnect)
		{
			m_bQuit = true;
			return false;
		}

		// any other type (shouldn't normally land here because the
		// tunnel read loop only forwards Data/Disconnect into the
		// connection queue, but be defensive): skip and keep reading
	}

} // Connection::ReadData

//-----------------------------------------------------------------------------
void KTunnel::Connection::WriteData(KString sPayload)
//-----------------------------------------------------------------------------
{
	if (m_bQuit.load(std::memory_order_relaxed) || !m_Tunnel)
	{
		return;
	}

	m_Tunnel(Message(Message::Data, m_iID, std::move(sPayload)));

} // Connection::WriteData

//-----------------------------------------------------------------------------
KTunnel::Connection::Connection (std::size_t iID, std::function<void(Message&&)> TunnelSend, KIOStreamSocket* DirectStream)
//-----------------------------------------------------------------------------
: m_Tunnel(std::move(TunnelSend))
, m_DirectStream(DirectStream)
, m_iID(iID)
{
}

//-----------------------------------------------------------------------------
std::size_t KTunnel::Connections::size() const
//-----------------------------------------------------------------------------
{
	return m_Connections.shared()->size();
}

//-----------------------------------------------------------------------------
std::vector<std::shared_ptr<KTunnel::Connection>> KTunnel::Connections::Snapshot() const
//-----------------------------------------------------------------------------
{
	std::vector<std::shared_ptr<Connection>> Result;

	auto Connections = m_Connections.shared();
	Result.reserve(Connections->size());

	for (const auto& kv : *Connections)
	{
		Result.push_back(kv.second);
	}

	return Result;

} // Connections::Snapshot

//-----------------------------------------------------------------------------
std::shared_ptr<KTunnel::Connection> KTunnel::Connections::Create(std::size_t iID, std::function<void(Message&&)> TunnelSend, KIOStreamSocket* DirectStream)
//-----------------------------------------------------------------------------
{
	auto Connections = m_Connections.unique();

	if (Connections->size() >= m_iMaxConnections)
	{
		kDebug(1, "max tunnel limit of {} reached - refusing new forward connection", m_iMaxConnections);

		return {};
	}

	bool bFindFreeID { false };

	std::size_t iIterations { 0 };

	for (;;)
	{
		if (iIterations == Message::MaxChannel())
		{
			kDebug(1, "cannot generate a new ID value");
			return {};
		}

		++iIterations;

		if (iID == 0)
		{
			bFindFreeID = true;
			iID = ++m_iConnection;

			// overflow?
			if (iID > Message::MaxChannel())
			{
				iID = m_iConnection = 1;
			}
		}
		else if (iID > Message::MaxChannel())
		{
			kDebug(1, "illegal ID value {}", iID);
			return {};
		}

		auto pair = Connections->insert({ iID, std::make_shared<Connection>(iID, TunnelSend, DirectStream) });

		if (!pair.second)
		{
			if (bFindFreeID)
			{
				iID = 0;
				continue;
			}

			kDebug(1, "connection ID {} already existing", iID);
			return {};
		}
		else
		{
			return pair.first->second;
		}
	}

} // Connections::Create

//-----------------------------------------------------------------------------
std::shared_ptr<KTunnel::Connection> KTunnel::Connections::Get(std::size_t iID, bool bAndRemove)
//-----------------------------------------------------------------------------
{
	auto Connections = m_Connections.unique();

	auto it = Connections->find(iID);

	if (it != Connections->end())
	{
		if (!bAndRemove)
		{
			return it->second;
		}

		auto Connection = it->second;

		Connections->erase(it);

		return Connection;
	}

	return {};

} // Connections::GetConnection

//-----------------------------------------------------------------------------
bool KTunnel::Connections::Remove(std::size_t iID)
//-----------------------------------------------------------------------------
{
	auto Connections = m_Connections.unique();

	auto it = Connections->find(iID);

	if (it != Connections->end())
	{
		kDebug(3, "removing ID {}", iID);
		Connections->erase(it);

		return true;
	}

	kDebug(1, "could not find ID {}", iID);
	return false;

} // Connections::RemoveConnection

//-----------------------------------------------------------------------------
bool KTunnel::Connections::Exists(std::size_t iID)
//-----------------------------------------------------------------------------
{
	auto Connections = m_Connections.shared();

	return Connections->find(iID) != Connections->end();

} // Connections::Exists

//-----------------------------------------------------------------------------
KTunnel::KTunnel
(
	Config                           Config,
	std::unique_ptr<KIOStreamSocket> Stream,
	KStringView                      sNode,
	KStringView                      sSecret,
	bool                             bNeverMask
)
//-----------------------------------------------------------------------------
: m_Config(Config)
, m_Connections(Config.iMaxTunneledConnections)
{
	if (!Stream)
	{
		throw KError("Stream is nullptr in KTunnel::KTunnel()");
	}

	{
		auto Tunnel = m_Tunnel.unique();
		Tunnel->Stream = std::move(Stream);

		// disable Nagle's algorithm on the tunnel stream to avoid
		// delaying the last TLS record of large transmissions
		Tunnel->Stream->SetNoDelay(true);
	}

	auto bIsClient = !sNode.empty() || !sSecret.empty();

	if (!bIsClient)
	{
		m_bWaitForLogin = true;
	}
	else
	{
		if (!bNeverMask)
		{
			m_bMaskTx = true;
		}

		Login(sNode, sSecret);
	}

} // KTunnel::KTunnel

//-----------------------------------------------------------------------------
KTunnel::~KTunnel()
//-----------------------------------------------------------------------------
{
	if (m_TimerID != KTimer::InvalidID)
	{
		Dekaf::getInstance().GetTimer().Cancel(m_TimerID);
	}

} // KTunnel::~KTunnel

//-----------------------------------------------------------------------------
void KTunnel::Stop()
//-----------------------------------------------------------------------------
{
	// Thread-safe stop: disconnecting the underlying stream makes
	// HaveTunnel() false, so Run() returns on its next loop iteration.
	{
		auto Tunnel = m_Tunnel.unique();

		if (Tunnel->Stream)
		{
			kDebug(2, "stopping tunnel");
			Tunnel->Stream->Disconnect();
		}
	}

	// and wake Run() out of its poll - closing a socket does not reliably
	// unblock a poll() that waits on it (that is what the interruptor is for),
	// so without this Run() would sit there until its idle timeout
	m_Wakeup.Wake();

} // KTunnel::Stop

//-----------------------------------------------------------------------------
KTunnel::ConnectResult KTunnel::Connect(KIOStreamSocket* DirectStream, const KTCPEndPoint& ConnectToEndpoint)
//-----------------------------------------------------------------------------
{
	if (!DirectStream || !DirectStream->Good())
	{
		throw KError("invalid stream socket");
	}

	auto Connection = m_Connections.Create(0, [this](Message&& msg){ QueueMessage(std::move(msg)); }, DirectStream);

	if (!Connection)
	{
		throw KError("cannot create new connection");
	}

	Connection->SetTarget(ConnectToEndpoint);
	Connection->SetPeer(DirectStream->GetEndPointAddress().Serialize());

	QueueMessage(Message(Message::Connect, Connection->GetID(), ConnectToEndpoint.Serialize()));

	auto iID = Connection->GetID();

	// we use a named lambda because we want compatibility with C++11, which needs
	// a type for KScopeGuard..
	auto namedLambdaGuard = [this, iID, DirectStream]() noexcept
	{
		kDebug(1, "[{}]: closed forward stream from {}", iID, DirectStream->GetEndPointAddress());
		// and remove this tunnel connection from the list of connections
		m_Connections.Remove(iID);
	};

	auto Guard = KScopeGuard<decltype(namedLambdaGuard)>(namedLambdaGuard);

	kDebug(1, "[{}]: requested forward connection to {}", iID, ConnectToEndpoint);

	// both directions on this thread - the stream tolerates only one
	Connection->Pump();

	ConnectResult Result;
	Result.sDisconnectReason = Connection->GetDisconnectReason();
	// on this side the direct stream is the downstream peer, so bytes
	// written to it came FROM the target, bytes read from it went TO it
	Result.iBytesToTarget    = Connection->GetBytesFromDirect();
	Result.iBytesFromTarget  = Connection->GetBytesToDirect();

	if (Result.sDisconnectReason.empty() && Result.iBytesFromTarget == 0)
	{
		// the target never answered - when the downstream closed first,
		// the peer's Disconnect frame with the error may still be in
		// flight, so give it a moment before reporting "no reason"
		Result.sDisconnectReason = Connection->WaitForDisconnectReason(chrono::milliseconds(250));
	}

	return Result;

} // Connect

//-----------------------------------------------------------------------------
KTunnel::ProbeResult KTunnel::ProbeConnect(const KTCPEndPoint& ConnectToEndpoint, KDuration Timeout)
//-----------------------------------------------------------------------------
{
	ProbeResult Result;

	auto Connection = m_Connections.Create(0, [this](Message&& msg){ QueueMessage(std::move(msg)); }, nullptr);

	if (!Connection)
	{
		Result.sMessage = "cannot create new connection";
		return Result;
	}

	auto iID = Connection->GetID();

	// mark the channel for the connection snapshot
	Connection->SetTarget(ConnectToEndpoint);
	Connection->SetPeer("probe");

	kDebug(1, "[{}]: probing connect to {}", iID, ConnectToEndpoint);

	KStopTime Took;

	QueueMessage(Message(Message::Connect, iID, ConnectToEndpoint.Serialize()));

	// Wait in one-second slices, and ping the channel with a Pause frame
	// between them. On a live channel the Pause is a no-op (the channel
	// is torn down right after the probe anyway), but every peer version
	// answers a control frame for an unknown channel with a Disconnect -
	// which turns a silently failed connect on an old node (that reports
	// no errors) into a detectable close.
	auto Verdict = Connection::Activity::Timeout;

	for (;;)
	{
		auto Remaining = Timeout - Took.elapsed();

		if (Remaining <= KDuration::zero())
		{
			break;
		}

		Verdict = Connection->WaitForActivity(std::min<KDuration>(Remaining, chrono::seconds(1)));

		if (Verdict != Connection::Activity::Timeout)
		{
			break;
		}

		QueueMessage(Message(Message::Pause, iID));
	}

	switch (Verdict)
	{
		case Connection::Activity::Data:
			Result.bConnected = true;
			Result.sMessage   = kFormat("target {} accepted the connection and sent data",
			                            ConnectToEndpoint);
			break;

		case Connection::Activity::Closed:
		{
			auto sReason = Connection->GetDisconnectReason();

			if (!sReason.empty())
			{
				// the peer told us what went wrong
				Result.sMessage = std::move(sReason);
			}
			else if (Took.elapsed() >= Timeout / 2)
			{
				// a silent close this late is the peer's idle timeout on a
				// connection that CAME UP but never saw data - a peer that
				// could not connect reports that error long before (a node
				// too old to report errors cannot be told apart here)
				Result.bConnected = true;
				Result.sMessage   = kFormat("connection to {} stayed open for {} until "
				                            "the node's idle timeout - the target accepted it "
				                            "(an old node without error reporting cannot rule "
				                            "out a connect timeout)",
				                            ConnectToEndpoint, Took.elapsed());
			}
			else
			{
				Result.sMessage = kFormat("the connect to {} failed after {} (the node "
				                          "reports no details - it runs an older ktunnel; "
				                          "typically the target refused the connection, the "
				                          "name did not resolve, or the target closed instantly)",
				                          ConnectToEndpoint, Took.elapsed());
			}
			break;
		}

		case Connection::Activity::Timeout:
			// the channel answered every liveness ping and reported no
			// error within the connect window - the connection is up, the
			// target just has not sent anything yet
			Result.bConnected = true;
			Result.sMessage   = kFormat("target {} accepted the connection (no data within "
			                            "{} - normal for protocols where the client speaks first)",
			                            ConnectToEndpoint, Timeout);
			break;
	}

	// tear the probe channel down on both sides
	QueueMessage(Message(Message::Disconnect, iID));
	Connection->Disconnect();
	m_Connections.Remove(iID);

	return Result;

} // ProbeConnect

//-----------------------------------------------------------------------------
std::shared_ptr<KTunnel::Connection> KTunnel::OpenRepl()
//-----------------------------------------------------------------------------
{
	if (!HaveTunnel())
	{
		kDebug(1, "cannot open REPL channel: no tunnel");
		return {};
	}

	// Allocate a new channel with no DirectStream — data flows over the
	// duplex Connection::ReadData() / WriteData() API. The initiator
	// side sends the OpenRepl frame; on the peer side the Run() switch
	// dispatches it into the configured OpenReplCallback.
	auto Connection = m_Connections.Create(0, [this](Message&& msg){ QueueMessage(std::move(msg)); }, nullptr);

	if (!Connection)
	{
		kDebug(1, "cannot open REPL channel: out of channels");
		return {};
	}

	QueueMessage(Message(Message::OpenRepl, Connection->GetID()));

	kDebug(1, "[{}]: requested REPL channel", Connection->GetID());

	return Connection;

} // OpenRepl

//-----------------------------------------------------------------------------
void KTunnel::CloseRepl(const std::shared_ptr<Connection>& Connection)
//-----------------------------------------------------------------------------
{
	if (!Connection) return;

	kDebug(1, "[{}]: closing REPL channel", Connection->GetID());

	// tell the peer so its handler thread unblocks from ReadData()
	QueueMessage(Message(Message::Disconnect, Connection->GetID()));

	Connection->Disconnect();
	m_Connections.Remove(Connection->GetID());

} // CloseRepl

//-----------------------------------------------------------------------------
bool KTunnel::HaveTunnel() const
//-----------------------------------------------------------------------------
{
	auto Tunnel = m_Tunnel.shared();

	if (!Tunnel->Stream)
	{
		return false;
	}

	return Tunnel->Stream->Good();
}

//-----------------------------------------------------------------------------
void KTunnel::SetTimeout(KDuration Timeout)
//-----------------------------------------------------------------------------
{
	auto Tunnel = m_Tunnel.unique();

	if (!Tunnel->Stream)
	{
		throw KError("no stream");
	}

	Tunnel->Stream->SetTimeout(Timeout);
}

//-----------------------------------------------------------------------------
KTCPEndPoint KTunnel::GetEndPointAddress() const
//-----------------------------------------------------------------------------
{
	auto Tunnel = m_Tunnel.shared();

	if (!Tunnel->Stream)
	{
		throw KError("no stream");
	}

	return Tunnel->Stream->GetEndPointAddress();
}

//-----------------------------------------------------------------------------
KString KTunnel::GetLoginNode() const
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_LoginNodeMutex);
	return m_sLoginNode;

} // KTunnel::GetLoginNode

//-----------------------------------------------------------------------------
void KTunnel::ReadMessage(Message& message)
//-----------------------------------------------------------------------------
{
	auto Tunnel = m_Tunnel.unique();

	if (!Tunnel->Stream)
	{
		throw KError("no tunnel stream for reading");
	}

	message.Read(*Tunnel->Stream, Tunnel->Decryptor.get());
	kDebug(3, message.Debug());

	// count only payload-carrying Data frames, not protocol frames
	// (Login/Helo/Ping/Pong/Control/Connect/...). This gives the admin UI a
	// metric that directly reflects how much tunneled application traffic
	// went through.
	if (message.GetType() == Message::Data)
	{
		m_iBytesRx.fetch_add(message.size(), std::memory_order_relaxed);
	}

} // KTunnel::ReadMessage

//-----------------------------------------------------------------------------
void KTunnel::WriteMessage(Message&& message)
//-----------------------------------------------------------------------------
{
	auto Tunnel = m_Tunnel.unique();

	if (!Tunnel->Stream)
	{
		throw KError("no tunnel stream for writing");
	}

	kDebug(3, message.Debug());
	// snapshot type+size BEFORE Write() - Write may move/consume the payload
	// buffer depending on Frame internals
	const auto msgType = message.GetType();
	const auto msgSize = message.size();

	KStopTime Stop;
	message.Write(*Tunnel->Stream, m_bMaskTx, Tunnel->Encryptor.get());
	kDebug(3, "took {}", Stop.elapsed());

	if (msgType == Message::Data)
	{
		m_iBytesTx.fetch_add(msgSize, std::memory_order_relaxed);
	}

} // KTunnel::WriteMessage

//-----------------------------------------------------------------------------
void KTunnel::QueueMessage(Message&& message)
//-----------------------------------------------------------------------------
{
	m_OutQueue.unique()->push_back(std::move(message));

	// wake Run() if it sits in its poll
	m_Wakeup.Wake();

} // KTunnel::QueueMessage

//-----------------------------------------------------------------------------
void KTunnel::ConnectToTarget(std::size_t iID, KTCPEndPoint Target)
//-----------------------------------------------------------------------------
{
	try
	{
		KStreamOptions Options;
		Options.SetTimeout(m_Config.ConnectTimeout);

		auto Connection = m_Connections.Get(iID, false);

		if (Connection)
		{
			kDebug(2, "connecting {}..", Target);

			auto TargetStream = CreateKTCPStream(Target, Options);

			if (TargetStream->Good())
			{
				Connection->SetDirectStream(TargetStream.get());

				// both directions on this thread - the stream tolerates only one
				Connection->Pump();
			}
			else
			{
				kDebug(1, "cannot connect to target {}", Target);
				// tell the other side WHY the channel dies - it logs the
				// reason for its operators, who otherwise only see a
				// silently dropped connection
				QueueMessage(Message(Message::Disconnect, iID,
				                     kFormat("cannot connect to target {}: {}",
				                             Target, TargetStream->Error())));
			}
			// should this not yet have been done
			Connection->Disconnect();
		}
		else
		{
			kDebug(1, "cannot find connection {}", iID);
		}
	}
	catch (const std::exception& ex)
	{
		kDebug(1, ex.what());
		QueueMessage(Message(Message::Disconnect, iID,
		                     kFormat("error connecting to target {}: {}", Target, ex.what())));
	}

	m_Connections.Remove(iID);

} // KTunnel::ConnectToTarget

#if DEKAF2_HAS_ED25519 && DEKAF2_HAS_X25519

namespace {

//-----------------------------------------------------------------------------
/// Build the byte string that both sides sign / verify in the v2 handshake.
/// Domain-separated by the ASCII tag "ktunnel-v2\0" so a forged signature
/// could not migrate from a different protocol that ever uses the same
/// underlying Ed25519 key. Order is fixed and binds every public input of
/// the handshake into a single transcript: client-eph-pub, server-eph-pub,
/// client-nonce, server-nonce, login-node.
KString BuildHandshakeTranscript(KStringView sEpubClient,
                                 KStringView sEpubServer,
                                 KStringView sNonceClient,
                                 KStringView sNonceServer,
                                 KStringView sNode)
//-----------------------------------------------------------------------------
{
	KString sTranscript;
	sTranscript.reserve(11 + 32 + 32 + 16 + 16 + sNode.size());
	sTranscript.append("ktunnel-v2");
	sTranscript.push_back('\0');
	sTranscript.append(sEpubClient);
	sTranscript.append(sEpubServer);
	sTranscript.append(sNonceClient);
	sTranscript.append(sNonceServer);
	sTranscript.append(sNode);
	return sTranscript;
}

} // anonymous namespace

//-----------------------------------------------------------------------------
KString KTunnel::FormatFingerprint(KStringView sRawPublicKey)
//-----------------------------------------------------------------------------
{
	// SSH-style: SHA-256 of the raw public-key bytes, rendered as
	// lowercase hex with a colon between every byte. 32 hash bytes
	// produce 64 hex chars + 31 colons = 95 chars.
	auto sHex = KSHA256(sRawPublicKey).HexDigest();

	KString sOut;
	sOut.reserve(sHex.size() + sHex.size() / 2);

	for (std::size_t i = 0; i < sHex.size(); i += 2)
	{
		if (i != 0) sOut.push_back(':');
		sOut.append(sHex.substr(i, 2));
	}

	return sOut;

} // KTunnel::FormatFingerprint

//-----------------------------------------------------------------------------
void KTunnel::SetupEncryption (KStringView sNode)
//-----------------------------------------------------------------------------
{
	// Client-side half of the v2 X25519 + Ed25519 + HKDF handshake.
	// Both sides run on top of the already-open tunnel stream which is
	// usually TLS-protected — but we deliberately do NOT trust TLS for
	// peer authentication, because an enterprise TLS-interception
	// middlebox can present a forged but technically valid chain. The
	// Ed25519 server-identity signature pinned via known_servers /
	// -trust-fingerprint is the actual MITM defence; the X25519 part
	// gives forward secrecy against a later compromise of either the
	// identity key or the TLS private key.

	if (sNode.empty()) sNode = "<none>";

	kDebug(2, "v2 handshake (client) for node {}", sNode);

	// 1) Generate ephemeral X25519 + 16-byte client nonce.
	KX25519 EphClient(true);
	if (EphClient.empty())
	{
		throw KError("v2 handshake: cannot generate ephemeral X25519 key");
	}
	KString sEpubClient  = EphClient.GetPublicKeyRaw();
	KString sNonceClient = kGetRandom(16);

	// 2) Send hello frame (plaintext JSON, type Login for wire-compat).
	KJSON oHello {
		{ "ver",  2 },
		{ "kind", "hello" },
		{ "epub", KEncode::Base64(sEpubClient) },
		{ "n_c",  KEncode::Base64(sNonceClient) },
		{ "node", sNode }
	};
	WriteMessage(Message(Message::Login, 0, oHello.dump()));

	// 3) Receive hello-ack (plaintext, server-signed).
	Message Ack;
	ReadMessage(Ack);

	if (Ack.GetType() != Message::Login)
	{
		throw KError(kFormat("v2 handshake: expected hello-ack, got {} ({})",
		                     Ack.PrintType(), DescribeUnexpectedFrame(Ack)));
	}

	KJSON oAck = kjson::Parse(Ack.GetMessage());
	if (!oAck.is_object() || kjson::GetUInt(oAck, "ver") != 2 ||
	    kjson::GetStringRef(oAck, "kind") != "hello-ack")
	{
		throw KError("v2 handshake: malformed hello-ack (legacy peer or wire-protocol mismatch)");
	}

	KString sEpubServer  = KDecode::Base64(kjson::GetStringRef(oAck, "epub"));
	KString sNonceServer = KDecode::Base64(kjson::GetStringRef(oAck, "n_s"));
	KString sIpubServer  = KDecode::Base64(kjson::GetStringRef(oAck, "ipub"));
	KString sSig         = KDecode::Base64(kjson::GetStringRef(oAck, "sig"));

	if (sEpubServer.size() != 32 || sNonceServer.size() != 16 ||
	    sIpubServer.size() != 32 || sSig.size() != 64)
	{
		throw KError("v2 handshake: malformed crypto fields in hello-ack");
	}

	// 4) Verify the Ed25519 signature over the transcript. This is
	//    the only thing standing between us and an active TLS MITM, so
	//    we verify *before* we touch the trust callback or derive any
	//    keys. A forged signature here is a hard error.
	KEd25519Key ServerIdentityPub;
	if (!ServerIdentityPub.CreateFromRaw(sIpubServer, /*bIsPrivateKey=*/false))
	{
		throw KError("v2 handshake: malformed server identity public key");
	}

	auto sTranscript = BuildHandshakeTranscript(sEpubClient, sEpubServer,
	                                            sNonceClient, sNonceServer, sNode);

	KEd25519Verify Verifier;
	if (!Verifier.Verify(ServerIdentityPub, sTranscript, sSig))
	{
		throw KError(kFormat("v2 handshake: server identity signature INVALID for {} "
		                     "(possible MITM, refusing to send credentials)",
		                     GetEndPointAddress()));
	}

	// 5) Trust check: ask the embedder if it knows / accepts this
	//    server identity. The embedder runs known_servers lookup, may
	//    consult -trust-fingerprint, may TOFU-prompt the operator.
	if (!m_Config.TrustCallback)
	{
		throw KError("v2 handshake: bAESPayload set on the client side but "
		             "no Config::TrustCallback installed — refusing to send credentials");
	}

	KString sFingerprint = FormatFingerprint(sIpubServer);
	KString sHostPort    = GetEndPointAddress().Serialize();

	if (!m_Config.TrustCallback(sHostPort, sIpubServer, sFingerprint))
	{
		throw KError(kFormat("v2 handshake: server identity for {} not trusted (fingerprint {})",
		                     sHostPort, sFingerprint));
	}

	// 6) ECDH + HKDF-SHA256 → tx_key (c2s), rx_key (s2c).
	KString sShared = EphClient.DeriveSharedSecret(sEpubServer);
	if (sShared.size() != 32)
	{
		throw KError("v2 handshake: ECDH derive failed");
	}

	KString sSalt   = sNonceClient + sNonceServer;
	KString sPRK    = KHKDF::Extract(sSalt, sShared);
	KString sKeyC2S = KHKDF::Expand(sPRK, "ktunnel-v2 c2s", 32);
	KString sKeyS2C = KHKDF::Expand(sPRK, "ktunnel-v2 s2c", 32);

	if (sKeyC2S.size() != 32 || sKeyS2C.size() != 32)
	{
		throw KError("v2 handshake: HKDF expand produced unexpected key size");
	}

	// 7) Install AES-256-GCM ciphers. Client perspective: we encrypt
	//    with key_c2s (towards the server), decrypt with key_s2c.
	{
		auto Tunnel = m_Tunnel.unique();

		Tunnel->Encryptor = std::make_unique<KBlockCipher>
		(
			KBlockCipher::Encrypt,
			KBlockCipher::AES,
			KBlockCipher::GCM,
			KBlockCipher::B256,
			false, // do not inline the IV — auto-increment counter
			true   // inline the verification tag
		);

		Tunnel->Decryptor = std::make_unique<KBlockCipher>
		(
			KBlockCipher::Decrypt,
			KBlockCipher::AES,
			KBlockCipher::GCM,
			KBlockCipher::B256,
			false,
			true
		);

		Tunnel->Encryptor->SetKey(sKeyC2S);
		Tunnel->Decryptor->SetKey(sKeyS2C);
		Tunnel->Encryptor->SetAutoIncrementNonceAsIV();
		Tunnel->Decryptor->SetAutoIncrementNonceAsIV();
	}

	kDebug(1, "v2 handshake (client) ok for {}, server fingerprint {}",
	       sHostPort, sFingerprint);

} // SetupEncryption (client)

//-----------------------------------------------------------------------------
bool KTunnel::SetupEncryption (Message& HelloFrame, KString& sOutNode)
//-----------------------------------------------------------------------------
{
	// Server-side half of the v2 handshake. The hello frame has been
	// read by WaitForLogin() and is passed in. We verify it parses,
	// generate our own ephemeral key, sign the transcript with our
	// long-term identity key, send hello-ack, derive session keys,
	// install ciphers. After this returns, WaitForLogin() reads the
	// next frame (the encrypted auth frame) and authenticates the node.

	if (HelloFrame.GetType() != Message::Login)
	{
		throw KError(kFormat("v2 handshake: first frame must be Login, got {} ({})",
		                     HelloFrame.PrintType(), DescribeUnexpectedFrame(HelloFrame)));
	}

	KJSON oHello = kjson::Parse(HelloFrame.GetMessage());
	if (!oHello.is_object() || kjson::GetUInt(oHello, "ver") != 2 ||
	    kjson::GetStringRef(oHello, "kind") != "hello")
	{
		// Most likely a v1 (PSK) peer trying to open an AES tunnel against
		// our v2-only server. We say so explicitly so the operator does
		// not stare at a 15-second silent timeout.
		throw KError(kFormat("v2 handshake: legacy or non-v2 hello from {} "
		                     "(upgrade peer, or the peer's -aes flag is set without v2 support)",
		                     GetEndPointAddress()));
	}

	KString sEpubClient  = KDecode::Base64(kjson::GetStringRef(oHello, "epub"));
	KString sNonceClient = KDecode::Base64(kjson::GetStringRef(oHello, "n_c"));
	sOutNode             = kjson::GetStringRef(oHello, "node");

	if (sEpubClient.size() != 32 || sNonceClient.size() != 16)
	{
		throw KError("v2 handshake: malformed hello (epub or n_c wrong size)");
	}

	if (!m_Config.ServerIdentity || m_Config.ServerIdentity->empty() ||
	    !m_Config.ServerIdentity->IsPrivateKey())
	{
		throw KError("v2 handshake: bAESPayload set on the server side but "
		             "Config::ServerIdentity holds no private Ed25519 key");
	}

	// Server-side ephemeral + nonce + signed transcript.
	KX25519 EphServer(true);

	if (EphServer.empty())
	{
		throw KError("v2 handshake: cannot generate ephemeral X25519 key");
	}

	KString sEpubServer  = EphServer.GetPublicKeyRaw();
	KString sNonceServer = kGetRandom(16);
	KString sIpubServer  = m_Config.ServerIdentity->GetPublicKeyRaw();

	auto sTranscript = BuildHandshakeTranscript(sEpubClient, sEpubServer,
	                                            sNonceClient, sNonceServer, sOutNode);

	KEd25519Sign Signer;
	KString sSig = Signer.Sign(*m_Config.ServerIdentity, sTranscript);
	if (sSig.size() != 64)
	{
		throw KError("v2 handshake: Ed25519 sign failed");
	}

	KJSON oAck {
		{ "ver",  2 },
		{ "kind", "hello-ack" },
		{ "epub", KEncode::Base64(sEpubServer)  },
		{ "n_s",  KEncode::Base64(sNonceServer) },
		{ "ipub", KEncode::Base64(sIpubServer)  },
		{ "sig",  KEncode::Base64(sSig)         }
	};
	WriteMessage(Message(Message::Login, 0, oAck.dump()));

	// ECDH + HKDF (mirror of the client side).
	KString sShared = EphServer.DeriveSharedSecret(sEpubClient);
	if (sShared.size() != 32)
	{
		throw KError("v2 handshake: ECDH derive failed");
	}

	KString sSalt   = sNonceClient + sNonceServer;
	KString sPRK    = KHKDF::Extract(sSalt, sShared);
	KString sKeyC2S = KHKDF::Expand(sPRK, "ktunnel-v2 c2s", 32);
	KString sKeyS2C = KHKDF::Expand(sPRK, "ktunnel-v2 s2c", 32);

	if (sKeyC2S.size() != 32 || sKeyS2C.size() != 32)
	{
		throw KError("v2 handshake: HKDF expand produced unexpected key size");
	}

	// Server perspective: encrypt with key_s2c (towards the client),
	// decrypt with key_c2s (the client's outgoing direction).
	{
		auto Tunnel = m_Tunnel.unique();

		Tunnel->Encryptor = std::make_unique<KBlockCipher>
		(
			KBlockCipher::Encrypt,
			KBlockCipher::AES,
			KBlockCipher::GCM,
			KBlockCipher::B256,
			false,
			true
		);
		Tunnel->Decryptor = std::make_unique<KBlockCipher>
		(
			KBlockCipher::Decrypt,
			KBlockCipher::AES,
			KBlockCipher::GCM,
			KBlockCipher::B256,
			false,
			true
		);

		Tunnel->Encryptor->SetKey(sKeyS2C);
		Tunnel->Decryptor->SetKey(sKeyC2S);
		Tunnel->Encryptor->SetAutoIncrementNonceAsIV();
		Tunnel->Decryptor->SetAutoIncrementNonceAsIV();
	}

	kDebug(1, "v2 handshake (server) ok for node '{}' from {}",
	       sOutNode, GetEndPointAddress());

	return true;

} // SetupEncryption (server)

#endif // DEKAF2_HAS_ED25519 && DEKAF2_HAS_X25519

//-----------------------------------------------------------------------------
bool KTunnel::Login(KStringView sNode, KStringView sSecret)
//-----------------------------------------------------------------------------
{
	if (m_Config.bAESPayload)
	{
#if DEKAF2_HAS_ED25519 && DEKAF2_HAS_X25519
		// Run the v2 handshake (X25519 + Ed25519 + HKDF). After this
		// returns the tunnel ciphers are active and every further frame
		// is AES-256-GCM. We then send the auth frame in the clear-on-
		// the-wire-but-encrypted-by-cipher path.
		SetupEncryption(sNode);

		KJSON oAuth {
			{ "kind", "auth"   },
			{ "pass", sSecret  }
		};
		WriteMessage(Message(Message::Login, 0, oAuth.dump()));
#else
		throw KError("v2 AES handshake unavailable: dekaf2 was built without "
		             "X25519/Ed25519 support (OpenSSL >= 1.1.1 / LibreSSL >= 3.7.0 required)");
#endif
	}
	else
	{
		WriteMessage(Message(Message::Login, 0, kFormat("Basic {}", KEncode::Base64(kFormat("{}:{}", sNode, sSecret)))));
	}

	// wait for the response (Helo, encrypted iff bAESPayload)
	Message Response;
	ReadMessage(Response);

	if (Response.GetType() != Message::Helo || Response.GetChannel() != 0)
	{
		kDebug(1, "no HELO response or channel != 0 at login");
		return false;
	}

	return true;

} // KTunnel::Login

//-----------------------------------------------------------------------------
void KTunnel::WaitForLogin()
//-----------------------------------------------------------------------------
{
	Message Login;
	ReadMessage(Login);

	KString sLoginNode;
	KString sLoginPass;

	if (m_Config.bAESPayload)
	{
#if DEKAF2_HAS_ED25519 && DEKAF2_HAS_X25519
		// v2 flow: the just-read frame is the client's hello. Run the
		// X25519+Ed25519 handshake which sends back a signed hello-ack
		// and wires up the AES-GCM ciphers. Then read the encrypted auth
		// frame to recover the password.
		SetupEncryption(Login, sLoginNode);

		Message Auth;
		ReadMessage(Auth);

		if (Auth.GetType() != Message::Login)
		{
			throw KError(kFormat("v2 handshake: expected auth frame, got {} ({})",
			                     Auth.PrintType(), DescribeUnexpectedFrame(Auth)));
		}

		KJSON oAuth = kjson::Parse(Auth.GetMessage());
		if (!oAuth.is_object() || kjson::GetStringRef(oAuth, "kind") != "auth")
		{
			throw KError("v2 handshake: malformed auth frame");
		}

		sLoginPass = kjson::GetStringRef(oAuth, "pass");
#else
		throw KError("v2 AES handshake unavailable: dekaf2 was built without "
		             "X25519/Ed25519 support");
#endif
	}
	else
	{
		if (Login.GetType() != Message::Login)
		{
			throw KError(kFormat("invalid message type {} ({})",
			                     Login.PrintType(), DescribeUnexpectedFrame(Login)));
		}
		// Basic flow: extract node/pass from a "Basic <base64>" string.
		// KHTTPHeaders calls the parsed login identifier `sUsername` for
		// historical HTTP-Basic reasons; in our domain that string is
		// always a node-endpoint name.
		auto Creds = KHTTPHeaders::DecodeBasicAuthFromString(Login.GetMessage());
		sLoginNode = Creds.sUsername;
		sLoginPass = Creds.sPassword;
	}

	// Authenticate. Embedder's AuthCallback wins if set (used by the
	// stateful sample to bcrypt-verify against the nodes table).
	// Otherwise fall back to Config::Secrets membership (the AdHoc
	// pre-shared-secret model). This block is now identical in both
	// transport modes — the v2 handshake established session secrecy,
	// it does not authenticate the node; that's still the password's
	// job, only over a now-confidential channel.
	bool bAccepted = false;

	if (m_Config.AuthCallback)
	{
		bAccepted = m_Config.AuthCallback(sLoginNode, sLoginPass);
	}
	else
	{
		bAccepted = !m_Config.Secrets.empty() && m_Config.Secrets.contains(sLoginPass);
	}

	if (!bAccepted)
	{
		throw KError(kFormat("login rejected from {} (node '{}')",
		                     GetEndPointAddress(), sLoginNode));
	}

	{
		std::lock_guard<std::mutex> Lock(m_LoginNodeMutex);
		m_sLoginNode = std::move(sLoginNode);
	}

	kDebug(1, "successful login from {} (node '{}')", GetEndPointAddress(), m_sLoginNode);

	// finally say hi
	WriteMessage(Message(Message::Helo, 0));

} // KTunnel::WaitForLogin

//-----------------------------------------------------------------------------
void KTunnel::PingTest(KUnixTime Time)
//-----------------------------------------------------------------------------
{
	if (HaveTunnel())
	{
		KStopTime RTT;
		QueueMessage(Message(Message::Ping, 0, kToStringView(RTT)));
	}

} // KTunnel::TimingCallback

//-----------------------------------------------------------------------------
void KTunnel::Run()
//-----------------------------------------------------------------------------
{
	if (m_bWaitForLogin)
	{
		WaitForLogin();
	}
	else
	{
		m_TimerID = Dekaf::getInstance().GetTimer().CallEvery
		(
			m_Config.ControlPing,
			std::bind(&KTunnel::PingTest, this, std::placeholders::_1)
		);
	}

	auto IdleTimeout = m_Config.ControlPing + m_Config.ConnectTimeout;

	SetTimeout(IdleTimeout);

	// From here on this thread owns the tunnel stream: it reads incoming
	// messages and it is the only one writing outgoing ones, which everybody
	// else hands over through QueueMessage(). Both directions are checked
	// separately with a zero timeout - a combined poll would report readability
	// alone as soon as any layer holds bytes (streambuf, or an undrained TLS
	// record) and the write side would starve under load.
	KStopTime Idle;

	for (;HaveTunnel();)
	{
		{
			auto Stream = GetStream();

			// Stop() (from any thread) closed the socket - a clean Disconnect()
			// leaves no error behind, so HaveTunnel() would still be true. Leave
			// here, and above all do not poll the closed descriptor: its number
			// may already belong to another socket
			if (!Stream || Stream->IsDisconnecting()) break;
		}

		bool bDidIO { false };

		// ---- send one message, if the stream takes it right now ------------

		// exactly one per round, strictly alternating with the read below:
		// draining the whole queue here would starve the reading direction
		// under load, and the channels waiting for incoming data would run
		// into their idle timeout
		if (!m_OutQueue.shared()->empty())
		{
			auto Stream = GetStream();

			if (Stream && Stream->IsWriteReady(KDuration(), false))
			{
				Message ToTunnel;
				bool    bHaveOne { false };

				{
					auto OutQueue = m_OutQueue.unique();

					if (!OutQueue->empty())
					{
						ToTunnel = std::move(OutQueue->front());
						OutQueue->pop_front();
						bHaveOne = true;
					}
				}

				if (bHaveOne)
				{
					WriteMessage(std::move(ToTunnel));
					bDidIO = true;
				}
			}
		}

		// ---- receive and dispatch one message ------------------------------

		{
			auto Stream = GetStream();

			if (Stream && Stream->IsReadReady(KDuration(), false))
			{
				Message FromTunnel;
				ReadMessage(FromTunnel);
				HandleMessage(std::move(FromTunnel));
				bDidIO = true;
			}
		}

		if (bDidIO)
		{
			// there was work - look for more before going to sleep
			Idle.clear();
			continue;
		}

		// ---- neither direction can proceed - wait --------------------------

		short iWhat { POLLIN | POLLHUP };

		if (!m_OutQueue.shared()->empty()) iWhat |= POLLOUT;

		struct pollfd fds[2];
		std::size_t   iCount { 1 };

		{
			auto Stream = GetStream();

			if (!Stream) break;

			fds[0].fd = Stream->GetNativeSocket();
		}

		fds[0].events  = iWhat;
		fds[0].revents = 0;

		if (m_Wakeup.IsValid())
		{
			fds[1].fd      = m_Wakeup.GetFD();
			fds[1].events  = POLLIN;
			fds[1].revents = 0;
			iCount         = 2;
		}

		// without a working interruptor (Windows) nobody can wake us, so we
		// wait in short slices to keep the outbound latency low
		auto Timeout = m_Wakeup.IsValid() ? IdleTimeout : KDuration(chrono::milliseconds(20));

		auto iResult = kPoll(KSpan<pollfd>(fds, iCount), Timeout);

		if (iResult < 0)
		{
			throw KError("poll error on the tunnel stream");
		}

		if (iResult > 0 && iCount == 2 && fds[1].revents)
		{
			// another thread queued a message - drain the interruptor and
			// re-evaluate at the top
			m_Wakeup.Clear();
			continue;
		}

		if (iResult > 0 && (fds[0].revents & (POLLHUP | POLLERR | POLLNVAL)) != 0)
		{
			throw KError("the tunnel connection was closed");
		}

		if (iResult == 0 && Idle.elapsed() >= IdleTimeout)
		{
			throw KError(kFormat("no message from the tunnel for {}", IdleTimeout));
		}
	}

} // KTunnel::Run

//-----------------------------------------------------------------------------
void KTunnel::HandleMessage(Message&& FromTunnel)
//-----------------------------------------------------------------------------
{
	kDebug(3, "open connections: {}", m_Connections.size());

	// get a shorthand for the channel
	auto chan = FromTunnel.GetChannel();

	switch (FromTunnel.GetType())
	{
		case Message::Ping:
		{
			// check if the channel is alive on our end
			if (!chan || m_Connections.Exists(chan))
			{
				// respond with pong
				FromTunnel.SetType(Message::Pong);
				QueueMessage(std::move(FromTunnel));
			}
			else
			{
				QueueMessage(Message(Message::Disconnect, chan));
			}


			break;
		}

		case Message::Pong:
			// do nothing, but note RTT
			if (FromTunnel.size() == sizeof(KStopTime))
			{
				auto RTT(kFromStringView<KStopTime>(FromTunnel.GetMessage()));
				m_RTT = RTT.elapsed();
				kDebug(1, "roundtrip: {}", m_RTT);
			}
			break;

		case Message::Idle:
			// really do nothing
			break;

		case Message::Control:
			if (chan != 0)
			{
				throw KError(kFormat("[{}]: received control message on non-zero channel", chan));
			}
			// we currently do not exchange control messages
			break;

		case Message::Connect:
		{
			// open a new stream to endpoint, and data stream to exposed host
			KTCPEndPoint TargetHost(FromTunnel.GetMessage());
			kDebug(3, "got connect request: {} for channel {}", TargetHost, chan);

			auto Connection = m_Connections.Create(chan, [this](Message&& msg){ QueueMessage(std::move(msg)); }, nullptr);

			if (Connection)
			{
				Connection->SetTarget(TargetHost);
				m_Threads.Create(&KTunnel::ConnectToTarget, this, chan, std::move(TargetHost));
			}
			else
			{
				QueueMessage(Message(Message::Disconnect, chan,
				                     kFormat("connection limit reached ({})",
				                             m_Config.iMaxTunneledConnections)));
			}

			break;
		}

		case Message::Data:
		{
			auto Connection = m_Connections.Get(chan, false);

			if (!Connection)
			{
				QueueMessage(Message(Message::Disconnect, chan));
			}
			else
			{
				Connection->SendData(std::move(FromTunnel));
			}
			break;
		}

		case Message::Pause:
		{
			auto Connection = m_Connections.Get(chan, false);

			if (!Connection)
			{
				QueueMessage(Message(Message::Disconnect, chan));
			}
			else
			{
				Connection->Pause();
			}
			break;
		}

		case Message::Resume:
		{
			auto Connection = m_Connections.Get(chan, false);

			if (!Connection)
			{
				QueueMessage(Message(Message::Disconnect, chan));
			}
			else
			{
				Connection->Resume();
			}
			break;
		}

		case Message::Disconnect:
		{
			auto Connection = m_Connections.Get(chan, false);

			if (Connection)
			{
				// put the disconnect into the queue
				Connection->SendData(std::move(FromTunnel));
			}
			break;
		}

		case Message::OpenRepl:
		{
			// the peer requests a REPL channel on our side — only
			// accept if an OpenReplCallback is configured
			if (!m_Config.OpenReplCallback)
			{
				kDebug(1, "[{}]: OpenRepl requested but no OpenReplCallback configured", chan);
				QueueMessage(Message(Message::Disconnect, chan));
				break;
			}

			auto Connection = m_Connections.Create(chan, [this](Message&& msg){ QueueMessage(std::move(msg)); }, nullptr);

			if (Connection)
			{
				// Hand the Connection to the REPL callback on its
				// own worker thread. When the callback returns the
				// connection is disconnected and removed from the
				// registry, mirroring ConnectToTarget()'s lifecycle.
				m_Threads.Create([this, cb=m_Config.OpenReplCallback, Connection]()
				{
					try
					{
						cb(Connection);
					}
					catch (const std::exception& ex)
					{
						kDebug(1, "[{}]: REPL handler threw: {}", Connection->GetID(), ex.what());
					}
					Connection->Disconnect();
					// tell the initiator, whose ReadData() otherwise
					// blocks until the tunnel itself dies
					QueueMessage(Message(Message::Disconnect, Connection->GetID()));
					m_Connections.Remove(Connection->GetID());
				});
			}
			else
			{
				QueueMessage(Message(Message::Disconnect, chan));
			}

			break;
		}

		case Message::OpenShell:
		case Message::ShellResize:
		{
			// reserved for a future PTY-shell channel — reject for now
			kDebug(1, "[{}]: {} not implemented yet", chan, FromTunnel.PrintType());
			QueueMessage(Message(Message::Disconnect, chan));
			break;
		}

		case Message::Login:
		case Message::Helo:
			throw KError("received login handshake in an established connection");

		case Message::None:
			throw KError("received invalid message type");
	}

} // KTunnel::HandleMessage

DEKAF2_NAMESPACE_END
