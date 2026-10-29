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

#include <dekaf2/http/server/khttplog.h>
#include <dekaf2/core/strings/kstringutils.h>
#include <dekaf2/io/readwrite/kwriter.h>
#include <dekaf2/data/json/kjson.h>
#include <dekaf2/rest/framework/krestserver.h>
#include <dekaf2/crypto/encoding/kencode.h>
#include <dekaf2/time/clock/ktime.h>
#include <dekaf2/system/os/ksystem.h>
#include <dekaf2/io/readwrite/kread.h>
#include <algorithm>

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
	PrintParsedLog()
	//-----------------------------------------------------------------------------
	{
		// room for a typical log line, so that the line does not have to grow several times
		m_sLogline.reserve(512);
	}

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
		// we want to display npos etc. as negative values
		if (DEKAF2_UNLIKELY(static_cast<int64_t>(iInteger) < 0))
		{
			m_sLogline += '-';
			// the two's complement is the absolute value, also for the smallest int64_t
			iInteger = ~iInteger + 1;
		}

		// the digits are written backwards from the end of the buffer, which has room
		// for the 20 digits of the largest uint64_t
		char  sBuffer[20];
		char* pEnd   = sBuffer + sizeof(sBuffer);
		char* pDigit = pEnd;

		do
		{
			*--pDigit = static_cast<char>('0' + iInteger % 10);
		}
		while (iInteger /= 10);

		m_sLogline.append(pDigit, static_cast<std::size_t>(pEnd - pDigit));

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
	/// escape like Escape(), and remove the spaces, for a field that is not framed in quotes
	void EscapeWithoutSpaces(KStringView sInput)
	//-----------------------------------------------------------------------------
	{
		auto iStart = m_sLogline.size();

		kEscapeForLogging(m_sLogline, sInput);

		m_sLogline.erase(std::remove(m_sLogline.begin() + iStart, m_sLogline.end(), ' '), m_sLogline.end());

		if (m_sLogline.size() == iStart)
		{
			m_sLogline += '-';
		}

	} // EscapeWithoutSpaces

	//-----------------------------------------------------------------------------
	/// append the current time in the common log format, like [18/Sep/2011:19:18:28 +0000]
	void Timestamp()
	//-----------------------------------------------------------------------------
	{
		detail::AppendCommonLogTimestamp(m_sLogline, KUTCTime::now());

	} // Timestamp

	//-----------------------------------------------------------------------------
	/// move the log line out
	KString Get() &&
	//-----------------------------------------------------------------------------
	{
		return std::move(m_sLogline);
	}

//------
protected:
//------

	KString m_sLogline;

}; // PrintParsedLog

// the fixed formats, written like the PARSED format
// https://httpd.apache.org/docs/current/mod/mod_log_config.html
constexpr KStringView s_sCommonFormat   = R"(%h %l %u %t "%r" %>s %b)";
constexpr KStringView s_sCombinedFormat = R"(%h %l %u %t "%r" %>s %b "%{Referer}i" "%{User-Agent}i")";
// in addition: the host, the received and the sent bytes, the thread ID, the time to the last byte
// in microseconds, and the X-Forwarded-For header
constexpr KStringView s_sExtendedFormat = R"(%h %l %u %t "%r" %>s %b "%{Referer}i" "%{User-Agent}i" "%V" %I %O %{tid}P %D; "%{X-Forwarded-For}i")";

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
void KHTTPLog::CompileFormat(KStringView sFormat)
//-----------------------------------------------------------------------------
{
	// https://httpd.apache.org/docs/current/mod/mod_log_config.html

	enum LOGSTATE { Raw, Flag, Variable, VariableType };

	KString  sText;
	KString  sVariable;
	LOGSTATE state { Raw };
//	char     chModifier { 0 };

	m_Fields.clear();

	// adds a field, after the literal text that precedes it
	auto Add = [this, &sText](FieldType Type, KString sValue = KString{}, KHTTPHeader Header = KHTTPHeader{})
	{
		if (!sText.empty())
		{
			m_Fields.push_back({ Text, std::move(sText), KHTTPHeader{} });
			sText.clear();
		}

		m_Fields.push_back({ Type, std::move(sValue), std::move(Header) });
	};

	for (auto ch : sFormat)
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
					sText += ch;
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
						sText += '%';
						state = Raw;
						continue;

					case '{':
						sVariable.clear();
						state = Variable;
						continue;

					case 'a': // remote IP
					case 'h': // remote hostname - we log the IP
						Add(RemoteIP);
						break;

					case 'A': // local IP address (interface) - TBD
					case 'f': // filename?
					case 'l': // remote log name
					case 'L': // error log ID
						Add(Empty);
						break;

					case 'b': // content length with -
						Add(ContentLength);
						break;

					case 'B': // content length with 0
						Add(ContentLengthZero);
						break;

					case 'D': // TTLB in usecs
						Add(MicroSeconds);
						break;

					case 'H': // protocol (HTTP or HTTPS)
						Add(Protocol);
						break;

					case 'k': // keepalive round, 0 based
						Add(KeepaliveRound);
						break;

					case 'm': // request method
						Add(Method);
						break;

					case 'p': // canonical port
						Add(Port);
						break;

					case 'P': // pid
						Add(ProcessID);
						break;

					case 'q': // query string (prepended with a ? if a query string exists, otherwise an empty string)
						Add(Query);
						break;

					case 'r': // "first line" - the request line
						Add(RequestLine);
						break;

					case 's':
						Add(Status);
						break;

					case 't':
						Add(Timestamp);
						break;

					case 'T': // TTLB in seconds..
						Add(Seconds);
						break;

					case 'u': // remote user
						Add(User);
						break;

					case 'U': // URL path, without query
						Add(Path);
						break;

					case 'v': // canonical server name
					case 'V': // server name
						Add(Host);
						break;

					case 'X':
						//	Connection status when response is completed:
						//	X =	Connection aborted before the response completed.
						//	+ =	Connection may be kept alive after the response is sent.
						//	- =	Connection will be closed after the response is sent
						Add(ConnectionStatus);
						break;

					case 'I': // rx bytes
						Add(ReceivedBytes);
						break;

					case 'O': // tx bytes
						Add(SentBytes);
						break;

					case 'S': // rx + tx bytes
						Add(TransferredBytes);
						break;

					case 'R': // "handler"
					default:
						kDebug(1, "selection '{}' not handled", ch);
						Add(Empty);
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
							Add(ConnectedClientIP);
						}
						else
						{
							kDebug(1, "bad variable '{{{}}}{}'", sVariable, ch);
							Add(Empty);
						}
						break;

					case 'i': // request header
						Add(RequestHeader, KString{}, KHTTPHeader(sVariable));
						break;

					case 'o': // response header
						Add(ResponseHeader, KString{}, KHTTPHeader(sVariable));
						break;

					case 'e': // Environment
						Add(Environment, sVariable);
						break;

					case 'C': // Cookie
						Add(Cookie, sVariable);
						break;

					case 'p': // port
						if (sVariable == "canonical" || sVariable == "local")
						{
							Add(Port);
						}
						else if (sVariable == "remote")
						{
							Add(RemotePort);
						}
						else
						{
							kDebug(1, "bad variable '{{{}}}{}'", sVariable, ch);
							Add(Empty);
						}
						break;

					case 'P': // pid or tid
						if (sVariable == "pid")
						{
							Add(ProcessID);
						}
						else if (sVariable == "tid")
						{
							Add(ThreadID);
						}
						else if (sVariable == "hextid")
						{
							Add(HexThreadID);
						}
						else
						{
							kDebug(1, "bad variable '{{{}}}{}'", sVariable, ch);
							Add(Empty);
						}
						break;

					case 'T': // used time in various formats
						if (sVariable == "us" || sVariable == "µs")
						{
							Add(MicroSeconds);
						}
						else if (sVariable == "ms")
						{
							Add(MilliSeconds);
						}
						else if (sVariable == "s")
						{
							Add(Seconds);
						}
						else
						{
							kDebug(1, "bad variable '{{{}}}{}'", sVariable, ch);
							Add(Empty);
						}
						break;

					case 't': // time in various formats
					case 'n': // other module
					default:  // not supported
						kDebug(1, "selection '{{{}}}{}' not handled", sVariable, ch);
						Add(Empty);
						break;
				}
				state = Raw;
				break;
		}
	}

	if (!sText.empty())
	{
		m_Fields.push_back({ Text, std::move(sText), KHTTPHeader{} });
	}

} // CompileFormat

//-----------------------------------------------------------------------------
void KHTTPLog::WriteParsedAccessLog(const KRESTServer& HTTP) const
//-----------------------------------------------------------------------------
{
	PrintParsedLog Log;

	for (const auto& Entry : m_Fields)
	{
		switch (Entry.Type)
		{
			case Text:
				Log.Raw(Entry.sValue);
				break;

			case Empty:
				Log.Write("");
				break;

			case RemoteIP:
				Log.Write(HTTP.GetRemoteIP());
				break;

			case ConnectedClientIP:
				Log.Write(HTTP.GetConnectedClientIP());
				break;

			case ContentLength:
				Log.Write(HTTP.GetContentLength());
				break;

			case ContentLengthZero:
				Log.Raw(HTTP.GetContentLength());
				break;

			case Protocol:
				Log.Write(HTTP.Protocol.Serialize());
				break;

			case KeepaliveRound:
				Log.Raw(HTTP.GetKeepaliveRound());
				break;

			case Method:
				Log.Write(HTTP.Request.RequestLine.GetMethod());
				break;

			case Port:
				Log.Write(HTTP.Port);
				break;

			case RemotePort:
				Log.Write(HTTP.GetRemotePort());
				break;

			case ProcessID:
				Log.Write(kGetPid());
				break;

			case ThreadID:
				Log.Write(kGetTid());
				break;

			case HexThreadID:
				Log.Write(KString::to_string(kGetTid(), 16, true, false));
				break;

			case Query:
				if (!HTTP.Request.RequestLine.GetQuery().empty())
				{
					Log.RawChar('?');
					Log.Escape(HTTP.Request.RequestLine.GetQuery());
				}
				break;

			case RequestLine:
				Log.Escape(HTTP.Request.RequestLine.Get());
				break;

			case Status:
				Log.Write(HTTP.Response.GetStatusCode());
				break;

			case Timestamp:
				Log.Timestamp();
				break;

			// the durations are written as numbers, also when they are 0
			case Seconds:
				Log.Raw(HTTP.GetTimeToLastByte().seconds().count());
				break;

			case MilliSeconds:
				Log.Raw(HTTP.GetTimeToLastByte().milliseconds().count());
				break;

			case MicroSeconds:
				Log.Raw(HTTP.GetTimeToLastByte().microseconds().count());
				break;

			case User:
				// the user is not framed in quotes
				Log.EscapeWithoutSpaces(HTTP.GetAuthenticatedUser());
				break;

			case Path:
				Log.Escape(HTTP.Request.Resource.Path.Serialize());
				break;

			case Host:
				Log.Escape(HTTP.Request.Headers.Get(KHTTPHeader::HOST));
				break;

			case ConnectionStatus:
				Log.RawChar(HTTP.GetLostConnection() ? 'X' : HTTP.GetKeepalive() ? '+' : '-');
				break;

			case ReceivedBytes:
				Log.Write(HTTP.GetReceivedBytes());
				break;

			case SentBytes:
				Log.Write(HTTP.GetSentBytes());
				break;

			case TransferredBytes:
				Log.Write(HTTP.GetReceivedBytes() + HTTP.GetSentBytes());
				break;

			case RequestHeader:
				Log.Escape(HTTP.Request.Headers.Get(Entry.Header));
				break;

			case ResponseHeader:
				Log.Escape(HTTP.Response.Headers.Get(Entry.Header));
				break;

			case Environment:
				Log.Escape(kGetEnv(Entry.sValue));
				break;

			case Cookie:
				Log.Escape(HTTP.Request.GetCookie(Entry.sValue));
				break;
		}
	}

	kLogger(*m_LogStream, std::move(Log).Get());

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

			case LOG_FORMAT::COMMON:
			case LOG_FORMAT::COMBINED:
			case LOG_FORMAT::EXTENDED:
			case LOG_FORMAT::PARSED:
				return WriteParsedAccessLog(HTTP);
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

		switch (LogFormat)
		{
			case LOG_FORMAT::NONE:
			case LOG_FORMAT::JSON:
				break;

			case LOG_FORMAT::COMMON:
				CompileFormat(s_sCommonFormat);
				break;

			case LOG_FORMAT::COMBINED:
				CompileFormat(s_sCombinedFormat);
				break;

			case LOG_FORMAT::EXTENDED:
				CompileFormat(s_sExtendedFormat);
				break;

			case LOG_FORMAT::PARSED:
				CompileFormat(m_sFormat);
				break;
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
KStringView KHTTPLogParser::Data::Unescape(KStringView sField)
//-----------------------------------------------------------------------------
{
	if (m_sUnescaped.capacity() < m_sInput.size())
	{
		// an unescaped field is never longer than the escaped one, so the buffer never grows
		// beyond the size of the input line, and the views on earlier fields stay valid
		m_sUnescaped.reserve(m_sInput.size());
	}

	auto iStart = m_sUnescaped.size();

	kUnescapeFromLogging(m_sUnescaped, sField);

	return KStringView(m_sUnescaped).substr(iStart);

} // Unescape

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
			// the field ends at the first quote that is not escaped with a backslash
			auto iEnd = kFindUnescaped(m_sLine, '"', '\\', 1);

			if (iEnd == KStringView::npos)
			{
				// no closing quote, the field takes the rest of the line
				sField = m_sLine.substr(1);
				m_sLine.clear();
			}
			else
			{
				sField = m_sLine.substr(1, iEnd - 1);
				m_sLine.remove_prefix(iEnd + 1);
			}

			if (sField.contains('\\'))
			{
				sField = Unescape(sField);
			}
		}
		else
		{
			sField = m_sLine.substr(0, m_sLine.find(' ', 1));
			m_sLine.remove_prefix(sField.size());

			if (sField.size() == 1 && *(sField.begin()) == '-')
			{
				sField.clear();
			}
			else if (sField.contains('\\'))
			{
				sField = Unescape(sField);
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

//-----------------------------------------------------------------------------
bool KHTTPLogParser::Rewind()
//-----------------------------------------------------------------------------
{
	if (!is_open())
	{
		return false;
	}

	return m_LogStream->Rewind();

} // Rewind

//-----------------------------------------------------------------------------
KUnixTime KHTTPLogParser::OldestDate()
//-----------------------------------------------------------------------------
{
	if (!is_open())
	{
		return {};
	}

	// save current state
	auto savedStartDate = m_StartDate;
	auto savedPos       = kGetReadPosition(m_LogStream->istream());

	// rewind and read the first valid entry
	m_LogStream->Rewind();
	m_StartDate = KUnixTime::min();

	auto log = Next();

	// restore state
	m_StartDate = savedStartDate;
	kSetReadPosition(m_LogStream->istream(), savedPos);

	if (log.IsValid())
	{
		return log.Timestamp;
	}

	return {};

} // OldestDate

//-----------------------------------------------------------------------------
KUnixTime KHTTPLogParser::NewestDate()
//-----------------------------------------------------------------------------
{
	if (!is_open())
	{
		return {};
	}

	// save current state
	auto savedStartDate = m_StartDate;
	auto savedPos       = kGetReadPosition(m_LogStream->istream());

	m_StartDate = KUnixTime::min();

	// try to find the last valid entry by seeking backward from the end
	// try up to 10 lines from the end to handle trailing empty lines or invalid entries
	for (std::size_t iLine = 0; iLine < 10; ++iLine)
	{
		if (!kGoToLine(m_LogStream->istream(), iLine, true))
		{
			break;
		}

		auto log = Data(m_LogStream->ReadLine());

		if (log.IsValid())
		{
			// restore state
			m_StartDate = savedStartDate;
			kSetReadPosition(m_LogStream->istream(), savedPos);
			return log.Timestamp;
		}
	}

	// restore state
	m_StartDate = savedStartDate;
	kSetReadPosition(m_LogStream->istream(), savedPos);

	return {};

} // NewestDate

DEKAF2_NAMESPACE_END
