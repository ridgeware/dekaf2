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
 *  February 2021, Joachim Schurig
 *   - adding diagnostics/statistics output
 *
 *  February 2022, Joachim Schurig
 *   - protection against exceptions, both in generating a thread and in
 *     executing a task
 *   - adding counter for max queue size
 *   - making sure detached threads (from resizes) are properly finished
 *     when stopping all threads
 *
 *********************************************************/


#include "kthreadpool.h"
#include "bits/kmake_unique.h"
#include "klog.h"

namespace dekaf2 {

//-----------------------------------------------------------------------------
KThreadPool::KThreadPool(std::size_t nThreads)
//-----------------------------------------------------------------------------
{
	resize(nThreads ? nThreads : std::thread::hardware_concurrency());

} // ctor

//-----------------------------------------------------------------------------
KThreadPool::~KThreadPool()
//-----------------------------------------------------------------------------
{
	stop(false);

} // dtor

//-----------------------------------------------------------------------------
std::size_t KThreadPool::size() const
//-----------------------------------------------------------------------------
{
	std::unique_lock<std::recursive_mutex> lock(m_resize_mutex);
	return m_threads.size();

} // size

//-----------------------------------------------------------------------------
std::size_t KThreadPool::n_queued() const
//-----------------------------------------------------------------------------
{
	return m_queue.size(m_cond_mutex);

} // n_queued

//-----------------------------------------------------------------------------
bool KThreadPool::restart()
//-----------------------------------------------------------------------------
{
	auto nsize = size();
	stop(false);
	return resize(nsize);

} // restart

//-----------------------------------------------------------------------------
bool KThreadPool::resize(std::size_t nThreads)
//-----------------------------------------------------------------------------
{
	std::unique_lock<std::recursive_mutex> lock(m_resize_mutex);

	// will be reset to current value with next push of a task
	ma_iMaxWaitingTasks = 0;

	auto oldNThreads = m_threads.size();

	if (oldNThreads < nThreads)
	{
		// the number of threads is increased
		m_threads .resize(nThreads);
		m_abort   .resize(nThreads);

		for (std::size_t i = oldNThreads; i < nThreads; ++i)
		{
			m_abort[i] = std::make_shared<std::atomic<eABORT>>(eABORT::NONE);

			if (!run_thread(i))
			{
				// could not start thread - stop increasing thread count
				m_threads.resize(i);
				m_abort  .resize(i);
				return false;
			}
		}
	}
	else if (oldNThreads > nThreads)
	{
		// the number of threads is decreased
		for (size_t i = oldNThreads - 1; i >= nThreads; --i)
		{
			// increase counter for threads that have to finish
			// - they are now detached and cannot be joined anymore
			++ma_iDetachedThreadsToFinish;
			*m_abort[i] = eABORT::RESIZE; // this thread will finish
			m_threads[i]->detach();

			if (i == 0)
			{
				break;
			}
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

	return true;

} // resize

//-----------------------------------------------------------------------------
void KThreadPool::clear()
//-----------------------------------------------------------------------------
{
	// empty the task queue
	m_queue.clear(m_cond_mutex);

} // clear

//-----------------------------------------------------------------------------
void KThreadPool::stop(bool bKill)
//-----------------------------------------------------------------------------
{
	std::unique_lock<std::recursive_mutex> lock(m_resize_mutex);

	if (bKill)
	{
		for (size_t i = 0, n = size(); i < n; ++i)
		{
			// do not increase the counter for detached threads, as we
			// want to finish these with join() - therefore set a differnt
			// abort flag than for the resize case
			*m_abort[i] = eABORT::STOP; // command the threads to stop
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

	// check if previously removed threads are still running
	while (ma_iDetachedThreadsToFinish > 0)
	{
		// yes, sleep a wink
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}

	// and lock again to do the final resize
	lock.lock();

	// we do not clear the task queue..
	m_threads .clear();
	m_abort   .clear();
	ma_n_idle = 0;
	ma_interrupt = false;
	ma_iAlreadyStopped = 0;

} // stop

//-----------------------------------------------------------------------------
bool KThreadPool::is_stopped()
//-----------------------------------------------------------------------------
{
	return size() == 0 && ma_iDetachedThreadsToFinish == 0;

} // is_stopped

//-----------------------------------------------------------------------------
// each thread pops jobs from the queue until:
//  - the queue is empty, then it waits (idle)
//  - the queue is empty and its abort flag is set, then it terminates
bool KThreadPool::run_thread(std::size_t i)
//-----------------------------------------------------------------------------
{
	// a copy of the shared ptr to the abort
	std::shared_ptr<std::atomic<eABORT>> abort_ptr(m_abort[i]);

	auto f = [this, abort_ptr]()
	{
		std::atomic<eABORT>& abort = *abort_ptr;
		std::packaged_task<void()> _f;
		std::unique_lock<std::mutex> lock(m_cond_mutex);

		bool bMoreTasks = m_queue.pop(_f);

		for (;;)
		{
			while (bMoreTasks) // if there is anything in the queue
			{
				lock.unlock();

				// increase total task counter
				++ma_iTotalTasks;

				// and run the task
				try
				{
					(_f)();
				}
				catch (const std::exception& ex)
				{
					kException(ex);
				}
				catch (...)
				{
					kUnknownException();
				}

				if (abort != eABORT::NONE)
				{
					notify_thread_shutdown(false, abort);

					return; // return even if the queue is not empty yet
				}

				lock.lock();

				bMoreTasks = m_queue.pop(_f);
			}

			if (ma_interrupt)
			{
				// unlock the cond mutex, the diagnostic output needs it, too
				lock.unlock();

				notify_thread_shutdown(false, abort);

				return;
			}

			// only enter the cond_wait if ma_interrupt is false, as
			// that is the global flag to shut down after all tasks
			// are completed

			++ma_n_idle;

			m_cond_var.wait(lock, [this, &_f, &bMoreTasks, &abort]()
			{
				if (abort != eABORT::NONE || ma_interrupt)
				{
					return true;
				}

				bMoreTasks = m_queue.pop(_f);

				return bMoreTasks;
			});

			--ma_n_idle;

			if (!bMoreTasks)
			{
				// we stopped waiting because of a thread abort
				// unlock the cond mutex, the diagnostic output needs it, too
				lock.unlock();

				notify_thread_shutdown(true, abort);

				return;
			}
		}
	};

	try
	{
		m_threads[i] = std::make_unique<std::thread>(f);
		return true;
	}
	catch (const std::exception& ex)
	{
		kException(ex);
	}

	return false;

} // setup_thread

//-----------------------------------------------------------------------------
void KThreadPool::push_packaged_task(std::packaged_task<void()> task)
//-----------------------------------------------------------------------------
{
	std::unique_lock<std::mutex> lock(m_cond_mutex);

	auto iWaiting = m_queue.push(std::move(task)) - 1;

	if (iWaiting > ma_iMaxWaitingTasks)
	{
		ma_iMaxWaitingTasks = iWaiting;
	}

	lock.unlock();

	// notify in the unlocked state!
	m_cond_var.notify_one();

} // push_packaged_task

//-----------------------------------------------------------------------------
KThreadPool::Diagnostics KThreadPool::get_diagnostics(bool bWasIdle) const
//-----------------------------------------------------------------------------
{
	Diagnostics Diag;

	Diag.iTotalThreads    = size() - ma_iAlreadyStopped;
	Diag.iIdleThreads     = n_idle();
	Diag.iUsedThreads     = Diag.iTotalThreads - Diag.iIdleThreads;
	Diag.iTotalTasks      = ma_iTotalTasks;
	Diag.iMaxWaitingTasks = ma_iMaxWaitingTasks;
	Diag.iWaitingTasks    = n_queued();
	Diag.bWasIdle         = bWasIdle;

	return Diag;

} // get_diagnostics

//-----------------------------------------------------------------------------
void KThreadPool::notify_thread_shutdown(bool bWasIdle, eABORT abort)
//-----------------------------------------------------------------------------
{
	if (abort == eABORT::STOP)
	{
		++ma_iAlreadyStopped;
	}

	if (m_shutdown_callback)
	{
		m_shutdown_callback(get_diagnostics(bWasIdle));
	}

	// don't run this as an else of above - it has to happen past
	// the shutdown callback, and the former before..
	if (abort == eABORT::RESIZE)
	{
		// decrease the count of detached threads to wait for in the join()
		--ma_iDetachedThreadsToFinish;
	}

} // notify_thread_shutdown

} // end of namespace dekaf2

