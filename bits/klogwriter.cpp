/*
//
// DEKAF(tm): Lighter, Faster, Smarter(tm)
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
//
*/

#include "klogwriter.h"
#include <dekaf2/kstring.h>
#include <dekaf2/ksplit.h>
#include <dekaf2/kfilesystem.h>
#include <dekaf2/kencode.h>

#ifndef DEKAF2_IS_WINDOWS
	#define DEKAF2_HAS_SYSLOG
#endif

#ifdef DEKAF2_HAS_SYSLOG
	#include <syslog.h>
#endif

#ifdef DEKAF2_KLOG_WITH_TCP
	#include <dekaf2/kurl.h>
	#include <dekaf2/khttpclient.h>
	#include <dekaf2/kconnection.h>
	#include <dekaf2/kmime.h>
	#include <dekaf2/kjson.h>
	#include <dekaf2/khttp_header.h> // for LogToRESTResponse()
#endif

namespace dekaf2
{

//---------------------------------------------------------------------------
KLogWriter::~KLogWriter()
//---------------------------------------------------------------------------
{
}

//---------------------------------------------------------------------------
KLogNullWriter::~KLogNullWriter()
//---------------------------------------------------------------------------
{
}

//---------------------------------------------------------------------------
bool KLogStdWriter::Write(int iLevel, bool bIsMultiline, KStringViewZ sOut)
//---------------------------------------------------------------------------
{
	return m_OutStream.write(sOut.data(), static_cast<std::streamsize>(sOut.size())).flush().good();

} // Write

//---------------------------------------------------------------------------
KLogFileWriter::KLogFileWriter(KStringView sFileName)
//---------------------------------------------------------------------------
    : m_OutFile(sFileName, std::ios_base::app)
{
	KString sBuffer(sFileName);
	// force mode 666 for the log file ...
	kChangeMode(sBuffer, DEKAF2_MODE_CREATE_FILE);

} // ctor

//---------------------------------------------------------------------------
bool KLogFileWriter::Write(int iLevel, bool bIsMultiline, KStringViewZ sOut)
//---------------------------------------------------------------------------
{
	return m_OutFile.Write(sOut).Flush().Good();

} // Write

//---------------------------------------------------------------------------
KLogStringWriter::KLogStringWriter(KString& sOutString, KString sConcat)
//---------------------------------------------------------------------------
    : m_OutString(sOutString)
	, m_sConcat(std::move(sConcat))
{
} // ctor

//---------------------------------------------------------------------------
bool KLogStringWriter::Write(int iLevel, bool bIsMultiline, KStringViewZ sOut)
//---------------------------------------------------------------------------
{
	if (!m_sConcat.empty() && !m_OutString.empty())
	{
		m_OutString += m_sConcat;
	}
	m_OutString += sOut;

	return true;

} // Write

#ifdef DEKAF2_HAS_SYSLOG

//---------------------------------------------------------------------------
bool KLogSyslogWriter::Write(int iLevel, bool bIsMultiline, KStringViewZ sOut)
//---------------------------------------------------------------------------
{
	int priority;
	switch (iLevel)
	{
		case -3:
		case -2:
		case -1:
			priority = LOG_ERR;
			break;
		case 0:
			priority = LOG_WARNING;
			break;
		case 1:
			priority = LOG_NOTICE;
			break;
		case 2:
			priority = LOG_INFO;
			break;
		default:
			priority = LOG_DEBUG;
			break;
	}

	if (!bIsMultiline)
	{
		syslog(priority, "%s", sOut.c_str());
	}
	else
	{
		KStringView svMessage = sOut;
		KString sPart;

		while (!svMessage.empty())
		{
			auto pos = svMessage.find('\n');
			sPart = svMessage.substr(0, pos);
			if (!sPart.empty())
			{
				syslog(priority, "%s", sPart.c_str());
			}
			svMessage.remove_prefix(pos);
			if (pos != KStringView::npos)
			{
				svMessage.remove_prefix(1);
			}
		}
	}

	return true;

} // Write

#endif // of DEKAF2_HAS_SYSLOG

#ifdef DEKAF2_KLOG_WITH_TCP

//---------------------------------------------------------------------------
KLogTCPWriter::KLogTCPWriter(KStringView sURL)
//---------------------------------------------------------------------------
    : m_sURL(sURL)
{
} // ctor

//---------------------------------------------------------------------------
KLogTCPWriter::~KLogTCPWriter()
//---------------------------------------------------------------------------
{
} // dtor

//---------------------------------------------------------------------------
bool KLogTCPWriter::Good() const
//---------------------------------------------------------------------------
{
	// this is special: we always return true if we have not (yet) created
	// the tcp client
	return !m_OutStream || m_OutStream->Good();

} // Good

//---------------------------------------------------------------------------
bool KLogTCPWriter::Write(int iLevel, bool bIsMultiline, KStringViewZ sOut)
//---------------------------------------------------------------------------
{
	if (!Good())
	{
		// we try one reconnect should the connection have gone stale
		m_OutStream.reset();
	}

	if (!m_OutStream)
	{
		m_OutStream = KConnection::Create(m_sURL);

		if (!m_OutStream->Good())
		{
			return false;
		}
	}

	return m_OutStream != nullptr
	        && m_OutStream->Stream().Write(sOut).Flush().Good();

} // Write

//---------------------------------------------------------------------------
KLogHTTPWriter::KLogHTTPWriter(KStringView sURL)
//---------------------------------------------------------------------------
    : m_sURL(sURL)
{
} // ctor

//---------------------------------------------------------------------------
KLogHTTPWriter::~KLogHTTPWriter()
//---------------------------------------------------------------------------
{
} // dtor

//---------------------------------------------------------------------------
bool KLogHTTPWriter::Good() const
//---------------------------------------------------------------------------
{
	// this is special: we always return true if we have not (yet) created
	// the http client
	return !m_OutStream || m_OutStream->Good();

} // Good

//---------------------------------------------------------------------------
bool KLogHTTPWriter::Write(int iLevel, bool bIsMultiline, KStringViewZ sOut)
//---------------------------------------------------------------------------
{
	if (!Good())
	{
		// we try one reconnect should the connection have gone stale
		m_OutStream.reset();
	}

	if (!m_OutStream)
	{
		m_OutStream = std::make_unique<KHTTPClient>(m_sURL);

		if (!m_OutStream->Good())
		{
			return false;
		}
	}

	m_OutStream->Post(m_sURL, sOut, KMIME::JSON);

	return m_OutStream->HttpSuccess();

} // Write

//---------------------------------------------------------------------------
KLogHTTPHeaderWriter::KLogHTTPHeaderWriter(KHTTPHeaders& HTTPHeaders, KStringView sHeader)
//---------------------------------------------------------------------------
    : m_Headers(HTTPHeaders)
	, m_sHeader(sHeader)
{
} // ctor

//---------------------------------------------------------------------------
KLogHTTPHeaderWriter::~KLogHTTPHeaderWriter()
//---------------------------------------------------------------------------
{
} // dtor

//---------------------------------------------------------------------------
bool KLogHTTPHeaderWriter::Good() const
//---------------------------------------------------------------------------
{
	return true;

} // Good

//---------------------------------------------------------------------------
bool KLogHTTPHeaderWriter::Write(int iLevel, bool bIsMultiline, KStringViewZ sOut)
//---------------------------------------------------------------------------
{
	for (auto sLine : sOut.Split("\n", ""))
	{
		if (!sLine.empty())
		{
			KString sValue;
			sValue.reserve(sLine.size());

			for (auto ch : sLine)
			{
				// escape non-printable characters..
				if (DEKAF2_UNLIKELY(ch == '\\'))
				{
					sValue += '\\';
					sValue += '\\';
				}
				else if (DEKAF2_UNLIKELY(!ch || KASCII::kIsCntrl(ch)))
				{
					sValue += '\\';
					sValue += 'x';
					KEnc::HexAppend(sValue, ch);
				}
				else
				{
					sValue += ch;
				}
			}

			// it is a bug that we cannot use a KString as the key to add a new
			// header but have to convert it into a KStringView first..
			m_Headers.Headers.Add(kPrintf("%s-%5.5u", m_sHeader, m_iCounter++).ToView(), std::move(sValue));
		}
	}

	return true;

} // Write

#endif // of DEKAF2_KLOG_WITH_TCP

} // of namespace dekaf2
