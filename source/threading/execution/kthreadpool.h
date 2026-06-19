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
 *  October 2025, Joachim Schurig
 *   - adding growth and shrink policy setup, to dynamically resize
 *     the thread pool depending on usage
 *   - adding iMaxThreadsEver to Diagnostics
 *
 *  March 2026, Joachim Schurig
 *   - fixing potential deadlock when using resize()
 *   - fixing inefficiency on shrink logic
 *   - fixing possible underflow in diagnostics values
 *   - fixing inefficient wait for detached threads to finish
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

 *********************************************************/


#pragma once

#include <dekaf2/core/init/kdefinitions.h> // for DEKAF2_NAMESPACE, DEKAF2_NODISCARD and DEKAF2_PUBLIC
#include <functional>
#include <thread>
#include <atomic>
#include <vector>
#include <memory>
#include <future>
#include <mutex>
#include <queue>
#include <chrono>
#include <unordered_map>
#include <cstdint>

/// @file kthreadpool.h
/// thread pool to run user's tasks (all types of callables) with signature
/// ret func(params, ...).
/// Besides the plain FIFO operation, the pool offers opt-in fair scheduling: tasks can be
/// pushed under a work-class Tag and are dispatched across tags by weighted Deficit Round
/// Robin, with optional per-tag queue bounds, a per-tag concurrency cap, and coalescing.
/// See the KThreadPool class documentation for the operating modes and examples.

DEKAF2_NAMESPACE_BEGIN

/// @addtogroup threading_execution
/// @{

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// ThreadPool implementation in form of a task queue - you define the number of concurrent
/// threads, and push tasks which the next free thread picks up. The pool grows and shrinks
/// between 0 and the configured size on demand (see GrowthPolicy / ShrinkPolicy).
///
/// ### Plain use (FIFO)
/// push() any callable (free function, lambda, functor, or a member function with its object)
/// and receive a std::future for the result. This is the default and behaves as one FIFO queue.
/// @code
/// KThreadPool Pool(8);        // 8 worker threads (0 = hardware concurrency)
/// auto future = Pool.push([](int a, int b){ return a + b; }, 2, 3);
/// int  iSum   = future.get(); // 5
/// @endcode
///
/// ### Fair scheduling with tags (opt-in)
/// Tasks can be pushed under a work-class Tag with PushTagged(). Each tag has its own sub-queue
/// and the workers dispatch across tags by weighted Deficit Round Robin (DRR), so a flood in one
/// tag cannot starve the others. A tag's weight (its relative dispatch share per round) is set with
/// ConfigureTag(); calling it is optional - a tag pushed to without it is auto-registered on first
/// use with the default TagConfig (weight 1, unbounded, no concurrency cap). Untagged push() uses
/// DefaultTag (likewise weight 1) and simply participates as one more class; if no tag is ever
/// configured the pool stays a plain FIFO (single-tag fast path).
/// @note Tags are meant to be a small, fixed set of work CLASSES (e.g. requests, background,
/// monitoring). Never use a distinct tag per request/connection - the tag set is never pruned.
/// (Coalesce keys, below, may be per-entity - they are dropped when drained.)
/// @code
/// constexpr KThreadPool::Tag Requests = 1, Monitoring = 2;
/// KThreadPool::TagConfig Req;  Req.iWeight = 9; // requests get ~9x the dispatch share ...
/// KThreadPool::TagConfig Mon;  Mon.iWeight = 1; // ... of monitoring
/// Pool.ConfigureTag(Requests,   Req);
/// Pool.ConfigureTag(Monitoring, Mon);
/// Pool.PushTagged(Requests,   [&]{ HandleRequest(); });
/// Pool.PushTagged(Monitoring, [&]{ Probe(); }); // a monitoring storm can't starve requests
/// @endcode
///
/// ### Backpressure (bounded sub-queue)
/// A tag may bound its queue depth; on overflow it either rejects the new task (its future then
/// completes broken, std::future_error / broken_promise) or drops the oldest queued task. This sheds
/// a bursty or untrusted producer instead of letting the queue grow without limit.
/// @code
/// KThreadPool::TagConfig Mon;
/// Mon.iMaxQueue      = 100;
/// Mon.OverflowPolicy = KThreadPool::Overflow::Reject;    // when full, drop the new task -> broken future
/// Pool.ConfigureTag(Monitoring, Mon);
/// @endcode
///
/// ### Concurrency cap (isolate blocking work)
/// A tag may cap how many of its tasks run at once, so a class of blocking/slow work cannot occupy
/// every worker and starve the other classes - one pool with per-class limits instead of several pools.
/// @code
/// KThreadPool::TagConfig IO;
/// IO.iMaxConcurrency = 4; // at most 4 of these run at once, the rest wait
/// Pool.ConfigureTag(SlowIO, IO);
/// @endcode
///
/// ### Coalescing (debounce-rerun)
/// PushCoalesced(tag, key, ...) collapses repeated submissions for the same key: while a run for the
/// key is queued or in progress, further submissions fold into a single follow-up run (latest callable
/// wins; a trigger arriving during a run causes exactly one rerun, so the last change is never missed).
/// Use it for idempotent "refresh / recompute X" work where running duplicates is pointless.
/// @code
/// // many triggers for the same resource collapse into at most one in-flight + one pending run
/// auto shared = Pool.PushCoalesced(Refresh, iResourceId, [&]{ RecomputeDerivedState(iResourceId); });
/// shared.get(); // shared_future: completes when the covering run is done
/// @endcode
class DEKAF2_PUBLIC KThreadPool
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	enum GrowthPolicy
	{
		PrestartNone      = 0, ///< do never preemptively start threads
		PrestartOne       = 1, ///< start one idle thread on top of the busy ones
		PrestartSome      = 2, ///< start ten idle threads on top of the busy ones
		PrestartAll       = 3, ///< start all threads immediately
	};

	enum ShrinkPolicy
	{
		ShrinkNever       = 0, ///< do not shrink
		ShrinkSome        = 1, ///< shrink in blocks of 10
		ShrinkOne         = 2, ///< shrink and leave one idle thread
		ShrinkImmediately = 3, ///< shrink and leave zero idle threads
	};

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
	/// threads as CPU threads are available
	KThreadPool(std::size_t nThreads, GrowthPolicy Growth = PrestartSome, ShrinkPolicy Shrink = ShrinkSome);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// The destructor waits for all the functions in the queue to be finished
	~KThreadPool();
	//-----------------------------------------------------------------------------

	struct Diagnostics
	{
		std::size_t iMaxThreadsEver  { 0 }; ///< max number of threads that were allowed at any time
		std::size_t iMaxThreads      { 0 }; ///< max number of threads allowed currently
		std::size_t iTotalThreads    { 0 }; ///< total number of threads spawned now
		std::size_t iIdleThreads     { 0 }; ///< number of idle threads
		std::size_t iUsedThreads     { 0 }; ///< number of used threads
		std::size_t iTotalTasks      { 0 }; ///< total number of serviced tasks
		std::size_t iMaxWaitingTasks { 0 }; ///< max size of wait queue since last resize
		std::size_t iWaitingTasks    { 0 }; ///< current number of tasks in wait queue
		bool        bWasIdle         { false };

	}; // Diagnostics

	//-----------------------------------------------------------------------------
	/// Query current threadpool status - ignore bWasIdle param
	DEKAF2_NODISCARD
	Diagnostics get_diagnostics(bool bWasIdle = false) const;
	//-----------------------------------------------------------------------------

	using ShutdownCallback = std::function<void(Diagnostics)>;

	//-----------------------------------------------------------------------------
	/// Shall we log the shutdown?
	/// Call this before the pool is started (or resized) - the callback is read by the
	/// worker threads without synchronization, so registering it on a running pool is a
	/// data race.
	/// @param callback callback function called at each shutdown thread with some diagnostics
	void register_shutdown_callback(ShutdownCallback callback)
	//-----------------------------------------------------------------------------
	{
		m_shutdown_callback = callback;
	}

	//-----------------------------------------------------------------------------
	/// Get the total number of threads in the pool
	DEKAF2_NODISCARD
	std::size_t size() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Get the number of idle threads
	DEKAF2_NODISCARD
	std::size_t n_idle() const
	//-----------------------------------------------------------------------------
	{
		return ma_n_idle;
	}

	//-----------------------------------------------------------------------------
	/// Get the number of tasks waiting in the queue
	DEKAF2_NODISCARD
	std::size_t n_queued() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Restart the pool - this method blocks until all existing tasks have been serviced
	/// @return false if some threads could not be restarted - check new size with size() then
	bool restart();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Pause or un-pause the pool (this is not the counterpart to restart())
	void pause(bool bYesNo);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Change the number of threads in the pool - this method does not block
	/// @param nThreads number of total threads
	/// @return false if some threads could not be started - check new size with size() then
	bool resize(std::size_t nThreads, GrowthPolicy Growth = PrestartAll, ShrinkPolicy Shrink = ShrinkNever);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// set the strategies for growing and shrinking the pool
	void set_strategy(GrowthPolicy Growth, ShrinkPolicy Shrink);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Clear all pending tasks in the task queue - does not stop running tasks
	void clear();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Stops all threads. Returns only after all threads are stopped.
	/// @param bKill if false, all the tasks in the queue are run, otherwise the threads are stopped without
	/// running the outstanding tasks (but the tasks stay in the queue for an eventual later restart)
	void stop(bool bKill = false);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// @return true if all threads are stopped, else false
	DEKAF2_NODISCARD
	bool is_stopped();
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

	// Fair scheduling: tasks can be pushed under a Tag (work-class). Each tag
	// gets its own sub-queue; the pool dispatches across tags by weighted Deficit Round
	// Robin, so a flood in one tag cannot starve the others. A tag may also bound its
	// queue (backpressure for bursty producers). Untagged push() uses DefaultTag (weight
	// 1, unbounded) and behaves exactly as before.

	/// a work-class identifier; 0 (DefaultTag) is used by the untagged push()
	using Tag = std::size_t;
	/// the default work-class used by untagged push() - weight 1, unbounded
	static constexpr Tag DefaultTag = 0;

	/// policy when a tag's bounded queue is full
	enum class Overflow
	{
		Reject,     ///< reject the new task (its future becomes broken_promise)
		DropOldest  ///< drop the oldest queued task of that tag (its future breaks), enqueue the new one
	};

	/// per work-class policy
	struct TagConfig
	{
		uint32_t    iWeight         { 1 };                 ///< relative DRR dispatch share per round (>= 1)
		std::size_t iMaxQueue       { 0 };                 ///< max queued tasks for this tag, 0 = unbounded
		std::size_t iMaxConcurrency { 0 };                 ///< max tasks of this tag running at once, 0 = unbounded
		Overflow    OverflowPolicy  { Overflow::Reject };  ///< what to do when iMaxQueue is exceeded

	}; // TagConfig

	//-----------------------------------------------------------------------------
	/// Register or replace the policy for a work-class tag. This is OPTIONAL: a tag used without a
	/// prior ConfigureTag() (and likewise DefaultTag) is auto-registered on first use - by push,
	/// PushTagged, or PushCoalesced - with the default TagConfig (weight 1, unbounded queue, no
	/// concurrency cap). Call this only to give a tag a non-default weight, queue bound or cap;
	/// calling it later updates the existing tag's policy.
	void ConfigureTag(Tag tag, TagConfig config);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Push a callable under a work-class tag and receive a future for the result. Mirrors push():
	/// it accepts free functions, lambdas and functors, as well as a class member function with its
	/// object (PushTagged(tag, &Class::Method, &obj, args...)). If the tag's queue is full and the
	/// policy rejects, the task is dropped and the future becomes broken (future::get() throws
	/// std::future_error / broken_promise). For void callables a single packaged_task is built (no
	/// extra wrapping); non-void callables wrap an inner packaged_task<R()> to carry the result.
	/// member function, void return - one packaged_task, no extra wrapping
	template<typename Function, typename Object, typename... Args>
	auto PushTagged(Tag tag, Function&& f, Object&& o, Args&&... args)
	-> typename std::enable_if<std::is_same<decltype((o->*f)(std::forward<Args>(args)...)), void>::value,
	                             std::future<void>>::type
	//-----------------------------------------------------------------------------
	{
		auto task = std::packaged_task<void()>(
			std::bind(std::forward<Function>(f), std::forward<Object>(o), std::forward<Args>(args)...)
		);

		auto future = task.get_future();

		push_to_tag(std::move(task), tag);

		return future;
	}

	//-----------------------------------------------------------------------------
	/// member function, non-void return
	template<typename Function, typename Object, typename... Args>
	auto PushTagged(Tag tag, Function&& f, Object&& o, Args&&... args)
	-> typename std::enable_if<!std::is_same<decltype((o->*f)(std::forward<Args>(args)...)), void>::value,
	                             std::future<decltype((o->*f)(std::forward<Args>(args)...))>>::type
	//-----------------------------------------------------------------------------
	{
		using FutureType = decltype((o->*f)(std::forward<Args>(args)...));

		auto InnerTask = std::packaged_task<FutureType()>(
			std::bind(std::forward<Function>(f), std::forward<Object>(o), std::forward<Args>(args)...)
		);

		auto future = InnerTask.get_future();

		auto task = std::packaged_task<void()>(std::move(InnerTask));

		push_to_tag(std::move(task), tag);

		return future;
	}

	//-----------------------------------------------------------------------------
	/// non-class callable, void return - one packaged_task, no extra wrapping
	template<typename Function, typename... Args>
	auto PushTagged(Tag tag, Function&& f, Args&&... args)
	-> typename std::enable_if<std::is_same<decltype(f(std::forward<Args>(args)...)), void>::value,
	                             std::future<void>>::type
	//-----------------------------------------------------------------------------
	{
		auto task = std::packaged_task<void()>(
			std::bind(std::forward<Function>(f), std::forward<Args>(args)...)
		);

		auto future = task.get_future();

		push_to_tag(std::move(task), tag);

		return future;
	}

	//-----------------------------------------------------------------------------
	/// non-class callable, non-void return
	template<typename Function, typename... Args>
	auto PushTagged(Tag tag, Function&& f, Args&&... args)
	-> typename std::enable_if<!std::is_same<decltype(f(std::forward<Args>(args)...)), void>::value,
	                             std::future<decltype(f(std::forward<Args>(args)...))>>::type
	//-----------------------------------------------------------------------------
	{
		using FutureType = decltype(f(std::forward<Args>(args)...));

		auto InnerTask = std::packaged_task<FutureType()>(
			std::bind(std::forward<Function>(f), std::forward<Args>(args)...)
		);

		auto future = InnerTask.get_future();

		auto task = std::packaged_task<void()>(std::move(InnerTask));

		push_to_tag(std::move(task), tag);

		return future;
	}

	//-----------------------------------------------------------------------------
	/// Push coalesced work under a (tag, key): if work for the same (tag, key) is already
	/// queued or running, this submission collapses into a single follow-up run (debounce) -
	/// the latest callable wins, overlapping/duplicate runs are avoided, and a trigger that
	/// arrives while a run is in progress causes exactly one more run afterwards (so the last
	/// state change is never missed). Ideal for idempotent "refresh/recompute X" work.
	/// Accepts free functions, lambdas and functors, as well as a class member function with its
	/// object (PushCoalesced(tag, key, &Class::Method, &obj, args...)).
	/// @returns a shared_future<void> that completes when the run covering this submission
	/// finishes (it is shared because several submissions may collapse into one run; the
	/// callable's own return value, if any, is discarded).
	template<typename Function, typename... Args>
	std::shared_future<void> PushCoalesced(Tag tag, uint64_t iKey, Function&& f, Args&&... args)
	//-----------------------------------------------------------------------------
	{
		return push_coalesced_task(tag, iKey,
		           std::bind(std::forward<Function>(f), std::forward<Args>(args)...));
	}

//------
private:
//------

	enum eAbort
	{
		None,
		Resize,
		Stop
	};

	void push_packaged_task(std::packaged_task<void()> task);
	std::size_t calc_shrink(std::size_t iHaveIdle) const;

	// --- fair scheduler (all sched_* assume m_cond_mutex is held by the caller) ---

	// per coalesce key: the latest callable, the promise pending futures attach to, and whether
	// a run is currently queued/executing for the key (see PushCoalesced for the semantics)
	struct CoalesceState
	{
		std::function<void()>               Callable;            ///< latest callable for the next run (debounce: latest wins)
		std::shared_ptr<std::promise<void>> Promise;             ///< promise the next run fulfills
		std::shared_future<void>            Shared;              ///< future shared by every submission of the current generation
		bool                                bRunning { false };  ///< a run for this key is executing
		bool                                bQueued  { false };  ///< a run for this key is queued
	};

	struct TagState
	{
		TagConfig                                    Config;
		std::queue<std::packaged_task<void()>>       Queue;
		std::unordered_map<uint64_t, CoalesceState>  Coalesce;        ///< active coalesce keys for this tag
		std::int64_t                                 iDeficit { 0 };  ///< DRR deficit counter
		std::size_t                                  iRunning { 0 };  ///< tasks of this tag currently executing
	};

	/// get (creating with default config if unknown) the state for a tag
	TagState&   sched_tag    (Tag tag);
	/// enqueue under a tag respecting its bound/overflow; sets bAccepted, returns total queued
	std::size_t sched_enqueue(Tag tag, std::packaged_task<void()>&& task, bool& bAccepted);
	/// dequeue the next task by weighted DRR honoring per-tag concurrency caps; on success sets
	/// task and tag (and bumps that tag's running count); returns false if nothing runnable
	bool        sched_dequeue(std::packaged_task<void()>& task, Tag& tag);
	/// account for a finished task of a tag (decrements its running count); returns true if a
	/// concurrency slot was freed for a tag that still has queued work (a waiting worker should be woken)
	bool        sched_task_done(Tag tag);
	/// enqueue a task under a tag and wake/grow as needed (the public push paths funnel here)
	void        push_to_tag  (std::packaged_task<void()> task, Tag tag);

	/// implementation of PushCoalesced (callable already bound to its args)
	std::shared_future<void>   push_coalesced_task(Tag tag, uint64_t iKey, std::function<void()> callable);
	/// build the trampoline task that runs the current callable for a coalesce key
	std::packaged_task<void()> make_coalesce_task (Tag tag, uint64_t iKey);
	/// trampoline body: claim the latest callable+promise for the key, run it, and re-run once
	/// more if triggers arrived during the run
	void                       run_coalesce       (Tag tag, uint64_t iKey);

	/// try to grow the pool by the chosen growth strategy - call if there are no idle threads, but queued tasks (and pass the count of tasks in iQueued).
	/// Must be called with m_cond_mutex held (lock order: m_cond_mutex before m_resize_mutex)
	bool grow(std::size_t iQueued);

	/// try to shrink the pool by the chosen growth strategy - call if there are idle threads, but no queued tasks.
	/// Must be called with m_cond_mutex held: shrink() sets thread abort flags that the workers'
	/// wait predicate reads under m_cond_mutex - writing them under the same mutex prevents a
	/// lost wakeup (lock order: m_cond_mutex before m_resize_mutex)
	bool shrink(std::size_t iQueued);

	/// start one thread
	/// @return false if thread could not be started
	DEKAF2_PRIVATE
	bool run_thread( size_t i );

	DEKAF2_PRIVATE
	void notify_thread_shutdown(bool bWasIdle, eAbort abort);

	std::vector<std::unique_ptr<std::thread>>             m_threads;
	std::vector<std::shared_ptr<std::atomic<eAbort>>>     m_abort;
	std::unordered_map<Tag, TagState>                     m_Tags;          ///< per work-class state (DefaultTag created lazily)
	std::vector<Tag>                                      m_TagOrder;      ///< stable round-robin order of tags
	std::size_t                                           m_iTagCursor   { 0 };  ///< DRR cursor into m_TagOrder
	std::size_t                                           m_iTotalQueued { 0 };  ///< total tasks queued across all tags
	bool                                                  m_bQueuePaused { false };

	std::atomic<std::size_t>     ma_iMaxThreadsEver          { 0 };
	std::atomic<std::size_t>     ma_iMaxThreads              { 0 };
	std::atomic<std::size_t>     ma_iTotalTasks              { 0 };
	std::atomic<std::size_t>     ma_iMaxWaitingTasks         { 0 };
	std::atomic<std::size_t>     ma_n_idle                   { 0 };
	std::atomic<std::size_t>     ma_iAlreadyStopped          { 0 };
	std::atomic<std::size_t>     ma_iDetachedThreadsToFinish { 0 };
	std::atomic<std::size_t>     ma_iShrinkCounter           { 0 };
	std::atomic<bool>            ma_interrupt                { false };

	mutable std::recursive_mutex m_resize_mutex;
	mutable std::mutex           m_cond_mutex;
	std::condition_variable      m_cond_var;
	mutable std::mutex           m_detached_mutex;
	std::condition_variable      m_detached_cv;
	std::chrono::steady_clock::time_point
	                             m_LastResize;
	ShutdownCallback             m_shutdown_callback;
	GrowthPolicy                 m_Growth                    { GrowthPolicy::PrestartSome };
	ShrinkPolicy                 m_Shrink                    { ShrinkPolicy::ShrinkSome   };

}; // KThreadPool


/// @}

DEKAF2_NAMESPACE_END
