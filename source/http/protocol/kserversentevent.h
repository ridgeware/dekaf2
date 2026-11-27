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

/// @file kserversentevent.h
/// events of an event stream (server-sent events, media type text/event-stream),
/// a parser for the stream, and a reader of the stream from an input stream

#include <dekaf2/core/init/kdefinitions.h>
#include <dekaf2/core/errors/kerror.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/strings/kstringview.h>
#include <dekaf2/time/duration/kduration.h>

DEKAF2_NAMESPACE_BEGIN

class KInStream;

/// @addtogroup http_protocol
/// @{

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// One event of an event stream. A server sends events to a client over a
/// response of the media type text/event-stream that it keeps open; in a
/// browser an EventSource receives them.
class DEKAF2_PUBLIC KServerSentEvent
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	//-----------------------------------------------------------------------------
	KServerSentEvent() = default;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// @param sData the data of the event
	/// @param sEvent the type of the event, empty for the default type "message"
	/// @param sID the id of the event
	KServerSentEvent(KString sData, KString sEvent = KString{}, KString sID = KString{});
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Appends the event in the format of the stream to sOut. An event with a type,
	/// but without data, gets one empty line of data: clients drop events without data.
	/// An id or a type with a line break, or an id with a NUL character, is not written.
	void Serialize(KStringRef& sOut) const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// @return the event in the format of the stream
	DEKAF2_NODISCARD
	KString Serialize() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// @return true if the event has neither data nor type nor id
	DEKAF2_NODISCARD
	bool empty() const
	//-----------------------------------------------------------------------------
	{
		return sData.empty() && sEvent.empty() && sID.empty();
	}

	//-----------------------------------------------------------------------------
	void clear();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// @param sComment the text of the comment
	/// @return a comment in the format of the stream. Clients ignore comments, a
	/// server sends them to keep a connection without events open.
	DEKAF2_NODISCARD
	static KString Comment(KStringView sComment = KStringView{});
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// @param Wait the time a client shall wait before it connects again
	/// @return the instruction for the client in the format of the stream, empty
	/// for a negative time
	DEKAF2_NODISCARD
	static KString Retry(KDuration Wait);
	//-----------------------------------------------------------------------------

	/// the data of the event, of one or multiple lines
	KString sData;
	/// the type of the event, empty for the default type "message"
	KString sEvent;
	/// the id of the event - a client that connects again sends the last id it
	/// received in the request header Last-Event-ID
	KString sID;

}; // KServerSentEvent

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Parser for an event stream. It takes the stream in pieces of any size, and
/// returns the events that are complete. A stream with a line that is longer than
/// permitted, or with an event whose data is larger than permitted, fails: Next()
/// returns false, HasError() is true, and the parser drops all further input until
/// clear().
///
/// @code
/// KServerSentEventParser Parser;
/// KServerSentEvent       Event;
///
/// while (ReadSome(sBuffer))
/// {
///     Parser.Add(sBuffer);
///
///     while (Parser.Next(Event))
///     {
///         // use Event
///     }
/// }
/// @endcode
class DEKAF2_PUBLIC KServerSentEventParser : public KErrorBase
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	/// the default for the largest permitted length of a line of the stream
	static constexpr std::size_t DefaultMaxLineLength { 16 * 1024 * 1024 };
	/// the default for the largest permitted size of the data of an event
	static constexpr std::size_t DefaultMaxEventSize  { 16 * 1024 * 1024 };

	//-----------------------------------------------------------------------------
	/// @param iMaxLineLength the largest permitted length of a line of the stream in
	/// bytes, without its line break, npos for no limit
	/// @param iMaxEventSize the largest permitted size of the data of an event in bytes,
	/// with the line breaks between its lines, npos for no limit
	explicit KServerSentEventParser(std::size_t iMaxLineLength = DefaultMaxLineLength,
	                                std::size_t iMaxEventSize  = DefaultMaxEventSize)
	//-----------------------------------------------------------------------------
	: m_iMaxLineLength(iMaxLineLength)
	, m_iMaxEventSize(iMaxEventSize)
	{
	}

	//-----------------------------------------------------------------------------
	/// @param sInput the next piece of the stream
	void Add(KStringView sInput);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// @param Event receives the next event. Its id is the last id that the stream
	/// has set, also if that was with an earlier event.
	/// @return false if the input that was added so far has no further complete event,
	/// or if the stream failed
	bool Next(KServerSentEvent& Event);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// @return the last id that the stream has set with a complete event - a client
	/// sends it in the request header Last-Event-ID when it connects again
	DEKAF2_NODISCARD
	const KString& GetLastEventID() const
	//-----------------------------------------------------------------------------
	{
		return m_sLastEventID;
	}

	//-----------------------------------------------------------------------------
	/// @return true if the stream has set a time to wait before connecting again
	DEKAF2_NODISCARD
	bool HasRetry() const
	//-----------------------------------------------------------------------------
	{
		return m_bHasRetry;
	}

	//-----------------------------------------------------------------------------
	/// @return the time to wait before connecting again, if the stream has set one
	DEKAF2_NODISCARD
	KDuration GetRetry() const
	//-----------------------------------------------------------------------------
	{
		return m_Retry;
	}

	//-----------------------------------------------------------------------------
	/// forget all input, state and error, to parse a new stream
	void clear();
	//-----------------------------------------------------------------------------

//------
private:
//------

	//-----------------------------------------------------------------------------
	/// get the next complete line of the input
	DEKAF2_PRIVATE
	bool NextLine(KStringView& sLine);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// take a line with a field into the event that is being built
	/// @return false if the stream failed
	DEKAF2_PRIVATE
	bool AddField(KStringView sLine);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// drop the input and the event that is being built, and set the error
	DEKAF2_PRIVATE
	void Fail(KStringViewZ sError);
	//-----------------------------------------------------------------------------

	KString     m_sInput;                 // the input that is not parsed yet
	std::size_t m_iMaxLineLength;
	std::size_t m_iMaxEventSize;
	std::size_t m_iPos        { 0 };      // start of the unparsed input in m_sInput
	std::size_t m_iScanned    { 0 };      // m_sInput has no line break between m_iPos and here
	KString     m_sData;                  // data of the event being built, each line with a LF
	KString     m_sEvent;                 // type of the event being built
	KString     m_sID;                    // the id that the stream has set last
	KString     m_sLastEventID;           // m_sID at the last complete event
	KDuration   m_Retry;
	bool        m_bHasRetry   { false };
	bool        m_bAtStart    { true  };  // a byte order mark may follow
	bool        m_bSkipLF     { false };  // the last line ended with a CR at the end of the input

}; // KServerSentEventParser

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Reads the events of an event stream from an input stream, e.g. from the body of
/// a response that a KHTTPClient receives. Next() waits for the next complete event.
///
/// The read timeout of the stream ends the stream when the server sends nothing for
/// that long - it has to be longer than the time between the comment lines that the
/// server sends.
///
/// @code
/// KHTTPClient HTTP;
/// HTTP.Connect(URL);
/// HTTP.Resource(URL);
/// HTTP.AddHeader(KHTTPHeader::ACCEPT, KMIME::EVENT_STREAM);
///
/// if (!sLastEventID.empty())
/// {
///     // a client that connects again
///     HTTP.AddHeader(KHTTPHeader::LAST_EVENT_ID, sLastEventID);
/// }
///
/// if (HTTP.SendRequest() && HTTP.Response.GetStatusCode() == KHTTPError::H2xx_OK)
/// {
///     KServerSentEventReader Events(HTTP.Response.FilteredStream());
///     KServerSentEvent       Event;
///
///     while (Events.Next(Event))
///     {
///         // use Event
///     }
///
///     sLastEventID = Events.GetLastEventID();
/// }
/// @endcode
class DEKAF2_PUBLIC KServerSentEventReader
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	//-----------------------------------------------------------------------------
	/// @param Stream the stream to read from
	/// @param iMaxLineLength the largest permitted length of a line of the stream in
	/// bytes, without its line break, npos for no limit
	/// @param iMaxEventSize the largest permitted size of the data of an event in bytes,
	/// with the line breaks between its lines, npos for no limit
	explicit KServerSentEventReader(KInStream&  Stream,
	                                std::size_t iMaxLineLength = KServerSentEventParser::DefaultMaxLineLength,
	                                std::size_t iMaxEventSize  = KServerSentEventParser::DefaultMaxEventSize);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// @param Event receives the next event, see KServerSentEventParser::Next()
	/// @return false at the end of the stream, or if the stream failed (see HasError())
	bool Next(KServerSentEvent& Event);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// @return the last id that the stream has set with a complete event - a client
	/// sends it in the request header Last-Event-ID when it connects again
	DEKAF2_NODISCARD
	const KString& GetLastEventID() const
	//-----------------------------------------------------------------------------
	{
		return m_Parser.GetLastEventID();
	}

	//-----------------------------------------------------------------------------
	/// @return true if the stream has set a time to wait before connecting again
	DEKAF2_NODISCARD
	bool HasRetry() const
	//-----------------------------------------------------------------------------
	{
		return m_Parser.HasRetry();
	}

	//-----------------------------------------------------------------------------
	/// @return the time to wait before connecting again, if the stream has set one
	DEKAF2_NODISCARD
	KDuration GetRetry() const
	//-----------------------------------------------------------------------------
	{
		return m_Parser.GetRetry();
	}

	//-----------------------------------------------------------------------------
	/// @return true if the stream failed: a line or an event exceeded its limit
	DEKAF2_NODISCARD
	bool HasError() const
	//-----------------------------------------------------------------------------
	{
		return m_Parser.HasError();
	}

	//-----------------------------------------------------------------------------
	/// @return the reason why the stream failed
	DEKAF2_NODISCARD
	KStringViewZ GetLastError() const
	//-----------------------------------------------------------------------------
	{
		return m_Parser.GetLastError();
	}

//------
private:
//------

	KInStream&             m_Stream;
	KServerSentEventParser m_Parser;

}; // KServerSentEventReader

/// @}

DEKAF2_NAMESPACE_END
