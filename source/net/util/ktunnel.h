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


/// @file ktunnel.h
#include <dekaf2/net/util/kiostreamsocket.h>
#include <dekaf2/net/util/kpoll.h>
#include <dekaf2/containers/associative/kassociative.h>
#include <dekaf2/crypto/cipher/kblockcipher.h>
#include <dekaf2/crypto/ec/ked25519sign.h>  // KEd25519Key for ServerIdentity (DEKAF2_HAS_ED25519)
#include <dekaf2/crypto/ec/kx25519.h>       // pulls DEKAF2_HAS_X25519 — the v2 AES handshake needs both,
                                            // and including it here avoids a chicken-and-egg with the
                                            // matching #if guards in ktunnel.cpp
#include <dekaf2/web/url/kurl.h>
#include <dekaf2/threading/primitives/kthreadsafe.h>
#include <dekaf2/threading/execution/kthreads.h>
#include <dekaf2/time/duration/ktimer.h>
#include <dekaf2/core/errors/ksourcelocation.h>
#include <dekaf2/http/websocket/kwebsocket.h>
#include <thread>
#include <memory>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>

DEKAF2_NAMESPACE_BEGIN

/// @addtogroup net_util
/// @{

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Creates a fully transparent multiplexed tunnel for TCP stream connections between two instances of itself.
/// Both ends will be instantiated around a single open stream for input/output, which could be derived from any
/// std::iostream.
/// One of the two instances is required to use the constructor with user and secret set, the other one has to
/// leave those empty - which one should be clear from the concrete implementation of the connection setup.
/// Normally, the side waiting for a tunnel connection should not set the credentials, and the other one
/// seeking to establish the tunnel should set them.
/// You find a sample implementation in samples/ktunnel.h / samples/ktunnel.cpp
class DEKAF2_PUBLIC KTunnel
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	//------------------------- subclasses of KTunnel -------------------------

	// forward declaration so Config can refer to Connection below
	class Connection;

	/// Authentication callback invoked on the server side of the tunnel
	/// after the login message has been received (and, in AES mode,
	/// after the v2 X25519+Ed25519 handshake has established the session
	/// keys and the auth frame has been decrypted). The callback gets the
	/// node-endpoint name and password as they were presented by the
	/// remote client and must return true to accept the login, false to
	/// reject it.
	///
	/// If the callback is not set (the default), the tunnel falls back to
	/// the simple behaviour of verifying that the presented password is
	/// contained in Config::Secrets (the AdHoc pre-shared-secret model).
	/// If the callback IS set, it takes precedence and is the only
	/// authority on who is allowed in.
	using Authenticator = std::function<bool(KStringView sNode, KStringView sSecret)>;

#if DEKAF2_HAS_ED25519
	/// Trust-checker callback invoked on the *client* side during the
	/// v2 AES handshake, after the server's hello-ack has been received
	/// and its Ed25519 signature over the handshake transcript has been
	/// verified locally. The callback decides whether the presented
	/// server identity should be trusted: typical implementations look
	/// the fingerprint up in a known-hosts store, compare it against an
	/// explicit -trust-fingerprint flag, or prompt the operator (TOFU).
	/// Returning false aborts the handshake before any credentials are
	/// transmitted.
	///
	/// Arguments:
	///   * sHostPort       "host:port" of the waiting peer (for use as a
	///                     known-hosts lookup key and for prompts).
	///   * sRawPubKey      32 raw bytes of the server's Ed25519 public
	///                     key (suitable for serialisation into a
	///                     known-hosts file).
	///   * sFingerprint    formatted SHA-256 fingerprint of sRawPubKey,
	///                     ready to display to the operator (lowercase
	///                     hex, byte-wise colon-separated; matches the
	///                     OpenSSH host-key fingerprint style).
	///
	/// Required when bAESPayload is true on the client (initiating)
	/// side; ignored on the server side.
	using TrustChecker = std::function<bool(KStringView sHostPort,
	                                        KStringView sRawPubKey,
	                                        KStringView sFingerprint)>;
#endif

	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// Config setup for KTunnel
	struct DEKAF2_PUBLIC Config
	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		/// a list of accepted secrets (strings); in AES mode these also serve
		/// as the set of pre-shared keys that can decrypt the Login frame
		KUnorderedSet<KString> Secrets;
		/// optional authentication callback — if set, the tunnel calls this
		/// on the server side after the login message has been parsed
		/// (and, in AES mode, decrypted) to decide if the presented
		/// (user, secret) pair is allowed. If unset, Secrets.contains(secret)
		/// is used as the sole criterion (legacy behaviour).
		Authenticator          AuthCallback;
		/// timeout for incoming and outgoing connections (not for the tunnel itself)
		KDuration              Timeout        { chrono::seconds(15)        };
		/// interval to query tunnel health - should be low enough to avoid firewall and proxy timeouts,
		/// and because it is also used to calculate the connection timeout should be same on both ends
		KDuration              ControlPing    { chrono::seconds(60)        };
		/// timeout for connection setup, either for the tunnel itself or for any of the tunneled connections
		KDuration              ConnectTimeout { chrono::seconds(15)        };
		/// count of max multiplexed connections per tunnel - technical upper limit is 16 millions
		std::size_t            iMaxTunneledConnections { 100 };
		/// encode payload with AES (and engage the v2 X25519+Ed25519 handshake)?
		bool                   bAESPayload    { false };

#if DEKAF2_HAS_ED25519
		/// Server-side identity key (Ed25519). REQUIRED on the *waiting*
		/// (waiting) side when bAESPayload is true: every v2 hello-ack
		/// frame is signed with this key so the protected side can
		/// authenticate the server against a known fingerprint and refuse
		/// to talk to a TLS-intercepting middlebox that owns a forged
		/// chain-of-trust certificate. Ignored on the protected (initiating)
		/// side. Held by shared_ptr because Config is value-copied around
		/// the tunnel and KEd25519Key is move-only.
		std::shared_ptr<KEd25519Key>  ServerIdentity;

		/// Client-side trust-checker callback. REQUIRED on the *protected*
		/// (initiating) side when bAESPayload is true: invoked exactly once
		/// per handshake to decide whether the presented server identity is
		/// trusted. See TrustChecker for the contract. Ignored on the
		/// waiting side.
		TrustChecker                  TrustCallback;
#endif

		/// Optional handler invoked when a Connect frame arrives, INSTEAD of
		/// dialing the requested endpoint from this process. The handler owns
		/// the Connection and drives it with Connection::ReadData() /
		/// Connection::WriteData(); returning tears the channel down. Runs on
		/// its own worker thread. Used by a relay that does not connect to the
		/// endpoint itself but forwards the channel into another tunnel - the
		/// endpoint is then a name the relay resolves, not necessarily a host.
		/// If unset, Connect frames dial the endpoint as usual.
		std::function<void(std::shared_ptr<Connection>, KTCPEndPoint)>  ConnectCallback;

		/// Optional handler invoked on the peer side when the remote end
		/// requests a REPL channel via an OpenRepl frame. The handler owns
		/// the Connection and should use Connection::ReadData() /
		/// Connection::WriteData() as a duplex text stream. Return to tear
		/// the channel down. Runs on its own worker thread. If unset,
		/// incoming OpenRepl frames are rejected with a Disconnect.
		std::function<void(std::shared_ptr<Connection>)>  OpenReplCallback;

	}; // Config

	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// the message protocol used on top of the websocket frame,
	/// basically adding multiplex channels and (own) message types,
	/// and encryption/decryption if configured
	class DEKAF2_PUBLIC Message : protected KWebSocket::Frame
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
			Disconnect,
			/// Open a REPL control channel on the peer. Payload is currently empty;
			/// reserved for a future JSON option bag. After the frame is accepted
			/// the channel becomes a duplex text stream (Data frames carry lines).
			OpenRepl,
			/// Reserved: open a PTY-shell channel on the peer. Payload will carry
			/// a JSON options blob (command, environment, window size).
			OpenShell,
			/// Reserved: window-size change for a shell channel (WINCH).
			ShellResize
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
		void           SetType    (Type _type)           { m_Preamble[0] = _type;       }
		/// sets the message (payload)
		void           SetMessage (KString sMessage)     { Binary(std::move(sMessage)); }
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
		bool           Encode          (KStringView sInput,   KStringRef& sEncoded) override final;
		bool           Decode          (KStringView sEncoded, KStringRef& sDecoded) override final;

	//----------
	private:
	//----------

		/// prints the message content except for Data and Unknown types
		KStringView    PrintData  () const;
		/// throws and prints a description as of why
		void           Throw      (KString sError, KStringView sFunction = KSourceLocation::current().function_name()) const;

		mutable std::array<char, 4> m_Preamble{};
		KBlockCipher*               m_Cipher { nullptr };

	}; // Message

	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// runs one single connection in the multiplexed tunnel - uses two threads to pump data in and out of
	/// tunnel and outside connection
	class DEKAF2_PUBLIC Connection
	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{

	//----------
	public:
	//----------

		/// ctor, gets a unique ID (channel), a function pointer to send messages, and either a connected stream to the outside target or nullptr
		Connection (std::size_t iID, std::function<void(Message&&)> TunnelSend, KIOStreamSocket* DirectStream = nullptr);

		/// set the outside connection stream if not given with the ctor
		void        SetDirectStream       (KIOStreamSocket* DirectStream) { m_DirectStream = DirectStream; }

		/// Runs the data pump for this connection in BOTH directions, on the
		/// calling thread - it owns the direct stream alone (socket streams
		/// tolerate only one thread at a time, and a reader plus a writer on
		/// one stream consume each other's completions). One poll covers
		/// reading and writing, and outbound data is written in whatever
		/// chunks the stream takes right now, so a stalled peer cannot block
		/// the reading direction. Returns when either side closed.
		void        Pump                  ();

		/// puts data into the direct connection's queue and wakes the pump, may force a Pause frame if queue is too large
		void        SendData              (Message&& FromTunnel);
		/// shuts this connection down
		void        Disconnect            ();

		/// Block until the next Data-frame payload arrives on this channel.
		/// On success writes the payload into sOut and returns true. Returns
		/// false when the channel is closed (Disconnect frame or tunnel
		/// stop); sOut is left unchanged in that case. Intended for channel
		/// types that do not have a DirectStream (OpenRepl / future
		/// OpenShell), so Pump() is not running. Safe to call from a single
		/// consumer thread.
		bool                   ReadData   (KStringRef& sOut);
		/// Send a Data-frame payload back over this channel. Safe to call
		/// concurrently with ReadData(). No-op if the channel is already
		/// torn down.
		void                   WriteData  (KString sPayload);

		/// returns the unique ID (channel) for this connection
		std::size_t GetID                 () const { return m_iID;      }

		/// pause sending frames for this connection
		void        Pause                 ()       { m_bPaused = true;  }
		/// resume sending frames for this connection
		void        Resume                ();
		/// check if this connection shall pause sending frames
		bool        IsPaused              () const { return m_bPaused;  }

		/// set the target endpoint this connection forwards to (for diagnostics)
		void        SetTarget             (KTCPEndPoint Target);
		/// returns the target endpoint this connection forwards to
		KTCPEndPoint GetTarget            () const;
		/// set the address of the local peer whose stream we forward (for diagnostics)
		void        SetPeer               (KString sPeer);
		/// returns the address of the local peer whose stream we forward
		KString     GetPeer               () const;
		/// returns the wall-clock time this connection was created
		KUnixTime   GetStartTime          () const { return m_tStart;   }
		/// returns the bytes written to the direct stream so far
		uint64_t    GetBytesToDirect      () const { return m_iBytesToDirect;   }
		/// returns the bytes read from the direct stream so far
		uint64_t    GetBytesFromDirect    () const { return m_iBytesFromDirect; }
		/// returns the error the peer reported in its Disconnect frame,
		/// or the empty string after a normal close
		KString     GetDisconnectReason   () const;
		/// like GetDisconnectReason(), but waits up to Timeout for a late
		/// Disconnect frame - for the case that the direct stream closed
		/// before the peer's error arrived
		KString     WaitForDisconnectReason (KDuration Timeout) const;

		/// what WaitForActivity() observed on the channel
		enum class Activity : uint8_t { Timeout, Closed, Data };

		/// wait up to Timeout for activity on this channel: Data = a data
		/// frame arrived, Closed = the channel was closed (the peer's
		/// reason, if any, is available from GetDisconnectReason()),
		/// Timeout = nothing happened. Does not consume queued frames.
		Activity    WaitForActivity       (KDuration Timeout) const;

		/// max size for the message queue for one connection
		static constexpr
		std::size_t MaxMessageQueueSize   ()       { return 20;         }

	//----------
	private:
	//----------

		std::queue<Message>                  m_MessageQueue;
		std::function<void(Message&&)>       m_Tunnel;
		KIOStreamSocket*                     m_DirectStream { nullptr };
		mutable std::mutex                   m_QueueMutex;
		/// diagnostics, all protected by m_QueueMutex
		KTCPEndPoint                         m_Target;
		KString                              m_sPeer;
		KString                              m_sDisconnectReason;
		const KUnixTime                      m_tStart { KUnixTime::now() };
		std::atomic<uint64_t>                m_iBytesToDirect   { 0 };
		std::atomic<uint64_t>                m_iBytesFromDirect { 0 };
		mutable std::condition_variable      m_FreshData;
		/// serialises the pause flag handover in Resume()
		std::mutex                           m_TunnelMutex;
		/// wakes Pump() out of its poll when another thread queued outbound
		/// data, lifted a pause, or asked for a disconnect. Owned by the
		/// Connection (not by the stream), so it stays valid as long as
		/// anyone holds the Connection
		KPollInterruptor                     m_Wakeup;
		std::size_t                          m_iID          { 0 };
		bool                                 m_bRXPaused    { false };
		std::atomic<bool>                    m_bPaused      { false };
		std::atomic<bool>                    m_bQuit        { false };

	}; // Connection

	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// holds all multiplexed connections through the tunnel, thread safe
	class DEKAF2_PUBLIC Connections
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
		/// returns a snapshot of all current connections
		std::vector<std::shared_ptr<Connection>> Snapshot () const;

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
		KStringView                      sNode      = KStringView{},
		KStringView                      sSecret    = KStringView{},
		bool                             bNeverMask = false
	);

	// dtor
	~KTunnel();

	/// how a forwarded connection ended, returned by Connect()
	struct ConnectResult
	{
		/// error the peer reported in its Disconnect frame, empty on a normal close
		KString  sDisconnectReason;
		/// payload bytes sent towards the target
		uint64_t iBytesToTarget   { 0 };
		/// payload bytes received from the target
		uint64_t iBytesFromTarget { 0 };
	};

	/// connect an incoming direct stream with the tunnel and the endpoint at the other side of the tunnel - will throw
	/// on error or return after connection is closed from the other end
	ConnectResult Connect           (KIOStreamSocket* DirectStream, const KTCPEndPoint& ConnectToEndpoint);

	/// returns a snapshot of all multiplexed connections currently carried
	/// over this tunnel
	std::vector<std::shared_ptr<Connection>> GetConnectionSnapshot () const { return m_Connections.Snapshot(); }

	/// verdict of a ProbeConnect()
	struct ProbeResult
	{
		/// true when the target accepted the connection
		bool    bConnected { false };
		/// what happened, suitable for display
		KString sMessage;
	};

	/// Probe a connect to an endpoint on the other side of the tunnel,
	/// over the exact Connect path a forwarded connection takes, but
	/// without a local stream. Waits up to Timeout for the peer's verdict:
	/// an error reported by the peer means the target is unreachable,
	/// arriving data or a silently open channel means the target accepted
	/// the connection. Timeout should exceed the peer's ConnectTimeout.
	/// Works against any peer version - old nodes without error reporting
	/// degrade to the silent-open/closed distinction.
	ProbeResult ProbeConnect (const KTCPEndPoint& ConnectToEndpoint, KDuration Timeout);

	/// Open a new REPL channel on the remote peer. Allocates a channel
	/// ID, sends an OpenRepl frame, and returns the local Connection
	/// proxy for duplex Connection::ReadData() / Connection::WriteData()
	/// I/O. The remote peer dispatches this into its
	/// Config::OpenReplCallback. Returns a null shared_ptr if no free
	/// channel is available or the tunnel is not established.
	std::shared_ptr<Connection> OpenRepl ();

	/// Open a new forwarding channel on the remote peer: allocates a
	/// channel, sends a Connect frame for @p Target, and returns the local
	/// Connection for duplex Connection::ReadData() / WriteData() I/O -
	/// the variant of Connect() for callers that have no local stream to
	/// pump, e.g. a relay bridging two tunnels. Close it with CloseRepl().
	/// Returns a null shared_ptr if no channel is available.
	std::shared_ptr<Connection> OpenForward (const KTCPEndPoint& Target);

	/// Close a REPL channel opened with OpenRepl(): sends a Disconnect
	/// frame so the peer's handler unblocks, tears the local Connection
	/// down, and removes it from the connection registry. Without this
	/// the channel slot stays occupied until the tunnel itself dies.
	void CloseRepl (const std::shared_ptr<Connection>& Connection);

	/// run the event handler for this tunnel - may throw or return on error, otherwise blocking the current thread
	void         Run                ();

	/// Stop a running Run(). Thread-safe: closes the underlying tunnel stream
	/// so a blocked ReadMessage()/WriteMessage() returns promptly and the
	/// for(;HaveTunnel();) loop in Run() exits. Safe to call multiple times
	/// and safe to call from a signal / service-control handler.
	void         Stop               ();

	/// returns the ip address of the opposite tunnel endpoint
	KTCPEndPoint GetEndPointAddress () const;

	/// Returns the node-endpoint name that was presented at login by the
	/// remote side. Empty on the client side of the tunnel (that is, on
	/// the side that called Login() itself) and on a server-side tunnel
	/// before the login frame has been processed. Thread-safe.
	KString      GetLoginNode       () const;

#if DEKAF2_HAS_ED25519
	/// Format an Ed25519 (or any 32-byte) raw public key as a colon-
	/// separated lowercase-hex SHA-256 fingerprint (95 chars: 32 bytes
	/// rendered as 64 hex chars + 31 separators). Matches the OpenSSH
	/// `ssh-keygen -E sha256 -lf` fingerprint style modulo the leading
	/// `SHA256:` prefix and base64 encoding (we use hex to keep visual
	/// comparison easier across paste boundaries). Pure function, safe
	/// to call without an instance.
	DEKAF2_NODISCARD
	static KString FormatFingerprint (KStringView sRawPublicKey);
#endif

	/// Cumulative count of payload bytes received through the tunnel
	/// (Data-frame payloads only — no protocol/framing overhead, no
	/// Login/Helo/Ping/Pong/Control bytes). Thread-safe.
	uint64_t     GetBytesRx         () const noexcept { return m_iBytesRx.load(std::memory_order_relaxed); }

	/// Cumulative count of payload bytes sent through the tunnel
	/// (Data-frame payloads only — no protocol/framing overhead, no
	/// Login/Helo/Ping/Pong/Control bytes). Thread-safe.
	uint64_t     GetBytesTx         () const noexcept { return m_iBytesTx.load(std::memory_order_relaxed); }

	/// Current count of multiplexed connections carried over this tunnel.
	/// Thread-safe.
	std::size_t  GetConnectionCount () const { return m_Connections.size(); }

//----------
protected:
//----------

	/// returns true if a stream is set and is good for reading and writing
	bool HaveTunnel      () const;
	/// read a message from the stream, blocks until timeout, then throws
	void ReadMessage     (Message&  message);
	/// Write a message to the stream, blocks until timeout, then throws. Only
	/// for the single threaded login phase and for the Run() reactor draining
	/// the outbound queue - everyone else uses QueueMessage()
	void WriteMessage    (Message&& message);
	/// Hand a message to the tunnel: it is appended to the outbound queue and
	/// Run() writes it as soon as the stream takes it. Callable from any
	/// thread, and it never touches the stream, which tolerates only one
	/// thread at a time - a blocking write from a foreign thread would starve
	/// the reader behind it (and with it the whole tunnel)
	void QueueMessage    (Message&& message);
	/// dispatch one message that arrived through the tunnel - Run() only
	void HandleMessage   (Message&& FromTunnel);
	/// if we are establishing the tunnel connection we have to login with node/secret
	bool Login           (KStringView sNode, KStringView sSecret);
	/// waits for the other tunnel side to connect
	void WaitForLogin    ();
	/// set timeout for the tunnel connection
	void SetTimeout      (KDuration Timeout);
	/// force pings to check the tunnel
	void PingTest        (KUnixTime Time);
	/// connects to an outside target from one tunnel end
	void ConnectToTarget (std::size_t iID, KTCPEndPoint Target);

#if DEKAF2_HAS_ED25519
	/// Run the client-side half of the v2 AES handshake on the already-
	/// open tunnel stream. Sends the hello frame, receives the signed
	/// hello-ack, verifies the server identity, runs the TrustCallback
	/// (TOFU / known-hosts), derives session keys via X25519+HKDF-SHA256,
	/// and configures the AES-256-GCM encryptor / decryptor on the
	/// tunnel. Throws on any failure (no signature, malformed frame,
	/// untrusted identity, etc.) so the operator sees a noisy error
	/// instead of a 15-second silent timeout. Does NOT send the auth
	/// frame — Login() does that with the now-active session keys.
	void SetupEncryption (KStringView sNode);

	/// Run the server-side half of the v2 AES handshake. The hello
	/// frame has already been read by WaitForLogin() and is passed in
	/// as @p HelloFrame; @p sOutNode receives the node-endpoint name
	/// field from that frame so the caller can later authenticate it.
	/// Sends the signed hello-ack, derives session keys, configures the
	/// AES ciphers. Throws on malformed input or missing ServerIdentity.
	/// Returns true on success (kept as bool for symmetry with the
	/// existing call site, even though the failure path always throws).
	bool SetupEncryption (Message& HelloFrame, KStringRef& sOutNode);
#endif

//----------
private:
//----------

	void Init            ();
	/// danger!
	KIOStreamSocket* GetStream() { return m_Tunnel.shared()->Stream.get(); }

	struct TunnelEnv
	{
		std::unique_ptr<KIOStreamSocket> Stream;
		std::unique_ptr<KBlockCipher>    Encryptor;
		std::unique_ptr<KBlockCipher>    Decryptor;
	};

	Config                 m_Config;
	KThreadSafe<TunnelEnv> m_Tunnel;
	/// messages waiting to go out through the tunnel, filled by any thread,
	/// drained by Run()
	KThreadSafe<std::deque<Message>> m_OutQueue;
	/// wakes Run() out of its poll when another thread queued a message
	KPollInterruptor       m_Wakeup;
	Connections            m_Connections;
	KThreads               m_Threads;
	KDuration              m_RTT;
	KTimer::ID_t           m_TimerID       { KTimer::InvalidID };
	/// name of the node-endpoint as verified at login — only populated on
	/// the server side (the side that called WaitForLogin()). Protected by
	/// m_LoginNodeMutex because it is written from the tunnel's Run()
	/// thread but may be read from admin / monitoring threads.
	KString                m_sLoginNode;
	mutable std::mutex     m_LoginNodeMutex;
	/// cumulative Data-frame payload byte counters for this tunnel
	std::atomic<uint64_t>  m_iBytesRx      { 0 };
	std::atomic<uint64_t>  m_iBytesTx      { 0 };
	bool                   m_bWaitForLogin { false };
	bool                   m_bMaskTx       { false };

}; // KTunnel


/// @}

DEKAF2_NAMESPACE_END
