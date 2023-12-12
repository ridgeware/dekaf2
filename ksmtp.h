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

#include "kmail.h"
#include "kstring.h"
#include "kstream.h"
#include "kurl.h"
#include "kconnection.h"
#include "kassociative.h"

/// @file ksmtp.h
/// Adds the KSMTP class which sends a KMail via SMTP to an MTA

DEKAF2_NAMESPACE_BEGIN

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// This class speaks the SMTP protocol with a mail relay. It takes a KMail class
/// as the mail to be sent. Multiple mails can be sent consecutively in one
/// session.
class DEKAF2_PUBLIC KSMTP
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
	bool Connect(const KURL& Relay, KStringView sUsername = KStringView{}, KStringView sPassword = KStringView{});
	/// Disconnect from mail relay
	void Disconnect();
	/// Returns true if connected to a mail relay
	bool Good() const;
	/// Send a KMail to the mail relay
	bool Send(const KMail& Mail);
	/// Set the connection timeout in seconds, preset is 10
	void SetTimeout(uint16_t iSeconds);
	/// Returns last error
	KString Error() const;

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

	mutable KString m_sError;
	// The TCP stream class
	std::unique_ptr<KConnection> m_Connection;
	// Ten seconds for the timeout per default
	uint16_t m_iTimeout { 10 };

}; // KSMTP

DEKAF2_NAMESPACE_END
