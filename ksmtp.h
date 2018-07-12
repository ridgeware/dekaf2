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

#include <map>
#include "kmail.h"
#include "kstring.h"
#include "kstream.h"
#include "kurl.h"
#include "kconnection.h"

/// @file ksmtp.h
/// Adds the KSMTP class which sends a KMail via SMTP to an MTA

namespace dekaf2 {

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// This class speaks the SMTP protocol with an MTA. It takes a KMail class
/// as the mail to be sent. Multiple mails can be sent consecutively in one
/// session.
class KSMTP
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	/// Ctor - connects to MTA if argument is not empty
	KSMTP(KStringView sServer = KStringView{}, bool bForceSSL = false, KStringView sUsername = KStringView{}, KStringView sPassword = KStringView{})
	{
		if (!sServer.empty())
		{
			Connect(sServer, bForceSSL, sUsername, sPassword);
		}
	}

	/// Ctor - connects to MTA
	KSMTP(const KURL& URL, bool bForceSSL, KStringView sUsername = KStringView{}, KStringView sPassword = KStringView{})
	{
		Connect(URL, bForceSSL, sUsername, sPassword);
	}

	KSMTP(const KSMTP&) = delete;
	KSMTP(KSMTP&&) = default;
	KSMTP& operator=(const KSMTP&) = delete;
	KSMTP& operator=(KSMTP&&) = default;

	/// Connect to MTA
	bool Connect(const KURL& URL, bool bForceSSL, KStringView sUsername = KStringView{}, KStringView sPassword = KStringView{});
	/// Connect to MTA
	bool Connect(KStringView sServer, bool bForceSSL, KStringView sUsername = KStringView{}, KStringView sPassword = KStringView{})
	{
		return Connect(KURL(sServer), bForceSSL, sUsername, sPassword);
	}
	/// Disconnect from MTA
	void Disconnect();
	/// Returns true if connected to an MTA
	bool Good() const;
	/// Send one KMail to MTA
	bool Send(const KMail& Mail);
	/// Set the connection timeout in seconds, preset is 30
	void SetTimeout(uint16_t iSeconds);
	/// Returns last error
	KString Error() const;

//----------
private:
//----------

	using ESMTPParms = std::map<KString, KString>;

	/// Talk to MTA and check response
	bool Talk(KStringView sTX, KStringView sRx, ESMTPParms* parms = nullptr);
	/// Pretty print and send to MTA one set of addresses
	bool PrettyPrint(KStringView sHeader, const KMail::map_t& map);
	/// Insert dots if needed
	bool SendDottedMessage(KStringView sMessage);

	mutable KString m_sError;
	// The TCP stream class
	std::unique_ptr<KConnection> m_Connection;
	// Half a minute for the timeout per default
	uint16_t m_iTimeout{ 30 };

}; // KSMTP

} // end of namespace dekaf2
