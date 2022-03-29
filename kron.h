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
#include "kduration.h"
#include "kthreadpool.h"
#include "kthreadsafe.h"
#include "kjson.h"
#include "kassociative.h"
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

	class Job;
	using SharedJob = std::shared_ptr<Job>;

	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// A Kron::Job - extensible Job control
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
			std::size_t    iStartCount    {  0 };
			int            iLastResult    {  0 };
			pid_t          ProcessID      { -1 };
			KStopDuration  Duration;
			KString        sLastResult;

		}; // Control

		Job() = default;

		Job(KString      sName,
			KStringView  sCronDef,
			bool         bHasSeconds = true,
			uint16_t     iMaxQueued = 1);

		Job(KString      sName,
			std::time_t  tOnce,
			KStringView  sCommand);

		Job(const KJSON& jConfig);

		virtual ~Job();

		void SetEnvironment(std::vector<std::pair<KString, KString>> Environment) { m_Environment = std::move(Environment); }

		/// return next execution time after tAfter, in unix time_t
		std::time_t Next(std::time_t tAfter = 0) const;
		/// return next execution time after tAfter, in KUTCTime
		KUTCTime Next(const KUTCTime& tAfter) const;

		/// return a copy of the timers and results
		struct Control Control() const;

		/// return the command argument
		const KString& Command() const { return m_sCommand; }

		/// returns true when the Job is currently executed
		bool IsRunning() const { return m_iIsRunning > 0; }

		/// returns true when the Job is currently queued for execution
		bool IsQueued() const { return m_iIsQueued > 0; }

		/// returns the Job ID
		ID JobID() const { return m_iJobID; }

		/// runs the Job - virtual, to allow extensions
		virtual int Execute();
		/// runs the Job
		virtual int operator()() { return Execute(); }

		/// print a json description of the job
		/// @param iMaxResultSize the output of the executed command will be cut off after this count
		/// @return a KJSON structure with information about the job and its last execution
		KJSON Print(std::size_t iMaxResultSize = 4096) const;

		/// return the job name, may be empty
		const KString& Name() const { return m_sName; }

		/// kill the job, will return false on Windows (and hence not work there)
		bool Kill();

		/// static, create a SharedJob
		template<typename... Args>
		static SharedJob Create(Args&&... args)
		{
			return std::make_shared<Job>(std::forward<Args>(args)...);
		}

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

		KString        m_sName;
		KString        m_sCommand;
		ID             m_iJobID     { 0 };
		uint16_t       m_iMaxQueued { 1 };
		uint16_t       m_iIsRunning { 0 };
		uint16_t       m_iIsQueued  { 0 };

		mutable std::mutex m_ExecMutex;

		KUniqueVoidPtr m_ParsedCron;
		std::time_t    m_tOnce      { INVALID_TIME };
		struct Control m_Control;
		std::vector<
			std::pair<KString, KString>
		>              m_Environment;

	}; // Kron::Job

	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// Scheduler base class
	class DEKAF2_PUBLIC Scheduler
	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{

	//----------
	public:
	//----------

		virtual ~Scheduler();

		/// add a Job to list of jobs - will fail if job (identified by its name) already exists
		virtual bool AddJob(const SharedJob& Job);
		/// delete a Job from list of jobs (by its job ID)
		virtual bool DeleteJob(Job::ID JobID);
		/// modify a Job - in the base class implemented as delete + add
		virtual bool ModifyJob(const SharedJob& Job);
		/// list all jobs
		virtual KJSON ListJobs() const;
		/// return number of jobs
		virtual std::size_t size() const;
		/// any jobs?
		virtual bool empty() const;
		/// parse a buffer as if it were a unix crontab, and generate jobs, and add them to the list of jobs
		/// @param sCrontab the buffer with a unix crontab
		/// @param bHasSeconds set to true if the crontab format includes also seconds as the first field.
		/// Defaults to false
		/// @return count of added jobs
		virtual std::size_t AddJobsFromCrontab(KStringView sCrontab, bool bHasSeconds = false);

	//----------
	protected:
	//----------

		friend class Kron;

		/// get next Job to execute, can return with nullptr
		virtual SharedJob GetJob(std::time_t tNow);
		/// a Job has been run and is returned to the scheduler
		virtual void JobFinished(const SharedJob& Job);


	}; // Kron::Scheduler

	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// Local, in-memory scheduler implementation
	class DEKAF2_PUBLIC LocalScheduler : public Scheduler
	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{

	//----------
	public:
	//----------

		/// add a Job to list of jobs
		virtual bool AddJob(const SharedJob& Job) override final;
		/// delete a Job from list of jobs (by its job ID)
		virtual bool DeleteJob(Job::ID JobID) override final;
		/// list all jobs
		virtual KJSON ListJobs() const override final;
		/// return number of jobs
		virtual std::size_t size() const override final;
		/// any jobs?
		virtual bool empty() const override final;

	//----------
	protected:
	//----------

		/// get next Job to execute, can return with nullptr
		virtual SharedJob GetJob(std::time_t tNow) override final;

	//----------
	private:
	//----------

		KThreadSafe<
			KMultiMap<time_t, SharedJob>
		> m_Jobs;  // the Job list, sorted by next execution time

	}; // Kron::LocalScheduler

	using SharedScheduler = std::shared_ptr<Scheduler>;

	/// ctor, takes the number of threads that will run the Jobs (default none), and a Scheduler (defaults to a LocalScheduler)
	Kron(std::size_t iThreads = 0,
		 SharedScheduler Scheduler = std::make_shared<LocalScheduler>());
	~Kron();

	/// set number of threads that will run the Jobs. If > 0 starts the Scheduler
	bool Resize(std::size_t iThreads);

	/// return the Scheduler object
	Scheduler& Scheduler() { return *m_Scheduler; }

//----------
protected:
//----------

	void Launcher();

	SharedScheduler               m_Scheduler;
	KThreadPool                   m_Tasks;           // the task queue
	std::unique_ptr<std::thread>  m_Chronos;         // the time keeper
	std::mutex                    m_ResizeMutex;
	bool                          m_bStop { true };  // syncronization with time keeper thread

}; // Kron

} // end of namespace dekaf2
