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

#include "bits/kcppcompat.h"
#ifndef DEKAF2_IS_WINDOWS
#include "kstringview.h"
#include "kstring.h"
#include "kthreadsafe.h"
#include "kfilesystem.h"
#include <fcntl.h>
#include <sys/types.h>

namespace dekaf2 {

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// IPC semaphore mutex, usable cross processes with std::unique_lock, std::lock_guard, KThreadSafe, etc..
class KSemaphoreMutex
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
class KSharedMemoryBase
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	KSharedMemoryBase(KStringView sPathname,
					  std::size_t iSize,
					  bool        bForceCreation = false,
					  int         iMode = DEKAF2_MODE_CREATE_FILE);

	~KSharedMemoryBase();

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

	KSharedMemory(KStringView sPathname,
				  bool        bForceCreation = false,
				  int         iMode = DEKAF2_MODE_CREATE_FILE)
	: base(sPathname, sizeof(Object), bForceCreation, iMode)
	{
	}

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

} // end of namespace dekaf2

#endif // DEKAF2_IS_WINDOWS
