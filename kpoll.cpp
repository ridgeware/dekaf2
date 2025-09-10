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

#include "kpoll.h"

#include "klog.h"
#include "ksystem.h"

#if !DEKAF2_IS_WINDOWS
	#include <sys/types.h>
	#include <sys/socket.h>
#endif

DEKAF2_NAMESPACE_BEGIN

//-----------------------------------------------------------------------------
int kPoll(int handle, int what, KDuration Timeout)
//-----------------------------------------------------------------------------
{
	struct pollfd pollfd;
	pollfd.fd      = handle;
#if !DEKAF2_IS_WINDOWS
	pollfd.events  = what;
#else
	// don't set any output flags on windows, WSAPoll will then fail instead
	// of ignoring them like on MacOS or Linux
	pollfd.events = what & (POLLPRI | POLLRDBAND | POLLRDNORM | POLLWRNORM);
#endif

	for (;;)
	{
		pollfd.revents = 0;

#if !DEKAF2_IS_WINDOWS
		int iResult = ::poll(&pollfd, 1, static_cast<int>(Timeout.milliseconds().count()));
#else
		int iResult = WSAPoll(&pollfd, 1, static_cast<INT>(Timeout.milliseconds().count()));
#endif

		if (iResult == 0)
		{
			// timed out, no events
			return 0;
		}

#if !DEKAF2_IS_WINDOWS
		if (iResult < 0)
		{
			if (errno == EAGAIN)
			{
				// interrupt
				continue;
			}

			kDebug(3, "error {}: {}", errno, strerror(errno));
			return 0;
		}
#else
		if (iResult == SOCKET_ERROR)
		{
			if (kWouldLog(3))
			{
				auto iExtendedError = WSAGetLastError();

				switch (iExtendedError)
				{
				case WSAENETDOWN:
					kDebug(3, "WSAENETDOWN");
					break;
				case WSAEFAULT:
					kDebug(3, "WSAEFAULT");
					break;
				case WSAEINVAL:
					kDebug(3, "WSAEINVAL");
					break;
				case WSAENOBUFS:
					kDebug(3, "WSAENOBUFS");
					break;
				default:
					kDebug(3, "Unknown Error: {}", iExtendedError);
					break;
				}
			}
			return 0;
		}
#endif
		// data available
		return pollfd.revents;
	}

} // kPoll


//-----------------------------------------------------------------------------
KPoll::~KPoll()
//-----------------------------------------------------------------------------
{
	Stop();

} // dtor

//-----------------------------------------------------------------------------
void KPoll::StartLocked()
//-----------------------------------------------------------------------------
{
	// check if the watcher thread is already running
	if (!m_Thread)
	{
		kDebug(1, "starting watcher");
		m_bStop  = false;
		m_Thread = std::make_unique<std::thread>(&KPoll::Watch, this);
	}

} // Start

//-----------------------------------------------------------------------------
void KPoll::Start()
//-----------------------------------------------------------------------------
{
	std::unique_lock<std::shared_mutex> Lock(m_Mutex);
	StartLocked();

} // Start

//-----------------------------------------------------------------------------
void KPoll::Stop()
//-----------------------------------------------------------------------------
{
	std::unique_lock<std::shared_mutex> Lock(m_Mutex);

	if (m_Thread)
	{
		kDebug(1, "stopping watcher");
		m_bStop = true;
		m_Thread->join();
		m_Thread.reset();
		kDebug(1, "watcher stopped");
	}

} // Stop

//-----------------------------------------------------------------------------
void KPoll::Add(int fd, Parameters Parms)
//-----------------------------------------------------------------------------
{
	std::unique_lock<std::shared_mutex> Lock(m_Mutex);

#ifdef DEKAF2_HAS_CPP_17
	m_FileDescriptors.insert_or_assign(fd, std::move(Parms));
#else
	auto pair = m_FileDescriptors.insert({fd, Parms});

	if (!pair.second)
	{
		pair.first->second = std::move(Parms);
	}
#endif

	kDebug(2, "added file descriptor {}", fd);

	m_bModified = true;

	if (!m_Thread && m_bAutoStart)
	{
		StartLocked();
	}

} // Add

//-----------------------------------------------------------------------------
void KPoll::Remove(int fd)
//-----------------------------------------------------------------------------
{
	std::unique_lock<std::shared_mutex> Lock(m_Mutex);

	if (m_FileDescriptors.erase(fd) != 1)
	{
		kDebug(2, "cannot remove file descriptor {}: not found", fd);
	}
	else
	{
		kDebug(2, "removed file descriptor {}", fd);
		m_bModified = true;
	}

} // Remove

//-----------------------------------------------------------------------------
void KPoll::BuildPollVec(std::vector<pollfd>& fds)
//-----------------------------------------------------------------------------
{
	fds.clear();

	std::shared_lock<std::shared_mutex> Lock(m_Mutex);

	// collect all file descriptors we should watch
	m_bModified = false;

	fds.reserve(m_FileDescriptors.size());

	for (const auto& FileDescriptor : m_FileDescriptors)
	{
		pollfd pfd;
		pfd.fd     = FileDescriptor.first;
		pfd.events = FileDescriptor.second.iEvents;
		fds.push_back(pfd);
	}

	kDebug(2, "built new poll fd vector with {} elements", fds.size());

} // BuildPollVec

//-----------------------------------------------------------------------------
void KPoll::Triggered(int fd, uint16_t events)
//-----------------------------------------------------------------------------
{
	kDebug(2, "fd {}: event {}", fd, events);

	Parameters CBP;

	{
		// lock the map
		std::unique_lock<std::shared_mutex> Lock(m_Mutex);

		// find the associated map entry
		auto it = m_FileDescriptors.find(fd);

		if (it == m_FileDescriptors.end())
		{
			// this fd is no more existing in the map
			kDebug(2, "could not find fd {}", fd);
			m_bModified = true;
			return;
		}

		// check if we should only trigger once
		if (it->second.bOnce)
		{
			// get callback and parm
			CBP = std::move(it->second);
			// and remove the file descriptor from the map
			m_FileDescriptors.erase(it);
			// and set a flag to rebuild the vector
			m_bModified = true;
		}
		else
		{
			// copy callback and parm
			CBP = it->second;
		}
	}

	if (CBP.Callback)
	{
		kDebug(2, "calling callback for fd {}, param {}", fd, CBP.iParameter);
		CBP.Callback(fd, events, CBP.iParameter);
	}

} // Triggered

//-----------------------------------------------------------------------------
void KPoll::Watch()
//-----------------------------------------------------------------------------
{
	std::vector<pollfd>  fds;

	while (!m_bStop)
	{
		if (m_bModified)
		{
			BuildPollVec(fds);

			if (fds.empty())
			{
				while (!m_bModified && !m_bStop)
				{
					kSleep(m_Timeout ? m_Timeout : chrono::milliseconds(100));
				}
				continue;
			}
		}
#if !DEKAF2_IS_WINDOWS
		auto iEvents = ::poll(fds.data(), static_cast<nfds_t>(fds.size()), static_cast<int>(m_Timeout.milliseconds().count()));
#else
		auto iEvents = ::WSAPoll(fds.data(), static_cast<ULONG>(fds.size()), static_cast<INT>(m_Timeout.milliseconds().count()));
#endif

		if (iEvents < 0)
		{
			if (errno != EINTR)
			{
				kDebug(1, "stopping watcher: poll returned with error: {}", strerror(errno));
				return;
			}
		}
		else if (iEvents > 0)
		{
			// find the file descriptors that have events:
			for (const auto& pfd : fds)
			{
				if (pfd.revents)
				{
					Triggered(pfd.fd, pfd.revents);

					if (--iEvents == 0)
					{
						break;
					}
				}
			}
		}
	}

} // Watch

//-----------------------------------------------------------------------------
void KSocketWatch::Watch()
//-----------------------------------------------------------------------------
{
	std::vector<pollfd> fds;

	while (!m_bStop)
	{
		if (m_bModified)
		{
			BuildPollVec(fds);

			if (fds.empty())
			{
				while (!m_bModified && !m_bStop)
				{
					kSleep(m_Timeout ? m_Timeout : chrono::milliseconds(100));
				}
				continue;
			}

#if DEKAF2_IS_MACOS
			for (auto& pfd : fds)
			{
				// let MacOS detect disconnects
				// (this looks silly, but test it - it is needed!)
				pfd.events |= POLLHUP;
			}
#endif
		}

#if DEKAF2_IS_WINDOWS
		auto iEvents = ::WSAPoll(fds.data(), static_cast<ULONG>(fds.size()), static_cast<ULONG>(m_Timeout.milliseconds().count()));
#else
		auto iEvents = ::poll(fds.data(), static_cast<nfds_t>(fds.size()), static_cast<int>(m_Timeout.milliseconds().count()));
#endif

		if (iEvents < 0)
		{
			if (errno != EINTR)
			{
				kDebug(1, "stopping watcher: poll returned with error: {}", strerror(errno));
				return;
			}
		}
		else if (iEvents > 0)
		{
			// find the file descriptors that have events:
			for (auto& pfd : fds)
			{
				if (pfd.revents)
				{
					Triggered(pfd.fd, pfd.revents);

					if (--iEvents == 0)
					{
						break;
					}
				}
			}
		}
	}

} // Watch

DEKAF2_NAMESPACE_END
