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

#include <dekaf2/http/server/kserversenteventserver.h>
#include <dekaf2/core/init/dekaf2.h>
#include <dekaf2/core/logging/klog.h>
#include <dekaf2/core/format/kformat.h>
#include <dekaf2/core/strings/kstringutils.h>
#include <dekaf2/time/clock/ktime.h>
#include <algorithm>

DEKAF2_NAMESPACE_BEGIN

//-----------------------------------------------------------------------------
KServerSentEventServer::KServerSentEventServer()
//-----------------------------------------------------------------------------
: KServerSentEventServer(Options{})
{
} // ctor

//-----------------------------------------------------------------------------
KServerSentEventServer::KServerSentEventServer(Options Options)
//-----------------------------------------------------------------------------
: m_Options(std::move(Options))
// the time of the start tells the ids of this server from those of an earlier run
, m_sRun(KString::to_hexstring(static_cast<uint64_t>(chrono::duration_cast<chrono::microseconds>(KUnixTime::now().time_since_epoch()).count()), false, false))
{
	// the watcher thread starts with the first connection, the writers in Start()
	m_Watch.SetThreadName("eventwatch");
	m_Writers.set_thread_name("eventwriter");

} // ctor

//-----------------------------------------------------------------------------
KServerSentEventServer::~KServerSentEventServer()
//-----------------------------------------------------------------------------
{
	if (m_iTimer != KTimer::InvalidID)
	{
		// after the return no comment line is on its way
		Dekaf::getInstance().GetTimer().Cancel(m_iTimer);
	}

	// first no more events of the watcher, then the writers finish their work,
	// then the registry closes the connections
	m_Watch.Stop();
	m_Writers.stop();

} // dtor

//-----------------------------------------------------------------------------
void KServerSentEventServer::SetOptions(Options Options)
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_StartMutex);

	m_Options = std::move(Options);

} // SetOptions

//-----------------------------------------------------------------------------
void KServerSentEventServer::SetCloseCallback(std::function<void(Handle)> Callback)
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_StartMutex);

	m_CloseCallback = std::move(Callback);

} // SetCloseCallback

//-----------------------------------------------------------------------------
void KServerSentEventServer::Start()
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_StartMutex);

	if (m_bStarted)
	{
		return;
	}

	m_bStarted = true;

	auto iThreads = std::max(m_Options.iWriterThreads, std::size_t(1));

	kDebug(2, "starting event server with {} writer threads", iThreads);

	m_Writers.resize(iThreads);

	if (m_Options.KeepAliveInterval > KDuration::zero())
	{
		// the callback only fills queues, it may run in the thread of the timer
		m_iTimer = Dekaf::getInstance().GetTimer().CallEvery(m_Options.KeepAliveInterval, [this](KUnixTime)
		{
			KeepAlive();

		}, false);
	}

} // Start

//-----------------------------------------------------------------------------
KServerSentEventServer::ConnectionPtr KServerSentEventServer::GetConnection(Handle iHandle) const
//-----------------------------------------------------------------------------
{
	auto Registry = m_Registry.shared();

	auto it = Registry->Connections.find(iHandle);

	if (it == Registry->Connections.end())
	{
		return nullptr;
	}

	return it->second;

} // GetConnection

//-----------------------------------------------------------------------------
std::vector<KServerSentEventServer::ConnectionPtr> KServerSentEventServer::GetConnections(KStringView sTopic, bool bAll) const
//-----------------------------------------------------------------------------
{
	// copy the connections out under the lock, the queues are filled without it
	std::vector<ConnectionPtr> Connections;

	auto Registry = m_Registry.shared();

	if (bAll)
	{
		Connections.reserve(Registry->Connections.size());

		for (const auto& Connection : Registry->Connections)
		{
			Connections.push_back(Connection.second);
		}
	}
	else
	{
		auto Topic = Registry->Topics.find(sTopic);

		if (Topic != Registry->Topics.end())
		{
			Connections.reserve(Topic->second.size());

			for (auto iHandle : Topic->second)
			{
				auto it = Registry->Connections.find(iHandle);

				if (it != Registry->Connections.end())
				{
					Connections.push_back(it->second);
				}
			}
		}
	}

	return Connections;

} // GetConnections

//-----------------------------------------------------------------------------
KServerSentEventServer::Handle KServerSentEventServer::Reserve(KStringView sClient, Refusal& Refused)
//-----------------------------------------------------------------------------
{
	auto Registry = m_Registry.unique();

	if (m_Options.iMaxConnections && Registry->Connections.size() >= m_Options.iMaxConnections)
	{
		kDebug(1, "refusing event connection from {}: {} connections", sClient, Registry->Connections.size());
		Refused = Refusal::TooManyConnections;
		return 0;
	}

	auto& iFromClient = Registry->Clients[sClient];

	if (m_Options.iMaxConnectionsPerClient && iFromClient >= m_Options.iMaxConnectionsPerClient)
	{
		kDebug(1, "refusing event connection from {}: {} connections of the client", sClient, iFromClient);
		Refused = Refusal::TooManyFromClient;
		return 0;
	}

	Handle iHandle { ++m_iLastHandle };

	if (!iHandle)
	{
		// 0 is no handle - skip it on overflow
		iHandle = ++m_iLastHandle;
	}

	Registry->Connections.insert({ iHandle, std::make_shared<Connection>(iHandle, sClient) });
	++iFromClient;

	kDebug(2, "reserved event connection with handle {} for {}", iHandle, sClient);

	Refused = Refusal::None;

	return iHandle;

} // Reserve

//-----------------------------------------------------------------------------
bool KServerSentEventServer::Attach(Handle iHandle, std::unique_ptr<KIOStreamSocket>& Stream)
//-----------------------------------------------------------------------------
{
	if (!Stream)
	{
		Remove(iHandle);
		return false;
	}

	auto pConnection = GetConnection(iHandle);

	if (!pConnection)
	{
		// closed by the application in the meantime
		kDebug(2, "event connection {} is gone", iHandle);
		return false;
	}

	// dekaf2 tracks fds as int (KPoll)
	auto iFd = static_cast<int>(Stream->GetNativeSocket());

	if (iFd < 0)
	{
		kDebug(1, "cannot add event connection {}: invalid socket", iHandle);
		Remove(iHandle);
		return false;
	}

	Start();

	KDuration WriteTimeout;
	KDuration Retry;

	{
		std::lock_guard<std::mutex> Lock(m_StartMutex);

		WriteTimeout = m_Options.WriteTimeout;
		Retry        = m_Options.Retry;
	}

	Stream->SetTimeout(WriteTimeout);

	{
		std::lock_guard<std::mutex> Lock(pConnection->Mutex);

		pConnection->Stream = std::move(Stream);
		pConnection->iFd    = iFd;

		if (Retry >= KDuration::zero())
		{
			// before the events that were queued since the route subscribed
			auto sRetry = std::make_shared<const KString>(KServerSentEvent::Retry(Retry));

			pConnection->iQueuedBytes += sRetry->size();
			pConnection->OutQueue.emplace_front(std::move(sRetry), false);
		}
	}

	kDebug(2, "added event connection with handle {} on fd {}", iHandle, iFd);

	// the watcher reports once when the client left
	KPoll::Parameters Params;
	Params.iParameter = iHandle;
	Params.bOnce      = true;
	Params.Callback   = [this](int, uint16_t, std::size_t iHandle)
	{
		kDebug(2, "client of event connection {} left", iHandle);
		Remove(iHandle);
	};

	m_Watch.Add(iFd, std::move(Params));

	if (pConnection->bDead)
	{
		// removed since the lookup above, without the fd in the watcher - take it
		// out again while pConnection keeps the stream open
		m_Watch.Remove(iFd);
		return true;
	}

	// a reserved connection is busy - pass the ownership of its stream to a writer,
	// which sends what was queued so far and then releases it
	m_Writers.Post([this, pConnection]() { Flush(pConnection); });

	return true;

} // Attach

//-----------------------------------------------------------------------------
void KServerSentEventServer::Remove(Handle iHandle)
//-----------------------------------------------------------------------------
{
	ConnectionPtr pConnection;

	{
		auto Registry = m_Registry.unique();

		auto it = Registry->Connections.find(iHandle);

		if (it == Registry->Connections.end())
		{
			kDebug(2, "event connection {} already removed", iHandle);
			return;
		}

		pConnection = std::move(it->second);
		Registry->Connections.erase(it);

		auto Client = Registry->Clients.find(pConnection->sClient);

		if (Client != Registry->Clients.end() && --Client->second == 0)
		{
			Registry->Clients.erase(Client);
		}

		for (const auto& sTopic : pConnection->Topics)
		{
			auto Topic = Registry->Topics.find(sTopic);

			if (Topic != Registry->Topics.end())
			{
				Topic->second.erase(iHandle);

				if (Topic->second.empty())
				{
					Registry->Topics.erase(Topic);
				}
			}
		}
	}

	kDebug(2, "removing event connection with handle {}", iHandle);

	// stops a writer before its next write
	pConnection->bDead = true;

	int iFd;

	{
		std::lock_guard<std::mutex> Lock(pConnection->Mutex);
		iFd = pConnection->iFd;
	}

	if (iFd >= 0)
	{
		// the stream is open as long as pConnection lives, its fd is not yet used again
		m_Watch.Remove(iFd);
	}

	std::function<void(Handle)> Callback;

	{
		std::lock_guard<std::mutex> Lock(m_StartMutex);
		Callback = m_CloseCallback;
	}

	if (Callback)
	{
		Callback(iHandle);
	}

} // Remove

//-----------------------------------------------------------------------------
bool KServerSentEventServer::Queue(const ConnectionPtr& pConnection, Connection::OutMessage Message, bool bKeepAlive)
//-----------------------------------------------------------------------------
{
	bool bTakeOwnership { false };

	{
		std::unique_lock<std::mutex> Lock(pConnection->Mutex);

		if (pConnection->bDead || pConnection->bClosing)
		{
			return false;
		}

		if (bKeepAlive)
		{
			auto bDue = !pConnection->bHadEvent && pConnection->Stream;

			pConnection->bHadEvent = false;

			if (!bDue)
			{
				return false;
			}
		}
		else
		{
			pConnection->bHadEvent = true;
		}

		auto iSize = Message.sData ? Message.sData->size() : 0;

		if (m_Options.iMaxQueueBytes && pConnection->iQueuedBytes + iSize > m_Options.iMaxQueueBytes)
		{
			// the client does not read fast enough
			kDebug(1, "queue of event connection {} is full with {} bytes - closing it",
			          pConnection->iHandle, pConnection->iQueuedBytes);
			pConnection->bDead = true;
			Lock.unlock();

			// a writer removes the connection: the caller may hold the lock of the
			// history, which the close callback of the application may need
			auto iHandle = pConnection->iHandle;
			Start();
			m_Writers.Post([this, iHandle]() { Remove(iHandle); });

			return false;
		}

		if (Message.bClose)
		{
			// nothing may be queued after a close
			pConnection->bClosing = true;
		}

		pConnection->iQueuedBytes += iSize;
		pConnection->OutQueue.push_back(std::move(Message));

		if (!pConnection->bBusy)
		{
			// no thread owns the stream - take it for a writer
			pConnection->bBusy = true;
			bTakeOwnership     = true;
		}
	}

	if (bTakeOwnership)
	{
		m_Writers.Post([this, pConnection]() { Flush(pConnection); });
	}

	return true;

} // Queue

//-----------------------------------------------------------------------------
std::size_t KServerSentEventServer::QueueAll(const std::vector<ConnectionPtr>& Connections, const KServerSentEvent& Event)
//-----------------------------------------------------------------------------
{
	if (Connections.empty())
	{
		return 0;
	}

	// all connections share the serialized event
	return QueueAll(Connections, std::make_shared<const KString>(Event.Serialize()));

} // QueueAll

//-----------------------------------------------------------------------------
std::size_t KServerSentEventServer::QueueAll(const std::vector<ConnectionPtr>& Connections, const std::shared_ptr<const KString>& sData)
//-----------------------------------------------------------------------------
{
	if (sData->empty())
	{
		// an event without content
		return 0;
	}

	std::size_t iQueued { 0 };

	for (const auto& pConnection : Connections)
	{
		if (Queue(pConnection, Connection::OutMessage(sData, false)))
		{
			++iQueued;
		}
	}

	return iQueued;

} // QueueAll

//-----------------------------------------------------------------------------
void KServerSentEventServer::Flush(ConnectionPtr pConnection)
//-----------------------------------------------------------------------------
{
	// this thread owns the stream until it releases bBusy

	for (;;)
	{
		std::deque<Connection::OutMessage> Messages;

		{
			std::lock_guard<std::mutex> Lock(pConnection->Mutex);

			if (pConnection->bDead)
			{
				return;
			}

			if (pConnection->OutQueue.empty())
			{
				// release under the lock that checked the queue: a concurrent
				// Queue() either sees the release, or this thread sees its item
				pConnection->bBusy = false;
				return;
			}

			Messages.swap(pConnection->OutQueue);
			pConnection->iQueuedBytes = 0;
		}

		// write without the lock - this may block, but no other thread uses the stream
		auto& Stream = *pConnection->Stream;

		for (const auto& Message : Messages)
		{
			if (Message.sData)
			{
				Stream.Write(*Message.sData);
			}

			if (Message.bClose)
			{
				Stream.Flush();
				Remove(pConnection->iHandle);
				return;
			}
		}

		Stream.Flush();

		if (!Stream.Good())
		{
			kDebug(1, "write failed on event connection {} - closing it", pConnection->iHandle);
			Remove(pConnection->iHandle);
			return;
		}
	}

} // Flush

//-----------------------------------------------------------------------------
void KServerSentEventServer::KeepAlive()
//-----------------------------------------------------------------------------
{
	auto Connections = GetConnections(KStringView{}, true);

	if (Connections.empty())
	{
		return;
	}

	auto sComment = std::make_shared<const KString>(KServerSentEvent::Comment());

	for (const auto& pConnection : Connections)
	{
		Queue(pConnection, Connection::OutMessage(sComment, false), true);
	}

} // KeepAlive

//-----------------------------------------------------------------------------
std::size_t KServerSentEventServer::Publish(KStringView sTopic, const KServerSentEvent& Event)
//-----------------------------------------------------------------------------
{
	if (!m_Options.iHistory)
	{
		return QueueAll(GetConnections(sTopic, false), Event);
	}

	if (Event.sData.empty() && Event.sEvent.empty())
	{
		// clients drop an event without data and type, it gets no id
		return 0;
	}

	// from the id to the queues under one lock: every connection gets the
	// events in the order of their ids, and a replay misses none
	std::lock_guard<std::mutex> Lock(m_HistoryMutex);

	auto iID = ++m_iLastID;

	KServerSentEvent WithID(Event);
	WithID.sID = kFormat("{}-{}", m_sRun, iID);

	auto sData = std::make_shared<const KString>(WithID.Serialize());

	auto& History = m_History[sTopic];

	History.Events.emplace_back(iID, sData);

	if (History.Events.size() > m_Options.iHistory)
	{
		History.iLastEvicted = History.Events.front().iID;
		History.Events.pop_front();
	}

	return QueueAll(GetConnections(sTopic, false), sData);

} // Publish

//-----------------------------------------------------------------------------
bool KServerSentEventServer::ParseID(KStringView sID, uint64_t& iID) const
//-----------------------------------------------------------------------------
{
	return sID.remove_prefix(m_sRun) && sID.remove_prefix("-") && kFromDigits(iID, sID);

} // ParseID

//-----------------------------------------------------------------------------
void KServerSentEventServer::Replay(const ConnectionPtr& pConnection, const std::vector<KStringView>& Topics, KStringView sLastEventID)
//-----------------------------------------------------------------------------
{
	uint64_t iLast { 0 };

	if (!ParseID(sLastEventID, iLast))
	{
		kDebug(2, "event connection {}: last event id '{}' is not of this server", pConnection->iHandle, sLastEventID);
		pConnection->bMissed = true;
		return;
	}

	std::vector<History::Item> Items;
	std::size_t                iFromTopics { 0 };

	for (auto sTopic : Topics)
	{
		auto it = m_History.find(sTopic);

		if (it == m_History.end())
		{
			// no event for this topic so far
			continue;
		}

		const auto& History = it->second;

		if (iLast < History.iLastEvicted)
		{
			// an event that the client has not seen left the history
			kDebug(2, "event connection {}: the history of '{}' does not reach back to id {}", pConnection->iHandle, sTopic, iLast);
			pConnection->bMissed = true;
			return;
		}

		auto iBefore = Items.size();

		for (const auto& Item : History.Events)
		{
			if (Item.iID > iLast)
			{
				Items.push_back(Item);
			}
		}

		if (Items.size() > iBefore)
		{
			++iFromTopics;
		}
	}

	if (iFromTopics > 1)
	{
		// the events of one topic are in the order of their ids already
		std::sort(Items.begin(), Items.end(), [](const History::Item& left, const History::Item& right)
		{
			return left.iID < right.iID;
		});
	}

	kDebug(2, "event connection {}: sending {} events again", pConnection->iHandle, Items.size());

	for (auto& Item : Items)
	{
		if (!Queue(pConnection, Connection::OutMessage(std::move(Item.sData), false)))
		{
			break;
		}
	}

} // Replay

//-----------------------------------------------------------------------------
bool KServerSentEventServer::HasMissedEvents(Handle iHandle) const
//-----------------------------------------------------------------------------
{
	auto pConnection = GetConnection(iHandle);

	return pConnection && pConnection->bMissed;

} // HasMissedEvents

//-----------------------------------------------------------------------------
std::size_t KServerSentEventServer::Broadcast(const KServerSentEvent& Event)
//-----------------------------------------------------------------------------
{
	return QueueAll(GetConnections(KStringView{}, true), Event);

} // Broadcast

//-----------------------------------------------------------------------------
bool KServerSentEventServer::Send(Handle iHandle, const KServerSentEvent& Event)
//-----------------------------------------------------------------------------
{
	auto pConnection = GetConnection(iHandle);

	if (!pConnection)
	{
		kDebug(2, "cannot send to handle {}: unknown connection", iHandle);
		return false;
	}

	return QueueAll({ pConnection }, Event) == 1;

} // Send

//-----------------------------------------------------------------------------
bool KServerSentEventServer::Subscribe(Handle iHandle, KStringView sTopic)
//-----------------------------------------------------------------------------
{
	return Subscribe(iHandle, std::vector<KStringView>{ sTopic }, KStringView{});

} // Subscribe

//-----------------------------------------------------------------------------
bool KServerSentEventServer::Subscribe(Handle iHandle, const std::vector<KStringView>& Topics, KStringView sLastEventID)
//-----------------------------------------------------------------------------
{
	auto pConnection = GetConnection(iHandle);

	if (!pConnection)
	{
		return false;
	}

	// with a history no event may be published between the replay and the subscription
	std::unique_lock<std::mutex> HistoryLock(m_HistoryMutex, std::defer_lock);

	if (m_Options.iHistory)
	{
		HistoryLock.lock();
	}

	// the topics that the connection does not have yet
	std::vector<KStringView> NewTopics;

	{
		auto Registry = m_Registry.shared();

		for (auto sTopic : Topics)
		{
			if (!sTopic.empty()
			    && std::find(pConnection->Topics.begin(), pConnection->Topics.end(), sTopic) == pConnection->Topics.end()
			    && std::find(NewTopics.begin(), NewTopics.end(), sTopic) == NewTopics.end())
			{
				NewTopics.push_back(sTopic);
			}
		}
	}

	if (HistoryLock.owns_lock() && !sLastEventID.empty() && !NewTopics.empty())
	{
		Replay(pConnection, NewTopics, sLastEventID);
	}

	auto Registry = m_Registry.unique();

	if (Registry->Connections.find(iHandle) == Registry->Connections.end())
	{
		// removed in the meantime
		return false;
	}

	for (auto sTopic : NewTopics)
	{
		if (Registry->Topics[sTopic].insert(iHandle).second)
		{
			pConnection->Topics.push_back(sTopic);
		}
	}

	return true;

} // Subscribe

//-----------------------------------------------------------------------------
bool KServerSentEventServer::Unsubscribe(Handle iHandle, KStringView sTopic)
//-----------------------------------------------------------------------------
{
	auto Registry = m_Registry.unique();

	auto it = Registry->Connections.find(iHandle);

	if (it == Registry->Connections.end())
	{
		return false;
	}

	auto Topic = Registry->Topics.find(sTopic);

	if (Topic == Registry->Topics.end() || !Topic->second.erase(iHandle))
	{
		return false;
	}

	if (Topic->second.empty())
	{
		Registry->Topics.erase(Topic);
	}

	auto& Topics = it->second->Topics;

	Topics.erase(std::remove(Topics.begin(), Topics.end(), sTopic), Topics.end());

	return true;

} // Unsubscribe

//-----------------------------------------------------------------------------
bool KServerSentEventServer::Close(Handle iHandle)
//-----------------------------------------------------------------------------
{
	auto pConnection = GetConnection(iHandle);

	if (!pConnection)
	{
		return false;
	}

	// the close lines up behind the events that wait - the writer removes
	// the connection when it arrives there
	return Queue(pConnection, Connection::OutMessage(nullptr, true));

} // Close

//-----------------------------------------------------------------------------
std::size_t KServerSentEventServer::size() const
//-----------------------------------------------------------------------------
{
	return m_Registry.shared()->Connections.size();

} // size

//-----------------------------------------------------------------------------
std::size_t KServerSentEventServer::size(KStringView sTopic) const
//-----------------------------------------------------------------------------
{
	auto Registry = m_Registry.shared();

	auto Topic = Registry->Topics.find(sTopic);

	return Topic == Registry->Topics.end() ? 0 : Topic->second.size();

} // size

DEKAF2_NAMESPACE_END
