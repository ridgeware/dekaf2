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

#include "kmail.h"
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
	if (m_Parts.empty())
	{
		m_Message = std::move(sMessage);
	}
	else
	{
		// find the text/utf8 part
		auto it = m_Parts.begin();
		while (it != m_Parts.end() && it->MIME() != KMIME::TEXT_UTF8)
		{
			++it;
		}
		if (it != m_Parts.end())
		{
			(*it) = sMessage;
		}
		else
		{
			m_Parts += KMIMEText(std::move(sMessage));
		}

	}
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
	if (m_Parts.empty())
	{
		m_Message += sMessage;
	}
	else
	{
		// find the text/utf8 part
		auto it = m_Parts.begin();
		while (it != m_Parts.end() && it->MIME() != KMIME::TEXT_UTF8)
		{
			++it;
		}
		if (it != m_Parts.end())
		{
			(*it) += sMessage;
		}
		else
		{
			m_Parts += KMIMEText(sMessage);
		}
	}
	return *this;
}

//-----------------------------------------------------------------------------
KMail& KMail::Body(KMIMEMultiPart&& parts)
//-----------------------------------------------------------------------------
{
	m_Parts = parts;
	return *this;
}

//-----------------------------------------------------------------------------
bool KMail::Attach(KStringView sFilename, KMIME MIME)
//-----------------------------------------------------------------------------
{
	KMIMEPart File(MIME);
	if (File.File(sFilename))
	{
		Attach(std::move(File));
		return true;
	}
	else
	{
		return false;
	}
}

//-----------------------------------------------------------------------------
KMail& KMail::Attach(KMIMEPart&& part)
//-----------------------------------------------------------------------------
{
	if (m_Parts.empty())
	{
		// create a multipart structure
		m_Parts = KMIMEMultiPart(KMIME::MULTIPART_MIXED);
		// append a text part if not empty
		if (!m_Message.empty())
		{
			m_Parts += KMIMEText(m_Message);
			m_Message.clear();
		}
	}
	m_Parts += std::move(part);
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

	if (m_Message.empty() && m_Parts.empty())
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
KString KMail::Serialize() const
//-----------------------------------------------------------------------------
{
	KString sBody;
	if (m_Parts.empty())
	{
		sBody = "Content-Type: ";
		sBody += KMIME::TEXT_UTF8;
		sBody += "\r\n\r\n";
		sBody += m_Message;
	}
	else
	{
		m_Parts.Serialize(sBody);
	}
	return sBody;

} // Serialize

//-----------------------------------------------------------------------------
time_t KMail::Time() const
//-----------------------------------------------------------------------------
{
	return m_Time;
}

//-----------------------------------------------------------------------------
bool KMail::Send(const KURL& URL, KStringView sUsername, KStringView sPassword)
//-----------------------------------------------------------------------------
{
	if (!Good())
	{
		return false;
	}

	KSMTP server;

	if (!server.Connect(URL, sUsername, sPassword))
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

} // end of namespace dekaf2
