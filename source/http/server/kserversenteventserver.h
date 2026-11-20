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

/// @file kserversenteventserver.h
/// a server that sends events (server-sent events) to many connections

#include <dekaf2/core/init/kdefinitions.h>
#include <dekaf2/http/protocol/kserversentevent.h>
#include <dekaf2/net/util/kiostreamsocket.h>
#include <dekaf2/net/util/kpoll.h>
#include <dekaf2/threading/execution/kthreadpool.h>
#include <dekaf2/threading/primitives/kthreadsafe.h>
#include <dekaf2/containers/associative/kassociative.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/strings/kstringview.h>
#include <dekaf2/time/duration/kduration.h>
#include <dekaf2/time/duration/ktimer.h>
#include <atomic>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

DEKAF2_NAMESPACE_BEGIN

class KRESTServer;

/// @addtogroup http_server
/// @{

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Sends events (server-sent events) to the connections of many clients. A
/// connection reaches the server from a route of the REST framework: the route
/// calls KRESTServer::SubscribeToEvents(), and after it returned, the connection
/// leaves the worker thread of the REST server and stays with this server. KREST
/// owns one instance, see KREST::GetEventServer().
///
/// A connection is known by its handle, and may be subscribed to topics. All
/// methods are thread safe. They never write themselves: an event is appended to
/// the queue of each connection, and writer threads of this server send the
/// queues. The events of one connection keep their order. A connection whose
/// queue grows over Options::iMaxQueueBytes, or whose client does not read for
/// Options::WriteTimeout, is closed - the client connects again by itself.
///
/// The server notices when a client leaves, and removes its connection. Without
/// events it sends a comment line to a connection from time to time, which
/// keeps proxies from closing it.
///
/// @code
/// KREST Rest;
///
/// Routes.AddRoute("/events/:TOPIC").Get([](KRESTServer& HTTP)
/// {
///     // check here that the client may read the topic
///     HTTP.SubscribeToEvents(HTTP.GetQueryParm(":TOPIC"));
/// });
///
/// // from any thread
/// Rest.GetEventServer().Publish("news", { sText, "headline" });
/// @endcode
class DEKAF2_PUBLIC KServerSentEventServer
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	friend class KRESTServer;

//----------
public:
//----------

	using Handle = std::size_t;

	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	struct Options
	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		/// the number of threads that write to the connections, at least one
		std::size_t iWriterThreads    { 2 };
		/// the time a write to a connection may take - a connection whose client reads
		/// slower is closed
		KDuration   WriteTimeout      { chrono::seconds(10) };
		/// the count of bytes that may wait in the queue of one connection - a connection
		/// with more is closed (0 = no limit)
		std::size_t iMaxQueueBytes    { 16 * 1024 * 1024 };
		/// a connection that got no event for this time gets a comment line (zero = never)
		KDuration   KeepAliveInterval { chrono::seconds(15) };
		/// the time a client shall wait before it connects again, sent to each new
		/// connection (negative = not sent, the client uses its default)
		KDuration   Retry             { chrono::milliseconds(-1) };

	}; // Options

	KServerSentEventServer();
	explicit KServerSentEventServer(Options Options);
	~KServerSentEventServer();

	KServerSentEventServer(const KServerSentEventServer&)            = delete;
	KServerSentEventServer& operator=(const KServerSentEventServer&) = delete;

	//-----------------------------------------------------------------------------
	/// Set the options. Call this before the first connection arrives: the threads
	/// and the timer start with the first connection, and keep their settings.
	void SetOptions(Options Options);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// @param sTopic the topic
	/// @param Event the event to send to all connections that are subscribed to the topic
	/// @return the count of connections that took the event into their queue
	std::size_t Publish(KStringView sTopic, const KServerSentEvent& Event);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// @param iHandle the handle of a connection
	/// @param Event the event to send to this connection
	/// @return false if the handle is not known, if the connection is closing, or if
	/// it was closed because its queue was full
	bool Send(Handle iHandle, const KServerSentEvent& Event);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// @param Event the event to send to all connections
	/// @return the count of connections that took the event into their queue
	std::size_t Broadcast(const KServerSentEvent& Event);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// @param iHandle the handle of a connection
	/// @param sTopic a topic the connection shall receive the events of
	/// @return false if the handle is not known
	bool Subscribe(Handle iHandle, KStringView sTopic);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// @param iHandle the handle of a connection
	/// @param sTopic a topic the connection shall no more receive the events of
	/// @return false if the handle is not known, or was not subscribed to the topic
	bool Unsubscribe(Handle iHandle, KStringView sTopic);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Close a connection after the events that wait in its queue are sent. Further
	/// events for the handle are refused.
	/// @param iHandle the handle of a connection
	/// @return false if the handle is not known, or the connection is closing already
	bool Close(Handle iHandle);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// @param Callback called with the handle of each connection that was removed,
	/// from the thread that removed it. Set it before the first connection arrives.
	/// It may be called until this server is destroyed.
	void SetCloseCallback(std::function<void(Handle)> Callback);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// @return the count of connections
	DEKAF2_NODISCARD
	std::size_t size() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// @param sTopic a topic
	/// @return the count of connections that are subscribed to the topic
	DEKAF2_NODISCARD
	std::size_t size(KStringView sTopic) const;
	//-----------------------------------------------------------------------------

//----------
private:
//----------

	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// one connection: the stream, the queue of output, and the flag that gives
	/// the stream to one thread at a time
	struct Connection
	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		/// one item of the queue
		struct OutMessage
		{
			OutMessage(std::shared_ptr<const KString> sData, bool bClose)
			: sData(std::move(sData)), bClose(bClose) {}

			std::shared_ptr<const KString> sData;   // shared between the connections of a topic
			bool                           bClose;  // close the connection at this item
		};

		explicit Connection(Handle iHandle) : iHandle(iHandle) {}

		std::unique_ptr<KIOStreamSocket> Stream;                 // empty until the route returned
		Handle                           iHandle;
		int                              iFd          { -1 };
		std::mutex                       Mutex;                  // guards the queue and the flags below
		std::deque<OutMessage>           OutQueue;
		std::size_t                      iQueuedBytes { 0 };
		bool                             bBusy        { true  }; // a thread owns the stream - or the stream is not here yet
		bool                             bClosing     { false }; // a close waits in the queue
		bool                             bHadEvent    { false }; // an event was queued since the last comment line was due
		std::atomic<bool>                bDead        { false }; // the connection was removed
		std::vector<KString>             Topics;                 // guarded by the lock of the registry

	}; // Connection

	using ConnectionPtr = std::shared_ptr<Connection>;

	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	struct Registry
	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		KUnorderedMap<Handle, ConnectionPtr>          Connections;
		KUnorderedMap<KString, KUnorderedSet<Handle>> Topics;
	};

	//-----------------------------------------------------------------------------
	/// Create a connection that waits for its stream. It takes events into its
	/// queue already. Called by KRESTServer when a route subscribes.
	/// @return the handle of the connection
	DEKAF2_PRIVATE
	Handle Reserve();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Give the stream to a reserved connection, and start to send its queue.
	/// Called by KRESTServer after the route returned.
	/// @return false if the handle is not known, the stream is then not taken
	DEKAF2_PRIVATE
	bool Attach(Handle iHandle, std::unique_ptr<KIOStreamSocket>& Stream);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// remove a connection, and call the close callback
	DEKAF2_PRIVATE
	void Remove(Handle iHandle);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// start the writer threads and the timer of the comment lines, once
	DEKAF2_PRIVATE
	void Start();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	DEKAF2_PRIVATE
	ConnectionPtr GetConnection(Handle iHandle) const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// @return the connections of a topic, or all connections
	DEKAF2_PRIVATE
	std::vector<ConnectionPtr> GetConnections(KStringView sTopic, bool bAll) const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// append to the queue of a connection, and start a writer if no thread owns the stream
	/// @param bKeepAlive the item is a comment line, which is only due without events
	DEKAF2_PRIVATE
	bool Queue(const ConnectionPtr& pConnection, Connection::OutMessage Message, bool bKeepAlive = false);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// send to a list of connections
	DEKAF2_PRIVATE
	std::size_t QueueAll(const std::vector<ConnectionPtr>& Connections, const KServerSentEvent& Event);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// owner of the stream: send the queue of a connection, then release the stream
	DEKAF2_PRIVATE
	void Flush(ConnectionPtr pConnection);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// send a comment line to the connections that had no event
	DEKAF2_PRIVATE
	void KeepAlive();
	//-----------------------------------------------------------------------------

	Options                         m_Options;
	KSocketWatch                    m_Watch;          // tells when a client left
	KThreadPool                     m_Writers;
	KThreadSafe<Registry>           m_Registry;
	std::function<void(Handle)>     m_CloseCallback;
	std::mutex                      m_StartMutex;     // guards the start, the options and the callback
	KTimer::ID_t                    m_iTimer         { KTimer::InvalidID };
	std::atomic<Handle>             m_iLastHandle    { 0 };
	bool                            m_bStarted       { false };

}; // KServerSentEventServer

/// @}

DEKAF2_NAMESPACE_END
