/*
//
// DEKAF(tm): Lighter, Faster, Smarter (tm)
//
// Copyright (c) 2021, Ridgeware, Inc.
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

#include "khttplog.h"
#include "kstringutils.h"
#include "kwriter.h"
#include "kjson.h"
#include "krestserver.h"
#include "kencode.h"
#include "ktime.h"

DEKAF2_NAMESPACE_BEGIN

//-----------------------------------------------------------------------------
bool KHTTPLogBase::SetFormat(LOG_FORMAT LogFormat, KStringView sFormat)
//-----------------------------------------------------------------------------
{
	if (LogFormat == LOG_FORMAT::PARSED)
	{
		if (!sFormat.empty())
		{
			kDebug(2, "log format: {}", sFormat);
			m_sFormat   = sFormat;
		}
		else
		{
			kWarning("no format string defined for PARSED log format");
			return false;
		}
	}
	else
	{
		if (!sFormat.empty())
		{
			kDebug(1, "will ignore format string, log format is not set to PARSED");
		}
	}

	m_LogFormat = LogFormat;

	return true;

} // SetFormat

namespace {

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class PrintJSONLog
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	//-----------------------------------------------------------------------------
	void Write(KStringView sKey, KString sValue)
	//-----------------------------------------------------------------------------
	{
		if (!sValue.empty())
		{
			m_jLogline.emplace(sKey, std::move(sValue));
		}

	} // Write

	//-----------------------------------------------------------------------------
	void Write(KStringView sKey, uint64_t iInteger)
	//-----------------------------------------------------------------------------
	{
		if (iInteger)
		{
			m_jLogline.emplace(sKey, iInteger);
		}

	} // Write

	//-----------------------------------------------------------------------------
	const KJSON& Get()
	//-----------------------------------------------------------------------------
	{
		return m_jLogline;
	}

	//-----------------------------------------------------------------------------
	KString Dump()
	//-----------------------------------------------------------------------------
	{
		return m_jLogline.dump(-1);
	}

//------
private:
//------

	KJSON m_jLogline = KJSON::object();

}; // PrintJSONLog

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class PrintParsedLog
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	//-----------------------------------------------------------------------------
	/// escape any evil characters (basically, double quotes, backslashes, spaces, control characters)
	void Escape(KStringView sInput)
	//-----------------------------------------------------------------------------
	{
		if (sInput.empty())
		{
			m_sLogline += '-';
		}
		else
		{
			kEscapeForLogging(m_sLogline, sInput);
		}

	} // Escape

	//-----------------------------------------------------------------------------
	void Raw(KStringView sInput)
	//-----------------------------------------------------------------------------
	{
		m_sLogline += sInput;

	} // Raw

	//-----------------------------------------------------------------------------
	void RawChar(char chInput)
	//-----------------------------------------------------------------------------
	{
		m_sLogline += chInput;

	} // RawChar

	//-----------------------------------------------------------------------------
	void Raw(uint64_t iInteger)
	//-----------------------------------------------------------------------------
	{
		// we want to display npos etc. as negative values, hence the cast
		m_sLogline += KString::to_string(static_cast<int64_t>(iInteger));

	} // Raw

	//-----------------------------------------------------------------------------
	void Write(KStringView sInput)
	//-----------------------------------------------------------------------------
	{
		if (sInput.empty())
		{
			m_sLogline += '-';
		}
		else
		{
			m_sLogline += sInput;
		}

	} // Write

	//-----------------------------------------------------------------------------
	void operator+=(KStringView sInput)
	//-----------------------------------------------------------------------------
	{
		Write(sInput);
	}

	//-----------------------------------------------------------------------------
	/// print integer, and set to - if 0
	void Write(uint64_t iInteger)
	//-----------------------------------------------------------------------------
	{
		if (!iInteger)
		{
			m_sLogline += '-';
		}
		else
		{
			Raw(iInteger);
		}

	} // Write

	//-----------------------------------------------------------------------------
	void operator+=(uint64_t iInteger)
	//-----------------------------------------------------------------------------
	{
		Write(iInteger);
	}

	//-----------------------------------------------------------------------------
	/// get log line
	const KString& Get()
	//-----------------------------------------------------------------------------
	{
		return m_sLogline;
	}

//------
protected:
//------

	KString m_sLogline;

}; // PrintParsedLog

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class PrintLog : public PrintParsedLog
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	using Parent = PrintParsedLog;

	//-----------------------------------------------------------------------------
	/// frame sInput in double quotes and escape any evil characters (basically, double quotes and backslashes..)
	void Quote(KStringView sInput)
	//-----------------------------------------------------------------------------
	{
		RawChar ('"');
		Escape  (sInput);
		Raw     ("\" ");

	} // Quote

	//-----------------------------------------------------------------------------
	void Write(KStringView sInput)
	//-----------------------------------------------------------------------------
	{
		Parent::Write(sInput);
		RawChar(' ');

	} // Write

	//-----------------------------------------------------------------------------
	void operator+=(KStringView sInput)
	//-----------------------------------------------------------------------------
	{
		Write(sInput);
	}

	//-----------------------------------------------------------------------------
	/// print integer, and set to - if 0
	void Write(uint64_t iInteger)
	//-----------------------------------------------------------------------------
	{
		Parent::Write(iInteger);
		RawChar(' ');

	} // Write

	//-----------------------------------------------------------------------------
	void operator+=(uint64_t iInteger)
	//-----------------------------------------------------------------------------
	{
		Write(iInteger);
	}

}; // PrintLog

} // end of anonymous namespace

//-----------------------------------------------------------------------------
void KHTTPLog::WriteJSONAccessLog(const KRESTServer& HTTP) const
//-----------------------------------------------------------------------------
{
	PrintJSONLog Log;

	Log.Write("time"      , kFormCommonLogTimestamp().Mid(1, 26)                     ); // cut off the framing []
	Log.Write("remote_ip" , HTTP.GetRemoteIP()                                       );
	Log.Write("host"      , HTTP.Request.Headers.Get(KHTTPHeader::HOST)              );
	Log.Write("request"   , HTTP.Request.Resource.Path.get()                         );
	Log.Write("query"     , HTTP.Request.Resource.Query.Serialize()                  );
	Log.Write("method"    , HTTP.Request.RequestLine.GetMethod()                     );
	Log.Write("status"    , HTTP.Response.GetStatusCode()                            );
	Log.Write("rx-size"   , HTTP.GetRequestBodyLength()                              );
	Log.Write("rx-recv"   , HTTP.GetReceivedBytes()                                  );
	Log.Write("tx-size"   , HTTP.GetContentLength()                                  );
	Log.Write("tx-sent"   , HTTP.GetSentBytes()                                      );
	Log.Write("user_agent", HTTP.Request.Headers.Get(KHTTPHeader::USER_AGENT)        );
	Log.Write("referer"   , HTTP.Request.Headers.Get(KHTTPHeader::REFERER)           );
	Log.Write("tx-comp"   , HTTP.Response.Headers.Get(KHTTPHeader::CONTENT_ENCODING) );
	Log.Write("forwarded_for",HTTP.Response.Headers.Get(KHTTPHeader::X_FORWARDED_FOR));
	Log.Write("user"      , kjson::GetString(HTTP.GetAuthToken(), "sub")             );
	Log.Write("TTLB"      , HTTP.GetTimeToLastByte().microseconds().count()          );

	kLogger(*m_LogStream, Log.Dump());

} // WriteJSONAccessLog

//-----------------------------------------------------------------------------
void KHTTPLog::WriteAccessLog(const KRESTServer& HTTP) const
//-----------------------------------------------------------------------------
{
	PrintLog Log;

	// LogFormat "%h %l %u %t \"%r\" %>s %b" common
	// LogFormat "%h %l %u %t \"%r\" %>s %b \"%{Referer}i\" \"%{User-Agent}i\"" combined
	// LogFormat "%h %l %u %t \"%r\" %>s %b \"%{Referer}i\" \"%{User-Agent}i\" %V %I %O %P %D; \"%{X-Forwarded-For}i\"" extended

	// %h - remote IP / hostname
	Log += HTTP.GetRemoteIP();
	// %l Remote logname (from identd, if supplied). This will return a dash unless mod_ident is present and IdentityCheck is set On.
	Log += "";
	// %u Remote user if the request was authenticated
	auto sUser = kEscapeForLogging(HTTP.GetAuthenticatedUser());
	// remove all spaces
	sUser.RemoveChars(" ");
	// finally append (without quotes, hence the filtering above)
	Log += sUser;
	// %t Time the request was received, in the format [18/Sep/2011:19:18:28 -0400]. The last number indicates the timezone offset from GMT
	Log += kFormCommonLogTimestamp();
	// \"%r\" First line of request
	Log.Quote(HTTP.Request.RequestLine.Get());
	// %>s final http status
	Log += HTTP.Response.GetStatusCode();
	// %b content-length, uncompressed
	Log += HTTP.GetContentLength();

	if (m_LogFormat != LOG_FORMAT::COMMON)
	{
		// \"%{Referer}i\"
		Log.Quote(HTTP.Request.Headers.Get(KHTTPHeader::REFERER));
		// \"%{User-Agent}i\"
		Log.Quote(HTTP.Request.Headers.Get(KHTTPHeader::USER_AGENT));

		if (m_LogFormat != LOG_FORMAT::COMBINED)
		{
			// %V host
			Log.Quote(HTTP.Request.Headers.Get(KHTTPHeader::HOST));
			// %I true input byte count before uncompressing
			Log += HTTP.GetReceivedBytes();
			// %O true output byte count after compression and chunking
			Log += HTTP.GetSentBytes();
			// %P The process ID of the child that serviced the request - we take the thread id
			Log += kGetTid();
			// %D The time taken to serve the request, in microseconds
			Log += kFormat("{};", HTTP.GetTimeToLastByte().microseconds());
			// \"%{X-Forwarded-For}i\"
			Log.Quote(HTTP.Request.Headers.Get(KHTTPHeader::X_FORWARDED_FOR));
		}
	}

	kLogger(*m_LogStream, Log.Get());

} // WriteAccessLog

//-----------------------------------------------------------------------------
void KHTTPLog::WriteParsedAccessLog(const KRESTServer& HTTP) const
//-----------------------------------------------------------------------------
{
	// https://httpd.apache.org/docs/current/mod/mod_log_config.html

	enum LOGSTATE { Raw, Flag, Variable, VariableType };

	KString        sVariable;
	PrintParsedLog Log;
	LOGSTATE       state { Raw };
//	char           chModifier { 0 };

	for (auto ch : m_sFormat)
	{
		switch (state)
		{
			case Raw:
				if (ch == '%')
				{
					state = Flag;
				}
				else
				{
					Log.RawChar(ch);
				}
				break;

			case Flag:
				switch (ch)
				{
					case '<':
					case '>':
//						chModifier = ch;
						continue;

					case '%':
						Log.RawChar('%');
						state = Raw;
						continue;

					case '{':
						sVariable.clear();
						state = Variable;
						continue;

					case 'a': // remote IP
					case 'h': // remote hostname - we log the IP
						Log.Write(HTTP.GetRemoteIP());
						break;

					case 'A': // local IP address (interface)
						Log.Write(""); // TBD
						break;

					case 'b': // content length with -
						Log.Write(HTTP.GetContentLength());
						break;

					case 'B': // content length with 0
						Log.Raw(HTTP.GetContentLength());
						break;

					case 'D': // TTLB in usecs
						Log.Write(chrono::duration_cast<chrono::microseconds>(HTTP.GetTimeToLastByte()).count());
						break;

					case 'f': // filename?
					case 'l': // remote log name
					case 'L': // error log ID
						Log.Write("");
						break;

					case 'H': // protocol (HTTP or HTTPS)
						Log.Write(HTTP.Protocol.Serialize());
						break;

					case 'k': // keepalive round, 0 based
						Log.Raw(HTTP.GetKeepaliveRound());
						break;

					case 'm': // request method
						Log.Write(HTTP.Request.RequestLine.GetMethod());
						break;

					case 'p': // canonical port
						Log.Write(HTTP.Port);
						break;

					case 'P': // pid
						Log.Write(kGetPid());
						break;

					case 'q': // query string (prepended with a ? if a query string exists, otherwise an empty string)
						if (HTTP.Request.RequestLine.GetQuery().empty())
						{
							Log.Raw("");
						}
						else
						{
							Log.RawChar('?');
							Log.Escape(HTTP.Request.RequestLine.GetQuery());
						}
						break;

					case 'r': // "first line" - the request line
						Log.Escape(HTTP.Request.RequestLine.Get());
						break;

					case 's':
						Log.Write(HTTP.Response.GetStatusCode());
						break;

					case 't':
						Log.Write(kFormCommonLogTimestamp());
						break;

					case 'T': // TTLB in seconds..
						Log.Write(HTTP.GetTimeToLastByte().seconds().count());
						break;

					case 'u': // remote user
						Log.Escape(HTTP.GetAuthenticatedUser());
						break;

					case 'U': // URL path, without query
						Log.Escape(HTTP.Request.Resource.Path.Serialize());
						break;

					case 'v': // canonical server name
					case 'V': // server name
						Log.Escape(HTTP.Request.Headers.Get(KHTTPHeader::HOST));
						break;

					case 'X':
						//	Connection status when response is completed:
						//	X =	Connection aborted before the response completed.
						//	+ =	Connection may be kept alive after the response is sent.
						//	- =	Connection will be closed after the response is sent
						Log.RawChar(HTTP.GetLostConnection() ? 'X' : HTTP.GetKeepalive() ? '+' : '-');
						break;

					case 'I': // rx bytes
						Log.Write(HTTP.GetReceivedBytes());
						break;

					case 'O': // rx bytes
						Log.Write(HTTP.GetSentBytes());
						break;

					case 'S': // rx + tx bytes
						Log.Write(HTTP.GetReceivedBytes() + HTTP.GetSentBytes());
						break;

					case 'R': // "handler"
					default:
						kDebug(1, "selection '{}' not handled", ch);
						Log.Write("");
						break;
				}
				state = Raw;
//				chModifier = 0; // as we do not use the chModifier currently...
				break;

			case Variable:
				if (ch == '}')
				{
					state = VariableType;
				}
				else
				{
					sVariable += ch;
				}
				break;

			case VariableType:
				switch (ch)
				{
					case 'a': // {c}a direct peer IP address
					case 'h': // {c}h direct peer hostname
						if (sVariable == "c")
						{
							Log.Write(HTTP.GetConnectedClientIP());
						}
						else
						{
							kDebug(1, "bad variable '{{{}}}{}'", sVariable, ch);
							Log.Write("");
						}
						break;

					case 'i': // request header
						Log.Escape(HTTP.Request.Headers.Get(sVariable));
						break;

					case 'o': // response header
						Log.Escape(HTTP.Response.Headers.Get(sVariable));
						break;

					case 'e': // Environment
						Log.Escape(kGetEnv(sVariable));
						break;

					case 'C': // Cookie
						Log.Escape(HTTP.Request.GetCookie(sVariable));
						break;

					case 'p': // port
						if (sVariable == "canonical")
						{
							Log.Write(HTTP.Port);
						}
						else if (sVariable == "local")
						{
							Log.Write(HTTP.Port);
						}
						else if (sVariable == "remote")
						{
							Log.Write(HTTP.GetRemotePort());
						}
						else
						{
							kDebug(1, "bad variable '{{{}}}{}'", sVariable, ch);
							Log.Write(0);
						}
						break;

					case 'P': // pid or tid
						if (sVariable == "pid")
						{
							Log.Write(kGetPid());
						}
						else if (sVariable == "tid")
						{
							Log.Write(kGetTid());
						}
						else if (sVariable == "hextid")
						{
							Log.Write(KString::to_string(kGetTid(), 16, true, false));
						}
						else
						{
							kDebug(1, "bad variable '{{{}}}{}'", sVariable, ch);
							Log.Write(0);
						}
						break;

					case 'T': // used time in various formats
						if (sVariable == "us" || sVariable == "µs")
						{
							Log.Write(HTTP.GetTimeToLastByte().microseconds().count());
						}
						else if (sVariable == "ms")
						{
							Log.Write(HTTP.GetTimeToLastByte().milliseconds().count());
						}
						else if (sVariable == "s")
						{
							Log.Write(HTTP.GetTimeToLastByte().seconds().count());
						}
						else
						{
							kDebug(1, "bad variable '{{{}}}{}'", sVariable, ch);
							Log.Write(0);
						}

						break;

					case 't': // time in various formats
					case 'n': // other module
					default:  // not supported
						kDebug(1, "selection '{{{}}}{}' not handled", sVariable, ch);
						Log.Write(0);
						break;
				}
				state = Raw;
				break;
		}
	}

	kLogger(*m_LogStream, Log.Get());

} // WriteParsedAccessLog

//-----------------------------------------------------------------------------
void KHTTPLog::Log(const KRESTServer& HTTP) const
//-----------------------------------------------------------------------------
{
	if (m_LogStream != nullptr)
	{
		switch (m_LogFormat)
		{
			case LOG_FORMAT::NONE:
				return;

			case LOG_FORMAT::JSON:
				return WriteJSONAccessLog(HTTP);

			case LOG_FORMAT::PARSED:
				return WriteParsedAccessLog(HTTP);

			default:
				return WriteAccessLog(HTTP);
		}
	}

} // Log

//-----------------------------------------------------------------------------
bool KHTTPLog::Open(LOG_FORMAT LogFormat, KStringViewZ sAccessLogFile, KStringView sFormat)
//-----------------------------------------------------------------------------
{
	if (m_LogStream == nullptr)
	{
		if (!SetFormat(LogFormat, sFormat))
		{
			return false;
		}

		m_LogStream = kOpenOutStream(sAccessLogFile, std::ios::app);

		if (is_open())
		{
			kDebug(1, "opened log stream at: {}", sAccessLogFile);
			return true;
		}
		else
		{
			kDebug(1, "could not open log stream at: {}", sAccessLogFile);
			return false;
		}
	}
	else
	{
		// we can only open once, as we have refs on the log stream
		// in the task queue that handles the output!
		kWarning("tried to open an already opened log stream");
		return false;
	}

} // Open

//-----------------------------------------------------------------------------
bool KHTTPLogParser::Open(KStringViewZ sAccessLogFile, KUnixTime StartDate)
//-----------------------------------------------------------------------------
{
	m_StartDate = StartDate;
	
	m_LogFile = std::make_unique<KInFile>(sAccessLogFile);

	if (m_LogFile->is_open())
	{
		m_LogStream = m_LogFile.get();
		kDebug(1, "opened log file: {}", sAccessLogFile);
		return m_LogStream->Good();
	}
	else
	{
		kDebug(1, "could not open log file: {}", sAccessLogFile);
		return false;
	}

} // Open

//-----------------------------------------------------------------------------
bool KHTTPLogParser::Open(KInStream& InStream, KUnixTime StartDate)
//-----------------------------------------------------------------------------
{
	m_StartDate = StartDate;
	m_LogStream = &InStream;

	return m_LogStream->Good();

} // Open

//-----------------------------------------------------------------------------
KStringView KHTTPLogParser::Data::GetString(bool bNoQuotes)
//-----------------------------------------------------------------------------
{
	KStringView sField;

	SkipSpaces();

	if (!m_sLine.empty())
	{
		if (!bNoQuotes && m_sLine.front() == '"')
		{
			// KStringView::substr() has internal checks for overflow
			sField = m_sLine.substr(1, m_sLine.find('"', 1) - 1);
			m_sLine.remove_prefix(sField.size() + 2);
		}
		else
		{
			sField = m_sLine.substr(0, m_sLine.find(' ', 1));
			m_sLine.remove_prefix(sField.size());

			if (sField.size() == 1 && *(sField.begin()) == '-')
			{
				sField.clear();
			}
		}
	}
	else
	{
		m_bValid = false;
	}

	return sField;

} // GetString

//-----------------------------------------------------------------------------
KUnixTime KHTTPLogParser::Data::GetTime()
//-----------------------------------------------------------------------------
{
	SkipSpaces();

	if (m_sLine.front() == '[')
	{
		// KStringView::substr() has internal checks for overflow
		auto sField = m_sLine.substr(1, m_sLine.find(']', 1) - 1);
		m_sLine.remove_prefix(sField.size() + 2);
		KUnixTime TimeStamp(sField);

		if (!TimeStamp.ok())
		{
			m_bValid = false;
		}
		else
		{
			return TimeStamp;
		}
	}
	else
	{
		m_bValid = false;
	}

	return {};

} // GetTime

//-----------------------------------------------------------------------------
KHTTPLogParser::Data::Data(KString sLine)
//-----------------------------------------------------------------------------
: m_sInput(std::move(sLine))
, m_sLine(m_sInput)
, m_bValid(true) // we check later
{
	// COMMON format
	sRemoteIP       = GetString(true);
	sIdentity       = GetString(true);
	sUser           = GetString(true);
	Timestamp       = GetTime();
	auto sRequest   = GetString();
	// first word is method
	auto sMethod    = sRequest.substr(0, sRequest.find(' '));
	sRequest.remove_prefix(sMethod.size() + 1);
	Method          = KHTTPMethod(sMethod);
	auto iNextSpace = sRequest.find(' ');
	Resource        = KResource(sRequest.substr(0, iNextSpace));
	sRequest.remove_prefix(iNextSpace + 1);
	HTTPVersion     = KHTTPVersion(sRequest);
	iHTTPStatus     = GetNumber();
	iContentLength  = GetNumber();

	if (IsValid())
	{
		// here starts COMBINED
		sReferrer = GetString();

		if (IsValid())
		{
			sUserAgent = GetString();

			// here starts EXTENDED
			sHost      = GetString();

			if (IsValid())
			{
				iRXBytes      = GetNumber();
				iTXBytes      = GetNumber();
				iProcessID    = GetNumber();
				TotalTime     = KDuration(chrono::microseconds(GetNumber()));
				sForwardedFor = GetString();
			}
			else m_bValid = true;
		}
		else m_bValid = true;
	}
}

//-----------------------------------------------------------------------------
KHTTPLogParser::Data KHTTPLogParser::Next(KUnixTime EndDate)
//-----------------------------------------------------------------------------
{
	if (!is_open()) return {};

	for (;;)
	{
		auto log = Data(m_LogStream->ReadLine());

		if (!log.IsValid())
		{
			if (!m_LogStream->Good()) return {};
			// else repeat
		}
		// log is valid - check range
		else if (log.Timestamp >= m_StartDate)
		{
			if (log.Timestamp < EndDate)
			{
				return log;
			}
			else
			{
				return {};
			}
		}
	}

} // Next

DEKAF2_NAMESPACE_END
