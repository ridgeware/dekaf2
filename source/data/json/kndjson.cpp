/*
//
// DEKAF(tm): Lighter, Faster, Smarter (tm)
//
// Copyright (c) 2026, Ridgeware, Inc.
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


#include <dekaf2/data/json/kndjson.h>
#include <dekaf2/core/logging/klog.h>
#include <dekaf2/core/format/kformat.h>
#include <dekaf2/io/readwrite/kreader.h>
#include <dekaf2/io/readwrite/kread.h>
#ifndef DEKAF2_WRAPPED_KJSON
	#include <dekaf2/core/types/kscopeguard.h>
#endif
#include <algorithm>
#include <array>

DEKAF2_NAMESPACE_BEGIN

namespace {

//-----------------------------------------------------------------------------
/// Parses the value of one line
/// @param json receives the value, null after an error
/// @param sValue the line from its first byte that is no white space
/// @param iLine the number of the line in the input, from 1
/// @param iColumn the count of bytes of the line before sValue
/// @param sError receives the error, with the line and the column in the input
bool ParseLine(LJSON& json, KStringView sValue, std::size_t iLine, std::size_t iColumn, KString& sError)
//-----------------------------------------------------------------------------
{
#ifndef DEKAF2_WRAPPED_KJSON
	// a line that is no JSON value is an error of the input, not of the program
	bool bResetFlag = KLog::getInstance().ShowStackOnJsonError(false);
	KAtScopeEnd( KLog::getInstance().ShowStackOnJsonError(bResetFlag) );
#endif

	DEKAF2_TRY
	{
		// this parser enforces kjson::MaxParseDepth
		kjson::Parse(json, sValue);

		return true;
	}

	DEKAF2_CATCH (const LJSON::parse_error& Error)
	{
		// The message of the json library is "[name] parse error at line 1, column 5: text",
		// without a position it is "[name] parse error: text". The text is kept, the
		// position becomes the one in the input.
		KStringView sText = Error.what();
		auto        iText = sText.find(": ");

		if (iText != KStringView::npos)
		{
			sText.remove_prefix(iText + 2);
		}

		if (Error.byte)
		{
			sError = kFormat("parse error at line {}, column {}: {}", iLine, iColumn + Error.byte, sText);
		}
		else
		{
			sError = kFormat("parse error at line {}: {}", iLine, sText);
		}
	}

	DEKAF2_CATCH (const LJSON::exception& Error)
	{
		// not a parse error for the json library, like a number that is out of range
		sError = kFormat("error at line {}: {}", iLine, kjson::kStripJSONExceptionMessage(Error.what()));
	}

	json = LJSON{};

	return false;

} // ParseLine

} // end of anonymous namespace

//-----------------------------------------------------------------------------
void KNDJSONParser::Add(KStringView sInput)
//-----------------------------------------------------------------------------
{
	if (HasError() || m_bFinished)
	{
		// a failed or finished input takes no more
		return;
	}

	if (m_iPos > 0)
	{
		// remove the input that is parsed
		m_sBuffer.erase(0, m_iPos);
		m_iScanned -= std::min(m_iScanned, m_iPos);
		m_iPos      = 0;
	}

	m_sBuffer += sInput;
	// the buffer may have moved
	m_sInput   = m_sBuffer;

} // Add

//-----------------------------------------------------------------------------
void KNDJSONParser::Finish()
//-----------------------------------------------------------------------------
{
	m_bFinished = true;

} // Finish

//-----------------------------------------------------------------------------
bool KNDJSONParser::NextLine(KStringView& sLine)
//-----------------------------------------------------------------------------
{
	if (HasError())
	{
		return false;
	}

	auto sInput = m_sInput;
	sInput.remove_prefix(m_iPos);

	// what was searched in an earlier call is not searched again
	auto iEnd = sInput.find('\n', std::max(m_iScanned, m_iPos) - m_iPos);

	// the length of the line, or of what is there of a line that is not complete
	if (std::min(iEnd, sInput.size()) > m_iMaxLineLength)
	{
		++m_iLine;
		Fail(kFormat("line {} is longer than {} bytes", m_iLine, m_iMaxLineLength));
		return false;
	}

	auto iNext = iEnd + 1;

	if (iEnd == KStringView::npos)
	{
		if (!m_bFinished || sInput.empty())
		{
			// the line is not complete, or the input has ended with a line break
			m_iScanned = m_sInput.size();
			return false;
		}

		// the last line of the input has no line break
		iEnd  = sInput.size();
		iNext = iEnd;
	}

	sLine      = sInput.substr(0, iEnd);

	m_iPos    += iNext;
	m_iScanned = m_iPos;

	++m_iLine;

	return true;

} // NextLine

//-----------------------------------------------------------------------------
bool KNDJSONParser::Next(KJSON& Value)
//-----------------------------------------------------------------------------
{
	KStringView sLine;

	while (NextLine(sLine))
	{
		// A line of JSON white space only holds no value. The json parser skips the
		// white space after a value itself, which includes the carriage return of
		// a Windows line break.
		auto iValue = sLine.find_first_not_of(" \t\r");

		if (iValue == KStringView::npos)
		{
			continue;
		}

		sLine.remove_prefix(iValue);

		LJSON   Line;
		KString sError;

		if (ParseLine(Line, sLine, m_iLine, iValue, sError))
		{
			Value = std::move(Line);

			return true;
		}

		if (!m_bSkipBrokenLines)
		{
			Fail(sError);

			return false;
		}

		kDebug(2, "skipping a line: {}", sError);

		++m_iSkippedLines;
	}

	return false;

} // Next

//-----------------------------------------------------------------------------
KJSON KNDJSONParser::GetAll()
//-----------------------------------------------------------------------------
{
	KJSON Values = KJSON::array();
	KJSON Value;

	while (Next(Value))
	{
		Values.push_back(std::move(Value));
	}

	if (HasError())
	{
		return KJSON{};
	}

	return Values;

} // GetAll

//-----------------------------------------------------------------------------
void KNDJSONParser::Fail(KStringViewZ sError)
//-----------------------------------------------------------------------------
{
	// give the memory back before the error is set, which may throw
	KString().swap(m_sBuffer);
	m_sInput   = KStringView{};
	m_iPos     = 0;
	m_iScanned = 0;

	SetError(sError);

} // Fail

//-----------------------------------------------------------------------------
void KNDJSONParser::clear()
//-----------------------------------------------------------------------------
{
	ClearError();
	m_sBuffer.clear();
	m_sInput        = KStringView{};
	m_iPos          = 0;
	m_iScanned      = 0;
	m_iLine         = 0;
	m_iSkippedLines = 0;
	m_bFinished     = false;

} // clear

//-----------------------------------------------------------------------------
KNDJSONReader::KNDJSONReader(KInStream& Stream, std::size_t iMaxLineLength, bool bSkipBrokenLines)
//-----------------------------------------------------------------------------
: m_Stream(Stream)
, m_Parser(iMaxLineLength, bSkipBrokenLines)
{
}

//-----------------------------------------------------------------------------
bool KNDJSONReader::Next(KJSON& Value)
//-----------------------------------------------------------------------------
{
	std::array<char, 4096> Buffer;

	while (!m_Parser.Next(Value))
	{
		if (m_Parser.HasError() || m_bAtEnd)
		{
			return false;
		}

		// what has arrived, without waiting for a full buffer
		auto iRead = kReadSome(m_Stream.istream(), Buffer.data(), Buffer.size());

		if (iRead)
		{
			m_Parser.Add(KStringView(Buffer.data(), iRead));
		}
		else
		{
			// the end of the stream - the last line may have no line break
			m_Parser.Finish();
			m_bAtEnd = true;
		}
	}

	return true;

} // KNDJSONReader::Next

//-----------------------------------------------------------------------------
KJSON KNDJSONReader::GetAll()
//-----------------------------------------------------------------------------
{
	KJSON Values = KJSON::array();
	KJSON Value;

	while (Next(Value))
	{
		Values.push_back(std::move(Value));
	}

	if (HasError())
	{
		return KJSON{};
	}

	return Values;

} // KNDJSONReader::GetAll

#ifdef DEKAF2_REPEAT_CONSTEXPR_VARIABLE
constexpr std::size_t KNDJSONParser::DefaultMaxLineLength;
#endif

DEKAF2_NAMESPACE_END
