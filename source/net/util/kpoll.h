/*
// DEKAF(tm): Lighter, Faster, Smarter (tm)
//
// Copyright (c) 2021, Ridgeware, Inc.
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

/// @file kpoll.h
/// Maintaining a list of file descriptors and associated actions to call when the file descriptor creates an event

#include <dekaf2/core/init/kdefinitions.h>
#include <dekaf2/containers/sequential/kspan.h>
#include <dekaf2/threading/primitives/kthreadsafe.h>
#include <dekaf2/time/duration/kduration.h>
#include <functional>
#include <thread>
#include <atomic>
#include <memory>
#include <vector>
#include <unordered_map>
#include <cinttypes>

#if DEKAF2_IS_WINDOWS
	#include <winsock2.h>
#else
	#include <poll.h>
	#include <fcntl.h>
	#include <unistd.h>
#endif

#if DEKAF2_IS_LINUX
	#include <sys/eventfd.h>
#endif

DEKAF2_NAMESPACE_BEGIN

/// @addtogroup net_util
/// @{

//-----------------------------------------------------------------------------
/// poll multiple fds — sanitizes event masks on Windows
/// @param fds pollfd structs (events set by caller, revents cleared before poll)
/// @param Timeout timeout
/// @return >0 number of fds with events, 0 timeout, < 0 errno error number * -1
int kPoll(KSpan<pollfd> fds, KDuration Timeout);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// poll one single fd
/// @param fd a file descriptor
/// @param what which events to look for
/// @param Timeout timeout
/// @return >0 the triggered events, 0 timeout, < 0 errno error number * -1
int kPoll(int fd, int what, KDuration Timeout);
//-----------------------------------------------------------------------------

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Helper class to interrupt poll() from another thread.
/// Uses eventfd on Linux (efficient) and pipe on other POSIX systems.
/// On Windows this is a no-op because WSAPoll correctly unblocks on socket close.
class DEKAF2_PUBLIC KPollInterruptor
{
public:
#if !DEKAF2_IS_WINDOWS
	/// Create the interruptor (eventfd on Linux, pipe on other POSIX)
	KPollInterruptor();

	/// Close the interruptor file descriptors
	~KPollInterruptor();

	// Non-copyable, non-movable
	KPollInterruptor(const KPollInterruptor&) = delete;
	KPollInterruptor& operator=(const KPollInterruptor&) = delete;
	KPollInterruptor(KPollInterruptor&&) = delete;
	KPollInterruptor& operator=(KPollInterruptor&&) = delete;

	/// Get the file descriptor to poll on (read end for pipe, eventfd for Linux)
	int GetFD() const { return m_fd; }

	/// Wake up any poll() call waiting on this interruptor
	void Wake();

	/// Clear the interruptor state (drain any pending wake events)
	void Clear();

	/// Close the file descriptors
	void Close();

	/// Check if the interruptor is valid
	bool IsValid() const { return m_fd >= 0; }

private:
	int m_fd { -1 };
#if !DEKAF2_IS_LINUX
	int m_write_fd { -1 };  // only needed for pipe
#endif
#else  // DEKAF2_IS_WINDOWS
	// Windows: no-op implementation
	KPollInterruptor() = default;
	int GetFD() const { return -1; }
	void Wake() {}
	void Clear() {}
	void Close() {}
	bool IsValid() const { return false; }
#endif  // DEKAF2_IS_WINDOWS
}; // KPollInterruptor

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Maintaining a list of file descriptors and associated actions to call when the file descriptor creates an event
class DEKAF2_PUBLIC KPoll
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	/// the callback 
	using CallbackT = std::function<void(int, uint16_t, std::size_t)>;

	struct Parameters
	{
		CallbackT   Callback;              ///< the callback function
		std::size_t iParameter { 0 };      ///< arbitrary parameter to pass in callback
		uint16_t    iEvents    { 0 };      ///< the file desc events to watch for
		bool        bOnce      { false };  ///< trigger only once, or repeatedly?
	};

	KPoll(KDuration Timeout = chrono::milliseconds(100), bool bAutoStart = true)
	: m_Timeout(Timeout)
	, m_bAutoStart(bAutoStart)
	{
	}

	virtual ~KPoll();

	/// add a file descriptor to watch
	void Add(int fd, Parameters Parms);
	/// remove a file descriptor from watch
	void Remove(int fd);

	/// start the watcher
	void Start();
	/// stop the watcher
	void Stop();

//----------
protected:
//----------

	void BuildPollVec(std::vector<pollfd>& fds);
	void Triggered(int fd, uint16_t events);
	void Watch();
	/// called after BuildPollVec() to let subclasses adjust event masks
	virtual void AdjustPollVec(std::vector<pollfd>& fds);

	KDuration         m_Timeout    { chrono::milliseconds(100) };
	std::atomic<bool> m_bModified  { false };
	std::atomic<bool> m_bStop      { false };

//----------
private:
//----------

	void StartLocked();

	std::shared_mutex m_Mutex;
	std::unique_ptr<std::thread> m_Thread;
	std::unordered_map<int, Parameters> m_FileDescriptors;

	bool              m_bAutoStart {  true };

}; // KPoll

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Watch sockets for disconnects - no need to set an event mask. Adapts to different trigger
/// conditions on MacOS and Linux
class DEKAF2_PUBLIC KSocketWatch : public KPoll
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	using KPoll::KPoll;

//----------
protected:
//----------

	virtual void AdjustPollVec(std::vector<pollfd>& fds) override final;

}; // KSocketWatch


/// @}

DEKAF2_NAMESPACE_END
