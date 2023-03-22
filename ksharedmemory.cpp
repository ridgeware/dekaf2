/*
 //
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

#include "ksharedmemory.h"
#ifndef DEKAF2_IS_WINDOWS
#include "kfilesystem.h"
#include "ksystem.h"
#include "klog.h"
#include "kcrashexit.h"
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <mutex>

// we use the posix interface to shared memory

namespace dekaf2 {


//-----------------------------------------------------------------------------
KSemaphoreMutex::KSemaphoreMutex(key_t iIPCKey) noexcept
//-----------------------------------------------------------------------------
: m_iIPCKey(iIPCKey)
{
	if (!m_iIPCKey)
	{
		m_iIPCKey = kRandom(1);
	}

	m_iSem = semget (m_iIPCKey, 0, IPC_PRIVATE);

	if (m_iSem < 0)
	{
		m_iSem = semget (m_iIPCKey, 1, IPC_CREAT | IPC_EXCL | DEKAF2_MODE_CREATE_FILE);

		if (m_iSem < 0)
		{
			kDebug(2, "semget({}): {}", m_iIPCKey, strerror(errno));
			return;
		}

		// we own this semaphore
		m_bCreator = true;

		// init semaphore with 1
		if (semctl (m_iSem, 0, SETVAL, 1) < 0)
		{
			kDebug(2, "semctl({}): {}", m_iIPCKey, strerror(errno));
			return;
		}
	}

} // ctor

//-----------------------------------------------------------------------------
KSemaphoreMutex::KSemaphoreMutex(KStringView sKey) noexcept
//-----------------------------------------------------------------------------
: KSemaphoreMutex(static_cast<key_t>(sKey.Hash()))
{
} // ctor

//-----------------------------------------------------------------------------
KSemaphoreMutex::~KSemaphoreMutex()
//-----------------------------------------------------------------------------
{
	if (m_bCreator)
	{
		if (semctl (m_iSem, 0, IPC_RMID, 0) < 0)
		{
			kDebug(2, "semctl({}): {}", m_iIPCKey, strerror(errno));
		}
	}

} // dtor

//-----------------------------------------------------------------------------
bool KSemaphoreMutex::operation(int iOperation, bool bTry)
//-----------------------------------------------------------------------------
{
	struct sembuf semaphore;
	semaphore.sem_num = 0;
	semaphore.sem_op  = iOperation;
	semaphore.sem_flg = SEM_UNDO | (bTry ? IPC_NOWAIT : 0);

	if (semop (m_iSem, &semaphore, 1) < 0)
	{
		// EAGAIN means semaphore is locked
		if (!bTry || errno != EAGAIN)
		{
			kDebug(2, "semop({}): {}", m_iIPCKey, strerror(errno));
		}
		return false;
	}

	return true;

} // operation

//-----------------------------------------------------------------------------
detail::KSharedMemoryBase::KSharedMemoryBase(KStringView sPathname,
											 std::size_t iSize,
											 bool        bForceCreation,
											 int         iMode)
//-----------------------------------------------------------------------------
: m_sPathname(sPathname)
, m_iSize(iSize)
{
	if (m_sPathname.front() != '/')
	{
		m_sPathname.insert(KString::size_type(0), KString::size_type(1), '/');
	}

	// first try open mode (if bForceCreation is not set)
	m_iFD = bForceCreation ? -1 : shm_open(m_sPathname.c_str(), O_RDWR, iMode);

	if (m_iFD < 0)
	{
		// now try to create in exclusive mode
		m_iFD = shm_open(m_sPathname.c_str(), O_CREAT | O_EXCL | O_RDWR, iMode);

		if (m_iFD < 0)
		{
			if (errno != EEXIST || !bForceCreation)
			{
				SetError(kFormat("shm_open({}): {}", m_sPathname, KStringView(strerror(errno))));
				return;
			}
			// try to remove
			if (shm_unlink(m_sPathname.c_str()) < 0)
			{
				// could not remove
				SetError(kFormat("shm_unlink for shm_open({}): {}", m_sPathname, KStringView(strerror(errno))));
				return;
			}
			kDebug(2, "removed existing shared memory({})", m_sPathname);
			// now try to open again
			m_iFD = shm_open(m_sPathname.c_str(), O_CREAT | O_EXCL | O_RDWR, iMode);

			if (m_iFD < 0)
			{
				SetError(kFormat("2:shm_open({}): {}", m_sPathname, KStringView(strerror(errno))));
				return;
			}
		}

		m_bIsCreator = true;

		// set requested size
		if (ftruncate(m_iFD, m_iSize) < 0)
		{
			SetError(kFormat("ftruncate({},{}): {}", m_sPathname, m_iSize, KStringView(strerror(errno))));
			return;
		}
	}

	m_pAddr = mmap(NULL, m_iSize, PROT_READ | PROT_WRITE, MAP_SHARED, m_iFD, 0);

	if (m_pAddr == MAP_FAILED)
	{
		m_pAddr = nullptr;
		SetError(kFormat("mmap({}): {}", m_sPathname, KStringView(strerror(errno))));
		return;
	}

} // ctor

//-----------------------------------------------------------------------------
detail::KSharedMemoryBase::~KSharedMemoryBase()
//-----------------------------------------------------------------------------
{
	if (m_pAddr)
	{
		if (munmap(m_pAddr, m_iSize) < 0)
		{
			SetError(kFormat("munmap({}): {}", m_sPathname, KStringView(strerror(errno))));
		}
	}

	if (m_iFD >= 0)
	{
		if (close(m_iFD) < 0)
		{
			SetError(kFormat("close({}): {}", m_sPathname, KStringView(strerror(errno))));
		}
	}

	if (m_bIsCreator)
	{
		if (!m_sPathname.empty())
		{
			if (shm_unlink(m_sPathname.c_str()) < 0)
			{
				SetError(kFormat("shm_unlink({}): {}", m_sPathname, KStringView(strerror(errno))));
			}
		}
	}

} // dtor

//-----------------------------------------------------------------------------
bool detail::KSharedMemoryBase::SetError(KString sError)
//-----------------------------------------------------------------------------
{
	m_sError = std::move(sError);
	kDebug(2, m_sError);
	return false;

} // SetError


//-----------------------------------------------------------------------------
KIPCMessages::KIPCMessages(key_t iIPCKey, std::size_t iMinMessageQueueSize, int iMode)
//-----------------------------------------------------------------------------
{
	m_iIPCKey = msgget(iIPCKey, IPC_CREAT | iMode);

	if (m_iIPCKey == -1)
	{
		kDebug(2, strerror(errno));
		return;
	}

	if (iMinMessageQueueSize)
	{
		msqid_ds info;

		auto iResult = msgctl(m_iIPCKey, IPC_STAT, &info);

		if (iResult == -1)
		{
			kDebug(2, strerror(errno));
			return;
		}

		if (info.msg_qbytes < iMinMessageQueueSize)
		{
			info.msg_qbytes = iMinMessageQueueSize;

			iResult = msgctl(m_iIPCKey, IPC_SET, &info);

			if (iResult == -1)
			{
				kDebug(2, strerror(errno));
				return;
			}
		}
	}

} // ctor

//-----------------------------------------------------------------------------
bool KIPCMessages::SendRaw(const void* sMessage, std::size_t size)
//-----------------------------------------------------------------------------
{
	kAssert(size >= sizeof(long), "payload is too small");

	auto iResult = msgsnd(m_iIPCKey, sMessage, size - sizeof(long), IPC_NOWAIT);

	if (iResult == -1)
	{
		kDebug(2, strerror(errno));
	}

	return !iResult;

} // Send

//-----------------------------------------------------------------------------
bool KIPCMessages::Send(const KStringView sMessage, long msgtype)
//-----------------------------------------------------------------------------
{
	KString sSendBuffer;

	sSendBuffer.reserve(sizeof(long) + sMessage.size());
	sSendBuffer.assign(reinterpret_cast<const char*>(&msgtype), sizeof(long));
	sSendBuffer.append(sMessage);

	return SendRaw(sSendBuffer.data(), sSendBuffer.size());

} // Send

//-----------------------------------------------------------------------------
KString KIPCMessages::Receive(std::size_t iExpectedSize, KDuration timeout, long msgtype)
//-----------------------------------------------------------------------------
{
	KString sMessage;
	KStopTime Timer(KStopTime::Halted);

	if (iExpectedSize == npos)
	{
#ifdef DEKAF2_IS_APPLE_CLANG
		iExpectedSize = 22 - sizeof(long);
#else
		iExpectedSize = 15 - sizeof(long);
#endif
	}

	if (timeout != KDuration::zero())
	{
		Timer.clear();
	}

	for (;;)
	{
		sMessage.resize_uninitialized(iExpectedSize + sizeof(long));

		auto iReceived = msgrcv(m_iIPCKey, sMessage.data(), sMessage.size() - sizeof(long), msgtype, (timeout != KDuration::zero()) ? IPC_NOWAIT : 0);

		if (iReceived >= 0)
		{
			// success - remove the leading type from the string
			sMessage.erase(0, sizeof(long));
			// and make sure we resize the string to the length of the received message
			sMessage.resize(iReceived);
			// return the net message
			return sMessage;
		}

		switch (errno)
		{
			case E2BIG:
				// exponential strategy as for vector
				iExpectedSize *= 2;
				break;

			case ENOMSG:
				if (Timer.elapsed() >= timeout)
				{
					return sMessage;
				}
				// else sleep and retry
				kMilliSleep(10);
				break;

			default:
				kDebug(2, strerror(errno));
				return sMessage;
		}
	}

} // Receive

//-----------------------------------------------------------------------------
bool KIPCMessages::ReceiveRaw (void* addr, std::size_t size, long msgtype, KDuration timeout)
//-----------------------------------------------------------------------------
{
	kAssert(size >= sizeof(long), "payload is too small");

	KStopTime Timer(KStopTime::Halted);

	if (timeout != KDuration::zero())
	{
		Timer.clear();
	}

	for (;;)
	{
		auto iReceived = msgrcv(m_iIPCKey, addr, size - sizeof(long), msgtype, (timeout != KDuration::zero()) ? IPC_NOWAIT : 0);

		if (iReceived >= 0)
		{
			return (static_cast<std::size_t>(iReceived) == size - sizeof(long));
		}

		switch (errno)
		{
			case ENOMSG:
				if (Timer.elapsed() >= timeout)
				{
					return false;
				}
				// else sleep and retry
				kMilliSleep(10);
				break;

			default:
				kDebug(2, strerror(errno));
				return false;
		}
	}

} // Receive

} // end of namespace dekaf2

#endif // DEKAF2_IS_WINDOWS
