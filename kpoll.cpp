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
	pollfd.events  = what;

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

		if (iResult < 0)
		{
			if (errno == EAGAIN)
			{
				// interrupt
				kDebug(2, "have an EAGAIN ..");
				continue;
			}
			return errno;
		}

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
					kMilliSleep(m_Timeout != KDuration::zero() ? m_Timeout.milliseconds().count() : 100);
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

#if !DEKAF2_IS_OSX && !DEKAF2_IS_WINDOWS
	// Linux requires at least POLLIN being set to detect disconnects,
	// and requires an additional check with recv()
	#define DEKAF2_ADD_POLLIN 1
#endif

//-----------------------------------------------------------------------------
void KSocketWatch::Watch()
//-----------------------------------------------------------------------------
{
#ifdef DEKAF2_ADD_POLLIN
	std::array<char, 4> buffer;
#endif
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
					kMilliSleep(m_Timeout != KDuration::zero() ? m_Timeout.milliseconds().count() : 100);
				}
				continue;
			}

			for (auto& pfd : fds)
			{
#if DEKAF2_ADD_POLLIN
				pfd.events |= POLLIN;
#else
				// let MacOS detect disconnects
				pfd.events |= POLLERR | POLLHUP;
#endif
			}
		}

#if DEKAF2_IS_WINDOWS
		// on Windows, poll with timeout
		auto iEvents = ::WSAPoll(fds.data(), static_cast<ULONG>(fds.size()), static_cast<ULONG>(m_Timeout.milliseconds().count()));
#elif DEKAF2_ADD_POLLIN
		// on Linux, return immediately from poll
		auto iEvents = ::poll(fds.data(), static_cast<nfds_t>(fds.size()), 0);
#else
		// on MacOS, poll with timeout
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
#if DEKAF2_ADD_POLLIN
					// remove POLLIN from events
					pfd.revents &= ~POLLIN;

					if (pfd.revents == 0)
					{
						// check if socket is disconnected
						if (::recv(pfd.fd, buffer.data(), buffer.size(), MSG_PEEK | MSG_DONTWAIT) != 0)
						{
							// still alive
							if (--iEvents == 0)
							{
								break;
							}
							else
							{
								continue;
							}
						}

						// set disconnect event
						pfd.revents |= POLLHUP;
					}
#endif
					Triggered(pfd.fd, pfd.revents);

					if (--iEvents == 0)
					{
						break;
					}
				}
			}
		}

#if DEKAF2_ADD_POLLIN
		// on Linux, sleep outside poll to minimize calls to recv()
		// for active sockets..
		kMilliSleep(m_Timeout.milliseconds().count());
#endif
	}

} // Watch

DEKAF2_NAMESPACE_END
