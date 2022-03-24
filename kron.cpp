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
	}

	return sCommand;

} // CutOffCommand

//-----------------------------------------------------------------------------
Kron::Job::Job(KStringView sCronDef, bool bHasSeconds, uint16_t iMaxQueued)
//-----------------------------------------------------------------------------
: m_sCommand   (CutOffCommand(sCronDef, bHasSeconds ? 6 : 5)) // we need to split in cron expression and command right here
, m_iJobID     (m_sCommand.Hash())
, m_iMaxQueued (iMaxQueued)
{
	m_ParsedCron = KUniqueVoidPtr(new KCronParser::cronexpr(KCronParser::make_cron(sCronDef)),
															CronParserDeleter);

} // ctor

//-----------------------------------------------------------------------------
Kron::Job::Job(std::time_t tOnce, KStringView sCommand)
//-----------------------------------------------------------------------------
: m_sCommand   (sCommand)
, m_iJobID     (sCommand.Hash())
, m_iMaxQueued (1)
, m_tOnce      (tOnce)
{
} // ctor

//-----------------------------------------------------------------------------
std::time_t Kron::Job::Next(std::time_t tAfter) const
//-----------------------------------------------------------------------------
{
	if (!m_ParsedCron)
	{
		// either INVALID_TIME or time_t value
		return m_tOnce;
	}

	if (tAfter == 0)
	{
		tAfter = Dekaf::getInstance().GetCurrentTime();
	}

	return KCronParser::cron_next(*cxget(m_ParsedCron), tAfter);

} // Next

//-----------------------------------------------------------------------------
KUTCTime Kron::Job::Next(const KUTCTime& tAfter) const
//-----------------------------------------------------------------------------
{
	if (!m_ParsedCron)
	{
		if (m_tOnce != INVALID_TIME)
		{
			return KUTCTime(m_tOnce);
		}
		else
		{
			return KUTCTime();
		}
	}

	return KCronParser::cron_next(*cxget(m_ParsedCron), tAfter.ToTM());

} // Next

//-----------------------------------------------------------------------------
bool Kron::Job::IncQueued()
//-----------------------------------------------------------------------------
{
	std::lock_guard Lock(m_ExecMutex);

	++m_iIsQueued;
	{
		if (m_iIsQueued > m_iMaxQueued)
		{
			--m_iIsQueued;

			kDebug(1, "{} job(s) still waiting in queue or running, skipped: {}",
				   m_iMaxQueued, Command());

			return false;
		}
	}

	return true;

} // IncQueued

//-----------------------------------------------------------------------------
void Kron::Job::DecQueued()
//-----------------------------------------------------------------------------
{
	std::lock_guard Lock(m_ExecMutex);

	--m_iIsQueued;

} // DecQueued

//-----------------------------------------------------------------------------
int Kron::Job::Execute()
//-----------------------------------------------------------------------------
{
	std::unique_lock Lock(m_ExecMutex);

	++m_iIsRunning;

	if (m_iIsRunning > m_iMaxQueued)
	{
		--m_iIsRunning;
		kDebug(1, "new execution skipped, still running: {}", Command());
		return -1;
	}

	auto tNow = Dekaf::getInstance().GetCurrentTime();

	m_Control.tLastStarted = tNow;
	++m_Control.iStartCount;

	// unlock during execution
	Lock.unlock();

	KInShell Shell(Command());

	KString sOutput;

	// read until EOF
	Shell.ReadRemaining(sOutput);

	// do something with the output..

	// close the shell and report the return value to the caller
	int iResult = Shell.Close();

	// protect again
	Lock.lock();

	m_Control.tLastStopped = Dekaf::getInstance().GetCurrentTime();

	--m_iIsRunning;

	return iResult;

} // Execute

//-----------------------------------------------------------------------------
struct Kron::Job::Control Kron::Job::Control() const
//-----------------------------------------------------------------------------
{
	std::lock_guard Lock(m_ExecMutex);

	return m_Control;
}

//-----------------------------------------------------------------------------
Kron::~Kron()
//-----------------------------------------------------------------------------
{
	std::lock_guard Lock(m_ResizeMutex);

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
bool Kron::Resize(std::size_t iThreads)
//-----------------------------------------------------------------------------
{
	std::lock_guard Lock(m_ResizeMutex);

	if (iThreads == m_Tasks.size())
	{
		kDebug(2, "size already at: {}", iThreads);
		return true;
	}

	kDebug(2, "resizing threads in task pool to: {}", iThreads);

	auto bResult = m_Tasks.resize(iThreads);

	if (iThreads > 0 && !m_Chronos)
	{
		m_bStop = false;

		// start the time keeper
		m_Chronos = std::make_unique<std::thread>([this]()
		{
			// this is the main timer loop
			for(;m_bStop == false;)
			{
				kSleep(1); // one second

				time_t tNow = Dekaf::getInstance().GetCurrentTime();

				auto Jobs = m_Jobs.unique();

				while (!Jobs->empty())
				{
					// the job map is sorted by next execution time
					if (Jobs->begin()->first > tNow)
					{
						// a job in the future - abort pulling jobs here
						break;
					}

					auto job = Jobs->begin()->second;

					// we will remove the job, and insert it with the next execution time again
					Jobs->erase(Jobs->begin());

					auto tNext = job->Next(tNow);

					if (tNext != INVALID_TIME && tNext > tNow)
					{
						Jobs->insert( { tNext, job } );
					}

					// make sure we have not more than the max allowed count of Jobs
					// like this in the queue
					if (job->IncQueued())
					{
						// start task (use value copy of the shared ptr)
						m_Tasks.push([job]()
						{
							job->Execute();
							job->DecQueued();
						});
					}
				}
			}
		});
	}
	else if (iThreads == 0 && m_Chronos)
	{
		// tell the thread to stop
		m_bStop = true;
		// and block until it is home (could take a second)
		m_Chronos->join();
		m_Chronos.reset();
	}

	return bResult;

} // Resize

//-----------------------------------------------------------------------------
bool Kron::Add(std::shared_ptr<Job>& job)
//-----------------------------------------------------------------------------
{
	auto tNow   = Dekaf::getInstance().GetCurrentTime();
	auto tNext  = job->Next(tNow);
	auto JobID  = job->GetJobID();

	// get a unique lock
	auto Jobs = m_Jobs.unique();

	// check for job ID, and for good insertion point
	for (auto it = Jobs->begin(); it != Jobs->end(); ++it)
	{
		if (JobID == it->second.get()->GetJobID())
		{
			kDebug(1, "job is already part of the job list: {}", job->Command());
			return false;
		}
	}

	Jobs->insert( { tNext, job } );

	return true;

} // Add

//-----------------------------------------------------------------------------
bool Kron::Delete(Job::ID JobID)
//-----------------------------------------------------------------------------
{
	// get a unique lock
	auto Jobs = m_Jobs.unique();

	// check for job ID
	for (auto it = Jobs->begin(); it != Jobs->end(); ++it)
	{
		if (JobID == it->second.get()->GetJobID())
		{
			kDebug(2, "deleting JobID: {}", JobID);

			Jobs->erase(it);

			return true;
		}
	}

	kDebug(2, "JobID not found: {}", JobID);

	return false;

} // Delete

} // end of namespace dekaf2
