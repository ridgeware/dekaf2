/*
 //
 // DEKAF(tm): Lighter, Faster, Smarter (tm)
 //
 // Copyright (c) 2018, Ridgeware, Inc.
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
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/io/streams/kstream.h>
#include <dekaf2/web/url/kurl.h>
#include <dekaf2/net/util/kiostreamsocket.h>
#include <dekaf2/containers/associative/kassociative.h>
#include <dekaf2/net/util/kstreamoptions.h>
#include <dekaf2/core/errors/kerror.h>

/// @file ksmtp.h
/// Adds the KSMTP class which sends a KMail via SMTP to an MTA

DEKAF2_NAMESPACE_BEGIN

/// @addtogroup util_mail
/// @{

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// This class speaks the SMTP protocol with a mail relay. It takes a KMail class
/// as the mail to be sent. Multiple mails can be sent consecutively in one
/// session.
class DEKAF2_PUBLIC KSMTP : public KErrorBase
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	/// Ctor - connects to mail relay. Relay may contain a user's name and pass, or
	/// they can be set explicitly with sUsername / sPassword, which will override
	/// anything in the URL.
	KSMTP(const KURL& Relay = KURL{}, KStringView sUsername = KStringView{}, KStringView sPassword = KStringView{})
	{
		if (!Relay.empty())
		{
			Connect(Relay, sUsername, sPassword);
		}
	}

	KSMTP(const KSMTP&) = delete;
	KSMTP(KSMTP&&) = default;
	KSMTP& operator=(const KSMTP&) = delete;
	KSMTP& operator=(KSMTP&&) = default;

	/// Connect to mail relay. Relay may contain a user's name and pass, or
	/// they can be set explicitly with sUsername / sPassword, which will override
	/// anything in the URL.
	/// The session starts in plaintext and upgrades through STARTTLS when the
	/// server offers it - except on port 465 (submissions), where TLS is
	/// negotiated right away. With SetRequireTLS() the connect fails instead
	/// of continuing in plaintext.
	bool Connect(const KURL& Relay, KStringView sUsername = KStringView{}, KStringView sPassword = KStringView{});
	/// Disconnect from mail relay
	void Disconnect();
	/// Returns true if connected to a mail relay. Reflects the socket state,
	/// not the SMTP session: a relay that closed the connection is only
	/// noticed on the next write
	bool Good() const;
	/// Send a KMail to the mail relay
	bool Send(const KMail& Mail);
	/// Returns the last SMTP reply code received from the server, 0 if none
	DEKAF2_NODISCARD
	uint16_t GetLastReplyCode() const { return m_iLastReplyCode; }
	/// Set the connection timeout, preset is 15 seconds
	void SetTimeout(KDuration Timeout) { m_Timeout = Timeout; }
	/// Set the connection timeout in seconds, preset is 15
	void SetTimeout(uint16_t iSeconds) { SetTimeout(chrono::seconds(iSeconds)); }
	/// Require an encrypted session: Connect() then fails unless TLS gets established,
	/// either right away (port 465) or through STARTTLS. Preset is off (opportunistic TLS)
	void SetRequireTLS(bool bYesNo = true) { m_bRequireTLS = bYesNo; }
	/// Verify the server certificate? Preset is off
	void SetVerifyCerts(bool bYesNo = true) { m_bVerifyCerts = bYesNo; }
	/// Set the name announced in the EHLO/HELO handshake, preset is "localhost".
	/// Set it to the sending host's FQDN when the relay checks it
	void SetHeloName(KString sName) { m_sHeloName = std::move(sName); }

//----------
private:
//----------

	using ESMTPParms = KMap<KString, KString>;

	/// Talk to MTA and check response
	DEKAF2_PRIVATE
	bool Talk(KStringView sTX, KStringView sRx, ESMTPParms* parms = nullptr, bool bDisconnectOnFailure = true);
	/// Pretty print and send to MTA one set of addresses
	DEKAF2_PRIVATE
	bool PrettyPrint(KStringView sHeader, const KMail::map_t& map);

	// The TCP stream class
	std::unique_ptr<KIOStreamSocket> m_Connection;
	// the name announced in the EHLO/HELO handshake
	KString m_sHeloName { "localhost" };
	// the TCP timeout
	KDuration m_Timeout { KStreamOptions::GetDefaultTimeout() };
	// the last SMTP reply code received from the server
	uint16_t m_iLastReplyCode { 0 };
	// fail Connect() if the session cannot be encrypted?
	bool m_bRequireTLS { false };
	// verify the server certificate?
	bool m_bVerifyCerts { false };

}; // KSMTP


/// @}

DEKAF2_NAMESPACE_END
