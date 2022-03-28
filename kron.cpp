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

#include "kron.h"
#include "kstringview.h"
#include "kstring.h"
#include "ktime.h"
#include "kinshell.h"
#include "klog.h"
#include "dekaf2.h"
#include "bits/kron_utils.h"
#include "croncpp.h"

namespace dekaf2 {

using KCronParser = cron_base<cron_standard_traits<KString, KStringView>, Kron_utils>;

namespace {

//-----------------------------------------------------------------------------
auto CronParserDeleter = [](void* data)
//-----------------------------------------------------------------------------
{
	delete static_cast<KCronParser::cronexpr*>(data);
};

//-----------------------------------------------------------------------------
inline const KCronParser::cronexpr* cxget(const KUniqueVoidPtr& p)
//-----------------------------------------------------------------------------
{
	return static_cast<KCronParser::cronexpr*>(p.get());
}

} // end of anonymous namespace

//-----------------------------------------------------------------------------
KStringView Kron::Job::CutOffCommand(KStringView& sCronDef, uint16_t iElements)
//-----------------------------------------------------------------------------
{
	// split an input of "* 0/5 * * * ? fstrim -a >/dev/null 2>&1"
	// into "* 0/5 * * * ?" and "fstrim -a >/dev/null 2>&1"
	sCronDef.Trim();

	KStringView sCommand;

	// advance to first non-space after 6 fields, delimited by white space
	uint16_t iConsumedFields { 0 };
	KStringView::size_type iConsumedChars { 0 };
	bool bPassedField { false };

	for (const auto ch : sCronDef)
	{
		++iConsumedChars;

		if (!bPassedField)
		{
			if (KASCII::kIsSpace(ch))
			{
				bPassedField = true;
			}
		}
		else
		{
			if (KASCII::kIsSpace(ch) == false)
			{
				++iConsumedFields;

				if (iConsumedFields == iElements)
				{
					break;
				}
				else
				{
					bPassedField = false;
					continue;
				}
			}
		}
	}

	if (iConsumedFields == iElements)
	{
		sCommand = sCronDef.substr(iConsumedChars - 1);
		sCronDef.erase(iConsumedChars - 2);
		sCronDef.TrimRight();
	}

	return sCommand;

} // CutOffCommand

//-----------------------------------------------------------------------------
Kron::Job::Job(KString sName, KStringView sCronDef, bool bHasSeconds, uint16_t iMaxQueued)
//-----------------------------------------------------------------------------
: m_sName      (std::move(sName))
, m_sCommand   (CutOffCommand(sCronDef, bHasSeconds ? 6 : 5)) // we need to split in cron expression and command right here
, m_iJobID     (m_sName.Hash())
, m_iMaxQueued (iMaxQueued)
{
	kDebug(2, "constructing new crontab job: {}", Name());

	m_ParsedCron = KUniqueVoidPtr(
		new KCronParser::cronexpr(KCronParser::make_cron(sCronDef)),
		CronParserDeleter
	);

} // ctor

//-----------------------------------------------------------------------------
Kron::Job::Job(KString sName, std::time_t tOnce, KStringView sCommand)
//-----------------------------------------------------------------------------
: m_sName      (std::move(sName))
, m_sCommand   (sCommand)
, m_iJobID     (m_sName.Hash())
, m_iMaxQueued (1)
, m_tOnce      (tOnce)
{
	kDebug(2, "constructing new single execution job: {}", Name());

} // ctor

//-----------------------------------------------------------------------------
Kron::Job::Job(const KJSON& jConfig)
//-----------------------------------------------------------------------------
{
	m_sName        = kjson::GetStringRef (jConfig, "Name"        );
	m_sCommand     = kjson::GetStringRef (jConfig, "Command"     );
	m_iMaxQueued   = kjson::GetUInt      (jConfig, "MaxInQueue"  );
	m_tOnce        = kjson::GetInt       (jConfig, "tOnce"       );
	KString sDef   = kjson::GetStringRef (jConfig, "Definition"  );
	auto jEnviron  = kjson::GetArray     (jConfig, "Environment" );

	kDebug(2, "constructing new job from json: {}", Name());

	for (const auto& env : jEnviron)
	{
		const auto& sName  = kjson::GetStringRef(env, "name" );
		const auto& sValue = kjson::GetStringRef(env, "value");

		if (!sName.empty() && !sValue.empty())
		{
			m_Environment.push_back( { sName, sValue } );
		}
	}

	if (m_iMaxQueued == 0)
	{
		m_iMaxQueued = 1;
	}

	if (m_tOnce == 0)
	{
		m_tOnce = INVALID_TIME;
	}

	sDef.Trim();

	if (!sDef.empty())
	{
		m_ParsedCron = KUniqueVoidPtr(
			new KCronParser::cronexpr(KCronParser::make_cron(sDef)),
			CronParserDeleter
		);
	}

	m_iJobID = m_sName.Hash();

	auto iJobID = kjson::GetStringRef(jConfig, "ID").UInt64();

	if (iJobID && iJobID != m_iJobID)
	{
		kDebug(1, "job '{}': using ID from json ({}) instead of calculated one ({})",
			   m_sName, iJobID, m_iJobID);

		m_iJobID = iJobID;
	}

} // ctor

//-----------------------------------------------------------------------------
// virtual dtor, we want to be able to extend the data in inheriting classes
Kron::Job::~Job() {}
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
std::time_t Kron::Job::Next(std::time_t tAfter) const
//-----------------------------------------------------------------------------
{
	if (!m_ParsedCron)
	{
		kDebug(2, "single execution time for job '{}': {}", Name(), kFormTimestamp(m_tOnce));
		return m_tOnce;
	}

	if (tAfter == 0)
	{
		tAfter = Dekaf::getInstance().GetCurrentTime();
	}

	std::time_t tNext = KCronParser::cron_next(*cxget(m_ParsedCron), tAfter);
	kDebug(2, "next execution time for job '{}': {}", Name(), kFormTimestamp(tNext));

	return tNext;

} // Next

//-----------------------------------------------------------------------------
KUTCTime Kron::Job::Next(const KUTCTime& tAfter) const
//-----------------------------------------------------------------------------
{
	if (!m_ParsedCron)
	{
		kDebug(2, "single execution time for job '{}': {}", Name(), kFormTimestamp(m_tOnce));
		return KUTCTime(m_tOnce);
	}

	KUTCTime UTC = KCronParser::cron_next(*cxget(m_ParsedCron), tAfter.ToTM());
	kDebug(2, "next execution time for job '{}': {}", Name(), UTC.Format());

	return UTC;

} // Next

//-----------------------------------------------------------------------------
bool Kron::Job::IncQueued()
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_ExecMutex);

	++m_iIsQueued;
	{
		if (m_iIsQueued > m_iMaxQueued)
		{
			--m_iIsQueued;

			kDebug(1, "{} job(s) still waiting in queue or running, skipped '{}'",
				   m_iMaxQueued, Name());

			return false;
		}
	}

	return true;

} // IncQueued

//-----------------------------------------------------------------------------
void Kron::Job::DecQueued()
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_ExecMutex);

	--m_iIsQueued;

} // DecQueued

//-----------------------------------------------------------------------------
int Kron::Job::Execute()
//-----------------------------------------------------------------------------
{
	std::unique_lock<std::mutex> Lock(m_ExecMutex);

	++m_iIsRunning;

	if (m_iIsRunning > m_iMaxQueued)
	{
		--m_iIsRunning;
		kDebug(1, "new execution skipped, still running {} instance(s) of '{}'", m_iIsRunning, Name());
		return -1;
	}

	kDebug(1, "now starting job '{}'", Name());
	auto tNow = Dekaf::getInstance().GetCurrentTime();

	m_Control.tLastStarted = tNow;
	++m_Control.iStartCount;
	m_Control.Duration.Start();

	// and execute including the environment
	KInShell Shell(Command(), "/bin/sh", m_Environment);

#ifdef DEKAF2_IS_UNIX
	// record the process ID, but not on Windows, as there we use popen and not fork ..
	m_Control.ProcessID = Shell.GetProcessID();
#endif

	// unlock during execution
	Lock.unlock();

	KString sOutput;

	// read until EOF
	Shell.ReadRemaining(sOutput);

	// close the shell and report the return value to the caller
	int iResult = Shell.Close();

	// protect again
	Lock.lock();

	m_Control.Duration.Stop();
	m_Control.ProcessID    = 0;
	m_Control.sLastResult  = std::move(sOutput);
	m_Control.iLastResult  = iResult;
	m_Control.tLastStopped = Dekaf::getInstance().GetCurrentTime();

	kDebug(1, "job '{}' finished with status {}, took {}",
		   Name(), iResult, kTranslateSeconds(m_Control.Duration.seconds()));

	--m_iIsRunning;

	return iResult;

} // Execute

//-----------------------------------------------------------------------------
bool Kron::Job::Kill()
//-----------------------------------------------------------------------------
{
#ifdef DEKAF2_IS_UNIX
	std::unique_lock<std::mutex> Lock(m_ExecMutex);

	if (m_Control.ProcessID > 0)
	{
		auto iFailed = kill(m_Control.ProcessID, SIGKILL);

		// ESRCH == no process found, that means the process was killed in between
		if (iFailed && iFailed != ESRCH)
		{
			kDebug(1, "cannot kill job '{}': {}", Name(), strerror(errno));
			return false;
		}
	}
	return true;
#else
	return false;
#endif

} // Kill

//-----------------------------------------------------------------------------
struct Kron::Job::Control Kron::Job::Control() const
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_ExecMutex);

	return m_Control;
}

//-----------------------------------------------------------------------------
KJSON Kron::Job::Print(std::size_t iMaxResultSize) const
//-----------------------------------------------------------------------------
{
	KJSON jEnv;
	jEnv = KJSON::array();

	for (const auto& env : m_Environment)
	{
		jEnv.push_back(
		{
			{ "name"  , env.first  },
			{ "value" , env.second }
		});
	}

	std::lock_guard<std::mutex> Lock(m_ExecMutex);

	return KJSON
	{
		{ "Name"        , m_sName                                               },
		{ "Command"     , m_sCommand                                            },
		{ "ID"          , KString::to_string(m_iJobID)                          },
		{ "MaxInQueue"  , m_iMaxQueued                                          },
		{ "IsRunning"   , IsRunning()                                           },
		{ "IsQueued"    , IsQueued()                                            },
		{ "tOnce"       , m_tOnce                                               },
		{ "LastStarted" , m_Control.tLastStarted                                },
		{ "LastStopped" , m_Control.tLastStopped                                },
		{ "Starts"      , m_Control.iStartCount                                 },
		{ "Definition"  , m_ParsedCron ? cxget(m_ParsedCron)->to_cronstr() : "" },
		{ "Environment" , std::move(jEnv)                                       },
		{ "Duration"    , m_Control.Duration.milliseconds()                     },
		{ "Result"      , m_Control.sLastResult.Left(iMaxResultSize)            },
		{ "ResultCode"  , m_Control.iLastResult                                 }
	};

} // Print

// the empty base class methods for the Scheduler
      Kron::Scheduler::~Scheduler  () /* need a virtual dtor */              {                 }
bool  Kron::Scheduler::AddJob      (const SharedJob& Job)                    { return false;   }
bool  Kron::Scheduler::DeleteJob   (Job::ID JobID)                           { return false;   }
KJSON Kron::Scheduler::ListJobs    () const                                  { return KJSON{}; }
bool  Kron::Scheduler::UpdateJob   (KStringView sCronKey, const KJSON& jJob) { return false;   }
void  Kron::Scheduler::JobFinished (const SharedJob& Job)                    {                 }

//-----------------------------------------------------------------------------
bool Kron::LocalScheduler::AddJob(const std::shared_ptr<Job>& job)
//-----------------------------------------------------------------------------
{
	auto tNow   = Dekaf::getInstance().GetCurrentTime();
	auto tNext  = job->Next(tNow);
	auto JobID  = job->JobID();

	// get a unique lock
	auto Jobs = m_Jobs.unique();

	// check for job ID
	for (const auto& it : *Jobs)
	{
		if (JobID == it.second.get()->JobID())
		{
			kDebug(1, "job '()' is already part of the job list", job->Name());
			return false;
		}
	}

	Jobs->insert( { tNext, job } );
	kDebug(1, "added '{}': '{}'\nnext execution: {}",
		   job->Name(), job->Command(), kFormTimestamp(tNext));

	return true;

} // LocalScheduler::AddJob

//-----------------------------------------------------------------------------
bool Kron::LocalScheduler::DeleteJob(Job::ID JobID)
//-----------------------------------------------------------------------------
{
	// get a unique lock
	auto Jobs = m_Jobs.unique();

	// check for job ID
	for (auto it = Jobs->begin(); it != Jobs->end(); ++it)
	{
		if (JobID == it->second.get()->JobID())
		{
			kDebug(2, "deleting job '{}' with ID {}", it->second->Name(), JobID);

			Jobs->erase(it);

			return true;
		}
	}

	kDebug(2, "JobID not found: {}", JobID);

	return false;

} // LocalScheduler::DeleteJob

//-----------------------------------------------------------------------------
Kron::SharedJob Kron::LocalScheduler::GetJob(std::time_t tNow)
//-----------------------------------------------------------------------------
{
	auto Jobs = m_Jobs.unique();

	if (Jobs->empty())
	{
		return nullptr;
	}

	// the job map is sorted by next execution time
	if (Jobs->begin()->first > tNow)
	{
		// a job in the future - abort pulling jobs here
		return nullptr;
	}

	auto job = Jobs->begin()->second;

	// we will remove the job, and insert it with the next execution time again
	Jobs->erase(Jobs->begin());

	auto tNext = job->Next(tNow);

	if (tNext != INVALID_TIME && tNext > tNow)
	{
		Jobs->insert( { tNext, job } );
	}

	return job;

} // LocalScheduler::GetJob

//-----------------------------------------------------------------------------
KJSON Kron::LocalScheduler::ListJobs() const
//-----------------------------------------------------------------------------
{
	KJSON jobs;
	jobs = KJSON::array();

	auto Jobs = m_Jobs.shared();

	for (const auto& job : *Jobs)
	{
		jobs.push_back(job.second->Print());
		jobs.back()["tNext"] = job.first;
	}

	return jobs;

} // ListJobs

//-----------------------------------------------------------------------------
Kron::Kron(std::size_t iThreads,
		   SharedScheduler Scheduler)
//-----------------------------------------------------------------------------
: m_Scheduler(Scheduler)
{
	Resize(iThreads);
}

//-----------------------------------------------------------------------------
Kron::~Kron()
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_ResizeMutex);

	m_Tasks.stop(false);

	if (m_Chronos)
	{
		kDebug(1, "stopping Chronos");
		// tell the thread to stop
		m_bStop = true;
		// and block until it is home
		m_Chronos->join();
	}

	kDebug(1, "stopped");

} // dtor

//-----------------------------------------------------------------------------
void Kron::Launcher()
//-----------------------------------------------------------------------------
{
	if (!m_Scheduler)
	{
		kDebug(1, "no scheduler");
		return;
	}

	auto tSleepUntil = Dekaf::getInstance().GetCurrentTimepoint();

	for(;m_bStop == false;)
	{
		tSleepUntil += std::chrono::seconds(1);
		std::this_thread::sleep_until(tSleepUntil);

		auto tNow = Dekaf::getInstance().GetCurrentTime();

		for (;;)
		{
			auto job = m_Scheduler->GetJob(tNow);

			if (!job)
			{
				// no more jobs right now
				break;
			}

			// make sure we have not more than the max allowed count of Jobs
			// like this in the queue
			if (job->IncQueued())
			{
				// queue task (use value copy of the shared ptr)
				m_Tasks.push([this, job]() mutable
				{
					job->Execute();
					job->DecQueued();
					m_Scheduler->JobFinished(job);
				});
			}
		}
	}

} // Launcher

//-----------------------------------------------------------------------------
bool Kron::Resize(std::size_t iThreads)
//-----------------------------------------------------------------------------
{
	if (!m_Scheduler)
	{
		kDebug(1, "no scheduler");
		return false;
	}

	std::lock_guard<std::mutex> Lock(m_ResizeMutex);

	if (iThreads == m_Tasks.size())
	{
		kDebug(2, "size already at: {}", iThreads);
		return true;
	}

	kDebug(2, "resizing threads in task pool to: {}", iThreads);

	if (iThreads == 0 && m_Chronos)
	{
		// tell the thread to stop
		m_bStop = true;

		// and block until it is home (could take a second)
		m_Chronos->join();
		m_Chronos.reset();
	}

	auto bResult = m_Tasks.resize(iThreads);

	if (iThreads > 0 && !m_Chronos)
	{
		m_bStop = false;

		// start the time keeper
		m_Chronos = std::make_unique<std::thread>(&Kron::Launcher, this);
	}

	return bResult;

} // Resize

//-----------------------------------------------------------------------------
bool Kron::AddJob(const std::shared_ptr<Job>& job)
//-----------------------------------------------------------------------------
{
	if (!m_Scheduler)
	{
		kDebug(1, "no scheduler");
		return false;
	}

	return m_Scheduler->AddJob(job);

} // Add

//-----------------------------------------------------------------------------
bool Kron::DeleteJob(Job::ID JobID)
//-----------------------------------------------------------------------------
{
	if (!m_Scheduler)
	{
		kDebug(1, "no scheduler");
		return false;
	}

	return m_Scheduler->DeleteJob(JobID);

} // Delete

//-----------------------------------------------------------------------------
KJSON Kron::ListJobs() const
//-----------------------------------------------------------------------------
{
	if (!m_Scheduler)
	{
		kDebug(1, "no scheduler");
		return KJSON{};
	}

	return m_Scheduler->ListJobs();

} // ListJobs

//-----------------------------------------------------------------------------
std::size_t Kron::AddJobsFromCrontab(KStringView sCrontab, bool bHasSeconds)
//-----------------------------------------------------------------------------
{
	std::size_t iCount { 0 };
	std::vector<std::pair<KString, KString>> Environment;

	for (auto sLine : sCrontab.Split("\n"))
	{
		sLine.erase(sLine.find('#'));
		sLine.Trim();

		if (sLine.empty())
		{
			continue;
		}

		try
		{
			auto job = Kron::Job::Create(kFormat("crontab-{}", iCount + 1), sLine, bHasSeconds, 1);
			// copy the environment into the job
			job->SetEnvironment(Environment);
			// and try to add the job to the scheduler
			if (AddJob(job))
			{
				++iCount;
			}
		}
		catch (const std::exception& ex)
		{
			// this was not a crontab line. Maybe it's an env variable?
			auto Pair = kSplitToPair(sLine, "=");

			if (!Pair.first.empty() && !Pair.second.empty())
			{
				// looks like..
				kDebug(2, "adding env var: {}={}", Pair.first, Pair.second);
				Environment.push_back( { Pair.first, Pair.second } );
			}
			else
			{
				kDebug(1, "junk line: {}", sLine);
			}
		}
	}

	return iCount;

} // AddJobsFromCrontab

} // end of namespace dekaf2
