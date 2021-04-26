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

#ifndef DEKAF2_IS_WINDOWS

#include "klog.h"
#include "ksystem.h"
#include <sys/types.h>
#include <sys/socket.h>


namespace dekaf2 {

//-----------------------------------------------------------------------------
KPoll::~KPoll()
//-----------------------------------------------------------------------------
{
	Stop();

} // dtor

//-----------------------------------------------------------------------------
void KPoll::Start()
//-----------------------------------------------------------------------------
{
	// check if the watcher thread is already running
	if (m_Thread)
	{
		kDebug(1, "watcher is already running");
	}
	else
	{
		kDebug(1, "starting watcher");
		m_bStop  = false;
		m_Thread = std::make_unique<std::thread>(&KPoll::Watch, this);
	}

} // Start

//-----------------------------------------------------------------------------
void KPoll::Stop()
//-----------------------------------------------------------------------------
{
	if (!m_Thread)
	{
		kDebug(1, "no watcher running");
	}
	else
	{
		kDebug(1, "stopping watcher");
		m_bStop = true;
		m_Thread->join();
		kDebug(1, "watcher stopped");
	}

} // Stop

//-----------------------------------------------------------------------------
void KPoll::Add(int fd, Parameters Parms)
//-----------------------------------------------------------------------------
{
	auto Map = m_FileDescriptors.unique();

#ifdef DEKAF2_HAS_CPP_17
	Map->insert_or_assign(fd, std::move(Parms));
#else
	auto pair = Map->insert({fd, Parms});

	if (!pair.second)
	{
		pair.first->second = std::move(Parms);
	}
#endif

	kDebug(2, "added file descriptor {}", fd);

	m_bModified = true;

	if (!m_Thread && m_bAutoStart)
	{
		Start();
	}

} // Add

//-----------------------------------------------------------------------------
void KPoll::Remove(int fd)
//-----------------------------------------------------------------------------
{
	if (m_FileDescriptors.unique()->erase(fd) != 1)
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

	// collect all file descriptors we should watch
	auto FileDescriptors = m_FileDescriptors.shared();

	m_bModified = false;

	fds.reserve(FileDescriptors->size());

	for (const auto& FileDescriptor : *FileDescriptors)
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
		auto FileDescriptors = m_FileDescriptors.unique();

		// find the associated map entry
		auto it = FileDescriptors->find(fd);

		if (it != FileDescriptors->end())
		{
			// get callback and parm
			CBP = std::move(it->second);

			// check if we should only trigger once
			if (CBP.bOnce)
			{
				// and remove the file descriptor from the map
				FileDescriptors->erase(it);
				// and set a flag to rebuild the vector
				m_bModified = true;
			}
		}
		else
		{
			// this fd is no more existing in the map
			kDebug(2, "could not find fd {}", fd);
			m_bModified = true;
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
					kMilliSleep(m_iTimeout ? m_iTimeout : 100);
				}
				continue;
			}
		}

		auto iEvents = ::poll(fds.data(), fds.size(), m_iTimeout);

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

#ifndef DEKAF2_IS_OSX
	// Linux requires at least POLLIN being set to detect disconnects,
	// and requires an additional check with recv()
	#define DEKAF2_ADD_POLLIN
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
					kMilliSleep(m_iTimeout ? m_iTimeout : 100);
				}
				continue;
			}

			for (auto& pfd : fds)
			{
#ifdef DEKAF2_ADD_POLLIN
				pfd.events |= POLLIN;
#else
				// let MacOS detect disconnects
				pfd.events |= POLLERR | POLLHUP;
#endif
			}
		}

#ifdef DEKAF2_ADD_POLLIN
		// on Linux, return immediately from poll
		auto iEvents = ::poll(fds.data(), fds.size(), 0);
#else
		// on MacOS, poll with timeout
		auto iEvents = ::poll(fds.data(), fds.size(), m_iTimeout);
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
#ifdef DEKAF2_ADD_POLLIN
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

#ifdef DEKAF2_ADD_POLLIN
		// on Linux, sleep outside poll to minimize calls to recv()
		// for active sockets..
		kMilliSleep(m_iTimeout);
#endif
	}

} // Watch

#endif // DEKAF2_IS_WINDOWS

} // end of namespace dekaf2
