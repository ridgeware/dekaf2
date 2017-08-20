/*
//-----------------------------------------------------------------------------//
//
// DEKAF(tm): Lighter, Faster, Smarter (tm)
//
// Copyright (c) 2017, Ridgeware, Inc.
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

#include <memory>
#include <thread>
#include <mutex>
#include <vector>
#include <map>
#include <atomic>
#include "klog.h"
#include "bits/kcppcompat.h"

namespace dekaf2
{

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Base class for generated threads. Maintains a list of started threads
/// (and hence keeps them alive) and ensures they have joined at destruction.
class KThreadWait
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
//----------
public:
//----------
	typedef std::unique_ptr<std::thread> UPThread;

//----------
private:
//----------
	typedef std::vector<UPThread> threads_t;
	threads_t threads;

//----------
public:
//----------
	//-----------------------------------------------------------------------------
	/// Destructor waits for all threads to join
	virtual ~KThreadWait();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// add a new thread to the list of started threads
	void Add(UPThread newThread);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// add a new thread to the list of started threads
	void Add(std::thread* newThread);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// wait for all threads to join
	void Join();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// return count of started threads
	inline size_t size() const
	//-----------------------------------------------------------------------------
	{
		return threads.size();
	}
};

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Run a number of threads that call a given function. The number of threads
/// is automatically set to the number of processor cores if no count is given.
/// All threads have to join at destruction of the class, except those started
/// in detached mode. The thread launch mechanism maintains a thread counter
/// which can be accessed in a running thread via CRunThread's static method
/// get_ThreadNum(). The logging facilities use this.
/// This class also allows for an arbitrary number of different threads in
/// different modes to be started. The common denominator is that all joinable
/// threads have to join when the destructor of the class instance is called.
class KRunThreads : public KThreadWait
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
//----------
public:
//----------
	//-----------------------------------------------------------------------------
	/// Constructor. Sets number of threads to #cpu if numThreads == 0, but
	/// not higher than maxThreads if maxThreads > 0.
	KRunThreads(size_t iNumThreads = 0, size_t iMaxThreads = 0)
	//-----------------------------------------------------------------------------
	{
		SetSize(iNumThreads, iMaxThreads);
	}

	//-----------------------------------------------------------------------------
	virtual ~KRunThreads();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// sets number of threads to #cpu if numThreads == 0, but
	/// not higher than maxThreads if maxThreads > 0.
	size_t SetSize(size_t iNumThreads, size_t iMaxThreads = 0);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// request max number of threads to be started
	inline size_t MaxSize() const
	//-----------------------------------------------------------------------------
	{
		return m_numThreads;
	}

	//-----------------------------------------------------------------------------
	/// request a delay after starting each thread
	inline void StartWithPause(std::chrono::microseconds pause)
	//-----------------------------------------------------------------------------
	{
		m_pause = pause;
	}

	//-----------------------------------------------------------------------------
	/// create new threads in detached mode?
	inline void StartDetached(bool yesno = true)
	//-----------------------------------------------------------------------------
	{
		m_start_detached = yesno;
	}

	//-----------------------------------------------------------------------------
	/// create one thread, calling function f with arguments args
	template<class Function, class... Args>
	void CreateOne(Function&& f, Args&&... args)
	{
		s_bHasThreadsStarted = true;
		// create the thread and start it
		Store(std::make_unique<std::thread>(&KRunThreads::RunThread<Function, Args...>, this,
		                      std::forward<Function>(f), std::forward<Args>(args)...));
	}
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// create N threads (as determined by the constructor)
	/// calling function f with arguments args
	template<class Function, class... Args>
	size_t Create(Function&& f, Args&&... args)
	//-----------------------------------------------------------------------------
	{
		size_t iCount{0};

		for (size_t ct=size(); ct < m_numThreads; ++ct) {

			CreateOne(std::forward<Function>(f), std::forward<Args>(args)...);
			++iCount;

		}

		KLog().debug(2, "KRunThreads: started {} additional threads", iCount);

		return iCount;
	}

	//-----------------------------------------------------------------------------
	inline static bool HasThreadsStarted()
	//-----------------------------------------------------------------------------
	{
		return s_bHasThreadsStarted;
	}

	//-----------------------------------------------------------------------------
	inline static size_t GetThreadNum()
	//-----------------------------------------------------------------------------
	{
		return s_iThreadNum;
	}

//----------
protected:
//----------
	//-----------------------------------------------------------------------------
	template<class Function, class... Args>
	void RunThread(Function&& f, Args&&... args)
	//-----------------------------------------------------------------------------
	{
		s_iThreadNum = ++s_iThreadIdCount;
		std::bind(std::forward<Function>(f), std::forward<Args>(args)...)();
	}

	//-----------------------------------------------------------------------------
	/// store (or detach) the new thread object
	void Store(UPThread&& thread);
	//-----------------------------------------------------------------------------

//----------
private:
//----------
	size_t                     m_numThreads;
	std::chrono::microseconds  m_pause{0};
	bool                       m_start_detached{false};

	static bool                s_bHasThreadsStarted;
	static thread_local size_t s_iThreadNum;
	static std::atomic_size_t  s_iThreadIdCount;

}; // KRunThreads

//-----------------------------------------------------------------------------
void kParallelForEachPrintProgress(size_t iMax, size_t iDone, size_t iRunning);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Iterate with parallel threads over all elements of a range. Expects the size
/// of the range as the first element.
template<typename InputIterator,
         typename Func,
         typename Progress = decltype(kParallelForEachPrintProgress)>
void parallel_for_each(size_t size,
                       InputIterator first, InputIterator last,
                       const Func& f,
                       size_t max_threads = std::thread::hardware_concurrency(),
                       const Progress& fProgress = kParallelForEachPrintProgress)
//-----------------------------------------------------------------------------
{
	switch (size)
	{
		case 0:
			break;

		case 1:
			f(first);
			break;

		default:
			std::mutex m_IterMutex;
			size_t m_iDone{0};
			size_t m_iRunning{0};

			auto loop = [&](){
				bool m_bFirstLoop{true};
				for (;;)
				{
					InputIterator it;
					{
						std::lock_guard<std::mutex> lock(m_IterMutex);
						if (!m_bFirstLoop)
						{
							++m_iDone;
							--m_iRunning;
						}
						else
						{
							m_bFirstLoop = false;
						}
						if (first == last)
						{
							return;
						}
						it = first;
						++first;
						++m_iRunning;
					}

					fProgress(size, m_iDone, m_iRunning);

					f(it);
				}
			};

			KRunThreads threads((max_threads == 0) ? 0 : max_threads-1, size-1);

			// create the additional threads
			threads.Create(std::ref(loop));

			// and finally run the loop in the initial thread as well
			loop();

			break;
	}
}

//-----------------------------------------------------------------------------
/// Iterate with parallel threads over all elements of a range. If the iterator
/// type is not a RandomAccessIterator, the count of elements from first to last
/// is determined by incrementing first until it is equal to last. As this is a
/// linear operation you should avoid it for larger ranges by calling the sister
/// template parallel_for_each() that accepts the size of the range as the first
/// parameter.
template<typename InputIterator,
         typename Func,
         typename Progress = decltype(kParallelForEachPrintProgress)>
void kParallelForEach(InputIterator first, InputIterator last,
                      const Func& f,
                      size_t max_threads = std::thread::hardware_concurrency(),
                      const Progress& fProgress = kParallelForEachPrintProgress)
//-----------------------------------------------------------------------------
{
	parallel_for_each(std::distance(first, last), first, last, f, max_threads, fProgress);
}

//-----------------------------------------------------------------------------
/// Iterate over all elements of a container c and call function f with an iterator
/// on the element. Only works with iterable types that have a .size() member
/// implemented.
template<typename Container,
         typename Func,
         typename Progress = decltype(kParallelForEachPrintProgress)>
void kParallelForEach(Container c,
                      const Func& f,
                      size_t max_threads = std::thread::hardware_concurrency(),
                      const Progress& fProgress = kParallelForEachPrintProgress)
//-----------------------------------------------------------------------------
{
	kParallelForEach(c.size(), c.begin(), c.end(), f, max_threads, fProgress);
}

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KBlockOnID
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
//----------
public:
//----------

	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	class Data
	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		friend class KBlockOnID;

	//----------
	public:
	//----------
		//-----------------------------------------------------------------------------
		Data()
		//-----------------------------------------------------------------------------
		{
		}

		//-----------------------------------------------------------------------------
		bool WouldBlock(size_t ID);
		//-----------------------------------------------------------------------------

	//----------
	protected:
	//----------
		//-----------------------------------------------------------------------------
		void Lock(size_t ID);
		//-----------------------------------------------------------------------------

		//-----------------------------------------------------------------------------
		bool Unlock(size_t ID);
		//-----------------------------------------------------------------------------

	//----------
	private:
	//----------
		typedef std::unique_ptr<std::mutex> unique_mutex_t;
		typedef std::map<size_t, unique_mutex_t> lockmap_t;
		lockmap_t   m_id_mutexes;
		std::mutex  m_map_mutex;

	};

	//-----------------------------------------------------------------------------
	KBlockOnID(Data& data, size_t ID)
	//-----------------------------------------------------------------------------
	    : m_data(data)
	    , m_MyID(ID)
	{
		m_data.Lock(ID);
	}

	//-----------------------------------------------------------------------------
	~KBlockOnID()
	//-----------------------------------------------------------------------------
	{
		m_data.Unlock(m_MyID);
	}

//----------
private:
//----------
	Data&  m_data;
	size_t m_MyID;
};


} // of namespace dekaf2

