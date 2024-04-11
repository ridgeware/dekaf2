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
#include "kinshell.h"
#include "bits/kunique_deleter.h"
#include <mutex>
#include <map>
#include <vector>

DEKAF2_NAMESPACE_BEGIN

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC Kron
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	static constexpr KUnixTime INVALID_TIME = KUnixTime(-1);

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
			KUnixTime      tLastStarted   { INVALID_TIME }; ///< last start time
			KUnixTime      tLastStopped   { INVALID_TIME }; ///< last stop time
			std::size_t    iStartCount    {  0 };           ///< total count of executions
			int            iLastStatus    {  0 };           ///< last status return code
			pid_t          ProcessID      { -1 };           ///< process ID of the running job
			KStopDuration  ExecutionTime;                   ///< total execution time
			KString        sLastOutput;                     ///< last output of command (if any)

		}; // Control

		Job() = default;

		Job(KString      sName,
			KStringView  sCronDef,
			bool         bHasSeconds = true,
			KDuration    MaxExecutionTime = KDuration::max());

		Job(KString      sName,
			KUnixTime    tOnce,
			KStringView  sCommand,
			KDuration    MaxExecutionTime = KDuration::max());

		Job(const KJSON& jConfig);

		virtual ~Job();

		void SetEnvironment(std::vector<std::pair<KString, KString>> Environment) { m_Environment = std::move(Environment); }

		/// return next execution time after tAfter, in unix time_t
		DEKAF2_NODISCARD
		KUnixTime Next(KUnixTime tAfter = KUnixTime(0)) const;

		/// return next execution time after tAfter, in KUTCTime
		DEKAF2_NODISCARD
		KUTCTime Next(const KUTCTime& tAfter) const;

		/// return a copy of the timers and results
		DEKAF2_NODISCARD
		struct Control Control() const;

		/// return the command argument
		DEKAF2_NODISCARD
		const KString& Command() const { return m_sCommand; }

		/// returns true when the Job is currently executed
		DEKAF2_NODISCARD
		bool IsRunning() const { return m_Shell != nullptr; }

		/// returns true when the executing shell/pipe has finished and is waiting to be closed
		DEKAF2_NODISCARD
		bool IsWaiting();

		/// returns the Job ID
		DEKAF2_NODISCARD
		ID JobID() const { return m_iJobID; }

		/// runs the Job - virtual, to allow extensions
		virtual bool Start();
		/// runs the Job
		virtual bool operator()() { return Start(); }

		/// Waits up to the number of given milliseconds for the Job to terminate, returns status code
		virtual int Wait(int msecs);

		/// print a json description of the job
		/// @param iMaxResultSize the output of the executed command will be cut off after this count
		/// @return a KJSON structure with information about the job and its last execution
		DEKAF2_NODISCARD
		KJSON Print(std::size_t iMaxResultSize = 4096) const;

		/// return the job name, may be empty
		DEKAF2_NODISCARD
		const KString& Name() const { return m_sName; }

		/// kill the job, will return false on Windows (and hence not work there)
		bool Kill();

		/// returns true if the job is running longer than permitted, and will terminate it in that case
		bool KillIfOverdue();

		/// static, create a SharedJob
		template<typename... Args>
		DEKAF2_NODISCARD
		static SharedJob Create(Args&&... args)
		{
			return std::make_shared<Job>(std::forward<Args>(args)...);
		}

	//----------
	protected:
	//----------

		friend class Kron;

		static KStringView CutOffCommand(KStringView& sCronDef, uint16_t iElements = 6);

	//----------
	private:
	//----------

		KString                   m_sName;
		KString                   m_sCommand;
		KDuration                 m_MaxExecutionTime { KDuration::max() };
		ID                        m_iJobID           { 0 };
		KUnixTime                 m_tOnce            { INVALID_TIME };
		KUniqueVoidPtr            m_ParsedCron;
		struct Control            m_Control;
		std::vector<
			std::pair<KString, KString>
		>                         m_Environment;
		std::unique_ptr<KInShell> m_Shell;
		mutable std::shared_mutex m_ExecMutex;

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
		virtual SharedJob GetJob(KUnixTime tNow);
		/// a Job has been run and is returned to the scheduler
		virtual void JobFinished(const SharedJob& Job);
		/// a Job failed to run and is returned to the scheduler
		virtual void JobFailed(const SharedJob& Job, KStringView sError);


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
		virtual SharedJob GetJob(KUnixTime tNow) override final;

	//----------
	private:
	//----------

		KThreadSafe<
			KMultiMap<KUnixTime, SharedJob>
		> m_Jobs;  // the Job list, sorted by next execution time

	}; // Kron::LocalScheduler

	using SharedScheduler = std::shared_ptr<Scheduler>;

	/// ctor, takes a Scheduler (defaults to a LocalScheduler)
	Kron(bool bAllowLaunches       = true,
		 KDuration CheckEvery      = std::chrono::seconds(1),
		 SharedScheduler Scheduler = std::make_shared<LocalScheduler>());
	~Kron();

	/// return the Scheduler object
	Scheduler& Scheduler() { return *m_Scheduler; }

	DEKAF2_NODISCARD
	KJSON ListRunningJobs(Job::ID filterForID = 0) const;

	bool KillJob(Job::ID ID);

//----------
protected:
//----------

	std::size_t StartNewJobs      ();
	std::size_t FinishWaitingJobs ();
	void        Launcher          (KDuration CheckEvery);

	SharedScheduler               m_Scheduler;
	KThreadSafe<
		std::vector<SharedJob>
	>                             m_RunningJobs;     // the currently running jobs
	std::unique_ptr<std::thread>  m_Chronos;         // the time keeper
	std::atomic<bool>             m_bStop { false }; // syncronization with time keeper thread

}; // Kron

DEKAF2_NAMESPACE_END
