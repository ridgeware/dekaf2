
#include "__LowerProjectName__.h"
#include <dekaf2/core/init/dekaf2.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/strings/kstringview.h>
#include <dekaf2/core/errors/kexception.h>
#include <dekaf2/core/logging/klog.h>
#include <dekaf2/core/logging/kdatedlogfile.h>
#include <dekaf2/io/streams/kstream.h>
#include <dekaf2/system/os/kservice.h>
#include <dekaf2/system/os/ksignals.h>
#include <dekaf2/util/cli/koptions.h>
#include <csignal>

using namespace dekaf2;

//-----------------------------------------------------------------------------
__ProjectName__::__ProjectName__ ()
//-----------------------------------------------------------------------------
{
	KInit()
		.SetName(s_sProjectName)
		.SetMultiThreading()
		.SetOnlyShowCallerOnJsonError();
}

//-----------------------------------------------------------------------------
void __ProjectName__::ShowVersion ()
//-----------------------------------------------------------------------------
{
	kPrintLine(":: {} v{}", s_sProjectName, s_sProjectVersion);

} // ShowVersion

//-----------------------------------------------------------------------------
void __ProjectName__::Stop ()
//-----------------------------------------------------------------------------
{
	{
		std::lock_guard<std::mutex> Lock(m_Mutex);
		m_bStop = true;
	}

	m_Wakeup.notify_all();

} // Stop

//-----------------------------------------------------------------------------
void __ProjectName__::Work ()
//-----------------------------------------------------------------------------
{
	++m_iRounds;

	// ---- insert the work of one round here ----

	kDebug(1, "round {}", m_iRounds);

} // Work

//-----------------------------------------------------------------------------
int __ProjectName__::Main (int argc, char** argv)
//-----------------------------------------------------------------------------
{
	// ---------------- parse CLI ------------------
	{
		KOptions Options(false, argc, argv, KLog::STDOUT, /*bThrow*/true);

		Options
			.SetBriefDescription       ("dekaf2 __ProjectType__ template")
			// .SetHelpSeparator          ("::")             // the column separator between option and help text
			// .SetLinefeedBetweenOptions (false)            // add linefeed between separate options or commands?
			// .SetWrappedHelpIndent      (1)                // the indent for continuation help text
			.SetSpacingPerSection      (true)                // whether commands and options get the same or separate column layout
			.SetAdditionalHelp         (KService::GetHelp()) // the options to run as a service of systemd, launchd or Windows
		;

		bool bShowVersion = Options("version : show version information", false);

		// ---- insert project specific options here ----

		Options.Section("service options:");

		m_Interval              = std::chrono::seconds(Options("interval <seconds> : the time between two rounds of work (default 60)", 60).Range(1, 24*60*60).UInt32());
		KStringViewZ sLog       = Options("log <file>         : write the log into <file> - the rounds show with -d", "");
		KStringViewZ sLogPeriod = Options("logperiod <period> : start a new log file every day, week or month at midnight UTC, and rename the past ones after their period - default off", "");
		uint16_t     iLogKeep   = Options("logkeep <count>    : the number of past log files to keep with -logperiod (default 0 == all)", 0);

		// check that all options were consumed - this also prints the help if it was requested
		if (!Options.Check())
		{
			return Options.Terminate() ? 0 : 1;
		}

		if (bShowVersion)
		{
			ShowVersion();
			return 0;
		}

		KLog::getInstance().SetMode(KLog::SERVER);

		if (!sLog.empty() && !KLog::getInstance().SetDebugLog(sLog))
		{
			throw KException(kFormat("cannot open log file: {}", sLog));
		}

		if (!sLogPeriod.empty())
		{
			KDatedLogFile::Interval Period { KDatedLogFile::Day };

			switch (sLogPeriod.ToLowerASCII().Hash())
			{
				case "day"_hash:
					Period = KDatedLogFile::Day;
					break;
				case "week"_hash:
					Period = KDatedLogFile::Week;
					break;
				case "month"_hash:
					Period = KDatedLogFile::Month;
					break;
				default:
					throw KException(kFormat("unknown log period '{}' - must be one of day, week, month", sLogPeriod));
			}

			KLog::getInstance().SetDebugLogPeriod(Period, npos, iLogKeep ? iLogKeep : npos);
		}
	}

	// a stop of the service (SIGTERM, or the Windows service manager) ends the work loop
	KService::SetShutdownHandler([this]() { Stop(); });

	// and so does Ctrl-C when the service runs in a terminal
	if (auto Signals = Dekaf::getInstance().Signals())
	{
		Signals->SetSignalHandler(SIGINT, [this](int) { Stop(); });
	}

	kDebug(1, "started, a round every {} seconds", m_Interval.count());

	std::unique_lock<std::mutex> Lock(m_Mutex);

	while (!m_bStop)
	{
		Lock.unlock();
		Work();
		Lock.lock();

		// sleep until the next round, or until Stop()
		m_Wakeup.wait_for(Lock, m_Interval, [this]() { return m_bStop.load(); });
	}

	kDebug(1, "stopped after {} rounds", m_iRounds);

	return 0;

} // Main

//-----------------------------------------------------------------------------
int main (int argc, char** argv)
//-----------------------------------------------------------------------------
{
	auto RunMain = [](int ac, char** av)
	{
		try
		{
			return __ProjectName__().Main (ac, av);
		}
		catch (const KException& ex)
		{
			KErr.FormatLine(">> {}: {}", __ProjectName__::s_sProjectName, ex.what());
		}
		catch (const std::exception& ex)
		{
			KErr.FormatLine(">> {}: {}", __ProjectName__::s_sProjectName, ex.what());
		}
		return 1;
	};

	// KService::Run() handles -install, -uninstall, -start, -stop and -status, and runs
	// Main() under systemd, launchd or the Windows service manager, or interactively
	return KService::Run("__LowerProjectName__", argc, argv, RunMain,
		/*bSuppressHelp*/true, // the service options are part of our own help
		__ProjectName__::s_sProjectName,
		"dekaf2 __ProjectType__ template");

} // main

#ifdef DEKAF2_REPEAT_CONSTEXPR_VARIABLE
constexpr KStringViewZ __ProjectName__::s_sProjectName;
constexpr KStringViewZ __ProjectName__::s_sProjectVersion;
#endif
