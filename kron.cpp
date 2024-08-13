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
#include "kexception.h"
#include "bits/kron_utils.h"
#include "croncpp.h"

DEKAF2_NAMESPACE_BEGIN

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
Kron::Job::Job(KString sName, KStringView sCronDef, bool bHasSeconds, KDuration MaxExecutionTime)
//-----------------------------------------------------------------------------
: m_sName            (std::move(sName))
, m_sCommand         (CutOffCommand(sCronDef, bHasSeconds ? 6 : 5)) // we need to split in cron expression and command right here
, m_MaxExecutionTime (MaxExecutionTime)
, m_iJobID           (m_sName.Hash())
{
	kDebug(1, "building Job '{}' from cron expression", Name());

	m_ParsedCron = KUniqueVoidPtr(
		new KCronParser::cronexpr(KCronParser::make_cron(sCronDef)),
		CronParserDeleter
	);

	kDebug(2, Print().dump(1, '\t'));

} // ctor

//-----------------------------------------------------------------------------
Kron::Job::Job(KString sName, KUnixTime tOnce, KStringView sCommand, KDuration MaxExecutionTime)
//-----------------------------------------------------------------------------
: m_sName            (std::move(sName))
, m_sCommand         (sCommand)
, m_MaxExecutionTime (MaxExecutionTime)
, m_iJobID           (m_sName.Hash())
, m_tOnce            (tOnce)
{
	kDebug(1, "building Job '{}' with once time", Name());
	kDebug(2, Print().dump(1, '\t'));

} // ctor

//-----------------------------------------------------------------------------
Kron::Job::Job(const KJSON& jConfig)
//-----------------------------------------------------------------------------
{
#ifdef DEKAF2_WRAPPED_KJSON
	m_sName            = jConfig("Name");
	kDebug(1, "building Job '{}' from json", Name());
	m_sCommand         = jConfig("Command");
	m_MaxExecutionTime = std::chrono::seconds(jConfig("MaxExecutionTime"));
	m_tOnce            = KUnixTime::from_time_t(jConfig("tOnce"));
	KString sDef       = jConfig("Definition");
	auto jEnviron      = jConfig("Environment").Array();
#else
	m_sName            = kjson::GetStringRef (jConfig, "Name"        );
	kDebug(1, "building Job '{}' from json", Name());
	m_sCommand         = kjson::GetStringRef (jConfig, "Command"     );
	m_MaxExecutionTime = std::chrono::seconds(kjson::GetUInt(jConfig, "MaxExecutionTime"));
	m_tOnce            = kjson::GetInt       (jConfig, "tOnce"       );
	KString sDef       = kjson::GetStringRef (jConfig, "Definition"  );
	auto jEnviron      = kjson::GetArray     (jConfig, "Environment" );
#endif
	if (jEnviron.is_array() && !jEnviron.empty())
	{
		for (const auto& env : jEnviron)
		{
#ifdef DEKAF2_WRAPPED_KJSON_NOT_YET_ADD_AUTO_ITERATION
			const auto& sName  = env("name" ).String();
			const auto& sValue = env("value").String();
#else
			const auto& sName  = kjson::GetStringRef(env, "name" );
			const auto& sValue = kjson::GetStringRef(env, "value");
#endif

			if (!sName.empty() && !sValue.empty())
			{
				m_Environment.push_back( { sName, sValue } );
			}
		}
	}

	if (m_MaxExecutionTime <= KDuration::zero())
	{
		m_MaxExecutionTime = KDuration::max();
	}

	if (m_tOnce == KUnixTime(0))
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

	kDebug(2, Print().dump(1, '\t'));

} // ctor

//-----------------------------------------------------------------------------
// virtual dtor, we want to be able to extend the data in inheriting classes
Kron::Job::~Job() {}
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
KUnixTime Kron::Job::Next(KUnixTime tAfter) const
//-----------------------------------------------------------------------------
{
	if (!m_ParsedCron)
	{
		kDebug(2, "single execution time for job '{}': {}", Name(), kFormTimestamp(m_tOnce));
		return m_tOnce;
	}

	if (tAfter == KUnixTime(0))
	{
		tAfter = Dekaf::getInstance().GetCurrentTime();
	}

	KUnixTime tNext = KUnixTime::from_time_t(KCronParser::cron_next(*cxget(m_ParsedCron), tAfter.to_time_t()));
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

	KUTCTime UTC = KUTCTime(KCronParser::cron_next(*cxget(m_ParsedCron), tAfter.to_tm()));
	kDebug(2, "next execution time for job '{}': {}", Name(), UTC);

	return UTC;

} // Next

//-----------------------------------------------------------------------------
bool Kron::Job::Start()
//-----------------------------------------------------------------------------
{
	std::unique_lock<std::shared_mutex> Lock(m_ExecMutex);

	if (m_Shell != nullptr)
	{
		kDebug(1, "new execution skipped, still running '{}'", Name());
		return false;
	}

	kDebug(1, "now starting job '{}'", Name());
	auto tNow = Dekaf::getInstance().GetCurrentTime();

	m_Control.tLastStarted = tNow;
	++m_Control.iStartCount;
	m_Control.ExecutionTime.Start();

	// and execute including the environment
	m_Shell = std::make_unique<KInShell>();

	if (!m_Shell->Open(Command(), "/bin/sh", m_Environment))
	{
		// cannot open input pipe 'echo my first cronjob': Too many open files
		return false;
	}

#ifdef DEKAF2_IS_UNIX
	// record the process ID, but not on Windows, as there we use popen and not fork ..
	m_Control.ProcessID = m_Shell->GetProcessID();
#endif

	// unlock during execution
	Lock.unlock();

	return m_Shell->IsRunning();

} // Start

//-----------------------------------------------------------------------------
bool Kron::Job::IsWaiting()
//-----------------------------------------------------------------------------
{
	std::shared_lock<std::shared_mutex> Lock(m_ExecMutex);

	return m_Shell != nullptr && !m_Shell->IsRunning();

} // IsWaiting

//-----------------------------------------------------------------------------
int Kron::Job::Wait(int msecs)
//-----------------------------------------------------------------------------
{
	std::unique_lock<std::shared_mutex> Lock(m_ExecMutex);

	if (m_Shell == nullptr)
	{
		kDebug(1, "job '{}' no more running?", Name());
		return -1;
	}

	KString sOutput;

	// read until EOF
	m_Shell->ReadRemaining(sOutput);

	// close the shell and report the return value to the caller
	int iResult = m_Shell->Close(msecs);

#ifdef DEKAF2_WITH_KLOG
	auto lastDuration      = m_Control.ExecutionTime.Stop();
#endif
	m_Control.ProcessID    = -1;
	m_Control.sLastOutput  = std::move(sOutput);
	m_Control.iLastStatus  = iResult;
	m_Control.tLastStopped = Dekaf::getInstance().GetCurrentTime();

	kDebug(1, "job '{}' finished with status {}, took {}",
		   Name(), iResult, kTranslateDuration(lastDuration));

	m_Shell.reset();

	return iResult;

} // Wait

//-----------------------------------------------------------------------------
bool Kron::Job::Kill()
//-----------------------------------------------------------------------------
{
#ifdef DEKAF2_IS_UNIX
	std::shared_lock<std::shared_mutex> Lock(m_ExecMutex);

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
bool Kron::Job::KillIfOverdue()
//-----------------------------------------------------------------------------
{
	KDuration Runtime;

	std::shared_lock<std::shared_mutex> Lock(m_ExecMutex);

	if (!m_Shell)
	{
		return false;
	}

	Runtime = m_Control.ExecutionTime.CurrentRound().elapsed();

	if (Runtime <= m_MaxExecutionTime)
	{
		return false;
	}

	Lock.unlock();

	kDebug(1, "killing Job '{}' after runtime of {}", Name(), kTranslateDuration(Runtime));

	return Kill();

} // KillIfOverdue

//-----------------------------------------------------------------------------
struct Kron::Job::Control Kron::Job::Control() const
//-----------------------------------------------------------------------------
{
	std::shared_lock<std::shared_mutex> Lock(m_ExecMutex);

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

	std::shared_lock<std::shared_mutex> Lock(m_ExecMutex);

	return KJSON
	{
		{ "Name"        , m_sName                                               },
		{ "Command"     , m_sCommand                                            },
		{ "ID"          , KString::to_string(m_iJobID)                          },
		{ "IsRunning"   , IsRunning()                                           },
		{ "tOnce"       , m_tOnce.to_time_t()                                   },
		{ "LastStarted" , m_Control.tLastStarted.to_time_t()                    },
		{ "LastStopped" , m_Control.tLastStopped.to_time_t()                    },
		{ "Starts"      , m_Control.iStartCount                                 },
		{ "Definition"  , m_ParsedCron ? cxget(m_ParsedCron)->to_cronstr() : "" },
		{ "Environment" , std::move(jEnv)                                       },
		{ "Executed"    , m_Control.ExecutionTime.milliseconds().count()        },
		{ "Output"      , m_Control.sLastOutput.Left(iMaxResultSize)            },
		{ "MaxExecutionTime", m_MaxExecutionTime.seconds().count()              },
		{ "Status"      , m_Control.iLastStatus                                 },
		{ "ProcessID"   , m_Control.ProcessID                                   }
	};

} // Print

// the empty base class methods for the Scheduler
                Kron::Scheduler::~Scheduler  () /* need a virtual dtor */               {                 }
bool            Kron::Scheduler::AddJob      (const SharedJob& Job)                     { return false;   }
bool            Kron::Scheduler::DeleteJob   (Job::ID JobID)                            { return false;   }
KJSON           Kron::Scheduler::ListJobs    ()                                   const { return KJSON{}; }
std::size_t     Kron::Scheduler::size        ()                                   const { return 0;       }
bool            Kron::Scheduler::empty       ()                                   const { return !size(); }
Kron::SharedJob Kron::Scheduler::GetJob      (KUnixTime tNow)                           { return nullptr; }
void            Kron::Scheduler::JobFinished (const SharedJob& Job)                     {                 }
void            Kron::Scheduler::JobFailed   (const SharedJob& Job, KStringView sError) {                 }

//-----------------------------------------------------------------------------
bool  Kron::Scheduler::ModifyJob   (const SharedJob& Job)
//-----------------------------------------------------------------------------
{
	return DeleteJob(Job->JobID()) && AddJob(Job);
}

//-----------------------------------------------------------------------------
std::size_t Kron::Scheduler::AddJobsFromCrontab(KStringView sCrontab, bool bHasSeconds)
//-----------------------------------------------------------------------------
{
	std::size_t iCount { 0 };
	std::vector<std::pair<KString, KString>> Environment;

	for (auto sLine : sCrontab.Split('\n'))
	{
		sLine.erase(sLine.find('#'));
		sLine.Trim();

		if (sLine.empty())
		{
			continue;
		}

		try
		{
			auto job = Kron::Job::Create(kFormat("crontab-{}", iCount + 1), sLine, bHasSeconds);
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

//-----------------------------------------------------------------------------
bool Kron::LocalScheduler::AddJob(const std::shared_ptr<Job>& job)
//-----------------------------------------------------------------------------
{
	const auto tNow     = Dekaf::getInstance().GetCurrentTime();
	const auto tNext    = job->Next(tNow);
	const auto JobID    = job->JobID();
	const auto& JobName = job->Name();

	// get a unique lock
	auto Jobs = m_Jobs.unique();

	// check for job ID
	for (const auto& it : *Jobs)
	{
		const auto jj = it.second.get();

		if (JobID   == jj->JobID() ||
			JobName == jj->Name())
		{
			kDebug(1, "job '()' is already part of the job list (with ID {} and name '{}')",
				   JobName, jj->JobID(), jj->Name());
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
Kron::SharedJob Kron::LocalScheduler::GetJob(KUnixTime tNow)
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
		jobs.back()["tNext"] = job.first.to_time_t();
	}

	return jobs;

} // ListJobs

//-----------------------------------------------------------------------------
std::size_t Kron::LocalScheduler::size() const
//-----------------------------------------------------------------------------
{
	return m_Jobs.shared()->size();

}

//-----------------------------------------------------------------------------
bool Kron::LocalScheduler::empty() const
//-----------------------------------------------------------------------------
{
	return m_Jobs.shared()->empty();
}

//-----------------------------------------------------------------------------
Kron::Kron(bool bAllowLaunches, KDuration CheckEvery, SharedScheduler Scheduler)
//-----------------------------------------------------------------------------
: m_Scheduler(Scheduler)
{
	if (!m_Scheduler)
	{
		throw KException("cannot construct a Kron without scheduler");
	}

	if (bAllowLaunches)
	{
		kDebug(1, "starting");

		// start the time keeper
		m_Chronos = std::make_unique<std::thread>(&Kron::Launcher, this, CheckEvery);
	}

} // ctor

//-----------------------------------------------------------------------------
Kron::~Kron()
//-----------------------------------------------------------------------------
{
	if (m_Chronos)
	{
		kDebug(1, "stopping Chronos");
		// tell the thread to stop
		m_bStop = true;
		// and block until it is home
		m_Chronos->join();
	}

	kDebug(1, "finished");

} // dtor

//-----------------------------------------------------------------------------
// returns number of newly started jobs
std::size_t Kron::StartNewJobs()
//-----------------------------------------------------------------------------
{
	std::size_t iStarted { 0 };

	auto tNow = Dekaf::getInstance().GetCurrentTime();

	for (;m_bStop == false;)
	{
		auto job = m_Scheduler->GetJob(tNow);

		if (!job)
		{
			// no more jobs right now
			break;
		}

		if (job->Start())
		{
			m_RunningJobs.unique()->push_back(job);
			++iStarted;
		}
		else
		{
			m_Scheduler->JobFailed(job, strerror(errno));
		}
	}

	return iStarted;

} // StartNewJobs

//-----------------------------------------------------------------------------
// returns number of running jobs
std::size_t Kron::FinishWaitingJobs()
//-----------------------------------------------------------------------------
{
	auto Jobs = m_RunningJobs.unique();

	for (auto job = Jobs->begin(); job != Jobs->end();)
	{
		(*job)->KillIfOverdue();

		if ((*job)->IsWaiting())
		{
			// read result
			(*job)->Wait(100);

			m_Scheduler->JobFinished(*job);

			// remove from list
			Jobs->erase(job);
		}
		else
		{
			++job;
		}
	}

	return Jobs->size();

} // FinishWaitingJobs

//-----------------------------------------------------------------------------
void Kron::Launcher(KDuration CheckEvery)
//-----------------------------------------------------------------------------
{
	if (CheckEvery == KDuration::zero())
	{
		CheckEvery = std::chrono::seconds(1);
	}

	kDebug(1, "will check with interval of {} for jobs to start", kTranslateDuration(CheckEvery));

	std::size_t iFactor  { 1 };
	std::size_t iRunning { 0 };

	if (CheckEvery.milliseconds() > chrono::milliseconds(100))
	{
		// while jobs are running, we want to check for termination at
		// least every 100 millisecods
		iFactor = CheckEvery.milliseconds() / chrono::milliseconds(100);

		if (iFactor > 1)
		{
			kDebug(2, "will check {} times faster for terminated jobs", iFactor);
		}
	}

	std::size_t iCountDown { iFactor };

	for(;;)
	{
		try
		{
			if (iRunning)
			{
				if (--iCountDown == 0)
				{
					iCountDown = iFactor;
				}
			}

			if (!iRunning || iCountDown == iFactor)
			{
				// check for all new jobs to start
				iRunning += StartNewJobs();
			}

			if (iRunning)
			{
				// check for returns
				iRunning = FinishWaitingJobs();
			}

			if (m_bStop && iRunning == 0)
			{
				// all jobs finished, stop requested
				return;
			}
		}

		catch (const std::exception& ex)
		{
			kException(ex);
		}
		catch (...)
		{
			kUnknownException();
		}

		std::this_thread::sleep_for((iRunning && iFactor > 1)
									? chrono::milliseconds(100)
									: CheckEvery.duration());
	}

} // Launcher

//-----------------------------------------------------------------------------
KJSON Kron::ListRunningJobs(Job::ID filterForID) const
//-----------------------------------------------------------------------------
{
	KJSON jJobs;
	jJobs = KJSON::array();

	auto Jobs = m_RunningJobs.shared();

	for (auto& job : *Jobs)
	{
		if (filterForID == 0 || job->JobID() == filterForID)
		{
			jJobs.push_back(job->Print());
		}
	}

	return jJobs;

} // ListRunningJobs

//-----------------------------------------------------------------------------
bool Kron::KillJob(Job::ID ID)
//-----------------------------------------------------------------------------
{
	auto Jobs = m_RunningJobs.unique();

	auto it = std::find_if(Jobs->begin(), Jobs->end(), [&ID](SharedJob& job)
	{
		return job->JobID() == ID;
	});

	if (it != Jobs->end())
	{
		(*it)->Kill();
		return true;
	}

	return false;

} // KillJob

#ifdef DEKAF2_REPEAT_CONSTEXPR_VARIABLE
constexpr KUnixTime Kron::INVALID_TIME;
#endif

DEKAF2_NAMESPACE_END
