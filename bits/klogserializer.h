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

/// @file klogserializer.h
/// Data serializers for the logging framework

#include <memory>
#include "../kstring.h"
#include "../kstringview.h"
#include "../kduration.h"

#ifdef DEKAF2_KLOG_WITH_TCP
	#include "../kjson.h"
#endif

#ifndef DEKAF2_IS_WINDOWS
	#define DEKAF2_HAS_SYSLOG
#endif

namespace dekaf2
{

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Base class for KLog serialization. Takes the data to be written someplace.
class DEKAF2_PUBLIC KLogData
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	virtual ~KLogData() {}

	virtual void Set(int iLevel, KStringView sShortName, KStringView sPathName, KStringView sFunction, KStringView sMessage);
	void SetBacktrace(KStringView sBacktrace)
	{
		m_sBacktrace = sBacktrace;
	}
	int GetLevel() const
	{
		return m_iLevel;
	}

//----------
protected:
//----------

	static KStringView SanitizeFunctionName(KStringView sFunction);

	int         m_iLevel { 0 };
	pid_t       m_Pid { 0 };
	uint64_t    m_Tid { 0 }; // tid is 64 bit on OSX
	KUnixTime   m_Time { 0 };
	KStringView m_sShortName;
	KStringView m_sPathName;
	KStringView m_sFunctionName;
	KStringView m_sMessage;
	KStringView m_sBacktrace;

}; // KLogData

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Extension of the data base class. Adds the generic serialization methods
/// as virtual functions.
class DEKAF2_PUBLIC KLogSerializer : public KLogData
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	const KString& Get();
	virtual operator KStringView();
	virtual void Set(int iLevel, KStringView sShortName, KStringView sPathName, KStringView sFunction, KStringView sMessage) override;
	bool IsMultiline() const { return m_bIsMultiline; }
	/// returns true if either function name or message contain or do not contain (bInverted)
	/// the grep expression
	bool Matches(bool bEgrep, bool bInverted, KStringView sGrepExpression);

//----------
protected:
//----------

	virtual void Serialize() = 0;

	KString m_sBuffer;
	bool m_bIsMultiline;

}; // KLogSerializer

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Specialization of the serializer for TTY like output devices: creates
/// simple text lines of output
class DEKAF2_PUBLIC KLogTTYSerializer : public KLogSerializer
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
protected:
//----------

	virtual void Serialize() override;
	virtual KString PrintStatus(KStringView sLevel);

	void AddMultiLineMessage(KStringView sPrefix, KStringView sMessage);

}; // KLogTTYSerializer

#ifdef DEKAF2_HAS_SYSLOG

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Specialization of the serializer for the Syslog: creates simple text lines
/// of output, but without the prefix like timestamp and warning level
class DEKAF2_PUBLIC KLogSyslogSerializer : public KLogTTYSerializer
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
protected:
//----------

	virtual void Serialize() override;
	
}; // KLogSyslogSerializer

#endif // of DEKAF2_HAS_SYSLOG

#ifdef DEKAF2_KLOG_WITH_TCP

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Specialization of the serializer for HTTP headers: creates simple text lines
/// of output, but without the prefix like timestamp, pid, tid, program name,
/// but with an upcounting microsecond time stamp
class DEKAF2_PUBLIC KLogHTTPHeaderSerializer : public KLogTTYSerializer
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	virtual void Set(int iLevel, KStringView sShortName, KStringView sPathName, KStringView sFunction, KStringView sMessage) override;

	KString GetTimeStamp(KStringView sWhat);

	static KStringViewZ GetHeader();
	static KStringViewZ GetFooter();

//----------
protected:
//----------

	virtual KString PrintStatus(KStringView sLevel) override;

	KStopTime m_Clock;
	std::chrono::microseconds m_iElapsedMicroSeconds { 0 };

}; // KLogHTTPHeaderSerializer

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Specialization of the serializer for JSON output: creates a serialized
/// JSON object
class DEKAF2_PUBLIC KLogJSONSerializer : public KLogSerializer
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
protected:
//----------

	virtual void Serialize() override;
	KJSON CreateObject() const;

}; // KLogJSONSerializer

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Specialization of the serializer for JSON output: creates a JSON array
class DEKAF2_PUBLIC KLogJSONArraySerializer : public KLogJSONSerializer
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	KLogJSONArraySerializer(KJSON& json) : m_json(json) {}

//----------
protected:
//----------

	virtual void Serialize() override;

	KJSON& m_json;

}; // KLogJSONArraySerializer

#endif // of DEKAF2_KLOG_WITH_TCP

} // of namespace dekaf2
