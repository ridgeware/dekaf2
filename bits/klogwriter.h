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

#pragma once

/// @file klogwriter.h
/// Output writers for the logging framework

#include "../kdefinitions.h"

#ifdef DEKAF2_WITH_KLOG
#include "../kstring.h"
#include "../kstringview.h"
#include "../kwriter.h"
#include <memory>

#ifndef DEKAF2_IS_WINDOWS
	#define DEKAF2_HAS_SYSLOG
#endif

#ifdef DEKAF2_KLOG_WITH_TCP
	#include "../kjson.h"
#endif

DEKAF2_NAMESPACE_BEGIN

class KWebClient;
class KHTTPHeaders;

#ifdef DEKAF2_KLOG_WITH_TCP
class KConnection;
#endif

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// ABC for the LogWriter object
class DEKAF2_PUBLIC KLogWriter
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------
	
	KLogWriter() = default;
	virtual ~KLogWriter();
	virtual bool Write(int iLevel, bool bIsMultiline, KStringViewZ sOut) = 0;
	virtual bool Good() const = 0;

}; // KLogWriter

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// LogWriter to /dev/null
class DEKAF2_PUBLIC KLogNullWriter : public KLogWriter
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	KLogNullWriter() = default;
	virtual ~KLogNullWriter();
	virtual bool Write(int iLevel, bool bIsMultiline, KStringViewZ sOut) override { return true; }
	virtual bool Good() const override { return true; }

}; // KLogNullWriter

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Logwriter that instantiates around any std::ostream
class DEKAF2_PUBLIC KLogStdWriter : public KLogWriter
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	KLogStdWriter(std::ostream& iostream)
	    : m_OutStream(iostream)
	{}
	virtual bool Write(int iLevel, bool bIsMultiline, KStringViewZ sOut) override;
	virtual bool Good() const override { return m_OutStream.good(); }

//----------
private:
//----------

	std::ostream& m_OutStream;

}; // KLogStdWriter


//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Logwriter that opens a file
class DEKAF2_PUBLIC KLogFileWriter : public KLogWriter
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	KLogFileWriter(KStringViewZ sFileName);
	virtual bool Write(int iLevel, bool bIsMultiline, KStringViewZ sOut) override;
	virtual bool Good() const override { return m_OutFile.good(); }

//----------
private:
//----------

	KOutFile m_OutFile;

}; // KLogFileWriter

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Logwriter that writes into a string, possibly with concatenation chars
class DEKAF2_PUBLIC KLogStringWriter : public KLogWriter
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	/// The sConcat will be written between individual log messages.
	/// It will not be written after the last message
	KLogStringWriter(KStringRef& sOutString, KString sConcat = "\n");
	virtual bool Write(int iLevel, bool bIsMultiline, KStringViewZ sOut) override;
	virtual bool Good() const override { return true; }

//----------
private:
//----------

	KStringRef& m_OutString;
	KString m_sConcat;

}; // KLogStringWriter

#ifdef DEKAF2_KLOG_WITH_TCP

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Logwriter that writes into a JSON array
class DEKAF2_PUBLIC KLogJSONWriter : public KLogWriter
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	KLogJSONWriter(KJSON& json);
	virtual ~KLogJSONWriter();
	virtual bool Write(int iLevel, bool bIsMultiline, KStringViewZ sOut) override;
	virtual bool Good() const override;

//----------
private:
//----------

	KJSON& m_json;

}; // KLogJSONWriter

#endif // DEKAF2_KLOG_WITH_TCP

#ifdef DEKAF2_HAS_SYSLOG

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Logwriter for the syslog
class DEKAF2_PUBLIC KLogSyslogWriter : public KLogWriter
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	KLogSyslogWriter() {}
	virtual ~KLogSyslogWriter() {}
	virtual bool Write(int iLevel, bool bIsMultiline, KStringViewZ sOut) override;
	virtual bool Good() const override { return true; }

}; // KLogSyslogWriter

#endif // of DEKAF2_HAS_SYSLOG

#ifdef DEKAF2_KLOG_WITH_TCP

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Logwriter that writes to any TCP endpoint
class DEKAF2_PUBLIC KLogTCPWriter : public KLogWriter
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	KLogTCPWriter(KStringView sURL);
	virtual ~KLogTCPWriter();
	virtual bool Write(int iLevel, bool bIsMultiline, KStringViewZ sOut) override;
	virtual bool Good() const override;

//----------
protected:
//----------

	std::unique_ptr<KConnection> m_OutStream;
	KString m_sURL;

}; // KLogTCPWriter

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Logwriter that writes to any TCP endpoint using the HTTP(s) protocol
class DEKAF2_PUBLIC KLogHTTPWriter : public KLogWriter
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	KLogHTTPWriter(KStringView sURL);
	virtual ~KLogHTTPWriter();
	virtual bool Write(int iLevel, bool bIsMultiline, KStringViewZ sOut) override;
	virtual bool Good() const override;

//----------
protected:
//----------

	std::unique_ptr<KWebClient> m_OutStream;
	KString m_sURL;

}; // KLogHTTPWriter

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Logwriter that writes into a HTTP header
class DEKAF2_PUBLIC KLogHTTPHeaderWriter : public KLogWriter
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	KLogHTTPHeaderWriter(KHTTPHeaders& HTTPHeaders, KStringView sHeader = "x-klog");
	virtual ~KLogHTTPHeaderWriter();
	virtual bool Write(int iLevel, bool bIsMultiline, KStringViewZ sOut) override;
	virtual bool Good() const override;

//----------
protected:
//----------

	KHTTPHeaders& m_Headers;
	KString m_sHeader;
	size_t m_iCounter { 0 };

}; // KLogHTTPHeaderWriter

#endif // of DEKAF2_KLOG_WITH_TCP

DEKAF2_NAMESPACE_END

#endif // of DEKAF2_WITH_KLOG
