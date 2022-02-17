// This file was initially copied from https://raw.githubusercontent.com/sheljohn/CTPL/master/ctpl_stl.h

/*********************************************************
 *
 *  Copyright (C) 2014 by Vitaliy Vitsentiy
 *  https://github.com/vit-vit/CTPL
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *
 *  October 2015, Jonathan Hadida:
 *   - rename, reformat and comment some functions
 *   - add a restart function
 *  	- fix a few unsafe spots, eg:
 *  		+ order of testing in setup_thread;
 *  		+ atomic guards on pushes;
 *  		+ make clear_queue private
 *
 *  October 2018, Joachim Schurig
 *   - adapt dekaf2 brace standard and swap into the dekaf2 namespace
 *   - remove most shared pointers by using the move idiom, enable member
 *     function callables, and splitting up the code into header and
 *     implementation
 *   - remove unsafe atomic guards, replace with a 'resize' mutex
 *   - safeguarding against lost tasks with consistent condition locking
 *
 *  January 2021, Joachim Schurig
 *   - allowing arbitrary return types (futures) for any task (function)
 *    pushed to the task queue
 *
 *********************************************************/


#pragma once

#include "bits/kcppcompat.h"
#include <functional>
#include <thread>
#include <atomic>
#include <vector>
#include <memory>
#include <future>
#include <mutex>
#include <queue>

/// @file kthreadpool.h
/// thread pool to run user's tasks (all types of callables) with signature
/// ret func(params, ...)

namespace dekaf2 {

namespace detail {
namespace threadpool {

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// move semantics queue implementation - helper type for thread pool
template <typename T>
class Queue
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	//-----------------------------------------------------------------------------
	void push(T&& value)
	//-----------------------------------------------------------------------------
	{
		m_queue.push(std::move(value));
	}

	//-----------------------------------------------------------------------------
	bool pop(T& v)
	//-----------------------------------------------------------------------------
	{
		if (m_queue.empty())
		{
			return false;
		}
		v = std::move(m_queue.front());
		m_queue.pop();
		return true;
	}

	//-----------------------------------------------------------------------------
	void clear(std::mutex& mutex)
	//-----------------------------------------------------------------------------
	{
		std::unique_lock<std::mutex> lock(mutex);

		while (!m_queue.empty())
		{
			m_queue.pop();
		}
	}

	//-----------------------------------------------------------------------------
	bool empty(std::mutex& mutex) const
	//-----------------------------------------------------------------------------
	{
		std::unique_lock<std::mutex> lock(mutex);

		return m_queue.empty();
	}

	//-----------------------------------------------------------------------------
	size_t size(std::mutex& mutex) const
	//-----------------------------------------------------------------------------
	{
		std::unique_lock<std::mutex> lock(mutex);

		return m_queue.size();
	}

//------
private:
//------

	std::queue<T> m_queue;

}; // Queue

} // end of namespace threadpool
} // end of namespace detail

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// ThreadPool implementation in form of a task queue - you define the number
/// of concurrent threads, and push tasks which then will be picked by the
/// next free thread
class DEKAF2_PUBLIC KThreadPool
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	//-----------------------------------------------------------------------------
	/// Construct an empty thread pool - it will not start unless you resize it!
	KThreadPool() = default;
	//-----------------------------------------------------------------------------

	KThreadPool(const KThreadPool &) = delete;
	KThreadPool(KThreadPool &&) = delete;
	KThreadPool & operator=(const KThreadPool &) = delete;
	KThreadPool & operator=(KThreadPool &&) = delete;

	//-----------------------------------------------------------------------------
	/// Construct a thread pool with nThreads size - if nThreads == 0 starts as many
	/// threads as CPU cores are available
	KThreadPool(size_t nThreads);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// The destructor waits for all the functions in the queue to be finished
	~KThreadPool();
	//-----------------------------------------------------------------------------

	struct Diagnostics
	{
		std::size_t iTotalThreads  { 0 };
		std::size_t iIdleThreads   { 0 };
		std::size_t iUsedThreads   { 0 };
		std::size_t iTotalTasks    { 0 };
		std::size_t iWaitingTasks  { 0 };
		bool        bWasIdle       { false };

	}; // Diagnostics

	//-----------------------------------------------------------------------------
	/// Query current threadpool status - ignore bWasIdle param
	Diagnostics get_diagnostics(bool bWasIdle = false) const;
	//-----------------------------------------------------------------------------

	using ShutdownCallback = std::function<void(Diagnostics)>;

	//-----------------------------------------------------------------------------
	/// Shall we log the shutdown?
	/// @param callback callback function called at each shutdown thread with some diagnostics
	void register_shutdown_callback(ShutdownCallback callback)
	//-----------------------------------------------------------------------------
	{
		m_shutdown_callback = callback;
	}

	//-----------------------------------------------------------------------------
	/// Get the total number of threads in the pool
	size_t size() const
	//-----------------------------------------------------------------------------
	{
		std::unique_lock<std::recursive_mutex> lock(m_resize_mutex);
		return m_threads.size();
	}

	//-----------------------------------------------------------------------------
	/// Get the number of idle threads
	size_t n_idle() const
	//-----------------------------------------------------------------------------
	{
		return ma_n_idle;
	}

	//-----------------------------------------------------------------------------
	/// Get the number of tasks waiting in the queue
	size_t n_queued() const
	//-----------------------------------------------------------------------------
	{
		return m_queue.size(m_cond_mutex);
	}

	//-----------------------------------------------------------------------------
	/// Restart the pool
	void restart();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Change the number of threads in the pool
	void resize(size_t nThreads);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Wait for all computing threads to finish and stop all threads.
	/// May be called asynchronously to not pause the calling thread while waiting.
	/// If kill == false, all the functions in the queue are run, otherwise the queue is cleared without running the functions
	void stop( bool kill = false );
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Push a class member function for asynchronous execution with arbitrary args. Receive result in returned future,
	/// or ignore..
	// SFINAE version for decltype(Function()) == void
	template<typename Function, typename Object, typename... Args>
	auto push(Function&& f, Object&& o, Args&&... args)
	-> typename std::enable_if<std::is_same<decltype((o->*f)(std::forward<Args>(args)...)), void>::value,
	                             std::future<void>>::type
	//-----------------------------------------------------------------------------
	{
		auto task = std::packaged_task<void()>(
			std::bind(std::forward<Function>(f), std::forward<Object>(o), std::forward<Args>(args)...)
		);

		auto future = task.get_future();

		push_packaged_task(std::move(task));

		return future;
	}

	//-----------------------------------------------------------------------------
	/// Push a class member function for asynchronous execution with arbitrary args. Receive result in returned future,
	/// or ignore..
	// SFINAE version for decltype(Function()) different than void
	template<typename Function, typename Object, typename... Args>
	auto push(Function&& f, Object&& o, Args&&... args)
	-> typename std::enable_if<!std::is_same<decltype((o->*f)(std::forward<Args>(args)...)), void>::value,
	                             std::future<decltype((o->*f)(std::forward<Args>(args)...))>>::type
	//-----------------------------------------------------------------------------
	{
		using FutureType = decltype((o->*f)(std::forward<Args>(args)...));

		auto InnerTask = std::packaged_task<FutureType()>(
			std::bind(std::forward<Function>(f), std::forward<Object>(o), std::forward<Args>(args)...)
		);

		auto future = InnerTask.get_future();

		auto task = std::packaged_task<void()>(
			std::move(InnerTask)
		);

		push_packaged_task(std::move(task));

		return future;
	}

	//-----------------------------------------------------------------------------
	/// Push a non-class callable for asynchronous execution with arbitrary args. Receive result in returned future,
	/// or ignore..
	// SFINAE version for decltype(Function()) == void
	template<typename Function, typename... Args>
	auto push(Function&& f, Args&&... args)
	-> typename std::enable_if<std::is_same<decltype(f(std::forward<Args>(args)...)), void>::value,
	                             std::future<void>>::type
	//-----------------------------------------------------------------------------
	{
		auto task = std::packaged_task<void()>(
			std::bind(std::forward<Function>(f), std::forward<Args>(args)...)
		);

		auto future = task.get_future();

		push_packaged_task(std::move(task));

		return future;
	}

	//-----------------------------------------------------------------------------
	/// Push a non-class callable for asynchronous execution with arbitrary args. Receive result in returned future,
	/// or ignore (in which case you should better use void as function return type)
	// SFINAE version for decltype(Function()) different than void
	template<typename Function, typename... Args>
	auto push(Function&& f, Args&&... args)
	-> typename std::enable_if<!std::is_same<decltype(f(std::forward<Args>(args)...)), void>::value,
	                             std::future<decltype(f(std::forward<Args>(args)...))>>::type
	//-----------------------------------------------------------------------------
	{
		using FutureType = decltype(f(std::forward<Args>(args)...));

		auto InnerTask = std::packaged_task<FutureType()>(
			std::bind(std::forward<Function>(f), std::forward<Args>(args)...)
		);

		auto future = InnerTask.get_future();

		auto task = std::packaged_task<void()>(
			std::move(InnerTask)
		);

		push_packaged_task(std::move(task));

		return future;
	}

//------
private:
//------

	DEKAF2_PRIVATE
	void push_packaged_task(std::packaged_task<void()> task);

	DEKAF2_PRIVATE
	void run_thread( size_t i );

	DEKAF2_PRIVATE
	void notify_thread_shutdown(bool bWasIdle);

	std::vector<std::unique_ptr<std::thread>>             m_threads;
	std::vector<std::shared_ptr<std::atomic<bool>>>       m_abort;
	detail::threadpool::Queue<std::packaged_task<void()>> m_queue;

	std::atomic<size_t>      ma_iTotalTasks     { 0 };
	std::atomic<size_t>      ma_n_idle          { 0 };
	std::atomic<size_t>      ma_iAlreadyStopped { 0 };
	std::atomic<bool>        ma_interrupt       { false };

	mutable std::recursive_mutex m_resize_mutex;
	mutable std::mutex       m_cond_mutex;
	std::condition_variable  m_cond_var;
	ShutdownCallback         m_shutdown_callback;

}; // KThreadPool

} // end of namespace dekaf2

