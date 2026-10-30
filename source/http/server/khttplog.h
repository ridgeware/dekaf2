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

#pragma once


/// @file khttplog.h
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/strings/kstringview.h>
#include <dekaf2/core/logging/kdatedlogfile.h>
#include <dekaf2/io/readwrite/kreader.h>
#include <dekaf2/io/readwrite/kwriter.h>
#include <dekaf2/http/protocol/khttp_method.h>
#include <dekaf2/http/protocol/khttp_header.h>
#include <dekaf2/time/clock/ktime.h>
#include <dekaf2/time/duration/kduration.h>
#include <dekaf2/web/url/kurl.h>
#include <dekaf2/http/protocol/khttp_version.h>
#include <vector>

DEKAF2_NAMESPACE_BEGIN

/// @addtogroup http_server
/// @{

class KRESTServer;

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// common enums and values for http log classes
class DEKAF2_PUBLIC KHTTPLogBase
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
//------
public:
//------

	enum class LOG_FORMAT
	{
		NONE,
		JSON,
		COMMON,
		COMBINED,
		EXTENDED,
		PARSED
	};

//------
protected:
//------

	bool SetFormat(LOG_FORMAT LogFormat, KStringView sFormat);

	KString    m_sFormat;
	LOG_FORMAT m_LogFormat { LOG_FORMAT::NONE };

}; // KHTTPLogBase

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Write a HTTP log file in various formats, including standard Common and Combined log formats
class DEKAF2_PUBLIC KHTTPLog : public KHTTPLogBase
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
//------
public:
//------

	KHTTPLog() = default;

	/// Set the format and file name for the access log
	/// @param LogFormat one of JSON, COMMON, COMBINED, EXTENDED, PARSED
	/// @param sAccessLogFile the filename for the access log, or "stdout" / "stderr" for console output
	/// @param sFormat the format string for PARSED format
	/// @return true if file can be opened, false otherwise
	KHTTPLog(LOG_FORMAT LogFormat, KStringViewZ sAccessLogFile, KStringView sFormat = KStringView{})
	{
		Open(LogFormat, sAccessLogFile, sFormat);
	}

	/// Set the format and file name for an access log with a file per day, see KDatedLogFile
	/// @param LogFormat one of JSON, COMMON, COMBINED, EXTENDED, PARSED
	/// @param sAccessLogFile the filename for the access log, or "stdout" / "stderr" for console output
	/// without days
	/// @param sFormat the format string for PARSED format
	/// @param DailyOptions the time zone of the days, the maximum size of a file and the grace period
	/// for late records
	KHTTPLog(LOG_FORMAT LogFormat, KStringViewZ sAccessLogFile, KStringView sFormat, KDatedLogFile::Options DailyOptions)
	{
		Open(LogFormat, sAccessLogFile, sFormat, std::move(DailyOptions));
	}

	/// Set the format and file name for the access log - can only be called once per instance
	/// @param LogFormat one of JSON, COMMON, COMBINED, EXTENDED, PARSED
	/// @param sAccessLogFile the filename for the access log, or "stdout" / "stderr" for console output
	/// @param sFormat the format string for PARSED format
	/// @return true if file can be opened, false otherwise
	bool Open(LOG_FORMAT LogFormat, KStringViewZ sAccessLogFile, KStringView sFormat = KStringView{});

	/// Set the format and file name for an access log with a file per day - can only be called once
	/// per instance. The time stamp of each request decides about the day of its line, and the
	/// files of past days get their date appended, like access.log-20261029, see KDatedLogFile.
	/// @param LogFormat one of JSON, COMMON, COMBINED, EXTENDED, PARSED
	/// @param sAccessLogFile the filename for the access log, or "stdout" / "stderr" for console output
	/// without days
	/// @param sFormat the format string for PARSED format
	/// @param DailyOptions the time zone of the days, the maximum size of a file and the grace period
	/// for late records
	/// @return true if file can be opened, false otherwise
	bool Open(LOG_FORMAT LogFormat, KStringViewZ sAccessLogFile, KStringView sFormat, KDatedLogFile::Options DailyOptions);

	/// Set the file name for the access log - can only be called once per instance. Will use COMBINED output format
	/// @param sAccessLogFile the filename for the access log, or "stdout" / "stderr" for console output
	/// @return true if file can be opened, false otherwise
	bool Open(KStringViewZ sAccessLogFile) { return Open(LOG_FORMAT::COMBINED, sAccessLogFile); }

	/// Check if the log stream is available
	/// @return true if log stream is available
	bool is_open() const { return m_LogStream != nullptr || m_DatedLog != nullptr; }

	/// Log one server request/response
	/// @param HTTP the REST server object that handled the request
	void Log(const KRESTServer& HTTP) const;

//------
private:
//------

	DEKAF2_PRIVATE
	void WriteJSONAccessLog   (const KRESTServer& HTTP) const;
	DEKAF2_PRIVATE
	void WriteParsedAccessLog (const KRESTServer& HTTP) const;
	/// queues the line for the log stream or the dated log file
	DEKAF2_PRIVATE
	void Output               (KString sLine, KUnixTime tRecord) const;
	/// checks that the log is not open yet and sets and compiles the format
	DEKAF2_PRIVATE
	bool PrepareOpen          (LOG_FORMAT LogFormat, KStringView sFormat);
	/// reports whether the log stream or the dated log file could be opened
	DEKAF2_PRIVATE
	bool ReportOpen           (KStringView sAccessLogFile) const;

	/// the fields of the PARSED format
	enum FieldType : uint8_t
	{
		Text,              ///< literal text
		Empty,             ///< a field that is not supported, written as -
		RemoteIP,          ///< %a %h
		ConnectedClientIP, ///< %{c}a %{c}h
		ContentLength,     ///< %b
		ContentLengthZero, ///< %B
		Protocol,          ///< %H
		KeepaliveRound,    ///< %k
		Method,            ///< %m
		Port,              ///< %p %{canonical}p %{local}p
		RemotePort,        ///< %{remote}p
		ProcessID,         ///< %P %{pid}P
		ThreadID,          ///< %{tid}P
		HexThreadID,       ///< %{hextid}P
		Query,             ///< %q
		RequestLine,       ///< %r
		Status,            ///< %s
		Timestamp,         ///< %t
		Seconds,           ///< %T %{s}T
		MilliSeconds,      ///< %{ms}T
		MicroSeconds,      ///< %D %{us}T
		User,              ///< %u
		Path,              ///< %U
		Host,              ///< %v %V
		ConnectionStatus,  ///< %X
		ReceivedBytes,     ///< %I
		SentBytes,         ///< %O
		TransferredBytes,  ///< %S
		RequestHeader,     ///< %{name}i
		ResponseHeader,    ///< %{name}o
		Environment,       ///< %{name}e
		Cookie             ///< %{name}C
	};

	/// one field of the PARSED format
	struct Field
	{
		FieldType   Type;
		/// the literal text, or the name of the environment variable or the cookie
		KString     sValue;
		/// the header of a RequestHeader or ResponseHeader
		KHTTPHeader Header;
	};

	/// translates a format string into m_Fields - the COMMON, COMBINED and EXTENDED formats are
	/// written with their fixed format strings
	DEKAF2_PRIVATE
	void CompileFormat(KStringView sFormat);

	std::vector<Field>                     m_Fields;
	mutable std::unique_ptr<KOutStream>    m_LogStream;
	/// set instead of m_LogStream for a log with a file per day
	mutable std::unique_ptr<KDatedLogFile> m_DatedLog;

}; // KHTTPLog


//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Read a HTTP log and split into values
class DEKAF2_PUBLIC KHTTPLogParser
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
//------
public:
//------

	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	class Data
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
	//------
	public:
	//------

		Data() = default;
		Data(KString sLine);

		/// did the parsing succeed?
		bool IsValid() const { return m_bValid; }
		/// return the original log line before parsing
		const KString& GetLogLine() const { return m_sInput; }

		KStringView  sRemoteIP;
		KStringView  sIdentity; // we do not write this
		KStringView  sUser;
		KStringView  sReferrer;
		KStringView  sUserAgent;
		KStringView  sHost;
		KStringView  sForwardedFor;
		KResource    Resource;
		uint64_t     iContentLength;
		uint64_t     iRXBytes;
		uint64_t     iTXBytes;
		uint64_t     iProcessID;
		KDuration    TotalTime;
		KUnixTime    Timestamp;
		KHTTPMethod  Method;
		KHTTPVersion HTTPVersion;
		uint16_t     iHTTPStatus;

	//------
	private:
	//------

		KUnixTime   GetTime();
		KStringView GetString(bool bNoQuotes = false);
		uint64_t    GetNumber()  { return GetString(true).UInt64();    }
		void        SkipSpaces() { while (m_sLine.remove_prefix(' ')); }
		KStringView Unescape(KStringView sField);

		KString     m_sInput;
		/// the fields that contained escapes, without the escapes
		KString     m_sUnescaped;
		KStringView m_sLine;
		bool        m_bValid { false };

	}; // Data

	KHTTPLogParser() = default;

	/// construct from a file name
	/// @param sAccessLogFile the filename for the access log, must be in format COMMON, COMBINED, or EXTENDED
	/// @param StartDate return data that has a timestamp greater or equal than StartDate, defaults to KUnixTime::min()
	KHTTPLogParser(KStringViewZ sAccessLogFile, KUnixTime StartDate = KUnixTime::min())
	{
		Open(sAccessLogFile, StartDate);
	}

	/// construct from an input stream
	/// @param InStream the input stream for the access log, must be in format COMMON, COMBINED, or EXTENDED
	/// @param StartDate return data that has a timestamp greater or equal than StartDate, defaults to KUnixTime::min()
	KHTTPLogParser(KInStream& InStream, KUnixTime StartDate = KUnixTime::min())
	{
		Open(InStream, StartDate);
	}

	/// Set the file name for the access log
	/// @param sAccessLogFile the filename for the access log, must be in format COMMON, COMBINED, or EXTENDED
	/// @param StartDate return data that has a timestamp greater or equal than StartDate, defaults to KUnixTime::min()
	/// @return true if file can be opened, false otherwise
	bool Open(KStringViewZ sAccessLogFile, KUnixTime StartDate = KUnixTime::min());

	/// Set the input stream for the access log
	/// @param InStream the input stream for the access log, must be in format COMMON, COMBINED, or EXTENDED
	/// @param StartDate return data that has a timestamp greater or equal than StartDate, defaults to KUnixTime::min()
	/// @return true if file can be opened, false otherwise
	bool Open(KInStream& InStream, KUnixTime StartDate = KUnixTime::min());

	/// Check if the log stream is available
	/// @return true if log stream is available
	bool is_open() const { return m_LogStream != nullptr && m_LogStream->Good(); }

	/// Assuming increasing timestamp values in the log, return next data record until EndDate is reached
	/// @param EndDate only return data that has a timestamp less than EndDate
	/// @return the Data record for one log entry
	Data Next(KUnixTime EndDate = KUnixTime::max());

	/// Rewinds log file reader to the beginning
	/// @returns true if success, false otherwise, e.g. if no log file is open
	bool Rewind();

	/// @returns timestamp of oldest log entry
	KUnixTime OldestDate();

	/// @returns timestamp of newest log entry
	KUnixTime NewestDate();

//------
private:
//------

	mutable std::unique_ptr<KInFile> m_LogFile;
	KInStream* m_LogStream { nullptr };
	KUnixTime m_StartDate { KUnixTime::min() };

}; // KHTTPLogParser


/// @}

DEKAF2_NAMESPACE_END
