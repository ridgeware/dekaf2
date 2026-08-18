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

#include "catch.hpp"

#include <dekaf2/util/mail/ksmtp.h>
#include <dekaf2/util/mail/kmail.h>
#include <dekaf2/net/tcp/ktcpserver.h>
#include <dekaf2/net/util/kiostreamsocket.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/format/kformat.h>
#include <memory>
#include <mutex>

using namespace dekaf2;

namespace {

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
// a minimal plaintext MTA, enough dialogue for KSMTP::Connect() - it either
// only speaks HELO, or ESMTP without STARTTLS, or accepts a full mail and
// records the DATA block for inspection
class KFakeMTA : public KTCPServer
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

public:

	enum Mode { HeloOnly, EhloNoStartTLS, AcceptMail };

	KFakeMTA(uint16_t iPort, Mode mode)
	: KTCPServer(iPort, false, 3)
	, m_Mode(mode)
	{
	}

	KString GetData()
	{
		std::lock_guard<std::mutex> Lock(m_Mutex);
		return m_sData;
	}

protected:

	virtual void Session(std::unique_ptr<KIOStreamSocket>& Stream) override
	{
		Stream->SetReaderRightTrim("\r\n");
		Stream->SetWriterEndOfLine("\r\n");

		if (!Stream->WriteLine("220 fake ESMTP").Flush().Good())
		{
			return;
		}

		KString sLine;

		while (Stream->ReadLine(sLine))
		{
			if (sLine.starts_with("EHLO"))
			{
				if (m_Mode == HeloOnly)
				{
					Stream->WriteLine("502 command not implemented");
				}
				else
				{
					Stream->WriteLine("250-fake");
					Stream->WriteLine("250 AUTH LOGIN PLAIN");
				}
			}
			else if (sLine.starts_with("HELO"))
			{
				Stream->WriteLine("250 fake");
			}
			else if (m_Mode == AcceptMail &&
					 (sLine.starts_with("MAIL FROM") || sLine.starts_with("RCPT TO")))
			{
				Stream->WriteLine("250 OK");
			}
			else if (m_Mode == AcceptMail && sLine == "DATA")
			{
				if (!Stream->WriteLine("354 go ahead").Flush().Good())
				{
					return;
				}

				KString sData;

				while (Stream->ReadLine(sLine) && sLine != ".")
				{
					sData += sLine;
					sData += '\n';
				}

				{
					std::lock_guard<std::mutex> Lock(m_Mutex);
					m_sData = std::move(sData);
				}

				Stream->WriteLine("250 queued");
			}
			else if (sLine.starts_with("QUIT"))
			{
				Stream->WriteLine("221 bye").Flush();
				return;
			}
			else
			{
				Stream->WriteLine("500 unknown command");
			}

			if (!Stream->Flush().Good())
			{
				return;
			}
		}
	}

	Mode m_Mode;
	std::mutex m_Mutex;
	KString m_sData;

}; // KFakeMTA

} // end of anonymous namespace

TEST_CASE("KSMTP")
{
	SECTION("plain SMTP server (HELO only)")
	{
		KFakeMTA MTA(7830, KFakeMTA::HeloOnly);
		REQUIRE ( MTA.Start(chrono::seconds(5), false) );

		// without credentials the plaintext session is fine
		{
			KSMTP SMTP;
			SMTP.SetTimeout(chrono::seconds(3));
			CHECK ( SMTP.Connect(KURL("smtp://127.0.0.1:7830")) == true );
			SMTP.Disconnect();
		}

		// given credentials must not be dropped silently
		{
			KSMTP SMTP;
			SMTP.SetTimeout(chrono::seconds(3));
			CHECK ( SMTP.Connect(KURL("smtp://127.0.0.1:7830"), "alice", "secret") == false );
			CHECK ( SMTP.Error().contains("authentication") );
		}

		// required encryption must not silently degrade to plaintext
		{
			KSMTP SMTP;
			SMTP.SetTimeout(chrono::seconds(3));
			SMTP.SetRequireTLS();
			CHECK ( SMTP.Connect(KURL("smtp://127.0.0.1:7830")) == false );
			CHECK ( SMTP.Error().contains("encryption required") );
		}
	}

	SECTION("ESMTP server without STARTTLS")
	{
		KFakeMTA MTA(7831, KFakeMTA::EhloNoStartTLS);
		REQUIRE ( MTA.Start(chrono::seconds(5), false) );

		// opportunistic mode accepts the plaintext session
		{
			KSMTP SMTP;
			SMTP.SetTimeout(chrono::seconds(3));
			CHECK ( SMTP.Connect(KURL("smtp://127.0.0.1:7831")) == true );
			SMTP.Disconnect();
		}

		// but not when encryption is required
		{
			KSMTP SMTP;
			SMTP.SetTimeout(chrono::seconds(3));
			SMTP.SetRequireTLS();
			CHECK ( SMTP.Connect(KURL("smtp://127.0.0.1:7831")) == false );
			CHECK ( SMTP.Error().contains("STARTTLS") );
		}

		// the advertised AUTH does not help without encryption (existing gate)
		{
			KSMTP SMTP;
			SMTP.SetTimeout(chrono::seconds(3));
			CHECK ( SMTP.Connect(KURL("smtp://alice:secret@127.0.0.1:7831")) == false );
			CHECK ( SMTP.Error().contains("encryption") );
		}
	}

	SECTION("Reply-To header")
	{
		KFakeMTA MTA(7832, KFakeMTA::AcceptMail);
		REQUIRE ( MTA.Start(chrono::seconds(5), false) );

		auto SendMail = [](const KMail& Mail) -> bool
		{
			KSMTP SMTP;
			SMTP.SetTimeout(chrono::seconds(3));

			if (!SMTP.Connect(KURL("smtp://127.0.0.1:7832")))
			{
				return false;
			}

			bool bOK = SMTP.Send(Mail);
			SMTP.Disconnect();
			return bOK;
		};

		auto GetHeader = [](const KString& sData, KStringView sHeader) -> KString
		{
			for (const auto& sLine : sData.Split("\n"))
			{
				if (sLine.starts_with(sHeader))
				{
					return sLine;
				}
			}
			return {};
		};

		KMail Mail;
		Mail.From("server@example.com", "Server");
		Mail.To("user@example.com");
		Mail.Subject("test");
		Mail.Message("hello");

		CHECK ( Mail.ReplyTo().empty() );

		// without ReplyTo() the header mirrors From, as it always did
		CHECK ( SendMail(Mail) );
		auto sData = MTA.GetData();
		CHECK ( GetHeader(sData, "From:"    ).contains("server@example.com") );
		CHECK ( GetHeader(sData, "Reply-To:").contains("server@example.com") );

		// an explicitly set Reply-To wins, and only the last set value counts
		Mail.ReplyTo("nobody@example.com");
		Mail.ReplyTo("human@example.com", "Human");
		CHECK ( Mail.ReplyTo().size() == 1 );

		CHECK ( SendMail(Mail) );
		sData = MTA.GetData();
		CHECK ( GetHeader(sData, "From:"    ).contains("server@example.com") );
		CHECK ( GetHeader(sData, "Reply-To:").contains("human@example.com") );
		CHECK ( !sData.contains("nobody@example.com") );
	}
}
