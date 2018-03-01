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
#include <map>
#include <vector>


namespace dekaf2 {

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KMail
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	void To(KStringView sTo, KStringView sPretty = KStringView{});
	void Cc(KStringView sCc, KStringView sPretty = KStringView{});
	void Bcc(KStringView sBcc, KStringView sPretty = KStringView{});
	void From(KStringView sFrom, KStringView sPretty = KStringView{});
	void Subject(KStringView sSubject);
	void Message(KString&& sMessage);
	void Message(const KString& sMessage)
	{
		KString cp(sMessage);
		Message(std::move(cp));
	}

	KMail& operator=(KStringView sMessage);
	KMail& operator+=(KStringView sMessage);
	KMail& Append(KStringView sMessage);
	template<class... Args>
	KMail& AppendFormatted(Args&&... args)
	{
		return Append(kFormat(std::forward<Args>(args)...));
	}

//----------
private:
//----------

	using map_t = std::map<KString, KString>;

	void Add(map_t& map, KStringView Key, KStringView Value = KStringView{});

	map_t m_To;
	map_t m_Cc;
	map_t m_Bcc;
	map_t m_From;
	KString m_Subject;
	KString m_Message;

}; // KMail


//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KSMTP
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	KSMTP(KStringView sServer = KStringView{})
	: KSMTP(KURL(sServer))
	{
	}

	KSMTP(const KURL& URL)
	{
		Connect(URL);
	}

	bool Connect(const KURL& URL);
	bool Connect(KStringView sServer)
	{
		return Connect(KURL(sServer));
	}
	void Disconnect();

	bool Send(const KMail& Mail);

//----------
private:
//----------

	std::unique_ptr<KTCPStream> m_Stream;

}; // KSMTP

} // end of namespace dekaf2
