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

#include <dekaf2/util/mail/ksmtp.h>
#include <dekaf2/core/strings/ksplit.h>
#include <dekaf2/crypto/encoding/kbase64.h>
#include <dekaf2/core/logging/klog.h>
#include <dekaf2/crypto/encoding/kquotedprintable.h>
#include <dekaf2/time/clock/ktime.h>


DEKAF2_NAMESPACE_BEGIN

//-----------------------------------------------------------------------------
bool KSMTP::Talk(KStringView sTx, KStringView sRx, ESMTPParms* parms, bool bDisconnectOnFailure)
//-----------------------------------------------------------------------------
{
	if (!Good())
	{
		return SetError("no connection to SMTP server");
	}

	if (!sTx.empty())
	{
		kDebug(3, "TX: {}", sTx);
		
		if (!m_Connection->WriteLine(sTx).Flush().Good())
		{
			m_iLastReplyCode = 0;
			if (bDisconnectOnFailure)
			{
				Disconnect();
			}
			return SetError("cannot send to SMTP server");
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

			if (!m_Connection->ReadLine(sLine))
			{
				m_iLastReplyCode = 0;
				if (bDisconnectOnFailure)
				{
					Disconnect();
				}
				return SetError("cannot receive from SMTP server");
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

			m_iLastReplyCode = KStringView(sLine).Left(3).UInt16();

			if (!sLine.starts_with(sRx))
			{
				if (bDisconnectOnFailure)
				{
					Disconnect();
				}
				return SetError(kFormat("SMTP server responded with '{}' instead of '{}' on query '{}'", sLine, sRx, sTx));
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
		return SetError(kFormat("mail not sent: {}", Mail.GetLastError()));
	}

	// we use this convoluted form for anything bearing a < or > after the {} format
	// placeholder because AppleClang mistakenly attributes that > as the fill
	// instruction for std::format
	if (!Talk(kFormat("MAIL FROM:<{}{}", Mail.From().begin()->first, '>'), "250"))
	{
		return false;
	}

	for (const auto& it : Mail.To())
	{
		if (!Talk(kFormat("RCPT TO:<{}{}", it.first, '>'), "250"))
		{
			return false;
		}
	}

	for (const auto& it : Mail.Cc())
	{
		if (!Talk(kFormat("RCPT TO:<{}{}", it.first, '>'), "250"))
		{
			return false;
		}
	}

	for (const auto& it : Mail.Bcc())
	{
		if (!Talk(kFormat("RCPT TO:<{}{}", it.first, '>'), "250"))
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
	sDate += kFormSMTPTimestamp(Mail.Time());

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

	// Reply-To defaults to From, which keeps mail from existing callers
	// byte-identical - an explicitly set Reply-To wins
	if (!PrettyPrint("Reply-To", Mail.ReplyTo().empty() ? Mail.From() : Mail.ReplyTo()))
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
	if (!m_Connection->Write(Mail.Serialize()).Good())
	{
		Disconnect();
		return SetError("cannot send mail body");
	}

	// Talk() adds another \r\n at the end, which terminates the message
	if (!Talk("\r\n.", "250"))
	{
		return false;
	}

	m_Connection->Flush();

	return true;

} // Send

//-----------------------------------------------------------------------------
bool KSMTP::Connect(const KURL& Relay, KStringView sUsername, KStringView sPassword)
//-----------------------------------------------------------------------------
{
	kDebug(1, "connecting to SMTP server {} on port {}", Relay.Domain.Serialize(), Relay.Port.Serialize());

	Disconnect();

	ClearError();

	m_iLastReplyCode = 0;

	// force TLS socket for opportunistic TLS, do not allow ALPN HTTP2 upgrade
	m_Connection = KIOStreamSocket::Create(Relay, true,
	                                       KStreamOptions(m_bVerifyCerts ? KStreamOptions::VerifyCert
	                                                                     : KStreamOptions::None, m_Timeout));

	if (!Good())
	{
		return SetError(kFormat("cannot connect to SMTP server {}:{} - {}", Relay.Domain.Serialize(), Relay.Port.Serialize(), m_Connection->GetLastError()));
	}

	// on port 465 (submissions) the server expects a TLS handshake right away -
	// on all other ports the session starts in plaintext and upgrades through
	// STARTTLS after the EHLO
	uint16_t iPort = Relay.Port.empty() ? Relay.Protocol.DefaultPort() : Relay.Port.Serialize().UInt16();
	bool bImplicitTLS = (iPort == 465);

	if (m_Connection->IsTLS() && !bImplicitTLS)
	{
		// we want an opportunistic TLS handshake (after having issued STARTTLS)
		m_Connection->SetManualTLSHandshake(true);
	}

	m_Connection->SetWriterEndOfLine("\r\n");
	m_Connection->SetReaderRightTrim("\r\n");

	// get initial welcome message
	if (!Talk("", "220"))
	{
		return false;
	}

	if (sUsername.empty() && sPassword.empty())
	{
		// check if we have username and password in the URL
		sUsername = Relay.User.get();
		sPassword = Relay.Password.get();
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

		// SMTP success - but plain SMTP knows neither STARTTLS nor authentication,
		// so fail loudly instead of silently dropping what the caller asked for
		if (m_bRequireTLS && !bImplicitTLS)
		{
			Disconnect();
			return SetError("encryption required, but the server only speaks plain SMTP");
		}

		if (!sUsername.empty() || !sPassword.empty())
		{
			Disconnect();
			return SetError("credentials given, but the server does not support authentication");
		}

		return true;
	}

	bool bIsTLS { bImplicitTLS };

	if (!bIsTLS && m_Connection->IsTLS() && Parms.find("STARTTLS") != Parms.end())
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

	if (m_bRequireTLS && !bIsTLS)
	{
		Disconnect();
		return SetError("encryption required, but the server does not offer STARTTLS");
	}

	// evaluate ESMTP response
	if (!sUsername.empty() && sPassword.empty())
	{
		return SetError("missing password");
	}
	else if (sUsername.empty() && !sPassword.empty())
	{
		return SetError("missing username");
	}
	else if (sUsername.empty() && sPassword.empty())
	{
		return true;
	}

	if (!bIsTLS)
	{
		return SetError("cannot authenticate without encryption");
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

	return SetError(kFormat("Cannot authenticate with server - announced capabilities are {}", sAuth));

} // Connect

//-----------------------------------------------------------------------------
void KSMTP::Disconnect()
//-----------------------------------------------------------------------------
{
	if (Good())
	{
		// preserve the reply code of the last transaction across the
		// QUIT exchange
		auto iLastReplyCode = m_iLastReplyCode;
		Talk("QUIT", "221", nullptr, false);
		m_iLastReplyCode = iLastReplyCode;
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

DEKAF2_NAMESPACE_END
