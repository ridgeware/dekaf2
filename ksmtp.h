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

#include "kstring.h"
#include "kstringview.h"
#include "kstream.h"
#include "kstringutils.h"
#include "kurl.h"
#include "kmime.h"
#include "kconnection.h"
#include <map>
#include <vector>

/// @file ksmtp.h
/// Adds the KMail class to represent an email to be sent,
/// and the KSMTP class which sends a KMail via SMTP to an MTA

namespace dekaf2 {

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// This class takes all information for an email message. It can then be used
/// as an argument for the KSMTP class, or sent via the convenience Send()
/// method of KMail, which internally calls KSMTP.
class KMail
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	using map_t = std::map<KString, KString>;

	/// Add one recipient to the To list, first arg is the email,
	/// second arg is the full name or nothing
	void To(KStringView sTo, KStringView sPretty = KStringView{});
	/// Add one recipient to the Cc list, first arg is the email,
	/// second arg is the full name or nothing
	void Cc(KStringView sCc, KStringView sPretty = KStringView{});
	/// Add one recipient to the Bcc list, first arg is the email,
	/// second arg is the full name or nothing
	void Bcc(KStringView sBcc, KStringView sPretty = KStringView{});
	/// Set the sender for the From field, first arg is the email,
	/// second arg is the full name or nothing
	void From(KStringView sFrom, KStringView sPretty = KStringView{});
	/// Set the subject
	void Subject(KStringView sSubject);
	/// Set the message
	void Message(KString&& sMessage);
	/// Set the message
	void Message(const KString& sMessage)
	{
		KString cp(sMessage);
		Message(std::move(cp));
	}
	/// Set the MIME type
	void MIME(KMIME MimeType);
	/// Returns true if this mail has all elements needed for expedition
	bool Good() const;
	/// Send the mail via MTA at URL
	bool Send(const KURL& URL);
	/// Set the message
	KMail& operator=(KStringView sMessage);
	/// Append to message
	KMail& operator+=(KStringView sMessage);
	/// Append to message
	KMail& Append(KStringView sMessage);
	/// Append with formatting to message
	template<class... Args>
	KMail& AppendFormatted(Args&&... args)
	{
		return Append(kFormat(std::forward<Args>(args)...));
	}

	/// Returns the To recipients
	const map_t& To() const;
	/// Returns the Cc recipients
	const map_t& Cc() const;
	/// Returns the Bcc recipients
	const map_t& Bcc() const;
	/// Returns the sender (only first entry in map is valid)
	const map_t& From() const;
	/// Returns the subject
	KStringView Subject() const;
	/// Returns the message
	KStringView Message() const;
	/// Returns the MIME type
	KMIME MIME() const;
	/// Returns last error
	const KString& Error() const;

//----------
private:
//----------

	void Add(map_t& map, KStringView Key, KStringView Value = KStringView{});
	KStringView::size_type DottingNeededAt(KStringView sMessage, KStringView::size_type start = 0);
	void AppendDotted(KStringView sMessage, KStringView::size_type iSingleDot);

	map_t m_To;
	map_t m_Cc;
	map_t m_Bcc;
	map_t m_From; // actually we only need one single key and value for this
	KString m_Subject;
	KString m_Message;
	mutable KString m_sError;
	KMIME m_MimeType{ KMIME::NONE };

}; // KMail


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
	KSMTP(KStringView sServer = KStringView{})
	{
		if (!sServer.empty())
		{
			Connect(sServer);
		}
	}

	/// Ctor - connects to MTA
	KSMTP(const KURL& URL)
	{
		Connect(URL);
	}

	KSMTP(const KSMTP&) = delete;
	KSMTP(KSMTP&&) = default;
	KSMTP& operator=(const KSMTP&) = delete;
	KSMTP& operator=(KSMTP&&) = default;

	/// Connect to MTA
	bool Connect(const KURL& URL);
	/// Connect to MTA
	bool Connect(KStringView sServer)
	{
		return Connect(KURL(sServer));
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

	/// Talk to MTA and check response
	bool Talk(KStringView sTX, KStringView sRx);
	/// Pretty print and send to MTA one set of addresses
	bool PrettyPrint(KStringView sHeader, const KMail::map_t& map);

	mutable KString m_sError;
	// The TCP stream class
	KConnection m_Connection;
	// Half a minute for the timeout per default
	uint16_t m_iTimeout{ 30 };

}; // KSMTP

} // end of namespace dekaf2
