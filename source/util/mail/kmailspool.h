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

#pragma once

#include <dekaf2/util/mail/kmail.h>
#include <dekaf2/web/url/kurl.h>
#include <dekaf2/core/errors/kerror.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/strings/kstringview.h>
#include <dekaf2/net/util/kstreamoptions.h>
#include <dekaf2/time/clock/ktime.h>
#include <dekaf2/time/duration/kduration.h>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

/// @file kmailspool.h
/// Adds the KMailSpool class to send KMail asynchronously, spooling mail
/// to disk while it cannot be delivered

DEKAF2_NAMESPACE_BEGIN

/// @addtogroup util_mail
/// @{

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// This class sends KMail asynchronously through KSMTP. Mail that cannot be
/// delivered is spooled to disk and retried until MaxAge is reached,
/// surviving restarts of the process in between. A spool directory is owned
/// exclusively by one instance of this class.
///
/// @par Usage
/// @code
/// // create once at application start
/// KMailSpool::Options Options;
/// Options.sSpoolDir = "/var/spool/myapp/mail";
/// Options.Relay     = "smtp://user:pass@mail.example.com:587";
///
/// KMailSpool Spool(Options);
///
/// // compose and queue mail as usual, from any thread
/// KMail Mail;
/// Mail.From("status@example.com", "My Appliance");
/// Mail.To("ops@example.com");
/// Mail.Subject("disk almost full");
/// Mail.Message("only 2% left on /data");
///
/// Spool.Add(std::move(Mail)); // returns at once, delivery is asynchronous
/// @endcode
///
/// @par Delivery timing
/// Add() wakes the sender thread, which attempts delivery right away - mail
/// that can be delivered never touches the disk. Mail that cannot be
/// delivered is written into the spool directory at once (from that moment
/// on it survives even abrupt power loss) and retried with a delay that
/// starts at RetryInterval and doubles with every failed attempt, capped at
/// MaxRetryInterval. Any successful connection to the relay sends all
/// spooled mail right away, regardless of the retry schedule. After a
/// process restart, spooled mail is attempted immediately, with the retry
/// delay starting over. A mail is dropped (and logged) when a delivery
/// attempt fails beyond MaxAge - never without an attempt, so an overaged
/// mail is still delivered once the relay becomes reachable again.
/// The sender thread only exists while mail is waiting for delivery.
class DEKAF2_PUBLIC KMailSpool : public KErrorBase
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	struct Options
	{
		/// the spool directory, exclusively owned by this instance
		KString   sSpoolDir;
		/// the mail relay, may contain a user's name and pass
		KURL      Relay;
		/// overrides a user's name in Relay
		KString   sUsername;
		/// overrides a user's pass in Relay
		KString   sPassword;
		/// delay before the first retry, doubled with every failed attempt
		KDuration RetryInterval    { chrono::minutes(1) };
		/// upper limit for the retry delay
		KDuration MaxRetryInterval { chrono::hours(1)   };
		/// undeliverable mail is dropped (and logged) once a delivery attempt
		/// fails beyond this age - it is never dropped without an attempt
		KDuration MaxAge           { chrono::hours(72)  };
		/// the SMTP connection timeout
		KDuration Timeout          { KStreamOptions::GetDefaultTimeout() };
	};

	/// Ctor - creates the spool directory if needed, and resumes delivery of
	/// mail spooled by a previous run
	KMailSpool(Options Options);
	/// Dtor - stops delivery, spools mail that was not yet attempted
	~KMailSpool();

	KMailSpool(const KMailSpool&)            = delete;
	KMailSpool(KMailSpool&&)                 = delete;
	KMailSpool& operator=(const KMailSpool&) = delete;
	KMailSpool& operator=(KMailSpool&&)      = delete;

	/// Queue a mail for delivery. Returns false if the mail is not Good().
	/// The mail is only spooled to disk if it cannot be delivered right away.
	bool Add(KMail Mail);

	/// Returns count of mails waiting for delivery
	DEKAF2_NODISCARD
	std::size_t GetQueueSize() const;

//----------
private:
//----------

	struct SpoolEntry
	{
		KString   sFileName; // file name inside the spool directory
		KUnixTime tMailTime; // creation time of the mail
		KUnixTime tNextAttempt;
		uint16_t  iAttempts { 0 };
	};

	/// the sender thread - delivers until no mail is left, then ends itself
	DEKAF2_PRIVATE void Run();
	/// start the sender thread if it is not running - needs m_Mutex held
	DEKAF2_PRIVATE void StartThread();
	/// try to deliver the given mails, spool/reschedule failed ones into
	/// Pending - returns true if a connection to the relay succeeded
	DEKAF2_PRIVATE bool Deliver(std::vector<KMail> Fresh, std::vector<SpoolEntry> Due, std::vector<SpoolEntry>& Pending);
	/// write a mail into the spool directory
	DEKAF2_PRIVATE bool Spool(const KMail& Mail, SpoolEntry& Entry);
	/// read a mail back from the spool directory
	DEKAF2_PRIVATE bool Restore(const SpoolEntry& Entry, KMail& Mail) const;
	/// compose full path name for a file in the spool directory
	DEKAF2_PRIVATE KString FullPath(KStringView sFileName) const;
	/// returns the retry delay after the given count of failed attempts
	DEKAF2_PRIVATE KDuration Backoff(uint16_t iAttempts) const;

	Options                 m_Options;
	std::thread             m_Thread;
	mutable std::mutex      m_Mutex;
	std::condition_variable m_WakeUp;
	std::vector<KMail>      m_Fresh;
	std::vector<SpoolEntry> m_Spooled;
	std::size_t             m_iInFlight      { 0     };
	bool                    m_bThreadRunning { false };
	bool                    m_bQuit          { false };

}; // KMailSpool

/// @}

DEKAF2_NAMESPACE_END
