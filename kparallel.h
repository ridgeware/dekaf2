/*
//
// DEKAF(tm): Lighter, Faster, Smarter (tm)
//
// Copyright (c) 2017-2018, Ridgeware, Inc.
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

/// @file kparallel.h
/// collection of classes and functions for parallelization support

#include "kthreads.h"
#include "kthreadsafe.h"
#include "ktemplate.h"
#include "klog.h"
#include <algorithm>
#include <thread>
#include <utility>
#include <unordered_map>

DEKAF2_NAMESPACE_BEGIN

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Runs a number of threads that call a given function. The number of threads
/// is automatically set to the number of processor cores if no count is given.
/// All threads have to join at destruction of the class, except those started
/// in detached mode.
///
/// This class also allows for an arbitrary number of different threads in
/// different modes to be started. The common denominator is that all joinable
/// threads have to join when the destructor of the class instance is called.
class DEKAF2_PUBLIC KRunThreads : public KThreads
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	//-----------------------------------------------------------------------------
	/// Constructor. Sets number of threads to #cpu if numThreads == 0, but
	/// not higher than maxThreads if maxThreads > 0.
	KRunThreads(std::size_t iNumThreads = 0, std::size_t iMaxThreads = 0) noexcept
	//-----------------------------------------------------------------------------
	{
		SetSize(iNumThreads, iMaxThreads);
	}

	//-----------------------------------------------------------------------------
	/// sets number of threads to #cpu if numThreads == 0, but
	/// not higher than maxThreads if maxThreads > 0.
	std::size_t SetSize(std::size_t iNumThreads, std::size_t iMaxThreads = 0) noexcept;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// request max number of threads to be started
	DEKAF2_NODISCARD
	std::size_t MaxSize() const
	//-----------------------------------------------------------------------------
	{
		return m_numThreads;
	}

	//-----------------------------------------------------------------------------
	/// create new threads in detached mode?
	void StartDetached(bool yesno = true)
	//-----------------------------------------------------------------------------
	{
		m_start_detached = yesno;
	}

	//-----------------------------------------------------------------------------
	/// create one thread, calling function f with arguments args
	template<class Function, class... Args>
	std::thread::id CreateOne(Function&& f, Args&&... args)
	{
		// create the thread and start it
		return Store(std::thread(std::forward<Function>(f), std::forward<Args>(args)...));
	}
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// create N threads (as determined by the constructor)
	/// calling function f with arguments args
	template<class Function, class... Args>
	std::size_t Create(Function&& f, Args&&... args)
	//-----------------------------------------------------------------------------
	{
		std::size_t iCount { 0 };

		for (std::size_t ct=size(); ct < m_numThreads; ++ct)
		{
			CreateOne(std::forward<Function>(f), std::forward<Args>(args)...);
			++iCount;
		}

		kDebug(2, "started {} additional threads", iCount);

		return iCount;
	}

//----------
protected:
//----------

	//-----------------------------------------------------------------------------
	/// store (or detach) the new thread object
	std::thread::id Store(std::thread thread);
	//-----------------------------------------------------------------------------

//----------
private:
//----------

	std::size_t m_numThreads     { 0 };
	bool        m_start_detached { false };

}; // KRunThreads

namespace detail {

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// minimum prototype for a progress printer (this one does not output anything - use e.g. KBAR for
/// terminal output)
class KParallelForEachNoProgressPrinter
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

public:

	void Start  (std::size_t iMax)       {}
	void Move   (std::size_t iDelta = 1) {}
	void Finish ()                       {}

}; // KParallelForEachNoProgressPrinter

} // namespace detail

//-----------------------------------------------------------------------------
/// Iterate with parallel threads over all elements of a range. Expects the size
/// of the range as the first element.
/// @param iSize size of the iterable range
/// @param first iterator on the first element
/// @param last iterator past the last element
/// @param f functor to call with reference on one element of the range
/// @param iMaxThreads number of threads to start at most, 0/default = count of CPU cores
/// @param p progress output class, defaults to dummy printer. Use e.g. KBAR for real output to a terminal
template<typename InputIterator,
         typename Func,
         typename Progress = detail::KParallelForEachNoProgressPrinter>
void kParallelForEach(std::size_t iSize,
                      InputIterator first, InputIterator last,
                      Func&& f,
                      std::size_t iMaxThreads = std::thread::hardware_concurrency(),
                      Progress&& p = detail::KParallelForEachNoProgressPrinter())
//-----------------------------------------------------------------------------
{
	if (iSize)
	{
		p.Start(iSize);

		if (!iMaxThreads)
		{
			iMaxThreads = std::thread::hardware_concurrency();
		}

		if (iMaxThreads <= 1 || iSize == 1)
		{
			// just run single threaded ..
			for (auto it = first; it != last; ++it)
			{
				p.Move();
				f(*it);
			}
		}
		else
		{
			std::mutex m_IterMutex;

			auto loop = [&]()
			{
				for (;;)
				{
					InputIterator it;

					{
						std::lock_guard<std::mutex> lock(m_IterMutex);

						if (DEKAF2_UNLIKELY(first == last))
						{
							return;
						}

						it = first;

						++first;

						p.Move();
					}

					f(*it);
				}
			};

			KRunThreads threads(iMaxThreads - 1, iSize - 1);

			// create the additional threads
			threads.Create(std::ref(loop));

			// and finally run the loop in the initial thread as well
			loop();
		}

		p.Finish();
	}

} // kParallelForEach

//-----------------------------------------------------------------------------
/// Iterate with parallel threads over all elements of a range. If the iterator
/// type is not a RandomAccessIterator, the count of elements from first to last
/// is determined by incrementing first until it is equal to last. As this is a
/// linear operation you should avoid it for larger ranges by calling the sister
/// template kParallelForEach() that accepts the size of the range as the first
/// parameter.
/// @param first iterator on the first element
/// @param last iterator past the last element
/// @param f functor to call with reference on one element of the range
/// @param iMaxThreads number of threads to start at most, 0/default = count of CPU cores
/// @param p progress output class, defaults to dummy printer. Use e.g. KBAR for real output to a terminal
template<typename InputIterator,
         typename Func,
         typename Progress = detail::KParallelForEachNoProgressPrinter>
void kParallelForEach(InputIterator first, InputIterator last,
                      Func&& f,
                      std::size_t iMaxThreads = std::thread::hardware_concurrency(),
                      Progress&& p = detail::KParallelForEachNoProgressPrinter())
//-----------------------------------------------------------------------------
{
	kParallelForEach(std::distance(first, last), first, last,
	                 std::forward<Func>(f),
	                 iMaxThreads,
	                 std::forward<Progress>(p));
}

//-----------------------------------------------------------------------------
/// Iterate over all elements of a container c that has a size() member function, and call function f with a ref on the element
/// @param c any container that has a begin(), end(), and size() member function
/// @param f functor to call with reference on one element of the range
/// @param iMaxThreads number of threads to start at most, 0/default = count of CPU cores
/// @param p progress output class, defaults to dummy printer. Use e.g. KBAR for real output to a terminal
template<typename Container,
         typename Func,
         typename Progress = detail::KParallelForEachNoProgressPrinter,
         typename std::enable_if<detail::has_size<Container>::value == true, int>::type = 0>
void kParallelForEach(Container& c,
                      Func&& f,
                      std::size_t iMaxThreads = std::thread::hardware_concurrency(),
                      Progress&& p = detail::KParallelForEachNoProgressPrinter())
//-----------------------------------------------------------------------------
{
	kParallelForEach(c.size(), c.begin(), c.end(),
	                 std::forward<Func>(f),
	                 iMaxThreads,
	                 std::forward<Progress>(p));
}

//-----------------------------------------------------------------------------
/// Iterate over all elements of a container c that has no size() member function, and call function f with a ref on the element
/// @param iSize the count of elements of the container
/// @param c any container that has a begin(), and end(), but no size() member function
/// @param f functor to call with reference on one element of the range
/// @param iMaxThreads number of threads to start at most, 0/default = count of CPU cores
/// @param p progress output class, defaults to dummy printer. Use e.g. KBAR for real output to a terminal
template<typename Container,
         typename Func,
         typename Progress = detail::KParallelForEachNoProgressPrinter,
         typename std::enable_if<detail::has_size<Container>::value == false, int>::type = 0>
void kParallelForEach(std::size_t iSize,
                      Container& c,
                      Func&& f,
                      std::size_t iMaxThreads = std::thread::hardware_concurrency(),
                      Progress&& p = detail::KParallelForEachNoProgressPrinter())
//-----------------------------------------------------------------------------
{
	kParallelForEach(iSize, c.begin(), c.end(),
	                 std::forward<Func>(f),
	                 iMaxThreads,
	                 std::forward<Progress>(p));
}

//-----------------------------------------------------------------------------
/// Iterate over all elements of a container c that has no size() member function, and call function f with a ref on the element.
/// The size will internally be set to max(1, iMaxThreads), this enables execution but no progress output with a known end.
/// @param c any container that has a begin(), and end(), but no size() member function
/// @param f functor to call with reference on one element of the range
/// @param iMaxThreads number of threads to start at most, 0/default = count of CPU cores
/// @param p progress output class, defaults to dummy printer. Use e.g. KBAR for real output to a terminal
template<typename Container,
         typename Func,
         typename Progress = detail::KParallelForEachNoProgressPrinter,
         typename std::enable_if<detail::has_size<Container>::value == false, int>::type = 0>
void kParallelForEach(Container& c,
                      Func&& f,
                      std::size_t iMaxThreads = std::thread::hardware_concurrency(),
                      Progress&& p = detail::KParallelForEachNoProgressPrinter())
//-----------------------------------------------------------------------------
{
	kParallelForEach(std::max(iMaxThreads, std::size_t(1)), c.begin(), c.end(),
	                 std::forward<Func>(f),
	                 iMaxThreads,
	                 std::forward<Progress>(p));
}

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Provides an execution barrier that only lets one of multiple equal ID
/// values execute. All other instances with the same value have to wait until
/// the first one finishes, then the next, and so on.
class DEKAF2_PUBLIC KBlockOnID
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// Holds the actual locks for the KBlockOnID instance. Should be a static
	/// var at a given call site.
	class Data
	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		friend class KBlockOnID;

	//----------
	public:
	//----------

		//-----------------------------------------------------------------------------
		/// Tests if an actual ID would block right now.
		bool WouldBlock(std::size_t ID);
		//-----------------------------------------------------------------------------

	//----------
	protected:
	//----------

		//-----------------------------------------------------------------------------
		/// Lock an ID.
		void Lock(std::size_t ID);
		//-----------------------------------------------------------------------------

		//-----------------------------------------------------------------------------
		/// Unlock an ID.
		bool Unlock(std::size_t ID);
		//-----------------------------------------------------------------------------

	//----------
	private:
	//----------

		using lockmap_t = std::unordered_map<std::size_t, std::unique_ptr<std::mutex>>;
		KThreadSafe<lockmap_t> m_id_mutexes;

	}; // Data

	//-----------------------------------------------------------------------------
	/// Constructor.
	/// @param data The lock instance, typically a static var at the call site.
	/// @param ID The ID to lock for.
	KBlockOnID(Data& data, std::size_t ID)
	//-----------------------------------------------------------------------------
	    : m_data(data)
	    , m_MyID(ID)
	{
		m_data.Lock(ID);
	}

	//-----------------------------------------------------------------------------
	/// Destructor unlocks the ID.
	~KBlockOnID()
	//-----------------------------------------------------------------------------
	{
		m_data.Unlock(m_MyID);
	}

//----------
private:
//----------

	Data&  m_data;
	std::size_t m_MyID;

}; // KBlockOnID

DEKAF2_NAMESPACE_END
