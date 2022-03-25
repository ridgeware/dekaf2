/*
//
// DEKAF(tm): Lighter, Faster, Smarter(tm)
//
// Copyright (c) 2022, Ridgeware, Inc.
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
//
*/

/// @file kron.h
/// cron string parsing and control

#pragma once

#include "kstringview.h"
#include "ktime.h"
#include "kthreadpool.h"
#include "kthreadsafe.h"
#include "bits/kunique_deleter.h"
#include <mutex>
#include <map>

namespace dekaf2 {

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC Kron
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	static constexpr time_t INVALID_TIME = -1;

	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	class DEKAF2_PUBLIC Job
	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{

	//----------
	public:
	//----------

		using ID = std::size_t;

		struct Control
		{
			std::time_t    tLastStarted   { INVALID_TIME };
			std::time_t    tLastStopped   { INVALID_TIME };
			std::size_t    iStartCount    { 0 };

		}; // Control

		Job() = default;

		Job(KStringView sCronDef,
			bool        bHasSeconds = true,
			uint16_t    iMaxQueued = 1);

		Job(std::time_t tOnce,
			KStringView sCommand);

		/// return next execution time after tAfter, in unix time_t
		std::time_t Next(std::time_t tAfter = 0) const;
		/// return next execution time after tAfter, in KUTCTime
		KUTCTime Next(const KUTCTime& tAfter) const;

		struct Control Control() const;

		/// return the command argument
		const KString& Command() const { return m_sCommand; }

		/// returns true when the Job is currently executed
		bool IsRunning() const { return m_iIsRunning > 0; }

		/// returns true when the Job is currently queued for execution
		bool IsQueued() const { return m_iIsQueued > 0; }

		/// returns the Job ID
		ID GetJobID() const { return m_iJobID; }

		/// runs the Job
		int Execute();
		/// runs the Job
		int operator()() { return Execute(); }

	//----------
	protected:
	//----------

		friend class Kron;

		bool IncQueued();
		void DecQueued();

		static KStringView CutOffCommand(KStringView& sCronDef, uint16_t iElements = 6);

	//----------
	private:
	//----------

		KString        m_sCommand;
		ID             m_iJobID     { 0 };
		uint16_t       m_iMaxQueued { 1 };

		uint16_t       m_iIsRunning { 0 };
		uint16_t       m_iIsQueued  { 0 };

		mutable std::mutex m_ExecMutex;

		KUniqueVoidPtr m_ParsedCron;
		std::time_t    m_tOnce      { INVALID_TIME };
		struct Control m_Control;

	}; // Kron::Job

	/// default ctor, does not start any threads and hence no Jobs
	Kron() = default;
	~Kron();

	/// ctor, takes the number of threads that will run the Jobs
	Kron(std::size_t iThreads)
	{
		Resize(iThreads);
	}

	/// set number of threads that will run the Jobs
	bool Resize(std::size_t iThreads);

	/// add a Job to list of jobs
	bool Add(std::shared_ptr<Job>& job);

	/// construct a new Job in place and add it to the list of jobs
	template<typename... Args>
	bool Embed(Args&&... args)
	{
		auto job = std::make_shared<Job>(std::forward<Args>(args)...);
		return Add(job);
	}

	/// delete a Job from list of jobs (by its job ID)
	bool Delete(Job::ID JobID);

//----------
protected:
//----------

	KThreadSafe<
		std::multimap<time_t, std::shared_ptr<Job>>
	>                             m_Jobs;            // the Job list, sorted by next execution time
	KThreadPool                   m_Tasks;           // the task queue
	std::unique_ptr<std::thread>  m_Chronos;         // the time keeper
	std::mutex                    m_ResizeMutex;
	bool                          m_bStop { true };  // syncronization with time keeper thread

}; // Kron

} // end of namespace dekaf2
