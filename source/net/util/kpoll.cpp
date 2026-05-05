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

#include <dekaf2/net/util/kpoll.h>
#include <dekaf2/core/logging/klog.h>
#include <dekaf2/system/os/ksystem.h>
#include <dekaf2/core/init/kcompatibility.h>
#include <dekaf2/time/duration/kduration.h>
#include <dekaf2/threading/execution/kthreads.h>

#if !DEKAF2_IS_WINDOWS
	#include <sys/types.h>
	#include <sys/socket.h>
#endif

DEKAF2_NAMESPACE_BEGIN

namespace
{

//-----------------------------------------------------------------------------
KStringView GetPollError()
//-----------------------------------------------------------------------------
{
	KStringView sWhat;

#if !DEKAF2_IS_WINDOWS

	sWhat = strerror(errno);

#else

	auto iExtendedError = WSAGetLastError();

	switch (iExtendedError)
	{
		case WSAENETDOWN:
			sWhat = "WSAENETDOWN";
			break;
		case WSAEFAULT:
			sWhat = "WSAEFAULT";
			break;
		case WSAEINVAL:
			sWhat = "WSAEINVAL";
			break;
		case WSAENOBUFS:
			sWhat = "WSAENOBUFS";
			break;
		default:
			sWhat = "unknown error";
			break;
	}

#endif

	return sWhat;

} // GetPollError

} // end of anonymous namespace

//-----------------------------------------------------------------------------
int kPoll(KSpan<pollfd> fds, KDuration Timeout)
//-----------------------------------------------------------------------------
{
#if DEKAF2_IS_WINDOWS
	// WSAPoll rejects output-only flags in events — strip them once
	for (auto& pfd : fds)
	{
		pfd.events &= (POLLPRI | POLLRDBAND | POLLRDNORM | POLLWRNORM);
	}
#endif

	auto Remaining = Timeout;

	for (;;)
	{
		auto Start = KSteadyTime::now();

#if !DEKAF2_IS_WINDOWS
		int iResult = ::poll(fds.data(), static_cast<nfds_t>(fds.size()), static_cast<int>(Remaining.milliseconds().count()));
#else
		int iResult = WSAPoll(fds.data(), static_cast<ULONG>(fds.size()), static_cast<INT>(Remaining.milliseconds().count()));
#endif

		if (iResult == 0)
		{
			// timed out, no events
			return 0;
		}

#if !DEKAF2_IS_WINDOWS
		if (iResult < 0)
		{
			if (errno == EINTR)
			{
				// adjust remaining timeout after interrupt
				auto Elapsed = KDuration(KSteadyTime::now() - Start);

				if (Elapsed >= Remaining)
				{
					return 0;
				}

				Remaining -= Elapsed;
				continue;
			}

			kDebug(3, GetPollError());
			return -errno;
		}
#else
		if (iResult == SOCKET_ERROR)
		{
			kDebug(3, GetPollError());
			return -1;
		}
#endif
		return iResult;
	}

} // kPoll

//-----------------------------------------------------------------------------
int kPoll(int handle, int what, KDuration Timeout)
//-----------------------------------------------------------------------------
{
	struct pollfd pfd{};
	pfd.fd     = handle;
	pfd.events = what;

	// Windows event mask sanitization happens inside kPoll(KSpan<pollfd>, ..)
	int iResult = kPoll(KSpan<pollfd>(&pfd, 1), Timeout);

	return iResult > 0 ? pfd.revents : iResult;

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
		m_Thread = std::make_unique<std::thread>(kMakeThread(&KPoll::Watch, this));
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
	std::unique_ptr<std::thread> Thread;

	{
		std::unique_lock<std::shared_mutex> Lock(m_Mutex);

		if (m_Thread)
		{
			kDebug(1, "stopping watcher");
			m_bStop = true;
			Thread = std::move(m_Thread);
		}
	}

	if (Thread)
	{
		Thread->join();
		kDebug(1, "watcher stopped");
	}

} // Stop

//-----------------------------------------------------------------------------
void KPoll::Add(int fd, Parameters Parms)
//-----------------------------------------------------------------------------
{
	std::unique_lock<std::shared_mutex> Lock(m_Mutex);

#ifdef __cpp_lib_map_try_emplace
	m_FileDescriptors.insert_or_assign(fd, std::move(Parms));
#else
	auto it = m_FileDescriptors.find(fd);

	if (it != m_FileDescriptors.end())
	{
		it->second = std::move(Parms);
	}
	else
	{
		m_FileDescriptors.emplace(fd, std::move(Parms));
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

			AdjustPollVec(fds);
		}

		auto iEvents = kPoll(fds, m_Timeout);

		if (iEvents < 0)
		{
			kDebug(1, "stopping watcher: poll returned with error: {}", GetPollError());
			return;
		}

		if (iEvents > 0)
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
void KPoll::AdjustPollVec(std::vector<pollfd>& fds)
//-----------------------------------------------------------------------------
{
} // AdjustPollVec

//-----------------------------------------------------------------------------
void KSocketWatch::AdjustPollVec(std::vector<pollfd>& fds)
//-----------------------------------------------------------------------------
{
#if DEKAF2_IS_MACOS
	for (auto& pfd : fds)
	{
		// needed on macOS to detect disconnects
		// (this looks silly, but test it - it is needed - it is a known BSD quirk)
		pfd.events |= POLLHUP;
	}
#endif

} // AdjustPollVec

#if !DEKAF2_IS_WINDOWS

//-----------------------------------------------------------------------------
KPollInterruptor::KPollInterruptor()
//-----------------------------------------------------------------------------
{
#if DEKAF2_IS_LINUX
	m_fd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
	if (m_fd < 0)
	{
		kDebug(2, "failed to create eventfd: {}", strerror(errno));
		m_fd = -1;
	}
#else
	int fds[2];
	if (::pipe(fds) == 0)
	{
		// Set both ends non-blocking and close-on-exec
		for (int fd : fds)
		{
			int flags = ::fcntl(fd, F_GETFL, 0);
			if (flags >= 0)
			{
				::fcntl(fd, F_SETFL, flags | O_NONBLOCK);
			}
			flags = ::fcntl(fd, F_GETFD, 0);
			if (flags >= 0)
			{
				::fcntl(fd, F_SETFD, flags | FD_CLOEXEC);
			}
		}
		m_fd = fds[0];  // read end
		m_write_fd = fds[1];  // write end
	}
	else
	{
		kDebug(2, "failed to create pipe: {}", strerror(errno));
		m_fd = -1;
		m_write_fd = -1;
	}
#endif
} // ctor

//-----------------------------------------------------------------------------
KPollInterruptor::~KPollInterruptor()
//-----------------------------------------------------------------------------
{
	Close();
} // dtor

//-----------------------------------------------------------------------------
void KPollInterruptor::Wake()
//-----------------------------------------------------------------------------
{
	if (m_fd < 0) return;
#if DEKAF2_IS_LINUX
	eventfd_t value = 1;
	::eventfd_write(m_fd, value);
#else
	char byte = 1;
	::write(m_write_fd, &byte, 1);
#endif
} // Wake

//-----------------------------------------------------------------------------
void KPollInterruptor::Clear()
//-----------------------------------------------------------------------------
{
	if (m_fd < 0) return;
#if DEKAF2_IS_LINUX
	eventfd_t value;
	::eventfd_read(m_fd, &value);
#else
	char buf[16];
	while (::read(m_fd, buf, sizeof(buf)) > 0) {}
#endif
} // Clear

//-----------------------------------------------------------------------------
void KPollInterruptor::Close()
//-----------------------------------------------------------------------------
{
#if DEKAF2_IS_LINUX
	if (m_fd >= 0)
	{
		::close(m_fd);
		m_fd = -1;
	}
#else
	if (m_fd >= 0)
	{
		::close(m_fd);
		m_fd = -1;
	}
	if (m_write_fd >= 0)
	{
		::close(m_write_fd);
		m_write_fd = -1;
	}
#endif
} // Close

#endif // !DEKAF2_IS_WINDOWS

DEKAF2_NAMESPACE_END
