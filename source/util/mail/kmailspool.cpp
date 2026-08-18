/*
 //
 // DEKAF(tm): Lighter, Faster, Smarter (tm)
 //
 // Copyright (c) 2026, Ridgeware, Inc.
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

#include <dekaf2/util/mail/kmailspool.h>
#include <dekaf2/util/mail/ksmtp.h>
#include <dekaf2/util/id/kuuid.h>
#include <dekaf2/data/json/kjson2.h>
#include <dekaf2/system/filesystem/kfilesystem.h>
#include <dekaf2/system/os/ksystem.h>
#include <dekaf2/io/readwrite/kreader.h>
#include <dekaf2/io/readwrite/kwriter.h>
#include <dekaf2/core/logging/klog.h>
#include <algorithm>

DEKAF2_NAMESPACE_BEGIN

namespace {

constexpr KStringView sMailExt { ".mail" };
constexpr KStringView sTempExt { ".tmp"  };

//-----------------------------------------------------------------------------
bool ReadEnvelope(KInFile& File, KJSON& jEnvelope)
//-----------------------------------------------------------------------------
{
	KString sEnvelope;

	if (!File.ReadLine(sEnvelope))
	{
		return false;
	}

	KString sError;
	return kjson::Parse(jEnvelope, sEnvelope, sError);

} // ReadEnvelope

} // end of anonymous namespace

//-----------------------------------------------------------------------------
KMailSpool::KMailSpool(Options Options)
//-----------------------------------------------------------------------------
: m_Options(std::move(Options))
{
	if (!kCreateDir(m_Options.sSpoolDir))
	{
		SetError(kFormat("cannot create spool directory '{}'", m_Options.sSpoolDir));
		return;
	}

	auto tNow = KUnixTime::now();

	for (const auto& File : KDirectory(m_Options.sSpoolDir, KFileType::FILE))
	{
		if (File.Filename().ends_with(sTempExt))
		{
			// a spool file half written when a previous run died
			kRemoveFile(File.Path());
			continue;
		}

		if (!File.Filename().ends_with(sMailExt))
		{
			continue;
		}

		KInFile InFile(File.Path());
		KJSON   jEnvelope;

		if (!ReadEnvelope(InFile, jEnvelope))
		{
			kDebug(1, "removing corrupt spool file '{}'", File.Path());
			kRemoveFile(File.Path());
			continue;
		}

		SpoolEntry Entry;
		Entry.sFileName    = File.Filename();
		Entry.tMailTime    = KUnixTime(static_cast<std::time_t>(jEnvelope["time"].Int64()));
		Entry.tNextAttempt = tNow;

		m_Spooled.push_back(std::move(Entry));
	}

	if (!m_Spooled.empty())
	{
		// deliver in the order the mails were spooled - the file names
		// are time sorted UUIDs
		std::sort(m_Spooled.begin(), m_Spooled.end(),
		          [](const SpoolEntry& left, const SpoolEntry& right)
		          { return left.sFileName < right.sFileName; });

		kDebug(1, "resuming delivery of {} spooled mails in '{}'", m_Spooled.size(), m_Options.sSpoolDir);

		std::lock_guard<std::mutex> Lock(m_Mutex);
		StartThread();
	}

} // ctor

//-----------------------------------------------------------------------------
KMailSpool::~KMailSpool()
//-----------------------------------------------------------------------------
{
	{
		std::lock_guard<std::mutex> Lock(m_Mutex);
		m_bQuit = true;
	}

	m_WakeUp.notify_all();

	if (m_Thread.joinable())
	{
		m_Thread.join();
	}

	// spool mail that was not yet attempted, so that it survives the restart
	for (const auto& Mail : m_Fresh)
	{
		SpoolEntry Entry;
		Spool(Mail, Entry);
	}

} // dtor

//-----------------------------------------------------------------------------
bool KMailSpool::Add(KMail Mail)
//-----------------------------------------------------------------------------
{
	if (HasError())
	{
		return false;
	}

	if (!Mail.Good())
	{
		return SetError(Mail.CopyLastError());
	}

	std::lock_guard<std::mutex> Lock(m_Mutex);

	m_Fresh.push_back(std::move(Mail));
	StartThread();
	m_WakeUp.notify_one();

	return true;

} // Add

//-----------------------------------------------------------------------------
std::size_t KMailSpool::GetQueueSize() const
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);
	return m_Fresh.size() + m_Spooled.size() + m_iInFlight;

} // GetQueueSize

//-----------------------------------------------------------------------------
void KMailSpool::StartThread()
//-----------------------------------------------------------------------------
{
	if (!m_bThreadRunning)
	{
		if (m_Thread.joinable())
		{
			// reap a thread that ended after having delivered all mail
			m_Thread.join();
		}

		m_bThreadRunning = true;
		m_Thread = std::thread(&KMailSpool::Run, this);
	}

} // StartThread

//-----------------------------------------------------------------------------
void KMailSpool::Run()
//-----------------------------------------------------------------------------
{
	kSetThreadName("mailspool");

	std::unique_lock<std::mutex> Lock(m_Mutex);

	while (!m_bQuit)
	{
		auto tNow = KUnixTime::now();

		// move all entries due for another attempt out of m_Spooled
		auto itDue = std::stable_partition(m_Spooled.begin(), m_Spooled.end(),
		                                   [tNow](const SpoolEntry& Entry)
		                                   { return Entry.tNextAttempt > tNow; });

		std::vector<SpoolEntry> Due(std::make_move_iterator(itDue),
		                            std::make_move_iterator(m_Spooled.end()));
		m_Spooled.erase(itDue, m_Spooled.end());

		if (m_Fresh.empty() && Due.empty())
		{
			if (m_Spooled.empty())
			{
				// all mail is delivered - end this thread
				break;
			}

			// sleep until the next attempt is due, or until new mail arrives
			auto tNext = std::min_element(m_Spooled.begin(), m_Spooled.end(),
			                              [](const SpoolEntry& left, const SpoolEntry& right)
			                              { return left.tNextAttempt < right.tNextAttempt; })
			             ->tNextAttempt;

			m_WakeUp.wait_for(Lock, tNext - tNow);
			continue;
		}

		auto Fresh = std::move(m_Fresh);
		m_Fresh.clear();

		// count the mail we are working on as still queued
		m_iInFlight = Fresh.size() + Due.size();

		Lock.unlock();

		std::vector<SpoolEntry> Pending;
		bool bConnected = Deliver(std::move(Fresh), std::move(Due), Pending);

		Lock.lock();

		m_iInFlight = 0;

		if (bConnected)
		{
			// the relay is reachable - make all waiting mail due right away,
			// regardless of its retry schedule. Only the entries in Pending
			// keep their backoff, they just failed on this very connection.
			for (auto& Entry : m_Spooled)
			{
				Entry.tNextAttempt = KUnixTime::now();
			}
		}

		std::move(Pending.begin(), Pending.end(), std::back_inserter(m_Spooled));
	}

	m_bThreadRunning = false;

} // Run

//-----------------------------------------------------------------------------
bool KMailSpool::Deliver(std::vector<KMail> Fresh, std::vector<SpoolEntry> Due, std::vector<SpoolEntry>& Pending)
//-----------------------------------------------------------------------------
{
	KSMTP Server;
	Server.SetTimeout(m_Options.Timeout);

	bool bConnected     { false };
	bool bConnectFailed { false };

	auto EnsureConnected = [&]() -> bool
	{
		if (Server.Good())
		{
			return true;
		}

		if (!bConnectFailed)
		{
			if (Server.Connect(m_Options.Relay, m_Options.sUsername, m_Options.sPassword))
			{
				bConnected = true;
				return true;
			}

			bConnectFailed = true;
			kDebug(1, "{}", Server.GetLastError());
		}

		return false;
	};

	// reschedule a failed mail for another attempt, or give up once it has
	// exceeded MaxAge - the age is only checked after a failed attempt, so
	// that an overaged mail is still delivered when the relay comes back
	auto Reschedule = [this, &Pending](SpoolEntry& Entry)
	{
		if (KUnixTime::now() - Entry.tMailTime > m_Options.MaxAge)
		{
			kDebug(1, "mail in '{}' undeliverable for {} - giving up", Entry.sFileName, m_Options.MaxAge);
			kRemoveFile(FullPath(Entry.sFileName));
		}
		else
		{
			++Entry.iAttempts;
			Entry.tNextAttempt = KUnixTime::now() + Backoff(Entry.iAttempts);
			Pending.push_back(std::move(Entry));
		}
	};

	// the spooled mail first - it is older than the fresh one
	for (auto& Entry : Due)
	{
		if (!EnsureConnected())
		{
			Reschedule(Entry);
			continue;
		}

		KMail Mail;

		if (!Restore(Entry, Mail))
		{
			// Restore() has logged the reason
			kRemoveFile(FullPath(Entry.sFileName));
			continue;
		}

		if (Server.Send(Mail))
		{
			kDebug(1, "sent spooled mail '{}'", Mail.Subject());
			kRemoveFile(FullPath(Entry.sFileName));
			continue;
		}

		if (Server.GetLastReplyCode() >= 500)
		{
			kDebug(1, "relay rejected mail '{}': {} - giving up", Mail.Subject(), Server.GetLastError());
			kRemoveFile(FullPath(Entry.sFileName));
			continue;
		}

		Reschedule(Entry);
	}

	for (auto& Mail : Fresh)
	{
		bool bConnected = EnsureConnected();

		if (bConnected && Server.Send(Mail))
		{
			kDebug(1, "sent mail '{}'", Mail.Subject());
			continue;
		}

		if (bConnected && Server.GetLastReplyCode() >= 500)
		{
			kDebug(1, "relay rejected mail '{}': {} - giving up", Mail.Subject(), Server.GetLastError());
			continue;
		}

		SpoolEntry Entry;

		if (Spool(Mail, Entry))
		{
			Reschedule(Entry);
		}
		// else Spool() has logged the loss of the mail

	}

	return bConnected;

} // Deliver

//-----------------------------------------------------------------------------
bool KMailSpool::Spool(const KMail& Mail, SpoolEntry& Entry)
//-----------------------------------------------------------------------------
{
	auto AddAddresses = [](KJSON& jAddresses, const KMail::map_t& map)
	{
		for (const auto& it : map)
		{
			jAddresses[it.first] = it.second;
		}
	};

	KJSON jEnvelope;

	AddAddresses(jEnvelope["from"    ], Mail.From   ());
	AddAddresses(jEnvelope["reply_to"], Mail.ReplyTo());
	AddAddresses(jEnvelope["to"      ], Mail.To     ());
	AddAddresses(jEnvelope["cc"      ], Mail.Cc     ());
	AddAddresses(jEnvelope["bcc"     ], Mail.Bcc    ());
	jEnvelope["subject"] = Mail.Subject();
	jEnvelope["time"]    = Mail.Time().to_time_t();

	KString sName     = KUUID(KUUID::TimeRandom).ToString();
	KString sTempPath = FullPath(sName + sTempExt);
	KString sSpooled  = sName + sMailExt;

	{
		KOutFile File(sTempPath);

		if (!File.is_open()
		 || !File.WriteLine(jEnvelope.dump()).Write(Mail.Serialize()).Good())
		{
			kDebug(1, "cannot write spool file '{}' - mail '{}' is lost", sTempPath, Mail.Subject());
			kRemoveFile(sTempPath);
			return false;
		}
	}

	if (!kRename(sTempPath, FullPath(sSpooled)))
	{
		kDebug(1, "cannot rename spool file '{}' - mail '{}' is lost", sTempPath, Mail.Subject());
		kRemoveFile(sTempPath);
		return false;
	}

	kDebug(1, "spooled mail '{}' as '{}'", Mail.Subject(), sSpooled);

	Entry.sFileName = std::move(sSpooled);
	Entry.tMailTime = Mail.Time();

	return true;

} // Spool

//-----------------------------------------------------------------------------
bool KMailSpool::Restore(const SpoolEntry& Entry, KMail& Mail) const
//-----------------------------------------------------------------------------
{
	auto sPath = FullPath(Entry.sFileName);

	KInFile File(sPath);

	if (!File.is_open())
	{
		kDebug(1, "cannot open spool file '{}'", sPath);
		return false;
	}

	KJSON jEnvelope;

	if (!ReadEnvelope(File, jEnvelope))
	{
		kDebug(1, "corrupt spool file '{}'", sPath);
		return false;
	}

	for (const auto& it : jEnvelope["from"].items())
	{
		Mail.From(it.key(), it.value().String());
	}

	// not present in spool files written before Reply-To existed
	for (const auto& it : jEnvelope["reply_to"].items())
	{
		Mail.ReplyTo(it.key(), it.value().String());
	}

	for (const auto& it : jEnvelope["to"].items())
	{
		Mail.To(it.key(), it.value().String());
	}

	for (const auto& it : jEnvelope["cc"].items())
	{
		Mail.Cc(it.key(), it.value().String());
	}

	for (const auto& it : jEnvelope["bcc"].items())
	{
		Mail.Bcc(it.key(), it.value().String());
	}

	Mail.Subject(jEnvelope["subject"].String());
	Mail.Time(KUnixTime(static_cast<std::time_t>(jEnvelope["time"].Int64())));
	Mail.SerializedBody(File.ReadRemaining());

	if (!Mail.Good())
	{
		kDebug(1, "invalid mail in spool file '{}': {}", sPath, Mail.GetLastError());
		return false;
	}

	return true;

} // Restore

//-----------------------------------------------------------------------------
KString KMailSpool::FullPath(KStringView sFileName) const
//-----------------------------------------------------------------------------
{
	return kFormat("{}{}{}", m_Options.sSpoolDir, kDirSep, sFileName);

} // FullPath

//-----------------------------------------------------------------------------
KDuration KMailSpool::Backoff(uint16_t iAttempts) const
//-----------------------------------------------------------------------------
{
	KDuration Delay = m_Options.RetryInterval;

	while (--iAttempts && Delay < m_Options.MaxRetryInterval)
	{
		Delay *= 2;
	}

	return std::min(Delay, m_Options.MaxRetryInterval);

} // Backoff

DEKAF2_NAMESPACE_END
