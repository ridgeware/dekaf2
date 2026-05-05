/*
//
// DEKAF(tm): Lighter, Faster, Smarter (tm)
//
// Copyright (c) 2017-2021, Ridgeware, Inc.
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

/// @file kthreads.h
/// keep track of running threads

#include <dekaf2/threading/primitives/kthreadsafe.h>
#include <dekaf2/core/logging/klog.h>
#include <dekaf2/system/os/ksignals.h>
#include <thread>
#include <utility>
#include <functional>
#include <future>
#include <memory>
#include <vector>
#include <unordered_map>

DEKAF2_NAMESPACE_BEGIN

/// @addtogroup threading_execution
/// @{

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Base class for generated threads. Maintains a list of started threads
/// (and hence keeps them alive) and ensures they have joined at destruction.
class DEKAF2_PUBLIC KThreads
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
private:
//----------

	using Storage = std::unordered_map<std::thread::id, std::thread>;

	static constexpr std::size_t s_iDecayThreshold { 16 };

	KThreadSafe<Storage> m_Threads;
	KThreadSafe<std::vector<std::thread>> m_Decay;

//----------
protected:
//----------

	//-----------------------------------------------------------------------------
	/// add a new thread to the list of started threads while in unique lock
	std::thread::id AddLocked(KThreadSafe<Storage>::UniqueLocked& Threads, std::thread newThread);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// remove a thread from the list of started threads, offer option to warn if not found
	bool RemoveInt(std::thread::id ThreadID, bool bWarnIfFailed);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// wait for all non-decaying threads to join
	void JoinInt();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// wait for all decaying threads to join
	void DecayInt();
	//-----------------------------------------------------------------------------

//----------
public:
//----------

	//-----------------------------------------------------------------------------
	/// Destructor waits for all threads to join
	~KThreads()
	//-----------------------------------------------------------------------------
	{
		Join();
	}

	//-----------------------------------------------------------------------------
	/// add a new thread to the list of started threads
	std::thread::id Add(std::thread newThread);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// create one thread, calling function f with arguments args, removing thread from join list at destruction
	template<class Function, class... Args>
	std::thread::id Create(Function&& f, Args&&... args)
	//-----------------------------------------------------------------------------
	{
		// do the housekeeping only when enough threads have accumulated
		if (m_Decay.shared()->size() >= s_iDecayThreshold)
		{
			DecayInt();
		}

		// we use the detour with std::bind because otherwise (apple) clang errs
		// on some lambda parameters when forwarding f and args
		auto Callable = std::bind(std::forward<Function>(f), std::forward<Args>(args)...);

		// create the lock first, compilers do not have to follow left-to-right
		// argument evaluation
		auto Threads = m_Threads.unique();

		// finally create the lambda that will be executed in the new thread,
		// and store the thread in the map
		return AddLocked(Threads, std::thread([this, Func=std::move(Callable)]()
		{
			// set up per-thread signal mask and alternate signal stack for
			// the crash handler (sigaltstack is per-thread and not inherited)
			kSetupThreadSignalHandling();

			// call the callable
			DEKAF2_TRY
			{
				Func();
			}
			DEKAF2_CATCH (const std::exception& ex)
			{
				kException(ex);
			}

			// and remove this thread from the map
			RemoveInt(std::this_thread::get_id(), false);
		}));
	}

	//-----------------------------------------------------------------------------
	/// remove a thread from the list of started threads
	bool Remove(std::thread::id ThreadID)
	//-----------------------------------------------------------------------------
	{
		return RemoveInt(ThreadID, true);
	}

	//-----------------------------------------------------------------------------
	/// wait for all threads to join
	void Join();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// wait for one thread to join
	bool Join(std::thread::id ThreadID);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// return count of started threads
	DEKAF2_NODISCARD
	size_t size() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// return true if no started threads
	DEKAF2_NODISCARD
	bool empty() const;
	//-----------------------------------------------------------------------------

}; // KThreads

//-----------------------------------------------------------------------------
/// Drop-in replacement for std::thread() that, before invoking the user
/// callable, calls kSetupThreadSignalHandling() on the new thread. This
/// installs the per-thread signal mask and the alternate signal stack used
/// by the crash handler (SIGSEGV/SIGFPE/SIGILL/SIGBUS).
///
/// On platforms without a registered crash handler, kSetupThreadSignalHandling()
/// is a no-op, so this helper is safe to use everywhere.
///
/// In addition, uncaught std::exceptions that escape the callable are caught
/// and logged (same policy as KThreads::Create). std::thread() would otherwise
/// call std::terminate() for them, which tears down the whole process -
/// arguably a design wart in C++. The try/catch wrapper is zero-cost on
/// modern ABIs unless an exception is actually thrown.
///
/// Use this instead of raw std::thread() when starting threads from within
/// dekaf2 internals or from user code that wants the same crash protection
/// as KThreadPool/KThreads workers without paying for the bookkeeping of
/// KThreads.
///
/// Note: the signal handler thread itself MUST NOT use this (it has to keep
/// signals deliverable to call sigwait()).
template<class Function, class... Args>
std::thread kMakeThread(Function&& f, Args&&... args)
//-----------------------------------------------------------------------------
{
	// package up the callable + args via std::bind (C++11/14 compatible, and
	// works around an apple-clang issue with perfect-forwarding parameter
	// packs into a lambda - same workaround as in KThreads::Create above)
	auto Callable = std::bind(std::forward<Function>(f), std::forward<Args>(args)...);

	return std::thread([Func = std::move(Callable)]() mutable
	{
		// set up per-thread signal mask and alternate signal stack for the
		// crash handler (sigaltstack is per-thread and not inherited)
		kSetupThreadSignalHandling();

		// never let an uncaught exception tear down the whole process -
		// std::thread's default behavior (std::terminate) is hostile to
		// long-running server processes.
		DEKAF2_TRY
		{
			Func();
		}
		DEKAF2_CATCH (const std::exception& ex)
		{
			kException(ex);
		}
		DEKAF2_CATCH (...)
		{
			kUnknownException();
		}

	});

} // kMakeThread

//-----------------------------------------------------------------------------
/// Fire-and-forget variant of kMakeThread() that returns a std::future<R>
/// so that the parent thread can synchronize on task completion and pick up
/// any exception the callable threw.
///
/// Internally combines std::packaged_task with kMakeThread (so the worker
/// still gets the per-thread signal mask / alt stack setup). The started
/// thread is detached; the future is the only handle kept by the caller.
///
/// Exception semantics: if the callable throws a std::exception (or any
/// other exception type), std::packaged_task captures it into the future
/// and future.get() rethrows it in the parent thread. The try/catch in
/// kMakeThread does not trigger (packaged_task intercepts first) - which
/// is exactly what we want: the caller, not kException(), decides how to
/// handle the failure.
///
/// Usage:
/// @code
/// auto fut = kMakeAsync([]{ return compute(); });
/// // ... do other work on the main thread ...
/// auto result = fut.get();   // blocks until done; rethrows if worker threw
/// @endcode
///
/// If the returned future is dropped on the floor without .get() / .wait(),
/// the exception (if any) is silently swallowed. Callers that want logging
/// in the fire-and-forget case should use kMakeThread() directly.
template<class Function, class... Args>
auto kMakeAsync(Function&& f, Args&&... args)
//-----------------------------------------------------------------------------
	-> std::future<decltype(std::bind(std::forward<Function>(f), std::forward<Args>(args)...)())>
{
	// bind callable + args into a nullary thunk (same bind workaround as
	// kMakeThread for apple-clang perfect-forwarding quirks)
	auto Bound = std::bind(std::forward<Function>(f), std::forward<Args>(args)...);

	using Ret = decltype(Bound());

	// packaged_task is move-only but std::bind (inside kMakeThread) requires
	// a copyable callable. Wrap in shared_ptr so the capturing lambda stays
	// copyable while we retain access to the task from the caller side.
	auto Task = std::make_shared<std::packaged_task<Ret()>>(std::move(Bound));

	auto Fut = Task->get_future();

	kMakeThread([Task]()
	{
		(*Task)(); // captures exceptions into the future

	}).detach();

	return Fut;

} // kMakeAsync

/// @}

DEKAF2_NAMESPACE_END
