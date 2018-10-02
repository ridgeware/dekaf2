// This version copied from https://raw.githubusercontent.com/sheljohn/CTPL/master/ctpl_stl.h
//
// edits to adapt to brace standard and swap into the dekaf2 namespace October 1, 2018

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
 *********************************************************/


#pragma once

#include <functional>
#include <thread>
#include <atomic>
#include <vector>
#include <memory>
#include <future>
#include <mutex>
#include <queue>

/// @file kthreadpool.h
// thread pool to run user's functors with signature
//      ret func(size_t id, other_params)
// where id is the index of the thread that runs the functor
// ret is some return type


namespace dekaf2 {

namespace detail {
namespace threadpool {

template <typename T>
class Queue {

public:

	bool push(T && value)
	{
		std::unique_lock<std::mutex> lock(m_mutex);
		m_queue.push(std::move(value));
		return true;
	}

	// deletes the retrieved element, do not use for non integral types
	bool pop(T & v)
	{
		std::unique_lock<std::mutex> lock(m_mutex);
		if (m_queue.empty())
		{
			return false;
		}
		v = std::move(m_queue.front());
		m_queue.pop();
		return true;
	}

	void clear()
	{
		std::unique_lock<std::mutex> lock(m_mutex);
		while (!m_queue.empty())
		{
			m_queue.pop();
		}
	}

	bool empty() const
	{
		std::unique_lock<std::mutex> lock(m_mutex);
		return m_queue.empty();
	}

	size_t size() const
	{
		std::unique_lock<std::mutex> lock(m_mutex);
		return m_queue.size();
	}

private:

	std::queue<T> m_queue;
	mutable std::mutex m_mutex;

};

class thread_pool {

public:

	thread_pool(const thread_pool &) = delete;
	thread_pool(thread_pool &&) = delete;
	thread_pool & operator=(const thread_pool &) = delete;
	thread_pool & operator=(thread_pool &&) = delete;

	thread_pool() { init(); }
	thread_pool(size_t nThreads) { init(); resize(nThreads); }

	/// the destructor waits for all the functions in the queue to be finished
	~thread_pool() { interrupt(false); }

	/// get the number of running threads in the pool
	inline size_t size() const { return m_threads.size(); }

	/// number of idle threads
	inline size_t n_idle() const { return ma_n_idle; }

	/// number of tasks waiting in queue
	inline size_t n_queued() const { return m_queue.size(); }

	/// get a specific thread
	inline std::thread & get_thread( size_t i ) { return *m_threads.at(i); }


	/// restart the pool
	void restart()
	{
		interrupt(false); // finish all existing tasks but prevent new ones
		init(); // reset atomic flags
	}

	/// change the number of threads in the pool -
	/// should be called from one thread, otherwise be careful to not interleave, also with this->interrupt()
	void resize(size_t nThreads)
	{
		if (!ma_kill && !ma_interrupt)
		{

			size_t oldNThreads = m_threads.size();
			if (oldNThreads <= nThreads)
			{
				// the number of threads is increased
				m_threads .resize(nThreads);
				m_abort   .resize(nThreads);

				for (size_t i = oldNThreads; i < nThreads; ++i)
				{
					m_abort[i] = std::make_shared<std::atomic<bool>>(false);
					setup_thread(i);
				}
			}
			else
			{
				// the number of threads is decreased
				for (size_t i = oldNThreads - 1; i >= nThreads; --i)
				{
					*m_abort[i] = true;  // this thread will finish
					m_threads[i]->detach();
				}
				{
					// stop the detached threads that were waiting
					m_cond.notify_all();
				}
				m_threads .resize(nThreads); // safe to delete because the threads are detached
				m_abort   .resize(nThreads); // safe to delete because the threads have copies of shared_ptr of the flags, not originals
			}
		}
	}

	/// wait for all computing threads to finish and stop all threads
	/// may be called asynchronously to not pause the calling thread while waiting
	/// if kill == false, all the functions in the queue are run, otherwise the queue is cleared without running the functions
	void interrupt( bool kill = false )
	{
		if (kill)
		{
			if (ma_kill)
			{
				return;
			}

			ma_kill = true;

			for (size_t i = 0, n = size(); i < n; ++i)
			{
				*m_abort[i] = true;  // command the threads to stop
			}
		}
		else
		{
			if (ma_interrupt || ma_kill)
			{
				return;
			}

			ma_interrupt = true;  // give the waiting threads a command to finish
		}

		{
			m_cond.notify_all();  // stop all waiting threads
		}

		// wait for the computing threads to finish
		for (size_t i = 0; i < m_threads.size(); ++i)
		{
			if (m_threads[i]->joinable())
			{
				m_threads[i]->join();
			}
		}

		m_queue   .clear();
		m_threads .clear();
		m_abort   .clear();
	}

	/// Push a class member function for asynchronous execution with arbitrary args. Receive result in returned future,
	/// or ignore..
	template<typename F, typename Object, typename... Args>
	auto push(F && f, Object && o, Args&&... args) ->std::future<decltype((o->*f)(std::forward<Args>(args)...))>
	{
		std::future<decltype((o->*f)(std::forward<Args>(args)...))> future;

		if (!ma_kill && !ma_interrupt)
		{
			auto pck = std::packaged_task<decltype((o->*f)(std::forward<Args>(args)...))()>(
				std::bind(std::forward<F>(f), std::forward<Object>(o), std::forward<Args>(args)...)
			);
			future = pck.get_future();
			m_queue.push(std::move(pck));
			m_cond.notify_one();
		}

		return future;
	}

	/// Push a non-class callable for asynchronous execution with arbitrary args. Receive result in returned future,
	/// or ignore..
	template<typename F, typename... Args>
	auto push(F && f, Args&&... args) ->std::future<decltype(f(std::forward<Args>(args)...))>
	{
		std::future<decltype(f(std::forward<Args>(args)...))> future;

		if (!ma_kill && !ma_interrupt)
		{
			auto pck = std::packaged_task<decltype(f(std::forward<Args>(args)...))()>(
				std::bind(std::forward<F>(f), std::forward<Args>(args)...)
			);
			future = pck.get_future();
			m_queue.push(std::move(pck));
			m_cond.notify_one();
		}

		return future;
	}

private:

	// reset all flags
	void init()
	{
		ma_n_idle = 0;
		ma_kill = false;
		ma_interrupt = false;
	}

	// each thread pops jobs from the queue until:
	//  - the queue is empty, then it waits (idle)
	//  - its abort flag is set (terminate without emptying the queue)
	//  - a global interrupt is set, then only idle threads terminate
	void setup_thread( size_t i )
	{
		// a copy of the shared ptr to the abort
		std::shared_ptr<std::atomic<bool>> abort_ptr(m_abort[i]);

		auto f = [this, abort_ptr]()
		{
			std::atomic<bool> & abort = *abort_ptr;
			std::packaged_task<void()> _f;
			bool more_tasks = m_queue.pop(_f);

			while (true)
			{
				while (more_tasks) // if there is anything in the queue
				{
					(_f)();

					if (abort)
					{
						return; // return even if the queue is not empty yet
					}
					else
					{
						more_tasks = m_queue.pop(_f);
					}
				}

				// the queue is empty here, wait for the next command
				std::unique_lock<std::mutex> lock(m_mutex);

				++ma_n_idle;
				m_cond.wait(lock, [this, &_f, &more_tasks, &abort](){ more_tasks = m_queue.pop(_f); return abort || ma_interrupt || more_tasks; });
				--ma_n_idle;
				
				if ( ! more_tasks)
				{
					return; // we stopped waiting either because of interruption or abort
				}
			}
		};

		m_threads[i].reset( new std::thread(f) );
	}

	// ----------  =====  ----------

	std::vector<std::unique_ptr<std::thread>>        m_threads;
	std::vector<std::shared_ptr<std::atomic<bool>>>  m_abort;
	Queue<std::packaged_task<void()>>                m_queue;

	std::atomic<bool>        ma_interrupt, ma_kill;
	std::atomic<size_t>      ma_n_idle;

	std::mutex               m_mutex;
	std::condition_variable  m_cond;
};

} // end of namespace threadpool
} // end of namespace detail

using KThreadPool = detail::threadpool::thread_pool;

} // end of namespace dekaf2

