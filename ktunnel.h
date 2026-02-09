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

#pragma once

#include "kiostreamsocket.h"
#include "kassociative.h"
#include "kblockcipher.h"
#include "kurl.h"
#include "kthreadsafe.h"
#include "kthreads.h"
#include "ktimer.h"
#include "ksourcelocation.h"
#include "kwebsocket.h"
#include <thread>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <queue>

DEKAF2_NAMESPACE_BEGIN

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Creates a fully transparent multiplexed tunnel for TCP stream connections between two instances of itself.
/// Both ends will be instantiated around a single open stream for input/output, which could be derived from any
/// std::iostream.
/// One of the two instances is required to use the constructor with user and secret set, the other one has to
/// leave those empty - which one should be clear from the concrete implementation of the connection setup.
/// Normally, the side waiting for a tunnel connection should not set the credentials, and the other one
/// seeking to establish the tunnel should set them.
/// You find a sample implementation in samples/ktunnel.h / samples/ktunnel.cpp
class KTunnel
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	//------------------------- subclasses of KTunnel -------------------------

	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// Config setup for KTunnel
	struct Config
	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		/// a list of accepted secrets (strings)
		KUnorderedSet<KString> Secrets;
		/// timeout for incoming and outgoing connections (not for the tunnel itself)
		KDuration              Timeout        { chrono::seconds(15)        };
		/// interval to query tunnel health - should be low enough to avoid firewall and proxy timeouts,
		/// and because it is also used to calculate the connection timeout should be same on both ends
		KDuration              ControlPing    { chrono::seconds(60)        };
		/// timeout for connection setup, either for the tunnel itself or for any of the tunneled connections
		KDuration              ConnectTimeout { chrono::seconds(15)        };
		/// count of max multiplexed connections per tunnel - technical upper limit is 16 millions
		std::size_t            iMaxTunneledConnections { 100 };
		/// encode payload with AES?
		bool                   bAESPayload    { false };

	}; // Config

	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// the message protocol used on top of the websocket frame,
	/// basically adding multiplex channels and (own) message types,
	/// and encryption/decryption if configured
	class Message : protected KWebSocket::Frame
	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{

	//----------
	public:
	//----------

		enum Type : uint8_t
		{
			None = 0,
			Login,
			Helo,
			Ping,
			Pong,
			Idle,
			Control,
			Connect,
			Data,
			Pause,
			Resume,
			Disconnect
		};

		/// default construct
		Message() = default;
		/// construct from discrete parameters
		Message(Type _type, std::size_t iChannel, KString sMessage = KString{});

		/// read from a streamsocket, decrypt message with Decryptor if not nullptr
		void           Read       (KIOStreamSocket& Stream, KBlockCipher* Decryptor = nullptr);
		/// write to a streamsocket - if this is in websocket protocol and we are a "client", then
		/// bMask must be set to true, else to false,
		/// encrypt message with Encryptor if not nullptr
		void           Write      (KIOStreamSocket& Stream, bool bMask, KBlockCipher* Encryptor = nullptr);
		/// returns true if the received (read) message could be decrypted by the given decryptor - is only needed if the decryptor
		/// is not already known, to try a few keys
		bool           Decrypt    (KBlockCipher* Decryptor);

		/// returns the message type
		Type           GetType    () const { return static_cast<Type>(m_Preamble[0]); }
		/// returns the message as a ref
		const KString& GetMessage () const { return GetPayload(); }
		/// returns the channel ID this message was sent to
		std::size_t    GetChannel () const;

		/// sets the message type
		void           SetType    (Type _type)           { m_Preamble[0] = _type;                 }
		/// sets the message (payload)
		void           SetMessage (KString sMessage)     { SetPayload(std::move(sMessage), true); }
		/// sets the channel the message is sent to
		void           SetChannel (std::size_t iChannel);

		/// clears the message
		void           clear      ();
		/// returns the payload size
		std::size_t    size       () const { return GetPayload().size(); }

		/// returns max channel number
		static constexpr
		std::size_t    MaxChannel ()       { return (1 << 24) - 1;       }

		/// returns a debug string with core data about the message and type
		KString        Debug      () const;
		/// prints the message type in ASCII, for logging purposes
		KStringView    PrintType  () const;

	//----------
	protected:
	//----------

		std::size_t    GetPreambleSize () const override final;
		char*          GetPreambleBuf  () const override final;

	//----------
	private:
	//----------

		/// prints the message content except for Data and Unknown types
		KStringView    PrintData  () const;
		/// throws and prints a description as of why
		void           Throw      (KString sError, KStringView sFunction = KSourceLocation::current().function_name()) const;

		mutable std::array<char, 4> m_Preamble{};

	}; // Message

	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// runs one single connection in the multiplexed tunnel - uses two threads to pump data in and out of
	/// tunnel and outside connection
	class Connection
	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{

	//----------
	public:
	//----------

		/// ctor, gets a unique ID (channel), a function pointer to send messages, and either a connected stream to the outside target or nullptr
		Connection (std::size_t iID, std::function<void(Message&&)> TunnelSend, KIOStreamSocket* DirectStream = nullptr);

		/// set the outside connection stream if not given with the ctor
		void        SetDirectStream       (KIOStreamSocket* DirectStream) { m_DirectStream = DirectStream; }

		/// runs the data pump into the tunnel
		void        PumpToTunnel          ();
		/// runs the data pump out of the tunnel
		void        PumpFromTunnel        ();

		/// puts data into the direct connection's queue, may force a Pause frame if queue is too large
		void        SendData              (Message&& FromTunnel);
		/// shuts this connection down
		void        Disconnect            ();

		/// returns the unique ID (channel) for this connection
		std::size_t GetID                 () const { return m_iID;      }

		/// pause sending frames for this connection
		void        Pause                 ()       { m_bPaused = true;  }
		/// resume sending frames for this connection
		void        Resume                ();
		/// check if this connection shall pause sending frames
		bool        IsPaused              () const { return m_bPaused;  }

		/// max size for the message queue for one connection
		static constexpr
		std::size_t MaxMessageQueueSize   ()       { return 20;         }

	//----------
	private:
	//----------

		std::queue<Message>                  m_MessageQueue;
		std::function<void(Message&&)>       m_Tunnel;
		KIOStreamSocket*                     m_DirectStream { nullptr };
		std::mutex                           m_QueueMutex;
		std::condition_variable              m_FreshData;
		std::mutex                           m_TunnelMutex;
		std::condition_variable              m_ResumeTunnel;
		std::size_t                          m_iID          { 0 };
		bool                                 m_bRXPaused    { false };
		bool                                 m_bPaused      { false };
		bool                                 m_bQuit        { false };

	}; // Connection

	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// holds all multiplexed connections through the tunnel, thread safe
	class Connections
	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{

	//----------
	public:
	//----------

		/// ctor, takes the max connection count
		Connections(std::size_t iMaxConnections) : m_iMaxConnections(iMaxConnections) {}

		/// create and store a new connection. If iID is 0 (the normal case) it will be generated as a unique ID. TunnelSend is a function to send messages, DirectStream takes the outside connection's stream if already existing
		std::shared_ptr<Connection> Create (std::size_t iID, std::function<void(Message&&)> TunnelSend, KIOStreamSocket* DirectStream = nullptr);
		/// look for an existing connection
		std::shared_ptr<Connection> Get    (std::size_t iID, bool bAndRemove);
		/// remove one connection
		bool                        Remove (std::size_t iID);
		/// checks for existance of a connection by its ID (channel)
		bool                        Exists (std::size_t iID);
		/// returns current count of connections
		std::size_t                 size   () const;

	//----------
	private:
	//----------

		KThreadSafe<KUnorderedMap<std::size_t, std::shared_ptr<Connection>>> m_Connections;

		std::size_t m_iConnection     { 0 };
		std::size_t m_iMaxConnections { 50 };

	}; // Connections

	//------------------------- start of KTunnel -------------------------

	/// Construct around a KIOStreamSocket. If either user or secret are given, this instance
	/// will try to login at the opposite instance (and, in websocket terms, be a "client" and XOR
	/// its messages, except when XORing is explicitly switched off with bNeverMask = true)
	/// Then from either side you can Connect() new connections which will be multiplexed
	/// transparently through the tunnel.
	KTunnel
	(
		Config                           Config,
		std::unique_ptr<KIOStreamSocket> Stream,
		KStringView                      sUser      = KStringView{},
		KStringView                      sSecret    = KStringView{},
		bool                             bNeverMask = false
	);

	// dtor
	~KTunnel();

	/// connect an incoming direct stream with the tunnel and the endpoint at the other side of the tunnel - will throw
	/// on error or return after connection is closed from the other end
	void         Connect            (KIOStreamSocket* DirectStream, const KTCPEndPoint& ConnectToEndpoint);

	/// run the event handler for this tunnel - may throw or return on error, otherwise blocking the current thread
	void         Run                ();

	/// returns the ip address of the opposite tunnel endpoint
	KTCPEndPoint GetEndPointAddress () const;

//----------
protected:
//----------

	/// returns true if a stream is set and is good for reading and writing
	bool HaveTunnel      () const;
	/// read a message from the stream, blocks until timeout, then throws
	void ReadMessage     (Message&  message);
	/// write a message to the stream, blocks until timeout, then throws
	void WriteMessage    (Message&& message);
	/// if we are establishing the tunnel connection we have to login with user/secret
	bool Login           (KStringView sUser, KStringView sSecret);
	/// waits for the other tunnel side to connect
	void WaitForLogin    ();
	/// set timeout for the tunnel connection
	void SetTimeout      (KDuration Timeout);
	/// force pings to check the tunnel
	void PingTest        (KUnixTime Time);
	/// connects to an outside target from one tunnel end
	void ConnectToTarget (std::size_t iID, KTCPEndPoint Target);
	/// setup payload AES encryption with sSecret
	void SetupEncryption (KStringView sUser, KStringView sSecret);
	/// setup payload AES encryption by trying to decode a message with a list of secrets
	bool SetupEncryption (Message& message, const KUnorderedSet<KString>& Secrets);

//----------
private:
//----------

	void Init            ();
	/// danger!
	KIOStreamSocket* GetStream() { return m_Tunnel.shared()->Stream.get(); }

	void TimerLoop       ();

	struct TunnelEnv
	{
		std::unique_ptr<KIOStreamSocket> Stream;
		std::unique_ptr<KBlockCipher>    Encryptor;
		std::unique_ptr<KBlockCipher>    Decryptor;
		KStopTime                        LastTx;
		KSteadyTime                      SendIdleNotBefore;
	};

	Config                 m_Config;
	KThreadSafe<TunnelEnv> m_Tunnel;
	Connections            m_Connections;
	KThreads               m_Threads;
	KDuration              m_RTT;
	KTimer::ID_t           m_TimerID       { KTimer::InvalidID };
	std::thread            m_Timer;
	bool                   m_bWaitForLogin { false };
	bool                   m_bMaskTx       { false };
	bool                   m_bQuit         { false };

}; // KTunnel

DEKAF2_NAMESPACE_END
