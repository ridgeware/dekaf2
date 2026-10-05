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

#include <dekaf2/core/logging/bits/klogwriter.h>

#ifdef DEKAF2_WITH_KLOG

#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/strings/ksplit.h>
#include <dekaf2/system/filesystem/kfilesystem.h>
#include <dekaf2/crypto/encoding/kencode.h>
#include <dekaf2/core/strings/kstringutils.h>

#ifdef DEKAF2_IS_WINDOWS
	#include <dekaf2/core/init/dekaf2.h>
	#include <dekaf2/core/strings/kutf.h> // UTF-8 -> UTF-16 for the Windows W API
	#include <windows.h>
#else
	#include <syslog.h>
#endif

#ifdef DEKAF2_KLOG_WITH_TCP
	#include <dekaf2/web/url/kurl.h>
	#include <dekaf2/http/client/kwebclient.h>
	#include <dekaf2/net/util/kiostreamsocket.h>
	#include <dekaf2/web/url/kmime.h>
	#include <dekaf2/http/protocol/khttp_header.h> // for LogToRESTResponse()
#endif

DEKAF2_NAMESPACE_BEGIN

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
KLogFileWriter::KLogFileWriter(KStringViewZ sFileName)
//---------------------------------------------------------------------------
    : m_OutFile(sFileName, std::ios_base::app)
{
	// force mode 666 for the log file ...
	kChangeMode(sFileName, DEKAF2_MODE_CREATE_FILE);

} // ctor

//---------------------------------------------------------------------------
bool KLogFileWriter::Write(int iLevel, bool bIsMultiline, KStringViewZ sOut)
//---------------------------------------------------------------------------
{
	return m_OutFile.Write(sOut).Flush().Good();

} // Write

//---------------------------------------------------------------------------
KLogStringWriter::KLogStringWriter(KStringRef& sOutString, KString sConcat)
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

#ifdef DEKAF2_IS_WINDOWS

namespace {

// the key of the event sources of the Application log
constexpr wchar_t s_sApplicationLogKey[] = L"SYSTEM\\CurrentControlSet\\Services\\EventLog\\Application\\";

//---------------------------------------------------------------------------
/// Returns the message file for the event sources: the one of the .NET Framework,
/// which shows the text of every event ID as it is, and which PowerShell registers
/// with New-EventLog - or an empty string if it is missing
std::wstring MessageFile()
//---------------------------------------------------------------------------
{
	for (const wchar_t* sCandidate : { L"%SystemRoot%\\Microsoft.NET\\Framework64\\v4.0.30319\\EventLogMessages.dll",
	                                   L"%SystemRoot%\\Microsoft.NET\\Framework\\v4.0.30319\\EventLogMessages.dll" })
	{
		wchar_t sExpanded[MAX_PATH];

		auto iLen = ::ExpandEnvironmentStringsW(sCandidate, sExpanded, MAX_PATH);

		if (iLen > 0 && iLen <= MAX_PATH && ::GetFileAttributesW(sExpanded) != INVALID_FILE_ATTRIBUTES)
		{
			// the registry keeps the path with %SystemRoot%
			return sCandidate;
		}
	}

	return {};

} // MessageFile

//---------------------------------------------------------------------------
/// returns true if the event source is registered in the Application log
bool IsRegistered(const std::wstring& wsSource)
//---------------------------------------------------------------------------
{
	HKEY hKey { nullptr };

	if (::RegOpenKeyExW(HKEY_LOCAL_MACHINE, (s_sApplicationLogKey + wsSource).c_str(), 0, KEY_QUERY_VALUE, &hKey) != ERROR_SUCCESS)
	{
		return false;
	}

	::RegCloseKey(hKey);

	return true;

} // IsRegistered

} // end of anonymous namespace

//---------------------------------------------------------------------------
KLogSyslogWriter::KLogSyslogWriter()
//---------------------------------------------------------------------------
{
	auto wsSource = kutf::Convert<std::wstring>(SourceName());

	if (!IsRegistered(wsSource))
	{
		// succeeds for a service under LocalSystem - for other processes, Event
		// Viewer shows the events with a note about a missing description
		RegisterSource(SourceName());
	}

	// no logging here - this is the writer of the logging
	m_hEventLog = ::RegisterEventSourceW(nullptr, wsSource.c_str());

} // ctor

//---------------------------------------------------------------------------
KLogSyslogWriter::~KLogSyslogWriter()
//---------------------------------------------------------------------------
{
	if (m_hEventLog)
	{
		::DeregisterEventSource(m_hEventLog);
	}

} // dtor

//---------------------------------------------------------------------------
bool KLogSyslogWriter::Write(int iLevel, bool bIsMultiline, KStringViewZ sOut)
//---------------------------------------------------------------------------
{
	if (!m_hEventLog)
	{
		return false;
	}

	// the Event Log has no levels below the information
	WORD iType = (iLevel <  0) ? EVENTLOG_ERROR_TYPE
	           : (iLevel == 0) ? EVENTLOG_WARNING_TYPE
	           :                 EVENTLOG_INFORMATION_TYPE;

	// one event also for a message with several lines, like a stack trace - Event
	// Viewer shows a line break only for CRLF
	KString sMessage(sOut);
	sMessage.TrimRight();
	sMessage.Replace("\n", "\r\n");

	auto wsMessage = kutf::Convert<std::wstring>(sMessage);

	// an inserted string of an event has at most 31839 characters
	constexpr std::size_t iMaxChars = 31839;

	if (wsMessage.size() > iMaxChars)
	{
		wsMessage.resize(iMaxChars);

		if (wsMessage.back() >= 0xD800 && wsMessage.back() <= 0xDBFF)
		{
			// do not leave the first half of a surrogate pair
			wsMessage.pop_back();
		}
	}

	const wchar_t* Strings[] { wsMessage.c_str() };

	return ::ReportEventW(m_hEventLog, iType, 0, 0, nullptr, 1, 0, Strings, nullptr) != FALSE;

} // Write

//---------------------------------------------------------------------------
KString KLogSyslogWriter::SourceName(KStringView sProgram)
//---------------------------------------------------------------------------
{
	// the name of the executable, as the identity of syslog on Unix
	KStringView sName = kBasename(sProgram.empty() ? KStringView(Dekaf::getInstance().GetProgName()) : sProgram);

	if (sName.size() > 4 && kToLower(sName.substr(sName.size() - 4)) == ".exe")
	{
		sName.remove_suffix(4);
	}

	return sName.empty() ? KString("dekaf2") : KString(sName);

} // SourceName

//---------------------------------------------------------------------------
bool KLogSyslogWriter::RegisterSource(KStringView sSource)
//---------------------------------------------------------------------------
{
	auto wsMessageFile = MessageFile();

	if (wsMessageFile.empty() || sSource.empty())
	{
		return false;
	}

	HKEY hKey { nullptr };

	if (::RegCreateKeyExW(HKEY_LOCAL_MACHINE, (s_sApplicationLogKey + kutf::Convert<std::wstring>(sSource)).c_str(),
	                      0, nullptr, 0, KEY_SET_VALUE, nullptr, &hKey, nullptr) != ERROR_SUCCESS)
	{
		return false;
	}

	DWORD iTypes = EVENTLOG_ERROR_TYPE | EVENTLOG_WARNING_TYPE | EVENTLOG_INFORMATION_TYPE;

	bool bRegistered =
		::RegSetValueExW(hKey, L"EventMessageFile", 0, REG_EXPAND_SZ,
		                 reinterpret_cast<const BYTE*>(wsMessageFile.c_str()),
		                 static_cast<DWORD>((wsMessageFile.size() + 1) * sizeof(wchar_t))) == ERROR_SUCCESS
	 && ::RegSetValueExW(hKey, L"TypesSupported", 0, REG_DWORD,
		                 reinterpret_cast<const BYTE*>(&iTypes), sizeof(iTypes)) == ERROR_SUCCESS;

	::RegCloseKey(hKey);

	return bRegistered;

} // RegisterSource

//---------------------------------------------------------------------------
bool KLogSyslogWriter::UnregisterSource(KStringView sSource)
//---------------------------------------------------------------------------
{
	if (sSource.empty())
	{
		return false;
	}

	auto iResult = ::RegDeleteKeyW(HKEY_LOCAL_MACHINE, (s_sApplicationLogKey + kutf::Convert<std::wstring>(sSource)).c_str());

	return iResult == ERROR_SUCCESS || iResult == ERROR_FILE_NOT_FOUND;

} // UnregisterSource

#else // DEKAF2_IS_WINDOWS

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

#endif // DEKAF2_IS_WINDOWS

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
		m_OutStream = KIOStreamSocket::Create(m_sURL);

		if (!m_OutStream->Good())
		{
			return false;
		}
	}

	return m_OutStream != nullptr && m_OutStream->Write(sOut).Flush().Good();

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
		m_OutStream = std::make_unique<KWebClient>();

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
	for (auto sLine : sOut.Split('\n'))
	{
		if (!sLine.empty())
		{
			m_Headers.Headers.Add(kFormat("{}-{:05d}", m_sHeader, m_iCounter++), kEscapeForLogging(sLine));
		}
	}

	return true;

} // Write

#endif // of DEKAF2_KLOG_WITH_TCP

DEKAF2_NAMESPACE_END

#endif // of DEKAF2_WITH_KLOG
