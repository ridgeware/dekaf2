#include "catch.hpp"

#include <dekaf2/util/mail/kmailspool.h>
#include <dekaf2/net/tcp/ktcpserver.h>
#include <dekaf2/system/filesystem/kfilesystem.h>
#include <dekaf2/system/os/ksystem.h>
#include <memory>
#include <mutex>
#include <vector>

using namespace dekaf2;

namespace {

class FakeSMTPServer : public KTCPServer
{

public:

	FakeSMTPServer(uint16_t iPort, bool bRejectRcpt = false)
	: KTCPServer(iPort, false, 5)
	, m_bRejectRcpt(bRejectRcpt)
	{
	}

	using KTCPServer::GetPort;

	std::size_t GetReceivedCount() const
	{
		std::lock_guard<std::mutex> Lock(m_Mutex);
		return m_Messages.size();
	}

	KString GetMessage(std::size_t iIndex) const
	{
		std::lock_guard<std::mutex> Lock(m_Mutex);
		return iIndex < m_Messages.size() ? m_Messages[iIndex] : KString{};
	}

protected:

	void Session(KStream& stream, KStringView sRemoteEndPoint, int iSocketFd) override
	{
		stream.SetReaderRightTrim("\r\n");
		stream.SetWriterEndOfLine("\r\n");
		stream.WriteLine("220 fake ESMTP").Flush();

		bool    bData { false };
		KString sMessage;
		KString sLine;

		while (stream.ReadLine(sLine))
		{
			if (bData)
			{
				if (sLine == ".")
				{
					bData = false;
					std::lock_guard<std::mutex> Lock(m_Mutex);
					m_Messages.push_back(std::move(sMessage));
					sMessage.clear();
					stream.WriteLine("250 queued").Flush();
				}
				else
				{
					sMessage += sLine;
					sMessage += '\n';
				}
			}
			else if (sLine.starts_with("EHLO"))
			{
				stream.WriteLine("250-fake").WriteLine("250 OK").Flush();
			}
			else if (sLine.starts_with("RCPT"))
			{
				stream.WriteLine(m_bRejectRcpt ? "550 no such user" : "250 OK").Flush();
			}
			else if (sLine == "DATA")
			{
				bData = true;
				stream.WriteLine("354 go ahead").Flush();
			}
			else if (sLine == "QUIT")
			{
				stream.WriteLine("221 bye").Flush();
				return;
			}
			else
			{
				stream.WriteLine("250 OK").Flush();
			}
		}
	}

private:

	mutable std::mutex   m_Mutex;
	std::vector<KString> m_Messages;
	bool                 m_bRejectRcpt;

}; // FakeSMTPServer

std::unique_ptr<FakeSMTPServer> StartFakeServer(bool bRejectRcpt = false)
{
	static uint16_t s_iNextPort = 30000 + (kGetPid() % 10000);

	for (int i = 0; i < 20; ++i)
	{
		auto Server = std::make_unique<FakeSMTPServer>(s_iNextPort++, bRejectRcpt);

		if (Server->Start(chrono::seconds(2), false))
		{
			return Server;
		}
	}

	return nullptr;
}

template<typename Check>
bool WaitFor(KDuration Timeout, Check check)
{
	for (auto iRounds = Timeout / chrono::milliseconds(10); iRounds > 0; --iRounds)
	{
		if (check())
		{
			return true;
		}
		kSleep(chrono::milliseconds(10));
	}

	return check();
}

std::size_t CountSpoolFiles(KStringViewZ sDir)
{
	std::size_t iCount { 0 };

	for (const auto& File : KDirectory(sDir, KFileType::FILE))
	{
		if (File.Filename().ends_with(".mail"))
		{
			++iCount;
		}
	}

	return iCount;
}

KMail MakeMail(KStringView sSubject, KStringView sBody)
{
	KMail Mail;
	Mail.From("sender@example.com", "TestSender");
	Mail.To("rcpt@example.com", "TestRecipient");
	Mail.Cc("copy@example.com");
	Mail.Subject(sSubject);
	Mail.Message(sBody);
	return Mail;
}

} // end of anonymous namespace

TEST_CASE("KMailSpool")
{
	SECTION("direct delivery without disk contact")
	{
		auto Server = StartFakeServer();
		REQUIRE( Server != nullptr );

		KTempDir SpoolDir;

		KMailSpool::Options Options;
		Options.sSpoolDir = SpoolDir.Name();
		Options.Relay     = KURL(kFormat("smtp://localhost:{}", Server->GetPort()));
		Options.Timeout   = chrono::seconds(5);

		{
			KMailSpool Spool(Options);
			CHECK_FALSE ( Spool.HasError() );

			CHECK ( Spool.Add(MakeMail("mail-one", "body one")) );
			CHECK ( Spool.Add(MakeMail("mail-two", "body two")) );

			CHECK ( WaitFor(chrono::seconds(10), [&]()
			{
				return Spool.GetQueueSize() == 0 && Server->GetReceivedCount() == 2;
			}));
		}

		CHECK ( Server->GetReceivedCount() == 2 );
		CHECK ( CountSpoolFiles(SpoolDir.Name()) == 0 );

		auto sMessage = Server->GetMessage(0);
		CHECK ( sMessage.contains("mail-one") );
		CHECK ( sMessage.contains("body one") );
		CHECK ( sMessage.contains("sender@example.com") );
		CHECK ( sMessage.contains("rcpt@example.com") );
	}

	SECTION("spool on failure, redeliver after restart")
	{
		KTempDir SpoolDir;

		KMailSpool::Options Options;
		Options.sSpoolDir     = SpoolDir.Name();
		// nothing listens on this port
		Options.Relay         = KURL("smtp://localhost:1");
		Options.RetryInterval = chrono::milliseconds(50);
		Options.Timeout       = chrono::seconds(1);

		{
			KMailSpool Spool(Options);

			CHECK ( Spool.Add(MakeMail("spooled-mail", "spooled body")) );

			CHECK ( WaitFor(chrono::seconds(5), [&]()
			{
				return CountSpoolFiles(SpoolDir.Name()) == 1;
			}));

			CHECK ( Spool.GetQueueSize() == 1 );
		}

		// the mail survives the "restart"
		CHECK ( CountSpoolFiles(SpoolDir.Name()) == 1 );

		auto Server = StartFakeServer();
		REQUIRE( Server != nullptr );

		Options.Relay = KURL(kFormat("smtp://localhost:{}", Server->GetPort()));

		{
			KMailSpool Spool(Options);

			CHECK ( WaitFor(chrono::seconds(10), [&]()
			{
				return Spool.GetQueueSize() == 0 && Server->GetReceivedCount() == 1;
			}));
		}

		CHECK ( Server->GetReceivedCount() == 1 );
		CHECK ( CountSpoolFiles(SpoolDir.Name()) == 0 );

		auto sMessage = Server->GetMessage(0);
		CHECK ( sMessage.contains("spooled-mail") );
		CHECK ( sMessage.contains("spooled body") );
		CHECK ( sMessage.contains("TestSender") );
		CHECK ( sMessage.contains("rcpt@example.com") );
		CHECK ( sMessage.contains("copy@example.com") );
	}

	SECTION("successful connection flushes all spooled mail")
	{
		KTempDir SpoolDir;

		uint16_t iPort;

		{
			// find a free port, then close the server again
			auto Probe = StartFakeServer();
			REQUIRE( Probe != nullptr );
			iPort = Probe->GetPort();
		}

		KMailSpool::Options Options;
		Options.sSpoolDir     = SpoolDir.Name();
		Options.Relay         = KURL(kFormat("smtp://localhost:{}", iPort));
		// no scheduled retry within this test
		Options.RetryInterval = chrono::hours(1);
		Options.Timeout       = chrono::seconds(1);

		KMailSpool Spool(Options);

		CHECK ( Spool.Add(MakeMail("waiting-one", "waiting body")) );
		CHECK ( Spool.Add(MakeMail("waiting-two", "waiting body")) );

		CHECK ( WaitFor(chrono::seconds(5), [&]()
		{
			return CountSpoolFiles(SpoolDir.Name()) == 2;
		}));

		// now the relay comes up on the port
		auto Server = std::make_unique<FakeSMTPServer>(iPort);
		REQUIRE( Server->Start(chrono::seconds(2), false) );

		// a fresh mail triggers the connection - the spooled mail
		// has to ride along despite its one hour retry delay
		CHECK ( Spool.Add(MakeMail("fresh-mail", "fresh body")) );

		CHECK ( WaitFor(chrono::seconds(10), [&]()
		{
			return Spool.GetQueueSize() == 0 && Server->GetReceivedCount() == 3;
		}));

		CHECK ( Server->GetReceivedCount() == 3 );
		CHECK ( CountSpoolFiles(SpoolDir.Name()) == 0 );
	}

	SECTION("permanent rejection drops the mail")
	{
		auto Server = StartFakeServer(true);
		REQUIRE( Server != nullptr );

		KTempDir SpoolDir;

		KMailSpool::Options Options;
		Options.sSpoolDir = SpoolDir.Name();
		Options.Relay     = KURL(kFormat("smtp://localhost:{}", Server->GetPort()));
		Options.Timeout   = chrono::seconds(5);

		{
			KMailSpool Spool(Options);

			CHECK ( Spool.Add(MakeMail("rejected-mail", "rejected body")) );

			CHECK ( WaitFor(chrono::seconds(10), [&]()
			{
				return Spool.GetQueueSize() == 0;
			}));
		}

		CHECK ( Server->GetReceivedCount() == 0 );
		CHECK ( CountSpoolFiles(SpoolDir.Name()) == 0 );
	}

	SECTION("overaged mail is still delivered when the relay comes back")
	{
		KTempDir SpoolDir;

		uint16_t iPort;

		{
			// find a free port, then close the server again
			auto Probe = StartFakeServer();
			REQUIRE( Probe != nullptr );
			iPort = Probe->GetPort();
		}

		KMailSpool::Options Options;
		Options.sSpoolDir     = SpoolDir.Name();
		// pin the relay to IPv4 - "localhost" would make the failing connect
		// try ::1 first and double its duration
		Options.Relay         = KURL(kFormat("smtp://127.0.0.1:{}", iPort));
		// no scheduled retry within this test, MaxAge exceeded by the sleep below.
		// MaxAge has to comfortably outlast ONE failed connect attempt, or the
		// mail is already overaged at its first reschedule and gets dropped -
		// on Windows a refused loopback connect costs around a second (winsock
		// retries the SYN after a RST), not microseconds as on POSIX
		Options.RetryInterval = chrono::hours(1);
		Options.MaxAge        = chrono::seconds(2);
		Options.Timeout       = chrono::seconds(1);

		KMailSpool Spool(Options);

		CHECK ( Spool.Add(MakeMail("late-mail", "late body")) );

		CHECK ( WaitFor(chrono::seconds(5), [&]()
		{
			return CountSpoolFiles(SpoolDir.Name()) == 1;
		}));

		// age the mail beyond MaxAge, then bring up the relay
		kSleep(chrono::milliseconds(2500));

		auto Server = std::make_unique<FakeSMTPServer>(iPort);
		REQUIRE( Server->Start(chrono::seconds(2), false) );

		// a fresh mail triggers the connection - the overaged mail has to
		// be delivered, not dropped
		CHECK ( Spool.Add(MakeMail("fresh-mail", "fresh body")) );

		CHECK ( WaitFor(chrono::seconds(10), [&]()
		{
			return Spool.GetQueueSize() == 0 && Server->GetReceivedCount() == 2;
		}));

		CHECK ( Server->GetReceivedCount() == 2 );
		CHECK ( CountSpoolFiles(SpoolDir.Name()) == 0 );
	}

	SECTION("gives up after MaxAge")
	{
		KTempDir SpoolDir;

		KMailSpool::Options Options;
		Options.sSpoolDir     = SpoolDir.Name();
		// nothing listens on this port
		Options.Relay         = KURL("smtp://localhost:1");
		Options.RetryInterval = chrono::milliseconds(50);
		Options.MaxAge        = chrono::milliseconds(200);
		Options.Timeout       = chrono::seconds(1);

		KMailSpool Spool(Options);

		CHECK ( Spool.Add(MakeMail("expiring-mail", "expiring body")) );

		CHECK ( WaitFor(chrono::seconds(10), [&]()
		{
			return Spool.GetQueueSize() == 0;
		}));

		CHECK ( CountSpoolFiles(SpoolDir.Name()) == 0 );
	}

	SECTION("incomplete mail is refused")
	{
		KTempDir SpoolDir;

		KMailSpool::Options Options;
		Options.sSpoolDir = SpoolDir.Name();
		Options.Relay     = KURL("smtp://localhost:1");

		KMailSpool Spool(Options);

		KMail Mail;
		Mail.To("rcpt@example.com");

		CHECK_FALSE ( Spool.Add(std::move(Mail)) );
		CHECK       ( Spool.GetQueueSize() == 0  );
	}
}
