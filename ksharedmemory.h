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

#pragma once

#include "kcompatibility.h"
#ifndef DEKAF2_IS_WINDOWS
#include "kstringview.h"
#include "kstring.h"
#include "kthreadsafe.h"
#include "kfilesystem.h"
#include "kduration.h"
#include <fcntl.h>
#include <sys/types.h>

DEKAF2_NAMESPACE_BEGIN

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// IPC semaphore mutex, usable cross processes with std::unique_lock, std::lock_guard, KThreadSafe, etc..
class DEKAF2_PUBLIC KSemaphoreMutex
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	/// construct an IPC semaphore mutex based on a numeric key. If 0, random key will be chosen
	KSemaphoreMutex(key_t iIPCKey = 0) noexcept;
	/// construct an IPC semaphore mutex based on a string value that is hashed to generate a key
	KSemaphoreMutex(KStringView sKey) noexcept;
	~KSemaphoreMutex();

	/// mutex is ready to be used
	bool is_open()  const { return m_iSem >= 0; }
	operator bool() const { return is_open();   }

	/// try to get the lock, if not available return false without blocking, otherwise return true
	bool try_lock() { return operation(-1,  true); }
	/// get the lock, blocks until success
	void lock()     {        operation(-1, false); }
	/// release the lock
	void unlock()   {        operation( 1, false); }

	/// return the used key value (needed for use of default constructed mutexes)
	key_t get_key() const { return m_iIPCKey; }

//------
private:
//------

	bool operation(int iOperation, bool bTry);

	int   m_iSem     {    -1 };
	key_t m_iIPCKey;
	bool  m_bCreator { false };

}; // KSemaphoreMutex

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
template<key_t iIPCKey>
class KKeyedSemaphoreMutex : public KSemaphoreMutex
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	KKeyedSemaphoreMutex()
	: KSemaphoreMutex(iIPCKey)
	{
	}

}; // KKeyedSemaphoreMutex

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// reusing KThreadSafe to create safe accessor on an IPC object by help of an IPC semaphore..
template<typename IPCObject, key_t iIPCKey>
using KIPCSafe = KThreadSafe<IPCObject, KKeyedSemaphoreMutex<iIPCKey>>;
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

namespace detail {

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC KSharedMemoryBase
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	/// Opens/creates shared memory.
	/// @param sPathname path name (any valid path, does not have to exist, but should start
	/// with a slash). Will be used as common identifier for the shared memory.
	/// @param size the size of the shared memory to be opened/created
	/// @param bool bForceCreation if false, it is first tried to open an existing shared memory,
	/// if true it is immediately tried to create one.
	/// @param iMode access permissions as for any other file open
	KSharedMemoryBase(KStringView sPathname,
					  std::size_t iSize,
					  bool        bForceCreation = false,
					  int         iMode = DEKAF2_MODE_CREATE_FILE);

	~KSharedMemoryBase();

	/// @return true if shared memory is opened/created
	bool is_open()  const { return m_pAddr;   }
	operator bool() const { return is_open(); }

	/// Returns the file descriptor for the shared memory.
	/// Can be used e.g. with KFileStat to retrieve information.
	int GetFileDescriptor() const { return m_iFD;     }

	/// Returns error string if any
	const KString& Error() const  { return m_sError; }

//------
protected:
//------

	void* get() const { return m_pAddr; }
	bool SetError(KString sError);

//------
private:
//------

	KString     m_sPathname;
	KString     m_sError;
	void*       m_pAddr      { nullptr };
	std::size_t m_iSize      {       0 };
	int         m_iFD        {      -1 };
	bool        m_bIsCreator {   false };

}; // KSharedMemoryBase

} // end of namespace detail

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// creates an IPC shared memory section with the size of the templated object
template<typename Object,
         typename std::enable_if<std::is_trivially_copyable<Object>::value, int>::type = 0>
class KSharedMemory : public detail::KSharedMemoryBase
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	using base = detail::KSharedMemoryBase;

	/// Opens/creates shared memory of sizeof(Object).
	/// @param sPathname path name (any valid path, does not have to exist, but should start
	/// with a slash). Will be used as common identifier for the shared memory.
	/// @param bool bForceCreation if false, it is first tried to open an existing shared memory,
	/// if true it is immediately tried to create one.
	/// @param iMode access permissions as for any other file open
	KSharedMemory(KStringView sPathname,
				  bool        bForceCreation = false,
				  int         iMode = DEKAF2_MODE_CREATE_FILE)
	: base(sPathname, sizeof(Object), bForceCreation, iMode)
	{
	}

	/// get reference on object
	Object& get() const
	{
		return *static_cast<Object*>(base::get());
	}

	/// get pointer on object
	Object* operator->()
	{
		return &get();
	}

	/// get reference on object
	Object& operator*()
	{
		return get();
	}

	/// get reference on object
	operator Object&()
	{
		return get();
	}

}; // KSharedMemory

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// finally offering a type that gives IPC safe access on a shared memory object
template<typename Object, key_t iIPCKey>
using KIPCSafeSharedMemory = KIPCSafe<KSharedMemory<Object>, iIPCKey>;
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::


class KIPCMessages;

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
template<typename Object,
		 typename std::enable_if<std::is_trivially_copyable<Object>::value, int>::type = 0>
class KIPCMessagePayload
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

	friend class KIPCMessages;

//------
public:
//------

	/// get reference on object
	Object& get()
	{
		return m_object;
	}

	/// get pointer on object
	Object* operator->()
	{
		return &get();
	}

	/// get reference on object
	Object& operator*()
	{
		return get();
	}

	/// get reference on object
	operator Object&()
	{
		return get();
	}


//------
private:
//------

	using is_IPCMessagePayload = int;

	mutable long m_msgtype;
	Object m_object;

}; // KIPCMessagePayload

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Pass messages with IPC between processes on the same computer. Can send/receive
/// unstructured data (strings), or structured, trivially copyable data wrapped into KIPCMessagePayload<>.
class KIPCMessages
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	/// constructor
	/// @param iIPCKey the "channel" between two or more IPC users. Any uint32_t integer.
	/// @param iMinMessageQueueSize the requested minimum size for content in the message
	/// queue. Defaults normally to some kB on MacOS and Linux. Should be at least as high as the
	/// largest object that should be passed in a message.
	/// @param iMode access permissions as for any other file open
	KIPCMessages(key_t       iIPCKey,
				 std::size_t iMinMessageQueueSize = 0,
				 int         iMode = DEKAF2_MODE_CREATE_FILE);

	/// Send any string or unstructured data on the messaging channel.
	/// @param sMessage the message
	/// @param msgtype the message type, a positive, non-null long - defaults to 1
	bool Send(const KStringView sMessage, long msgtype = 1);

	/// Send an object wrapped into KIPCMessagePayload<> on the messaging channel
	/// @param payload the object to be sent. Must be trivially copyable
	/// @param msgtype the message type, a positive, non-null long - defaults to sizeof(payload)
	template<typename Payload,
			 typename std::enable_if<std::is_trivially_copyable<Payload>::value, typename Payload::is_IPCMessagePayload>::type = 0>
	bool Send(const Payload& payload, long msgtype = sizeof(Payload))
	{
		static_assert(sizeof(payload) >= sizeof(long), "payload is too small");

		payload.m_msgtype = msgtype;
		return SendRaw(std::addressof(payload), sizeof(payload));
	}


	/// Receive any string or unstructured data on the messaging channel.
	/// @param iExpectedSize the expected message size. Adjusts by power of two
	/// until message fits into the result string. Defaults to 15 - sizeof(long) or 22 - sizeof(long),
	/// depending on the SSO implementation of the string type.
	/// @param timeout a KDuration timeout after which a receive attempt is stopped. Defaults to
	/// KDuration::zero() = unlimited
	/// @param msgtype the message type to receive, a positive long - defaults to 1, 0 means any message
	KString Receive(std::size_t iExpectedSize = npos,
					KDuration   timeout = KDuration::zero(),
					long        msgtype = 1);

	/// Receive an object wrapped into KIPCMessagePayload<> on the messaging channel
	/// @param payload the object to be received. Must be trivially copyable
	/// @param msgtype the message type, a positive long - defaults to sizeof(payload), 0 means any message
	template<typename Payload,
			 typename std::enable_if<std::is_trivially_copyable<Payload>::value, typename Payload::is_IPCMessagePayload>::type = 0>
	bool Receive(Payload&  payload,
				 KDuration timeout = KDuration::zero(),
				 long      msgtype = sizeof(Payload))
	{
		static_assert(sizeof(payload) >= sizeof(long), "payload is too small");

		payload.m_msgtype = msgtype;
		return ReceiveRaw(std::addressof(payload), sizeof(payload), msgtype, timeout);
	}

//------
private:
//------

	bool  SendRaw    (const void* addr, std::size_t size);
	bool  ReceiveRaw (void* addr, std::size_t size, long msgtype, KDuration timeout);

	key_t m_iIPCKey;

}; // KIPCMessages

DEKAF2_NAMESPACE_END

#endif // DEKAF2_IS_WINDOWS
