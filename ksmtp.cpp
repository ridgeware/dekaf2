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
		m_sError = "missing To address";
		return false;
	}

	if (m_From.empty())
	{
		m_sError = "missing From address";
		return false;
	}

	if (m_Subject.empty())
	{
		m_sError = "missing subject";
		return false;
	}

	if (m_Message.empty())
	{
		kDebug(1, "no body in mail");
	}

	m_sError.clear();

	return true;

} // Good

//-----------------------------------------------------------------------------
const KString& KMail::Error() const
//-----------------------------------------------------------------------------
{
	return m_sError;
}

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
bool KMail::Send(const KURL& URL, bool bForceSSL)
//-----------------------------------------------------------------------------
{
	if (!Good())
	{
		return false;
	}

	KSMTP server;

	if (!server.Connect(URL, bForceSSL))
	{
		m_sError = server.Error();
		return false;
	}

	if (!server.Send(*this))
	{
		m_sError = server.Error();
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
		m_sError = "no connection to SMTP server";
		return false;
	}

	if (!sTx.empty())
	{
		if (!(*m_Connection)->WriteLine(sTx).Flush().Good())
		{
			m_sError = "cannot send to SMTP server";
			Disconnect();
			return false;
		}
	}

	if (!sRx.empty())
	{
		KString sLine;

		if (!(*m_Connection)->ReadLine(sLine))
		{
			m_sError = "cannot receive from SMTP server";
			Disconnect();
			return false;
		}

		if (!sLine.StartsWith(sRx))
		{
			m_sError.Format("SMTP server responded with '{}' instead of '{}' on query '{}'", sLine, sRx, sTx);
			Disconnect();
			return false;
		}
	}

	m_sError.clear();

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

} // PrettyPrint

//-----------------------------------------------------------------------------
bool KSMTP::SendDottedMessage(KStringView sMessage)
//-----------------------------------------------------------------------------
{
	for (;!sMessage.empty();)
	{
		auto pos = sMessage.find("\n.");
		if (pos != KStringView::npos)
		{
			pos += 2;
			if (!(*m_Connection)->Write(sMessage.substr(0, pos)).Good())
			{
				return false;
			}
			sMessage.remove_prefix(pos);
			if (!(*m_Connection)->Write('.').Good())
			{
				return false;
			}
		}
		else
		{
			return (*m_Connection)->Write(sMessage).Good();
		}
	}
	return true;

} // SendDottedMessage

//-----------------------------------------------------------------------------
bool KSMTP::Send(const KMail& Mail)
//-----------------------------------------------------------------------------
{
	if (!Mail.Good())
	{
		m_sError.Format("mail not sent: {}", Mail.Error());
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
		KString sReplyTo = "Reply-To: ";
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

	if (!PrettyPrint("To:", Mail.To()))
	{
		return false;
	}

	if (!PrettyPrint("Cc:", Mail.Cc()))
	{
		return false;
	}

	if (!Good())
	{
		return false;
	}

	// empty line ends the header
	if (!(*m_Connection)->WriteLine().Good())
	{
		m_sError = "cannot send end of header";
		Disconnect();
		return false;
	}

	if (!SendDottedMessage(Mail.Message()))
	{
		m_sError = "cannot send mail body";
		Disconnect();
		return false;
	}

	// Talk() adds another \r\n at the end, which terminates the message
	if (!Talk("\r\n.", "250"))
	{
		return false;
	}

	(*m_Connection)->Flush();

	return true;

} // Send

//-----------------------------------------------------------------------------
bool KSMTP::Connect(const KURL& URL, bool bForceSSL)
//-----------------------------------------------------------------------------
{
	kDebug(1, "connecting to SMTP server {} on port {}", URL.Domain.Serialize(), URL.Port.Serialize());

	m_sError.clear();

	m_Connection = KConnection::Create(URL, bForceSSL);

	if (!Good())
	{
		m_sError.Format("cannot connect to SMTP server {}:{} - {}", URL.Domain.Serialize(), URL.Port.Serialize(), Error());
		return false;
	}

	(*m_Connection)->SetWriterEndOfLine("\r\n");
	(*m_Connection)->SetReaderRightTrim("\r\n");

	m_Connection->SetTimeout(m_iTimeout);

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
void KSMTP::Disconnect()
//-----------------------------------------------------------------------------
{
	if (Good())
	{
		Talk("QUIT", "221");
	}
	if (m_Connection)
	{
		m_Connection->Disconnect();
	}

} // Disconnect

//-----------------------------------------------------------------------------
bool KSMTP::Good() const
//-----------------------------------------------------------------------------
{
	return m_Connection && m_Connection->Good();
}

//-----------------------------------------------------------------------------
KString KSMTP::Error() const
//-----------------------------------------------------------------------------
{
	KString sReturn;

	if (m_Connection)
	{
		sReturn = m_Connection->Error();
	}

	if (sReturn.empty() && !m_sError.empty())
	{
		sReturn = m_sError;
	}

	return sReturn;

} // Error

//-----------------------------------------------------------------------------
void KSMTP::SetTimeout(uint16_t iSeconds)
//-----------------------------------------------------------------------------
{
	m_iTimeout = iSeconds;
}


} // end of namespace dekaf2
