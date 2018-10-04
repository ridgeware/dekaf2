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
 *********************************************************/


#include "kthreadpool.h"

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
	std::unique_lock<std::mutex> lock(m_resize_mutex);

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

		// stop the detached threads that were waiting
		m_cond_var.notify_all();

		m_threads .resize(nThreads); // safe to delete because the threads are detached
		m_abort   .resize(nThreads); // safe to delete because the threads have copies of shared_ptr of the flags, not originals
	}

} // resize

//-----------------------------------------------------------------------------
void KThreadPool::stop( bool kill )
//-----------------------------------------------------------------------------
{
	std::unique_lock<std::mutex> lock(m_resize_mutex);

	if (kill)
	{
		for (size_t i = 0, n = size(); i < n; ++i)
		{
			*m_abort[i] = true;  // command the threads to stop
		}
	}

	ma_interrupt = true;

	m_cond_var.notify_all();  // stop all waiting threads

	// wait for the computing threads to finish
	for (size_t i = 0; i < m_threads.size(); ++i)
	{
		if (m_threads[i]->joinable())
		{
			m_threads[i]->join();
		}
	}

	m_queue   .clear(m_cond_mutex);
	m_threads .clear();
	m_abort   .clear();
	ma_n_idle = 0;
	ma_interrupt = false;

} // stop

//-----------------------------------------------------------------------------
// each thread pops jobs from the queue until:
//  - the queue is empty, then it waits (idle)
//  - its abort flag is set (terminate without emptying the queue)
//  - a global interrupt is set, then only idle threads terminate
void KThreadPool::setup_thread( size_t i )
//-----------------------------------------------------------------------------
{
	// a copy of the shared ptr to the abort
	std::shared_ptr<std::atomic<bool>> abort_ptr(m_abort[i]);

	auto f = [this, abort_ptr]()
	{
		std::atomic<bool> & abort = *abort_ptr;
		std::packaged_task<void()> _f;
		std::unique_lock<std::mutex> lock(m_cond_mutex);

		bool more_tasks = m_queue.pop(_f);

		for (;;)
		{
			while (more_tasks) // if there is anything in the queue
			{
				lock.unlock();

				(_f)();

				if (abort)
				{
					return; // return even if the queue is not empty yet
				}

				lock.lock();

				more_tasks = m_queue.pop(_f);
			}

			++ma_n_idle;

			m_cond_var.wait(lock, [this, &_f, &more_tasks, &abort]()
			{
				more_tasks = m_queue.pop(_f);
				return abort || ma_interrupt || more_tasks;
			});

			--ma_n_idle;

			if ( ! more_tasks)
			{
				// we stopped waiting either because of interruption or abort
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

} // end of namespace dekaf2

