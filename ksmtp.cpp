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
#include "ksplit.h"
#include "kbase64.h"
#include "klog.h"
#include "kquotedprintable.h"


namespace dekaf2 {

//-----------------------------------------------------------------------------
bool KSMTP::Talk(KStringView sTx, KStringView sRx, ESMTPParms* parms, bool bDisconnectOnFailure)
//-----------------------------------------------------------------------------
{
	if (!Good())
	{
		m_sError = "no connection to SMTP server";
		return false;
	}

	if (!sTx.empty())
	{
		kDebug(3, "TX: {}", sTx);
		
		if (!(*m_Connection)->WriteLine(sTx).Flush().Good())
		{
			m_sError = "cannot send to SMTP server";
			if (bDisconnectOnFailure)
			{
				Disconnect();
			}
			return false;
		}
	}

	if (!sRx.empty())
	{
		KString sLine;

		// be prepared for multiline responses like
		// 250-xyz
		// 250-abc
		// 250 OK
		// (in which case we read until the last line, and evaluate only that one)
		for (;;)
		{

			if (!(*m_Connection)->ReadLine(sLine))
			{
				m_sError = "cannot receive from SMTP server";
				if (bDisconnectOnFailure)
				{
					Disconnect();
				}
				return false;
			}

			kDebug(3, "RX: {}", sLine);

			if (sLine.size() > 3 && sLine[3] != ' ')
			{
				if (parms)
				{
					KStringView sParms = sLine;
					sParms.remove_prefix(4);
					// add key and value to parms
					kSplit(*parms, sParms, "\r\n", " ");
				}
				// this is a continuation line.. skip it
				continue;
			}

			if (!sLine.starts_with(sRx))
			{
				m_sError.Format("SMTP server responded with '{}' instead of '{}' on query '{}'", sLine, sRx, sTx);
				if (bDisconnectOnFailure)
				{
					Disconnect();
				}
				return false;
			}

			// success
			if (parms)
			{
				KStringView sParms = sLine;
				sParms.remove_prefix(4);
				// add key and value to parms
				kSplit(*parms, sLine, "\r\n", " ");
			}

			break;
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
			sString += ",\r\n ";
		}

		if (!it.second.empty())
		{
			sString += KQuotedPrintable::Encode(it.second, true);
			sString += " <";
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

	if (!Talk("MIME-Version: 1.0", ""))
	{
		return false;
	}

	KString sDate("Date: ");
	sDate += kFormTimestamp(Mail.Time(), "%a, %d %b %Y %H:%M:%S %z");

	if (!Talk(sDate, ""))
	{
		return false;
	}

	if (!Talk(kFormat("Subject: {}", KQuotedPrintable::Encode(Mail.Subject(), true)), ""))
	{
		return false;
	}

	if (!PrettyPrint("From", Mail.From()))
	{
		return false;
	}

	if (!PrettyPrint("Reply-To", Mail.From()))
	{
		return false;
	}

	if (!PrettyPrint("To", Mail.To()))
	{
		return false;
	}

	if (!PrettyPrint("Cc", Mail.Cc()))
	{
		return false;
	}

	if (!Good())
	{
		return false;
	}

	// the KMIMEPart serializer guarantees that no dots start a new line,
	// therefore we do not need to filter for them again with SendDottedMessage()
	if (!(*m_Connection)->Write(Mail.Serialize()).Good())
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
bool KSMTP::Connect(const KURL& Relay, KStringView sUsername, KStringView sPassword)
//-----------------------------------------------------------------------------
{
	kDebug(1, "connecting to SMTP server {} on port {}", Relay.Domain.Serialize(), Relay.Port.Serialize());

	Disconnect();

	m_sError.clear();

	// force SSL socket for opportunistic TLS
	m_Connection = KConnection::Create(Relay, true);

	if (!Good())
	{
		m_sError.Format("cannot connect to SMTP server {}:{} - {}", Relay.Domain.Serialize(), Relay.Port.Serialize(), Error());
		return false;
	}

	if (m_Connection->IsTLS())
	{
		// we want an opportunistic TLS handshake (after having issued STARTTLS)
		m_Connection->SetManualTLSHandshake(true);
	}

	(*m_Connection)->SetWriterEndOfLine("\r\n");
	(*m_Connection)->SetReaderRightTrim("\r\n");

	m_Connection->SetTimeout(m_iTimeout);

	// get initial welcome message
	if (!Talk("", "220"))
	{
		return false;
	}

	// try ESMTP
	ESMTPParms Parms;
	
	if (!Talk(kFormat("EHLO {}", "localhost"), "250", &Parms, false))
	{
		// failed. try SMTP
		if (!Talk(kFormat("HELO {}", "localhost"), "250"))
		{
			return false;
		}
		else
		{
			// SMTP success. No authentication, no STARTTLS
			return true;
		}
	}

	bool bIsTLS { false };

	if (m_Connection->IsTLS() && Parms.find("STARTTLS") != Parms.end())
	{
		// prepare for TLS handshake
		if (!Talk("STARTTLS", "220"))
		{
			return false;
		}

		// and finally kick off the TLS negotiation
		if (!m_Connection->StartManualTLSHandshake())
		{
			return false;
		}

		// clear the initial feature list from the unencrypted connection
		Parms.clear();

		// we have now switched to TLS, redo the EHLO (the server may now
		// advertise different features)
		if (!Talk("EHLO localhost", "250", &Parms))
		{
			return false;
		}

		bIsTLS = true;
	}

	if (sUsername.empty() && sPassword.empty())
	{
		// check if we have username and password in the URL
		sUsername = Relay.User.get();
		sPassword = Relay.Password.get();
	}

	// evaluate ESMTP response
	if (!sUsername.empty() && sPassword.empty())
	{
		m_sError = "missing password";
		return false;
	}
	else if (sUsername.empty() && !sPassword.empty())
	{
		m_sError = "missing username";
		return false;
	}
	else if (sUsername.empty() && sPassword.empty())
	{
		return true;
	}

	if (!bIsTLS)
	{
		m_sError = "cannot authenticate without encryption";
		return false;
	}

	// check for SMTP-Auth
	const KString& sAuth = Parms["AUTH"];

	if (sAuth.find("LOGIN") != KString::npos)
	{
		// try a LOGIN style authentication
		if (!Talk("AUTH LOGIN", "334"))
		{
			return false;
		}
		if (!Talk(KBase64::Encode(sUsername), "334"))
		{
			return false;
		}
		if (Talk(KBase64::Encode(sPassword), "235"))
		{
			return true;
		}
	}
	else if (sAuth.find("PLAIN") != KString::npos)
	{
		// try a PLAIN style authentication
		KString sCmd;
		sCmd += sUsername;
		sCmd += '\0';
		sCmd += sUsername;
		sCmd += '\0';
		sCmd += sPassword;
		if (Talk(kFormat("AUTH PLAIN {}", KBase64::Encode(sCmd)), "235"))
		{
			return true;
		}
	}

	m_sError.Format("Cannot authenticate with server - announced capabilities are {}", sAuth);

	return false;

} // Connect

//-----------------------------------------------------------------------------
void KSMTP::Disconnect()
//-----------------------------------------------------------------------------
{
	if (Good())
	{
		Talk("QUIT", "221", nullptr, false);
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
