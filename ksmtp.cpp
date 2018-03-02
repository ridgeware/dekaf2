/*
 //
 // DEKAF(tm): Lighter, Faster, Smarter (tm)
 //
 // Copyright (c) 2017, Ridgeware, Inc.
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

#include "ksmtp.h"
#include "klog.h"


namespace dekaf2 {

//-----------------------------------------------------------------------------
void KMail::To(KStringView sTo, KStringView sPretty)
//-----------------------------------------------------------------------------
{
	Add(m_To, sTo, sPretty);
}

//-----------------------------------------------------------------------------
void KMail::Cc(KStringView sCc, KStringView sPretty)
//-----------------------------------------------------------------------------
{
	Add(m_Cc, sCc, sPretty);
}

//-----------------------------------------------------------------------------
void KMail::Bcc(KStringView sBcc, KStringView sPretty)
//-----------------------------------------------------------------------------
{
	Add(m_Bcc, sBcc, sPretty);
}

//-----------------------------------------------------------------------------
void KMail::From(KStringView sFrom, KStringView sPretty)
//-----------------------------------------------------------------------------
{
	m_From.clear();
	Add(m_From, sFrom, sPretty);
}

//-----------------------------------------------------------------------------
void KMail::Subject(KStringView sSubject)
//-----------------------------------------------------------------------------
{
	m_Subject = sSubject;
}

//-----------------------------------------------------------------------------
void KMail::Message(KString&& sMessage)
//-----------------------------------------------------------------------------
{
	m_Message = std::move(sMessage);
}

//-----------------------------------------------------------------------------
KMail& KMail::operator=(KStringView sMessage)
//-----------------------------------------------------------------------------
{
	Message(sMessage);
	return *this;
}

//-----------------------------------------------------------------------------
KMail& KMail::operator+=(KStringView sMessage)
//-----------------------------------------------------------------------------
{
	return Append(sMessage);
}

//-----------------------------------------------------------------------------
KMail& KMail::Append(KStringView sMessage)
//-----------------------------------------------------------------------------
{
	m_Message += sMessage;
	return *this;
}

//-----------------------------------------------------------------------------
void KMail::Add(map_t& map, KStringView Key, KStringView Value)
//-----------------------------------------------------------------------------
{
	map.emplace(Key, Value);
}

//-----------------------------------------------------------------------------
bool KMail::Good() const
//-----------------------------------------------------------------------------
{
	if (m_To.empty())
	{
		kWarning("missing To address");
		return false;
	}

	if (m_From.empty())
	{
		kWarning("missing From address");
		return false;
	}

	if (m_Subject.empty())
	{
		kWarning("missing subject");
		return false;
	}

	if (m_Message.empty())
	{
		kDebug(1, "no body in mail");
	}

	size_t pos{0};
	for (;;)
	{
		pos = m_Message.find("\n.", pos);
		if (pos == KString::npos)
		{
			break;
		}
		if (++pos < m_Message.size() && (m_Message[pos] == '\r' || m_Message[pos] == '\n'))
		{
			kDebug(1, "message body contains unescaped terminator");
			break;
		}
	}

	return true;

} // Good

//-----------------------------------------------------------------------------
const KMail::map_t& KMail::To() const
//-----------------------------------------------------------------------------
{
	return m_To;
}

//-----------------------------------------------------------------------------
const KMail::map_t& KMail::Cc() const
//-----------------------------------------------------------------------------
{
	return m_Cc;
}

//-----------------------------------------------------------------------------
const KMail::map_t& KMail::Bcc() const
//-----------------------------------------------------------------------------
{
	return m_Bcc;
}

//-----------------------------------------------------------------------------
const KMail::map_t& KMail::From() const
//-----------------------------------------------------------------------------
{
	return m_From;
}

//-----------------------------------------------------------------------------
KStringView KMail::Subject() const
//-----------------------------------------------------------------------------
{
	return m_Subject;
}

//-----------------------------------------------------------------------------
KStringView KMail::Message() const
//-----------------------------------------------------------------------------
{
	return m_Message;
}

//-----------------------------------------------------------------------------
KMIME KMail::MIME() const
//-----------------------------------------------------------------------------
{
	return m_MimeType;
}

//-----------------------------------------------------------------------------
bool KMail::Send(const KURL& URL)
//-----------------------------------------------------------------------------
{
	KSMTP server;

	if (!server.Connect(URL))
	{
		return false;
	}

	if (!server.Send(*this))
	{
		return false;
	}

	return true;

} // Send


//-----------------------------------------------------------------------------
bool KSMTP::Talk(KStringView sTx, KStringView sRx)
//-----------------------------------------------------------------------------
{
	if (!Good())
	{
		kWarning("no connection to SMTP server");
		return false;
	}

	if (!sTx.empty())
	{
		ExpiresFromNow();

		if (!m_Stream->WriteLine(sTx).Flush().Good())
		{
			kWarning("cannot send to SMTP server");
			Disconnect();
			return false;
		}
	}

	if (!sRx.empty())
	{
		KString sLine;

		ExpiresFromNow();

		if (!m_Stream->ReadLine(sLine))
		{
			kWarning("cannot receive from SMTP server");
			Disconnect();
			return false;
		}

		if (!sLine.StartsWith(sRx))
		{
			kWarning("SMTP server responded with '{}' instead of '{}' on query '{}'", sLine, sRx, sTx);
			Disconnect();
			return false;
		}
	}

	return true;

} // Talk

//-----------------------------------------------------------------------------
bool KSMTP::PrettyPrint(KStringView sHeader, const KMail::map_t& map)
//-----------------------------------------------------------------------------
{
	KString sString = sHeader;
	sString += ": ";
	auto iEmpty = sString.size();

	bool bFirst = true;
	for (const auto& it : map)
	{
		if (bFirst)
		{
			bFirst = false;
		}
		else
		{
			sString += ", ";
		}

		if (!it.second.empty())
		{
			sString += '"';
			sString += it.second;
			sString += "\" <";
			sString += it.first;
			sString += '>';
		}
		else
		{
			sString += it.first;
		}

	}

	if (sString.size() > iEmpty)
	{
		if (!Talk(sString, ""))
		{
			return false;
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
bool KSMTP::Send(const KMail& Mail)
//-----------------------------------------------------------------------------
{
	if (!Mail.Good())
	{
		kWarning("mail not sent");
		return false;
	}

	if (!Talk(kFormat("MAIL FROM:<{}>", Mail.From().begin()->first), "250"))
	{
		return false;
	}

	for (const auto& it : Mail.To())
	{
		if (!Talk(kFormat("RCPT TO:<{}>", it.first), "250"))
		{
			return false;
		}
	}

	for (const auto& it : Mail.Cc())
	{
		if (!Talk(kFormat("RCPT TO:<{}>", it.first), "250"))
		{
			return false;
		}
	}

	for (const auto& it : Mail.Bcc())
	{
		if (!Talk(kFormat("RCPT TO:<{}>", it.first), "250"))
		{
			return false;
		}
	}

	if (!Talk("DATA", "354"))
	{
		return false;
	}

	if (Mail.MIME() != KMIME::NONE)
	{
		if (!Talk("MIME-Version: 1.0", "")
			|| !Talk(kFormat("Content-Type: {}", KStringView(Mail.MIME())), ""))
		{
			return false;
		}
	}

	KString sDate("Date: ");
	sDate += kFormTimestamp(0, "%a, %d %b %Y %H:%M:%S %z");

	if (!Talk(sDate, ""))
	{
		return false;
	}

	if (!Talk(kFormat("Subject: {}", Mail.Subject()), ""))
	{
		return false;
	}

	{
		// this one is special as it fills up two headers
		KString sFrom = "From: ";
		KString sReplyTo = "Reply-To";
		if (!Mail.From().empty())
		{
			sFrom += Mail.From().begin()->first;
			if (!Talk(sFrom, ""))
			{
				return false;
			}
			if (!Mail.From().begin()->second.empty())
			{
				sReplyTo += Mail.From().begin()->second;
				if (!Talk(sReplyTo, ""))
				{
					return false;
				}
			}
		}
	}

	if (!PrettyPrint("To:", Mail.To()));
	{
		return false;
	}

	if (!PrettyPrint("Cc:", Mail.Cc()));
	{
		return false;
	}

	ExpiresFromNow();

	// empty line ends the header
	if (!m_Stream->WriteLine().Good())
	{
		kWarning("cannot send end of header");
		Disconnect();
		return false;
	}

	ExpiresFromNow();

	if (!m_Stream->Write(Mail.Message()).Good())
	{
		kWarning("cannot send mail body");
		Disconnect();
		return false;
	}

	ExpiresFromNow();

	if (!m_Stream->WriteLine().Good()
		|| !m_Stream->Write('.').Good()
		|| !m_Stream->WriteLine().Good())
	{
		kWarning("cannot send EOM");
		Disconnect();
		return false;
	}

	m_Stream->Flush();

	return true;

} // Send

//-----------------------------------------------------------------------------
bool KSMTP::Connect(const KURL& URL)
//-----------------------------------------------------------------------------
{
	std::string domain = URL.Domain.Serialize().ToStdString();
	std::string port;

	if (!URL.Port.empty())
	{
		port = URL.Port.Serialize().ToStdString();
	}
	else
	{
		port = std::to_string(url::KProtocol(url::KProtocol::MAILTO).DefaultPort());
	}

	kDebug(1, "connecting to SMTP server {} on port {}", domain, port);

	m_Stream = std::make_unique<KTCPStream>(domain, port);

	if (!m_Stream || !m_Stream->good())
	{
		kWarning("cannot connect to SMTP server {}:{} - {}", domain, port, Error());
	}

	m_Stream->SetWriterEndOfLine("\r\n");
	m_Stream->SetReaderRightTrim("\r\n");

	if (Talk("", "220")
	 && Talk(kFormat("HELO {}", "localhost"), "250"))
	{
		return true;
	}
	else
	{
		Disconnect();
		return false;
	}

} // Connect

//-----------------------------------------------------------------------------
void KSMTP::ExpiresFromNow()
//-----------------------------------------------------------------------------
{
	m_Stream->expires_from_now(boost::posix_time::seconds(m_iTimeout));
}

//-----------------------------------------------------------------------------
void KSMTP::Disconnect()
//-----------------------------------------------------------------------------
{
	if (Good())
	{
		Talk("QUIT", "250");
	}
	m_Stream.reset();
}

//-----------------------------------------------------------------------------
bool KSMTP::Good() const
//-----------------------------------------------------------------------------
{
	return m_Stream && m_Stream->good();
}

//-----------------------------------------------------------------------------
KString KSMTP::Error()
//-----------------------------------------------------------------------------
{
	if (!m_Stream)
	{
		return KString{};
	}
	return m_Stream->error().message();

} // Error

//-----------------------------------------------------------------------------
void KSMTP::SetTimeout(uint16_t iSeconds)
//-----------------------------------------------------------------------------
{
	m_iTimeout = iSeconds;
}


} // end of namespace dekaf2
