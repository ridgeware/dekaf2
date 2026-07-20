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
 *     pushed to the task queue
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
 *  October 2025, Joachim Schurig
 *   - adding growth and shrink policy setup, permitting to dynamically
 *     resize the thread pool depending on usage
 *
 *  June 2026, Joachim Schurig
 *   - fixing lost thread wakeup (pool hang in stop()/join) by writing the
 *     abort flags and the interrupt flag under the condition variable's
 *     mutex, and establishing the global lock order m_cond_mutex before
 *     m_resize_mutex (stop(), resize())
 *   - threads aborting out of an idle wait now pass a possibly consumed
 *     task notification on to another waiting thread
 *   - resize() now refuses to run while a stop() is joining the threads
 *   - the detached thread counter now notifies under its mutex, to not
 *     touch a destroyed condition variable when racing pool destruction
 *
 *  June 2026, Joachim Schurig
 *   - opt-in fair scheduling: the single FIFO task queue was replaced by a
 *     per-work-class scheduler. Tasks may be pushed under a Tag; each tag
 *     gets its own sub-queue and the worker threads dispatch across tags by
 *     weighted Deficit Round Robin (DRR), so a flood in one tag cannot starve
 *     the others. Untagged push() uses DefaultTag (weight 1, unbounded) and
 *     keeps the exact previous FIFO behavior (single-tag fast path).
 *   - a tag may bound its sub-queue (Overflow::Reject / DropOldest) for
 *     backpressure, and cap how many of its tasks run concurrently
 *     (iMaxConcurrency) to isolate blocking work classes
 *   - PushCoalesced() collapses repeated submissions for the same key into a
 *     single debounced run with at most one follow-up (for idempotent
 *     "refresh/recompute X" work)
 *   - hardening after review: keep the wakeup for a freed concurrency slot
 *     when the finishing worker aborts (the lost wakeup could stall a capped
 *     tag's queued work after a shrink); destroy tasks dropped by DropOldest
 *     or clear() outside the pool mutex (destructors of captured user objects
 *     may touch the pool again); break the futures of a coalesced run that a
 *     bounded tag rejects, instead of leaving them pending forever; clamp a
 *     tag weight of 0 to 1; avoid a hash lookup per tag in the DRR scan
 *
 *********************************************************/


#include <dekaf2/threading/execution/kthreadpool.h>
#include <dekaf2/core/types/bits/kmake_unique.h>
#include <dekaf2/core/format/kformat.h>
#include <dekaf2/core/logging/klog.h>
#include <dekaf2/system/os/ksignals.h>
#include <dekaf2/system/os/ksystem.h>
#include <dekaf2/threading/execution/kthreads.h>
#include <deque>

DEKAF2_NAMESPACE_BEGIN

//-----------------------------------------------------------------------------
KThreadPool::KThreadPool(std::size_t nThreads, GrowthPolicy Growth, ShrinkPolicy Shrink)
//-----------------------------------------------------------------------------
{
	resize(nThreads ? nThreads : std::thread::hardware_concurrency(), Growth, Shrink);

} // ctor

//-----------------------------------------------------------------------------
KThreadPool::KThreadPool(std::size_t nThreads, KStringView sThreadName, GrowthPolicy Growth, ShrinkPolicy Shrink)
//-----------------------------------------------------------------------------
: m_sThreadName(sThreadName)
{
	resize(nThreads ? nThreads : std::thread::hardware_concurrency(), Growth, Shrink);

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
	std::unique_lock<std::mutex> lock(m_cond_mutex);
	return m_iTotalQueued;

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
void KThreadPool::set_strategy(GrowthPolicy Growth, ShrinkPolicy Shrink)
//-----------------------------------------------------------------------------
{
	std::unique_lock<std::recursive_mutex> lock(m_resize_mutex);

	m_Growth = Growth;
	m_Shrink = Shrink;

} // set_strategy

//-----------------------------------------------------------------------------
void KThreadPool::set_thread_name(KStringView sName)
//-----------------------------------------------------------------------------
{
	// the workers read the name under m_cond_mutex when they start
	std::unique_lock<std::mutex> lock(m_cond_mutex);

	m_sThreadName = sName;

} // set_thread_name

//-----------------------------------------------------------------------------
KString KThreadPool::get_thread_name() const
//-----------------------------------------------------------------------------
{
	std::unique_lock<std::mutex> lock(m_cond_mutex);

	return m_sThreadName;

} // get_thread_name

//-----------------------------------------------------------------------------
bool KThreadPool::resize(std::size_t nThreads, GrowthPolicy Growth, ShrinkPolicy Shrink)
//-----------------------------------------------------------------------------
{
	// the absolute value of n_queued() is not really important in grow()/shrink(),
	// as it is only used as a heuristic there, not as an exact value - so read it
	// before taking the locks below (n_queued() takes m_cond_mutex itself)
	auto iQueued = n_queued();

	// grow() and shrink() must be called with m_cond_mutex held: shrink() sets
	// thread abort flags that the workers' wait predicate reads under m_cond_mutex,
	// and only writing them under the same mutex prevents a lost wakeup (see the
	// comment in stop()). Lock order is m_cond_mutex before m_resize_mutex, as in
	// push_packaged_task(). As resize() is only called rarely, we do not care for
	// the cost of unconditionally locking m_cond_mutex here.
	std::unique_lock<std::mutex>           cond_lock(m_cond_mutex);
	std::unique_lock<std::recursive_mutex> lock(m_resize_mutex);

	if (ma_interrupt)
	{
		// a stop() is in progress and joins the threads without holding the locks -
		// we must neither change the thread vector nor detach threads now. (restart()
		// is unaffected: stop() resets ma_interrupt before it returns.)
		return false;
	}

	m_Growth       = Growth;
	m_Shrink       = Shrink;
	ma_iMaxThreads = nThreads;

	// will be reset to current value with next push of a task
	ma_iMaxWaitingTasks = 0;

	auto oldNThreads = m_threads.size();

	if (oldNThreads < nThreads)
	{
		grow(iQueued);
	}
	else if (oldNThreads > nThreads)
	{
		shrink(iQueued);
	}

	return true;

} // resize

//-----------------------------------------------------------------------------
bool KThreadPool::grow(std::size_t iQueued)
//-----------------------------------------------------------------------------
{
	std::unique_lock<std::recursive_mutex> lock(m_resize_mutex);

	auto oldNThreads = m_threads.size();

	std::size_t iMaxThreads = ma_iMaxThreads;

	if (oldNThreads < iMaxThreads)
	{
		std::size_t iAddMax = iMaxThreads - oldNThreads;
		std::size_t iAddImmediately { std::min(iQueued, iAddMax) };

		switch (m_Growth)
		{
			case PrestartNone:
				iAddImmediately = std::min(iAddImmediately, iAddMax);
				break;

			case PrestartOne:
				iAddImmediately = std::min(iAddImmediately + 1, iAddMax);
				break;

			case PrestartSome:
				iAddImmediately = std::min(iAddImmediately + 10, iAddMax);
				break;

			case PrestartAll:
				iAddImmediately = iAddMax;
				break;
		}

		if (iAddImmediately)
		{
			// the number of threads is increased
			auto iNewMax = oldNThreads + iAddImmediately;
			kDebug(2, "growing threadpool by {} to {}", iAddImmediately, iNewMax);

			m_threads .resize(iNewMax);
			m_abort   .resize(iNewMax);

			for (std::size_t i = oldNThreads; i < iNewMax; ++i)
			{
				m_abort[i] = std::make_shared<std::atomic<eAbort>>(eAbort::None);

				if (!run_thread(i))
				{
					// could not start thread - stop increasing thread count
					m_threads.resize(i);
					m_abort  .resize(i);
					return false;
				}
			}

			if (ma_iMaxThreadsEver < iNewMax)
			{
				ma_iMaxThreadsEver = iNewMax;
			}

			// reset the last resize timer to make sure we do not shrink too quickly
			m_LastResize = std::chrono::steady_clock::now();
		}

		return true;
	}

	return false;

} // grow

//-----------------------------------------------------------------------------
bool KThreadPool::shrink(std::size_t iQueued)
//-----------------------------------------------------------------------------
{
	std::unique_lock<std::recursive_mutex> lock(m_resize_mutex);

	auto oldNThreads = m_threads.size();

	std::size_t iShrinkImmediately { 0 };

	std::size_t iMaxThreads = ma_iMaxThreads;
	bool bForce { false };

	if (oldNThreads > iMaxThreads)
	{
		iShrinkImmediately = oldNThreads - iMaxThreads;
		bForce = true;
	}

	if (m_Shrink != ShrinkNever)
	{
		std::size_t iHaveIdle = ma_n_idle;
		iHaveIdle -= std::min(iHaveIdle, iShrinkImmediately);

		if (iHaveIdle)
		{
			iHaveIdle -= std::min(iHaveIdle, iQueued);
			iShrinkImmediately += calc_shrink(iHaveIdle);
		}
	}

	if (iShrinkImmediately)
	{
		auto now = std::chrono::steady_clock::now();

		// only shrink if last resize is longer than 10 seconds ago
		if (bForce || now - m_LastResize >= std::chrono::seconds(10))
		{
			if (iShrinkImmediately > oldNThreads)
			{
				kDebug(1, "shrink {} > size {}", iShrinkImmediately, oldNThreads);
				return false;
			}

			auto iNewMax = oldNThreads - iShrinkImmediately;

			kDebug(2, "shrinking threadpool by {} to {}", iShrinkImmediately, iNewMax);

			for (size_t i = oldNThreads - 1; i >= iNewMax; --i)
			{
				// increase counter for threads that have to finish
				// - they are now detached and cannot be joined anymore
				++ma_iDetachedThreadsToFinish;
				*m_abort[i] = eAbort::Resize; // this thread will finish
				m_threads[i]->detach();

				if (i == 0)
				{
					break;
				}
			}

			// stop the detached threads that were waiting
			m_cond_var.notify_all();

			m_threads .resize(iNewMax); // safe to delete because the threads are detached
			m_abort   .resize(iNewMax); // safe to delete because the threads have copies of shared_ptr of the flags, not originals

			// This comes too early for most stopped threads, but we need to
			// reset it as otherwise the count remains wrong forever. As we
			// typically use this for the shutdown callback on destruction
			// this does not affect normal use cases
			ma_iAlreadyStopped = 0;

			// and restart the timer
			m_LastResize = now;

			return true;
		}
	}

	return false;

} // shrink

//-----------------------------------------------------------------------------
std::size_t KThreadPool::calc_shrink(std::size_t iHaveIdle) const
//-----------------------------------------------------------------------------
{
	if (iHaveIdle)
	{
		switch (m_Shrink)
		{
			case ShrinkNever:
				iHaveIdle = 0;
				break;

			case ShrinkSome:
				if (iHaveIdle > 10)
				{
					iHaveIdle -= 10;
				}
				else
				{
					iHaveIdle = 0;
				}
				break;

			case ShrinkOne:
				iHaveIdle = iHaveIdle - 1;
				break;

			case ShrinkImmediately:
				break;
		}
	}

	return iHaveIdle;

} // calc_shrink

//-----------------------------------------------------------------------------
void KThreadPool::pause(bool bYesNo)
//-----------------------------------------------------------------------------
{
	{
		std::unique_lock<std::mutex> lock(m_cond_mutex);
		m_bQueuePaused = bYesNo;
	}

	if (!bYesNo)
	{
		m_cond_var.notify_all();
	}

} // pause

//-----------------------------------------------------------------------------
void KThreadPool::clear()
//-----------------------------------------------------------------------------
{
	// empty the task queues of all tags (destroying the tasks breaks their futures).
	// The dropped tasks and coalesce states are destroyed only after the unlock - the
	// destructors of their captured user objects may touch this pool again (e.g. push),
	// which would deadlock under the mutex.
	// Note: the queues are collected in a std::deque, not a std::vector - libstdc++'s
	// std::deque has a throwing move constructor, so vector growth (or reserve) falls
	// back to copy construction for the strong exception guarantee, which does not
	// compile for queues of move-only std::packaged_task. A deque as outer container
	// never relocates its elements and therefore needs neither copy nor move.
	std::deque<std::queue<std::packaged_task<void()>>>       DroppedQueues;
	std::vector<std::unordered_map<uint64_t, CoalesceState>> DroppedCoalesce;

	{
		std::unique_lock<std::mutex> lock(m_cond_mutex);

		DroppedCoalesce.reserve(m_Tags.size());

		for (auto& Pair : m_Tags)
		{
			DroppedQueues.emplace_back();
			std::swap(Pair.second.Queue, DroppedQueues.back());
			Pair.second.iDeficit = 0;
			// drop coalesce state too (pending coalesce futures break); a key whose run is
			// currently executing is left to its completion handler (it finds itself erased)
			DroppedCoalesce.emplace_back();
			std::swap(Pair.second.Coalesce, DroppedCoalesce.back());
		}

		m_iTotalQueued = 0;
	}

} // clear

//-----------------------------------------------------------------------------
void KThreadPool::stop(bool bKill)
//-----------------------------------------------------------------------------
{
	{
		// The abort flags and ma_interrupt MUST be written under m_cond_mutex: the
		// worker threads read them in the wait predicate under m_cond_mutex, and only
		// writing them under the same mutex makes the flag change and the worker's
		// transition into sleep mutually exclusive. Otherwise a worker could evaluate
		// the predicate to false, then miss our notify_all() below (fired while the
		// worker is between predicate and sleep), and sleep forever - hanging the
		// join() below.
		// Lock order is m_cond_mutex before m_resize_mutex, as in push_packaged_task()
		// (which holds m_cond_mutex when calling grow() and shrink()).
		std::unique_lock<std::mutex>           cond_lock(m_cond_mutex);
		std::unique_lock<std::recursive_mutex> lock(m_resize_mutex);

		if (bKill)
		{
			for (auto& abort : m_abort)
			{
				// do not increase the counter for detached threads, as we
				// want to finish these with join() - therefore set a different
				// abort flag than for the resize case
				*abort = eAbort::Stop; // command the threads to stop
			}
		}

		ma_interrupt = true;

		// both locks are released here - we have to unlock, as the stopping threads
		// may wish to access the size member when printing their shutdown diagnostics
	}

	m_cond_var.notify_all();  // stop all waiting threads

	// wait for the computing threads to finish - the thread vector can no longer
	// change, as neither push_packaged_task() nor resize() grow or shrink the
	// pool once ma_interrupt is set
	for (auto& m_thread : m_threads)
	{
		if (m_thread->joinable())
		{
			m_thread->join();
		}
	}

	// wait for previously detached threads to finish
	{
		std::unique_lock<std::mutex> detached_lock(m_detached_mutex);
		m_detached_cv.wait(detached_lock, [this]() { return ma_iDetachedThreadsToFinish == 0; });
	}

	// and lock again to do the final resize
	std::unique_lock<std::recursive_mutex> lock(m_resize_mutex);

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
	std::shared_ptr<std::atomic<eAbort>> abort_ptr(m_abort[i]);

	auto f = [this, abort_ptr, i]()
	{
		// kSetupThreadSignalHandling() is done by kMakeThread() below - no
		// need to call it explicitly here.
		std::atomic<eAbort>& abort = *abort_ptr;
		std::packaged_task<void()> _f;
		Tag                        _tag { DefaultTag };  // work-class of the task in _f
		std::unique_lock<std::mutex> lock(m_cond_mutex);

		if (!m_sThreadName.empty())
		{
			// name this worker for debugging tools (the name is guarded by m_cond_mutex)
			kSetThreadName(kFormat("{}:{}", m_sThreadName, i));
		}

		bool bMoreTasks = sched_dequeue(_f, _tag);

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

				lock.lock();

				// account for the finished task - this may free a per-tag concurrency slot
				bool bWakeOther = sched_task_done(_tag);

				if (bWakeOther)
				{
					// a capped tag can run again - wake a waiting worker (it blocks on our
					// mutex until we release it). This must happen BEFORE the abort check:
					// a worker that exits below would swallow the wakeup for the freed
					// slot, and the capped tag's queued work could stall until the next
					// push (all other workers may be asleep, the shrink's notify_all came
					// while this tag was still at its cap)
					m_cond_var.notify_one();
				}

				if (abort != eAbort::None)
				{
					lock.unlock();

					notify_thread_shutdown(false, abort);

					return; // return even if the queue is not empty yet
				}

				bMoreTasks = sched_dequeue(_f, _tag);
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

			m_cond_var.wait(lock, [this, &_f, &_tag, &bMoreTasks, &abort]()
			{
				if (abort != eAbort::None)
				{
					return true;
				}

				bMoreTasks = sched_dequeue(_f, _tag);

				return bMoreTasks || ma_interrupt;
			});

			--ma_n_idle;

			if (!bMoreTasks)
			{
				// we stopped waiting because of a thread abort
				// unlock the cond mutex, the diagnostic output needs it, too
				lock.unlock();

				// our wakeup may have consumed a notify_one() that was meant to
				// announce a queued task - pass the baton on to another waiting
				// thread, which will then check the queue (a stray notification
				// is harmless, the wait predicate guards against it)
				m_cond_var.notify_one();

				notify_thread_shutdown(true, abort);

				return;
			}
		}
	};

	try
	{
		m_threads[i] = std::make_unique<std::thread>(kMakeThread(std::move(f)));
		return true;
	}
	catch (const std::exception& ex)
	{
		kException(ex);
	}

	return false;

} // run_thread

//-----------------------------------------------------------------------------
void KThreadPool::ConfigureTag(Tag tag, TagConfig config)
//-----------------------------------------------------------------------------
{
	if (config.iWeight == 0)
	{
		// enforce the documented minimum of 1 - a weight of 0 would let the DRR
		// deficit counter sink without bound (and dispatch like weight 1 anyway)
		config.iWeight = 1;
	}

	std::unique_lock<std::mutex> lock(m_cond_mutex);
	sched_tag(tag).Config = config;

} // ConfigureTag

//-----------------------------------------------------------------------------
KThreadPool::TagState& KThreadPool::sched_tag(Tag tag)
//-----------------------------------------------------------------------------
{
	auto it = m_Tags.find(tag);

	if (it != m_Tags.end())
	{
		return it->second;
	}

	// first time we see this tag: auto-register it with the default TagConfig (weight 1, unbounded,
	// no concurrency cap) and add it to the round-robin order. ConfigureTag() can override this later.
	// The cached state pointer stays valid: unordered_map references survive rehashing, and tags
	// are never erased.
	auto& State = m_Tags[tag];
	m_TagOrder.emplace_back(tag, &State);
	return State;

} // sched_tag

//-----------------------------------------------------------------------------
std::size_t KThreadPool::sched_enqueue(Tag tag, std::packaged_task<void()>&& task, bool& bAccepted,
                                       std::packaged_task<void()>& dropped)
//-----------------------------------------------------------------------------
{
	bAccepted = true;

	auto& State = sched_tag(tag);

	if (State.Config.iMaxQueue != 0 && State.Queue.size() >= State.Config.iMaxQueue)
	{
		if (State.Config.OverflowPolicy == Overflow::Reject)
		{
			// reject the new task - it is destroyed by the caller, breaking its future
			bAccepted = false;
			return m_iTotalQueued;
		}

		// DropOldest: drop this tag's oldest task (its future breaks), then enqueue the new
		// one. The dropped task is handed to the caller and destroyed only after m_cond_mutex
		// is released - the destructors of the task's captured user objects may touch this
		// pool again (e.g. push), which would deadlock under the mutex
		dropped = std::move(State.Queue.front());
		State.Queue.pop();
		--m_iTotalQueued;
	}

	State.Queue.push(std::move(task));
	++m_iTotalQueued;

	return m_iTotalQueued;

} // sched_enqueue

//-----------------------------------------------------------------------------
bool KThreadPool::sched_dequeue(std::packaged_task<void()>& task, Tag& tag)
//-----------------------------------------------------------------------------
{
	if (m_bQueuePaused || m_iTotalQueued == 0)
	{
		return false;
	}

	auto iTags = m_TagOrder.size();

	// fast path: a single tag (the common case, incl. untagged-only use) is a plain FIFO,
	// no DRR arithmetic
	if (iTags == 1)
	{
		auto& State = *m_TagOrder[0].second;

		if (State.Queue.empty())
		{
			return false;
		}

		if (State.Config.iMaxConcurrency != 0 && State.iRunning >= State.Config.iMaxConcurrency)
		{
			// the only tag is at its concurrency cap - nothing runnable right now
			return false;
		}

		task = std::move(State.Queue.front());
		State.Queue.pop();
		--m_iTotalQueued;
		++State.iRunning;
		tag = m_TagOrder[0].first;
		return true;
	}

	// weighted Deficit Round Robin across the tags - a flood in one tag cannot starve others,
	// and a tag at its concurrency cap is skipped (its slots are busy)
	for (std::size_t scanned = 0; scanned < iTags; ++scanned)
	{
		auto  iThisTag = m_TagOrder[m_iTagCursor].first;
		auto& State    = *m_TagOrder[m_iTagCursor].second;

		bool bServiceable = !State.Queue.empty()
		                 && (State.Config.iMaxConcurrency == 0 || State.iRunning < State.Config.iMaxConcurrency);

		if (!bServiceable)
		{
			// empty or at its concurrency cap: forfeit the deficit and move the turn on
			State.iDeficit = 0;
			m_iTagCursor   = (m_iTagCursor + 1) % iTags;
			continue;
		}

		if (State.iDeficit < 1)
		{
			// fresh visit to a serviceable tag: charge one quantum (its weight)
			State.iDeficit += State.Config.iWeight;
		}

		task = std::move(State.Queue.front());
		State.Queue.pop();
		--m_iTotalQueued;
		State.iDeficit -= 1;
		++State.iRunning;
		tag = iThisTag;

		bool bStillServiceable = !State.Queue.empty()
		                      && (State.Config.iMaxConcurrency == 0 || State.iRunning < State.Config.iMaxConcurrency);

		if (State.iDeficit < 1 || !bStillServiceable)
		{
			// quantum spent, tag drained, or tag hit its cap - hand the turn to the next tag
			m_iTagCursor = (m_iTagCursor + 1) % iTags;
		}

		return true;
	}

	return false;

} // sched_dequeue

//-----------------------------------------------------------------------------
bool KThreadPool::sched_task_done(Tag tag)
//-----------------------------------------------------------------------------
{
	auto it = m_Tags.find(tag);

	if (it == m_Tags.end())
	{
		return false;
	}

	auto& State = it->second;

	if (State.iRunning > 0)
	{
		--State.iRunning;
	}

	// a concurrency slot just freed - if this capped tag still has queued work that can now
	// run, a waiting worker should be woken to pick it up (the finishing worker may have moved
	// on to a different tag by DRR)
	return State.Config.iMaxConcurrency != 0
	    && !m_bQueuePaused
	    && !State.Queue.empty()
	    && State.iRunning < State.Config.iMaxConcurrency;

} // sched_task_done

//-----------------------------------------------------------------------------
void KThreadPool::bookkeep_enqueue(std::size_t iWaiting)
//-----------------------------------------------------------------------------
{
	if (iWaiting - 1 > ma_iMaxWaitingTasks)
	{
		ma_iMaxWaitingTasks = iWaiting - 1;
	}

	if (!ma_interrupt)
	{
		if (ma_n_idle == 0)
		{
			grow(iWaiting);
		}
		else if (iWaiting <= 1 && ++ma_iShrinkCounter % 25 == 0)
		{
			shrink(iWaiting);
		}
	}

} // bookkeep_enqueue

//-----------------------------------------------------------------------------
void KThreadPool::push_to_tag(std::packaged_task<void()> task, Tag tag)
//-----------------------------------------------------------------------------
{
	// a task evicted by DropOldest lives here so that it is destroyed only after the
	// unlock (the destructors of its captured user objects may touch this pool again)
	std::packaged_task<void()> dropped;

	std::unique_lock<std::mutex> lock(m_cond_mutex);

	bool bAccepted = true;
	auto iWaiting  = sched_enqueue(tag, std::move(task), bAccepted, dropped);

	if (!bAccepted)
	{
		// rejected (queue full) - the task is destroyed when this function returns (after
		// the unlock, it still sits in the task parameter), breaking its future; no wakeup
		// or growth needed
		return;
	}

	bookkeep_enqueue(iWaiting);

	lock.unlock();

	// notify in the unlocked state!
	m_cond_var.notify_one();

} // push_to_tag

//-----------------------------------------------------------------------------
void KThreadPool::push_packaged_task(std::packaged_task<void()> task)
//-----------------------------------------------------------------------------
{
	// untagged tasks run on the default work-class (weight 1, unbounded) - same as before
	push_to_tag(std::move(task), DefaultTag);

} // push_packaged_task

//-----------------------------------------------------------------------------
std::packaged_task<void()> KThreadPool::make_coalesce_task(Tag tag, uint64_t iKey)
//-----------------------------------------------------------------------------
{
	return std::packaged_task<void()>([this, tag, iKey]() { run_coalesce(tag, iKey); });

} // make_coalesce_task

//-----------------------------------------------------------------------------
std::shared_future<void> KThreadPool::push_coalesced_task(Tag tag, uint64_t iKey, std::function<void()> callable)
//-----------------------------------------------------------------------------
{
	std::shared_future<void>   future;
	bool                       bNotify { false };
	// both live until after the unlock: destroying user captures (the dropped task's, or an
	// abandoned generation's callable) must not happen under m_cond_mutex, and destroying an
	// abandoned generation's promise breaks the waiters' futures (broken_promise)
	CoalesceState              Abandoned;
	std::packaged_task<void()> dropped;

	{
		std::unique_lock<std::mutex> lock(m_cond_mutex);

		auto& State = sched_tag(tag);
		auto& CS    = State.Coalesce[iKey];

		// debounce: the latest callable wins for the next run
		CS.Callable = std::move(callable);

		if (!CS.Promise)
		{
			// new generation: one promise, one shared future that every submission of this
			// generation hands out (get_future() may only be called once on a promise)
			CS.Promise = std::make_shared<std::promise<void>>();
			CS.Shared  = CS.Promise->get_future().share();
		}

		future = CS.Shared;

		if (!CS.bRunning && !CS.bQueued)
		{
			// nothing pending for this key yet - schedule a run
			CS.bQueued = true;

			bool bAccepted = true;
			auto iWaiting  = sched_enqueue(tag, make_coalesce_task(tag, iKey), bAccepted, dropped);

			if (bAccepted)
			{
				bookkeep_enqueue(iWaiting);
				bNotify = true;
			}
			else
			{
				// the bounded tag rejected the run - no run will ever cover this generation,
				// so drop the key's state entirely: destroying its promise (after the unlock)
				// breaks the returned futures, matching PushTagged's reject behavior
				Abandoned = std::move(CS);
				State.Coalesce.erase(iKey);
			}
		}
		// else: a run is queued or in progress; this submission attached to CS.Promise and will
		// be served by that run (or, if it is running, by the rerun it triggers on completion)
	}

	if (bNotify)
	{
		m_cond_var.notify_one();
	}

	return future;

} // push_coalesced_task

//-----------------------------------------------------------------------------
void KThreadPool::run_coalesce(Tag tag, uint64_t iKey)
//-----------------------------------------------------------------------------
{
	std::function<void()>               callable;
	std::shared_ptr<std::promise<void>> promise;

	// claim the current callable + promise for this key
	{
		std::unique_lock<std::mutex> lock(m_cond_mutex);

		auto itTag = m_Tags.find(tag);
		if (itTag == m_Tags.end()) { return; }

		auto itKey = itTag->second.Coalesce.find(iKey);
		if (itKey == itTag->second.Coalesce.end()) { return; }

		auto& CS = itKey->second;
		callable = std::move(CS.Callable);
		promise  = CS.Promise;
		CS.Promise.reset();                          // submissions during the run start a fresh generation
		CS.Shared = std::shared_future<void>{};
		CS.bQueued  = false;
		CS.bRunning = true;
	}

	// run the user work unlocked, then settle the promise
	try
	{
		if (callable) { callable(); }
		if (promise)  { promise->set_value(); }
	}
	catch (const std::exception& ex)
	{
		kException(ex);
		if (promise) { promise->set_exception(std::current_exception()); }
	}
	catch (...)
	{
		kUnknownException();
		if (promise) { promise->set_exception(std::current_exception()); }
	}

	// completion: schedule exactly one more run if submissions arrived during this run
	bool bNotify { false };
	// both live until after the unlock - see push_coalesced_task for the rationale
	CoalesceState              Abandoned;
	std::packaged_task<void()> dropped;
	{
		std::unique_lock<std::mutex> lock(m_cond_mutex);

		auto itTag = m_Tags.find(tag);
		if (itTag == m_Tags.end()) { return; }

		auto itKey = itTag->second.Coalesce.find(iKey);
		if (itKey == itTag->second.Coalesce.end()) { return; }

		auto& CS = itKey->second;
		CS.bRunning = false;

		if (CS.Promise)
		{
			// triggers arrived during the run - re-run once
			CS.bQueued = true;
			bool bAccepted = true;
			sched_enqueue(tag, make_coalesce_task(tag, iKey), bAccepted, dropped);
			bNotify = bAccepted;

			if (!bAccepted)
			{
				// the bounded tag rejected the rerun - no run will ever cover this
				// generation, so drop the key's state; destroying its promise (after the
				// unlock) breaks the waiters' futures instead of leaving them pending forever
				Abandoned = std::move(CS);
				itTag->second.Coalesce.erase(itKey);
			}
		}
		else
		{
			// fully drained - drop the key's state
			itTag->second.Coalesce.erase(itKey);
		}
	}

	if (bNotify)
	{
		m_cond_var.notify_one();
	}

} // run_coalesce

//-----------------------------------------------------------------------------
KThreadPool::Diagnostics KThreadPool::get_diagnostics(bool bWasIdle) const
//-----------------------------------------------------------------------------
{
	Diagnostics Diag;

	Diag.iMaxThreadsEver  = ma_iMaxThreadsEver;
	Diag.iMaxThreads      = ma_iMaxThreads;
	auto iTotal           = size() + ma_iDetachedThreadsToFinish;
	auto iStopped         = ma_iAlreadyStopped.load();
	Diag.iTotalThreads    = (iTotal > iStopped) ? iTotal - iStopped : 0;
	Diag.iIdleThreads     = n_idle();
	Diag.iUsedThreads     = (Diag.iTotalThreads > Diag.iIdleThreads) ? Diag.iTotalThreads - Diag.iIdleThreads : 0;
	Diag.iTotalTasks      = ma_iTotalTasks;
	Diag.iMaxWaitingTasks = ma_iMaxWaitingTasks;
	Diag.iWaitingTasks    = n_queued();
	Diag.bWasIdle         = bWasIdle;

	return Diag;

} // get_diagnostics

//-----------------------------------------------------------------------------
void KThreadPool::notify_thread_shutdown(bool bWasIdle, eAbort abort)
//-----------------------------------------------------------------------------
{
	if (abort == eAbort::Stop)
	{
		++ma_iAlreadyStopped;
	}

	if (m_shutdown_callback)
	{
		m_shutdown_callback(get_diagnostics(bWasIdle));
	}

	// don't run this as an else of above - it has to happen past
	// the shutdown callback, and the former before..
	if (abort == eAbort::Resize)
	{
		// decrease the count of detached threads to wait for in the join()
		std::lock_guard<std::mutex> lock(m_detached_mutex);
		--ma_iDetachedThreadsToFinish;
		// notify while still holding the mutex: once the counter dropped to zero,
		// a stop() that has not yet entered the wait can pass its predicate without
		// ever sleeping, return, and let the pool be destroyed - a notify after the
		// unlock would then touch a destroyed condition variable
		m_detached_cv.notify_one();
	}

} // notify_thread_shutdown

DEKAF2_NAMESPACE_END
