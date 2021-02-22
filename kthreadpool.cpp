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


#include "kthreadpool.h"
#include "bits/kmake_unique.h"

namespace dekaf2 {

//-----------------------------------------------------------------------------
KThreadPool::KThreadPool(size_t nThreads)
//-----------------------------------------------------------------------------
{
	if (nThreads == 0)
	{
		nThreads = std::thread::hardware_concurrency();
	}
	
	resize(nThreads);

} // ctor

//-----------------------------------------------------------------------------
KThreadPool::~KThreadPool()
//-----------------------------------------------------------------------------
{
	stop(false);

} // dtor

//-----------------------------------------------------------------------------
void KThreadPool::restart()
//-----------------------------------------------------------------------------
{
	auto nsize = size();
	stop(false);
	resize(nsize);

} // restart

//-----------------------------------------------------------------------------
void KThreadPool::resize(size_t nThreads)
//-----------------------------------------------------------------------------
{
	std::unique_lock<std::recursive_mutex> lock(m_resize_mutex);

	size_t oldNThreads = m_threads.size();

	if (oldNThreads <= nThreads)
	{
		// the number of threads is increased
		m_threads .resize(nThreads);
		m_abort   .resize(nThreads);

		for (size_t i = oldNThreads; i < nThreads; ++i)
		{
			m_abort[i] = std::make_shared<std::atomic<bool>>(false);
			run_thread(i);
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

		// stop the detached threads that were waiting
		m_cond_var.notify_all();

		m_threads .resize(nThreads); // safe to delete because the threads are detached
		m_abort   .resize(nThreads); // safe to delete because the threads have copies of shared_ptr of the flags, not originals

		// This comes too early for most stopped threads, but we need to
		// reset it as otherwise the count remains wrong forever. As we
		// typically use this for the shutdown callback on destruction
		// this does not affect normal use cases
		ma_iAlreadyStopped = 0;
	}

} // resize

//-----------------------------------------------------------------------------
void KThreadPool::stop( bool kill )
//-----------------------------------------------------------------------------
{
	std::unique_lock<std::recursive_mutex> lock(m_resize_mutex);

	if (kill)
	{
		for (size_t i = 0, n = size(); i < n; ++i)
		{
			*m_abort[i] = true;  // command the threads to stop
		}
	}

	ma_interrupt = true;

	// have to unlock here, as we stop other threads that may
	// wish to access to the size member when printing their
	// shutdown diagnostics
	lock.unlock();

	m_cond_var.notify_all();  // stop all waiting threads

	// wait for the computing threads to finish
	for (auto& m_thread : m_threads)
	{
		if (m_thread->joinable())
		{
			m_thread->join();
		}
	}

	// and lock again to do the final resize
	lock.lock();

	m_queue   .clear(m_cond_mutex);
	m_threads .clear();
	m_abort   .clear();
	ma_n_idle = 0;
	ma_interrupt = false;
	ma_iAlreadyStopped = 0;

} // stop

//-----------------------------------------------------------------------------
// each thread pops jobs from the queue until:
//  - the queue is empty, then it waits (idle)
//  - its abort flag is set (terminate without emptying the queue)
//  - a global interrupt is set, then only idle threads terminate
void KThreadPool::run_thread( size_t i )
//-----------------------------------------------------------------------------
{
	// a copy of the shared ptr to the abort
	std::shared_ptr<std::atomic_bool> abort_ptr(m_abort[i]);

	auto f = [this, abort_ptr]()
	{
		std::atomic_bool & abort = *abort_ptr;
		std::packaged_task<void()> _f;
		std::unique_lock<std::mutex> lock(m_cond_mutex);

		bool more_tasks = m_queue.pop(_f);

		for (;;)
		{
			while (more_tasks) // if there is anything in the queue
			{
				lock.unlock();

				// increase total task counter
				++ma_iTotalTasks;

				// and run the task
				(_f)();

				if (abort)
				{
					notify_thread_shutdown(false);

					return; // return even if the queue is not empty yet
				}

				lock.lock();

				more_tasks = m_queue.pop(_f);
			}

			if (ma_interrupt)
			{
				// unlock the cond mutex, the diagnostic output needs it, too
				lock.unlock();

				notify_thread_shutdown(false);

				return;
			}

			// only enter the cond_wait if ma_interrupt is false, as
			// that is the global flag to shut down after all tasks
			// are completed

			++ma_n_idle;

			m_cond_var.wait(lock, [this, &_f, &more_tasks, &abort]()
			{
				if (abort || ma_interrupt)
				{
					return true;
				}

				more_tasks = m_queue.pop(_f);

				return more_tasks;
			});

			--ma_n_idle;

			if (!more_tasks)
			{
				// we stopped waiting either because of interruption or abort
				// (it suffices to test for !more_tasks because otherwise the
				// wakeup from the cond_wait would only have happened if there
				// were more tasks..), and entering in ma_interrupt state would
				// only happen with !more_tasks either

				// unlock the cond mutex, the diagnostic output needs it, too
				lock.unlock();

				notify_thread_shutdown(true);

				return;
			}
		}
	};

	m_threads[i] = std::make_unique<std::thread>(f);

} // setup_thread

//-----------------------------------------------------------------------------
void KThreadPool::push_packaged_task(std::packaged_task<void()> task)
//-----------------------------------------------------------------------------
{
	std::unique_lock<std::mutex> lock(m_cond_mutex);

	m_queue.push(std::move(task));

	lock.unlock();

	// notify in the unlocked state!
	m_cond_var.notify_one();

} // push_packaged_task

//-----------------------------------------------------------------------------
KThreadPool::Diagnostics KThreadPool::get_diagnostics(bool bWasIdle) const
//-----------------------------------------------------------------------------
{
	Diagnostics Diag;

	Diag.iTotalThreads  = size() - ma_iAlreadyStopped;
	Diag.iIdleThreads   = n_idle();
	Diag.iUsedThreads   = Diag.iTotalThreads - Diag.iIdleThreads;
	Diag.iTotalTasks    = ma_iTotalTasks;
	Diag.iWaitingTasks  = n_queued();
	Diag.bWasIdle       = bWasIdle;

	return Diag;

} // get_diagnostics

//-----------------------------------------------------------------------------
void KThreadPool::notify_thread_shutdown(bool bWasIdle)
//-----------------------------------------------------------------------------
{
	++ma_iAlreadyStopped;

	if (m_shutdown_callback)
	{
		m_shutdown_callback(get_diagnostics(bWasIdle));
	}

} // notify_thread_shutdown

} // end of namespace dekaf2

