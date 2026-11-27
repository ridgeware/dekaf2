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

#pragma once

/// @file kndjson.h
/// a parser, a stream reader and a stream writer for data with one JSON value per
/// line (NDJSON, also named JSON Lines, media type application/x-ndjson)

#include <dekaf2/core/init/kdefinitions.h>
#include <dekaf2/core/errors/kerror.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/strings/kstringview.h>
#include <dekaf2/data/json/kjson.h>

DEKAF2_NAMESPACE_BEGIN

class KInStream;
class KOutStream;

/// @addtogroup data_json
/// @{

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Parser for input with one JSON value per line. It returns the values one by
/// one, so that only the value of one line is in memory at a time. Every line is
/// parsed on its own: a value that spans lines is an error. Empty lines are
/// skipped.
///
/// A line that is no JSON value, or that is longer than permitted, fails the
/// input: Next() returns false, HasError() is true, and the parser drops all
/// further input until clear(). On request a line that is no JSON value is
/// skipped instead.
///
/// The input is either a string that holds all of it, which is not copied:
///
/// @code
/// KNDJSONParser Parser(sInput);
/// KJSON         Value;
///
/// while (Parser.Next(Value))
/// {
///     // use Value
/// }
/// @endcode
///
/// or it is added in pieces of any size, and the parser returns the values of
/// the lines that are complete:
///
/// @code
/// KNDJSONParser Parser;
/// KJSON         Value;
///
/// while (ReadSome(sBuffer))
/// {
///     Parser.Add(sBuffer);
///
///     while (Parser.Next(Value))
///     {
///         // use Value
///     }
/// }
///
/// // the last line may come without a line break
/// Parser.Finish();
///
/// while (Parser.Next(Value))
/// {
///     // use Value
/// }
/// @endcode
///
/// GetAll() returns the values as one array:
///
/// @code
/// auto Values = KNDJSONParser(sInput).GetAll();
/// @endcode
class DEKAF2_PUBLIC KNDJSONParser : public KErrorBase
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	/// the default for the largest permitted length of a line
	static constexpr std::size_t DefaultMaxLineLength { 16 * 1024 * 1024 };

	//-----------------------------------------------------------------------------
	/// A parser for input that is added in pieces, see Add() and Finish()
	/// @param iMaxLineLength the largest permitted length of a line in bytes, without
	/// its line break, npos for no limit
	/// @param bSkipBrokenLines skip a line that is no JSON value
	explicit KNDJSONParser(std::size_t iMaxLineLength   = DefaultMaxLineLength,
	                       bool        bSkipBrokenLines = false)
	//-----------------------------------------------------------------------------
	: m_iMaxLineLength(iMaxLineLength)
	, m_bSkipBrokenLines(bSkipBrokenLines)
	{
	}

	//-----------------------------------------------------------------------------
	/// A parser for input that is complete
	/// @param sInput the whole input. It is not copied, and has to stay valid for as
	/// long as Next() is called.
	/// @param iMaxLineLength the largest permitted length of a line in bytes, without
	/// its line break, npos for no limit
	/// @param bSkipBrokenLines skip a line that is no JSON value
	explicit KNDJSONParser(KStringView sInput,
	                       std::size_t iMaxLineLength   = DefaultMaxLineLength,
	                       bool        bSkipBrokenLines = false)
	//-----------------------------------------------------------------------------
	: m_sInput(sInput)
	, m_iMaxLineLength(iMaxLineLength)
	, m_bSkipBrokenLines(bSkipBrokenLines)
	, m_bFinished(true)
	{
	}

	//-----------------------------------------------------------------------------
	/// @param sInput the next piece of the input, which is copied
	void Add(KStringView sInput);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// The input has ended: what follows the last line break is a line as well. Input
	/// that is added after this is dropped, until clear().
	void Finish();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// @param Value receives the value of the next line
	/// @return false if the input that was added so far has no further complete line,
	/// or if the input failed
	bool Next(KJSON& Value);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// @return the values of all lines that Next() has not returned yet as an array -
	/// also for one line and for none. If the input failed the result is null.
	DEKAF2_NODISCARD
	KJSON GetAll();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// @return the number of the line that was read last, from 1: the line of the value
	/// that Next() returned, or the line that failed
	DEKAF2_NODISCARD
	std::size_t GetLine() const
	//-----------------------------------------------------------------------------
	{
		return m_iLine;
	}

	//-----------------------------------------------------------------------------
	/// @return the count of lines that were skipped because they are no JSON value
	DEKAF2_NODISCARD
	std::size_t GetSkippedLines() const
	//-----------------------------------------------------------------------------
	{
		return m_iSkippedLines;
	}

	//-----------------------------------------------------------------------------
	/// forget all input, state and error, to parse a new input that is added in pieces
	void clear();
	//-----------------------------------------------------------------------------

//------
private:
//------

	//-----------------------------------------------------------------------------
	/// get the next complete line of the input
	/// @param sLine receives the line, without its line break
	DEKAF2_PRIVATE
	bool NextLine(KStringView& sLine);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// drop the input, and set the error
	DEKAF2_PRIVATE
	void Fail(KStringViewZ sError);
	//-----------------------------------------------------------------------------

	KString     m_sBuffer;                    // the pieces of the input that are not parsed yet
	KStringView m_sInput;                     // the input to parse: m_sBuffer, or the string of the caller
	std::size_t m_iMaxLineLength;
	std::size_t m_iPos            { 0 };      // start of the unparsed input in m_sInput
	std::size_t m_iScanned        { 0 };      // m_sInput has no line break between m_iPos and here
	std::size_t m_iLine           { 0 };
	std::size_t m_iSkippedLines   { 0 };
	bool        m_bSkipBrokenLines;
	bool        m_bFinished       { false };

}; // KNDJSONParser

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Reads the values of input with one JSON value per line from an input stream,
/// e.g. from a file, or from the body of a response that a KHTTPClient receives.
/// Next() waits for the next complete line.
///
/// @code
/// KInFile       File(sFilename);
/// KNDJSONReader Reader(File);
/// KJSON         Value;
///
/// while (Reader.Next(Value))
/// {
///     // use Value
/// }
///
/// if (Reader.HasError())
/// {
///     // Reader.GetLastError() names the line
/// }
/// @endcode
class DEKAF2_PUBLIC KNDJSONReader
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	//-----------------------------------------------------------------------------
	/// @param Stream the stream to read from
	/// @param iMaxLineLength the largest permitted length of a line in bytes, without
	/// its line break, npos for no limit
	/// @param bSkipBrokenLines skip a line that is no JSON value
	explicit KNDJSONReader(KInStream&  Stream,
	                       std::size_t iMaxLineLength   = KNDJSONParser::DefaultMaxLineLength,
	                       bool        bSkipBrokenLines = false);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// @param Value receives the value of the next line
	/// @return false at the end of the stream, or if the input failed (see HasError())
	bool Next(KJSON& Value);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// @return the values of all lines that Next() has not returned yet as an array -
	/// also for one line and for none. If the input failed the result is null.
	DEKAF2_NODISCARD
	KJSON GetAll();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// @return the number of the line that was read last, from 1: the line of the value
	/// that Next() returned, or the line that failed
	DEKAF2_NODISCARD
	std::size_t GetLine() const
	//-----------------------------------------------------------------------------
	{
		return m_Parser.GetLine();
	}

	//-----------------------------------------------------------------------------
	/// @return the count of lines that were skipped because they are no JSON value
	DEKAF2_NODISCARD
	std::size_t GetSkippedLines() const
	//-----------------------------------------------------------------------------
	{
		return m_Parser.GetSkippedLines();
	}

	//-----------------------------------------------------------------------------
	/// @return true if the input failed: a line is no JSON value, or exceeded the limit
	DEKAF2_NODISCARD
	bool HasError() const
	//-----------------------------------------------------------------------------
	{
		return m_Parser.HasError();
	}

	//-----------------------------------------------------------------------------
	/// @return the reason why the input failed
	DEKAF2_NODISCARD
	KStringViewZ GetLastError() const
	//-----------------------------------------------------------------------------
	{
		return m_Parser.GetLastError();
	}

//------
private:
//------

	KInStream&    m_Stream;
	KNDJSONParser m_Parser;
	bool          m_bAtEnd { false };

}; // KNDJSONReader

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Writes JSON values to an output stream, one value per line. A line is written
/// in one piece: a value that cannot be written leaves nothing in the stream.
///
/// A value with a string that is no UTF-8 cannot be written as JSON: Write()
/// returns false, and GetLastError() names the reason. On request the bytes that
/// are no UTF-8 are replaced by U+FFFD instead.
///
/// @code
/// KOutFile      File(sFilename);
/// KNDJSONWriter Writer(File);
///
/// Writer.Write(Value);
/// @endcode
class DEKAF2_PUBLIC KNDJSONWriter : public KErrorBase
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	//-----------------------------------------------------------------------------
	/// @param Stream the stream to write to
	/// @param bFlushLines flush the stream after every line, for a reader that waits
	/// for the lines at the other end of a connection
	/// @param bReplaceInvalidUTF8 write U+FFFD for bytes of a string that are no UTF-8
	explicit KNDJSONWriter(KOutStream& Stream,
	                       bool        bFlushLines         = false,
	                       bool        bReplaceInvalidUTF8 = false)
	//-----------------------------------------------------------------------------
	: m_Stream(Stream)
	, m_bFlushLines(bFlushLines)
	, m_bReplaceInvalidUTF8(bReplaceInvalidUTF8)
	{
	}

	//-----------------------------------------------------------------------------
	/// @param Value the value to write as one line
	/// @return false if the value cannot be written as JSON, of which nothing is written
	/// then, or if the stream failed
	bool Write(const KJSON& Value);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Writes every element of an array as one line, up to the first one that cannot
	/// be written.
	/// @param Values an array
	/// @return false if Values is no array, or if an element could not be written
	bool WriteAll(const KJSON& Values);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// @return the count of lines that were written
	DEKAF2_NODISCARD
	std::size_t GetLines() const
	//-----------------------------------------------------------------------------
	{
		return m_iLines;
	}

//------
private:
//------

	KOutStream& m_Stream;
	std::size_t m_iLines { 0 };
	bool        m_bFlushLines;
	bool        m_bReplaceInvalidUTF8;

}; // KNDJSONWriter

/// @}

DEKAF2_NAMESPACE_END
