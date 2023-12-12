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

#include "kstring.h"
#include "kstringview.h"
#include "kwriter.h"

DEKAF2_NAMESPACE_BEGIN

class KRESTServer;

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC KHTTPLog
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

	KHTTPLog() = default;

	/// Set the format and file name for the access log
	/// @param LogFormat one of JSON, COMMON, COMBINED, EXTENDED, PARSED
	/// @param sAccessLogFile the filename for the access log, or "stdout" / "stderr" for console output
	/// @return true if file can be opened, false otherwise
	KHTTPLog(LOG_FORMAT LogFormat, KStringViewZ sAccessLogFile, KStringView sFormat = KStringView{})
	{
		Open(LogFormat, sAccessLogFile, sFormat);
	}

	/// Set the format and file name for the access log - can only be called once per instance
	/// @param LogFormat one of JSON, COMMON, COMBINED, EXTENDED, PARSED
	/// @param sAccessLogFile the filename for the access log, or "stdout" / "stderr" for console output
	/// @return true if file can be opened, false otherwise
	bool Open(LOG_FORMAT LogFormat, KStringViewZ sAccessLogFile, KStringView sFormat = KStringView{});

	/// Check if the log stream is available
	/// @return true if log stream is available
	bool is_open() const { return m_LogStream != nullptr; }

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
	DEKAF2_PRIVATE
	void WriteAccessLog       (const KRESTServer& HTTP) const;

	KString                             m_sFormat;
	mutable std::unique_ptr<KOutStream> m_LogStream;
	LOG_FORMAT                          m_LogFormat { LOG_FORMAT::NONE };

}; // KHTTPLog

DEKAF2_NAMESPACE_END
