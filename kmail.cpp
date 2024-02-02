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
#include "khttp_header.h"
#include "kreader.h"
#include "kfilesystem.h"
#include "klog.h"
#include "kstringutils.h"

DEKAF2_NAMESPACE_BEGIN

//-----------------------------------------------------------------------------
KMail& KMail::To(KString sTo, KString sPretty)
//-----------------------------------------------------------------------------
{
	return Add("To", m_To, std::move(sTo), std::move(sPretty));
}

//-----------------------------------------------------------------------------
KMail& KMail::Cc(KString sCc, KString sPretty)
//-----------------------------------------------------------------------------
{
	return Add("Cc", m_Cc, std::move(sCc), std::move(sPretty));
}

//-----------------------------------------------------------------------------
KMail& KMail::Bcc(KString sBcc, KString sPretty)
//-----------------------------------------------------------------------------
{
	return Add("Bcc", m_Bcc, std::move(sBcc), std::move(sPretty));
}

//-----------------------------------------------------------------------------
KMail& KMail::From(KString sFrom, KString sPretty)
//-----------------------------------------------------------------------------
{
	m_From.clear();
	return Add("From", m_From, std::move(sFrom), std::move(sPretty));
}

//-----------------------------------------------------------------------------
KMail& KMail::Subject(KString sSubject)
//-----------------------------------------------------------------------------
{
	m_Subject = std::move(sSubject);
	return *this;
}

//-----------------------------------------------------------------------------
bool KMail::AsHTML()
//-----------------------------------------------------------------------------
{
	if (m_iBody)
	{
		kDebug(2, "Cannot switch to HTML, body part is already existing");
		return false;
	}
	else
	{
		Attach(KMIMEHTML());
		m_iBody = m_Parts.size();
		return true;
	}

} // AsHTML

//-----------------------------------------------------------------------------
KMail& KMail::Message(KString sMessage)
//-----------------------------------------------------------------------------
{
	if (m_iBody)
	{
		m_Parts[m_iBody-1] = std::move(sMessage);
	}
	else
	{
		Attach(KMIMEText(sMessage));
		m_iBody = m_Parts.size();
	}
	return *this;

} // Message

//-----------------------------------------------------------------------------
KMail& KMail::operator+=(KStringView sMessage)
//-----------------------------------------------------------------------------
{
	if (m_iBody)
	{
		m_Parts[m_iBody-1] += sMessage;
	}
	else
	{
		Attach(KMIMEText(sMessage));
		m_iBody = m_Parts.size();
	}
	return *this;

} // operator+=

//-----------------------------------------------------------------------------
KMail& KMail::Body(KMIMEMultiPart parts)
//-----------------------------------------------------------------------------
{
	m_Parts = std::move(parts);
	m_iBody = 0;
	return *this;

} // Body

//-----------------------------------------------------------------------------
KMail& KMail::LoadBodyFrom(KStringViewZ sPath)
//-----------------------------------------------------------------------------
{
	if (kDirExists(sPath))
	{
		return Body(KMIMEDirectory(sPath));
	}
	else
	{
		return Body(KMIMEFile("", sPath));
	}

} // LoadBodyFrom

//-----------------------------------------------------------------------------
KMail& KMail::LoadManifestFrom(KStringViewZ sPath)
//-----------------------------------------------------------------------------
{
	KString sManifestFileName   { sPath };
	bool bNeedsManifestPreamble { true  };

	if (kDirExists(sPath))
	{
		KDirectory Dir(sPath);

		sManifestFileName += '/';

		if (Dir.Contains("manifest.ini"))
		{
			sManifestFileName += "manifest.ini";
			bNeedsManifestPreamble = false;
		}
		else if (Dir.Contains("index.html"))
		{
			sManifestFileName += "index.html";
		}
		else if (Dir.Contains("index.txt"))
		{
			sManifestFileName += "index.txt";
		}
		else
		{
			m_sError = kFormat("cannot find manifest in directory '{}'", sPath);
			return *this;
		}
	}
	else if (!kFileExists(sPath))
	{
		m_sError = kFormat("file '{}' does not exist", sPath);
		return *this;
	}

	KInFile fManifest(sManifestFileName);

	if (bNeedsManifestPreamble)
	{
		KString sLine;
		if (fManifest.ReadLine(sLine))
		{
			sLine.Trim();
			if (sLine != "#manifest")
			{
				m_sError = kFormat("no #manifest preamble in file '{}'", sManifestFileName);
				return *this;
			}
		}
	}

	KHTTPHeaders Manifest;
	Manifest.Parse(fManifest);

	if (Manifest.Headers.empty())
	{
		m_sError = kFormat("no manifest headers in file '{}'", sManifestFileName);
		return *this;
	}

	// check if we already have a KReplacer instance attached:
	if (!m_Replacer)
	{
		// no, create one with "{{" / "}}" prefix / suffix
		m_Replacer = std::make_shared<KVariables>();
	}

	// add all found key / values
	m_Replacer->insert(Manifest.Headers);

	// and set hardcoded ones
	for (auto& it : *m_Replacer)
	{
		if (it.first == "{{FROM}}")
		{
			From(it.second);
		}
		else if (it.first == "{{TO}}")
		{
			To(it.second);
		}
		else if (it.first == "{{CC}}")
		{
			Cc(it.second);
		}
		else if (it.first == "{{BCC}}")
		{
			Bcc(it.second);
		}
		else if (it.first == "{{SUBJECT}}")
		{
			Subject(it.second);
		}
	}

	return *this;

} // LoadManifestFrom

//-----------------------------------------------------------------------------
KMail& KMail::AddReplaceVar(KStringView sKey, KStringView sValue)
//-----------------------------------------------------------------------------
{
	// check if we already have a KReplacer instance attached:
	if (!m_Replacer)
	{
		// no, create one with "{{" / "}}" prefix / suffix
		m_Replacer = std::make_shared<KVariables>();
	}

	// add key / value
	m_Replacer->insert(sKey, sValue);

	return *this;

} // AddReplaceVar

//-----------------------------------------------------------------------------
bool KMail::Attach(KStringViewZ sFilename, const KMIME& MIME)
//-----------------------------------------------------------------------------
{
	KMIMEPart File(MIME);
	
	if (File.File("", sFilename))
	{
		Attach(std::move(File));
		return true;
	}
	else
	{
		return false;
	}

} // Attach

//-----------------------------------------------------------------------------
KMail& KMail::Attach(KMIMEPart part)
//-----------------------------------------------------------------------------
{
	if (m_Parts.empty())
	{
		// create a multipart structure
		m_Parts = KMIMEMultiPart(KMIME::MULTIPART_MIXED);
	}
	m_Parts += std::move(part);
	return *this;

} // Attach

//-----------------------------------------------------------------------------
KMail& KMail::Add(KStringView sWhich, map_t& map, KString Key, KString Value)
//-----------------------------------------------------------------------------
{
	if (!kIsEmail(Key))
	{
		m_sError = kFormat("{} address is invalid: {}", sWhich, Key);
	}
	else
	{
		map.insert({std::move(Key), std::move(Value)});
	}
	return *this;

} // Add

//-----------------------------------------------------------------------------
bool KMail::Good() const
//-----------------------------------------------------------------------------
{
	if (!m_sError.empty())
	{
		return false;
	}
	
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

	if (m_Parts.empty())
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
const KString& KMail::Subject() const
//-----------------------------------------------------------------------------
{
	return m_Subject;
}

//-----------------------------------------------------------------------------
KString KMail::Serialize() const
//-----------------------------------------------------------------------------
{
	KString sBody;

	if (m_Replacer)
	{
		m_Parts.Serialize(sBody, false, *m_Replacer);
	}
	else
	{
		m_Parts.Serialize(sBody);
	}

	return sBody;

} // Serialize

//-----------------------------------------------------------------------------
KUnixTime KMail::Time() const
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

// if std::map is not yet supported by this lib to be nothrow, don't test dependant class
static_assert(!std::is_nothrow_move_constructible<std::map<int, int>>::value || std::is_nothrow_move_constructible<KMail>::value,
			  "KMail is intended to be nothrow move constructible, but is not!");

DEKAF2_NAMESPACE_END
