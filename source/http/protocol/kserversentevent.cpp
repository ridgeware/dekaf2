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


#include <dekaf2/http/protocol/kserversentevent.h>
#include <dekaf2/core/strings/bits/kfindsetofchars.h>
#include <dekaf2/core/strings/kstringutils.h>

DEKAF2_NAMESPACE_BEGIN

namespace {

constexpr KStringView sByteOrderMark { "\xef\xbb\xbf" };

//-----------------------------------------------------------------------------
// writes sValue as lines of the field sField, or as comment lines if sField is empty
void AddLines(KStringRef& sOut, KStringView sField, KStringView sValue)
//-----------------------------------------------------------------------------
{
	for (;;)
	{
		// the line breaks of a value are CR, LF or CRLF
		auto iEnd  = sValue.find_first_of(detail::kLineBreaksSet);
		auto sLine = sValue.substr(0, iEnd);

		sOut += sField;
		sOut += ':';

		if (!sLine.empty())
		{
			sOut += ' ';
			sOut += sLine;
		}

		sOut += '\n';

		if (iEnd == KStringView::npos)
		{
			break;
		}

		// after a line break follows a line, also if it is empty
		auto iNext = iEnd + 1;

		if (sValue[iEnd] == '\r' && iNext < sValue.size() && sValue[iNext] == '\n')
		{
			++iNext;
		}

		sValue.remove_prefix(iNext);
	}

} // AddLines

} // end of anonymous namespace

//-----------------------------------------------------------------------------
KServerSentEvent::KServerSentEvent(KString sData, KString sEvent, KString sID)
//-----------------------------------------------------------------------------
: sData  (std::move(sData ))
, sEvent (std::move(sEvent))
, sID    (std::move(sID   ))
{
}

//-----------------------------------------------------------------------------
void KServerSentEvent::Serialize(KStringRef& sOut) const
//-----------------------------------------------------------------------------
{
	// a line break in a value would start a new field, and clients drop an id with a NUL
	bool bEvent = !sEvent.empty() && sEvent.find_first_of(detail::kLineBreaksSet) == KString::npos;
	bool bID    = !sID.empty()    && sID.find_first_of(detail::kLineBreaksSet)    == KString::npos && !sID.contains('\0');
	// clients drop an event without data
	bool bData  = !sData.empty() || bEvent;

	if (!bEvent && !bID && !bData)
	{
		return;
	}

	if (bEvent)
	{
		AddLines(sOut, "event", sEvent);
	}

	if (bID)
	{
		AddLines(sOut, "id", sID);
	}

	if (bData)
	{
		AddLines(sOut, "data", sData);
	}

	// the empty line ends the event
	sOut += '\n';

} // Serialize

//-----------------------------------------------------------------------------
KString KServerSentEvent::Serialize() const
//-----------------------------------------------------------------------------
{
	KString sOut;

	Serialize(sOut);

	return sOut;

} // Serialize

//-----------------------------------------------------------------------------
void KServerSentEvent::clear()
//-----------------------------------------------------------------------------
{
	sData.clear();
	sEvent.clear();
	sID.clear();

} // clear

//-----------------------------------------------------------------------------
KString KServerSentEvent::Comment(KStringView sComment)
//-----------------------------------------------------------------------------
{
	KString sOut;

	AddLines(sOut, KStringView{}, sComment);

	return sOut;

} // Comment

//-----------------------------------------------------------------------------
KString KServerSentEvent::Retry(KDuration Wait)
//-----------------------------------------------------------------------------
{
	KString sOut;

	if (Wait >= KDuration::zero())
	{
		sOut  = "retry: ";
		sOut += KString::to_string(Wait.milliseconds().count());
		sOut += '\n';
	}

	return sOut;

} // Retry

//-----------------------------------------------------------------------------
void KServerSentEventParser::Add(KStringView sInput)
//-----------------------------------------------------------------------------
{
	if (HasError())
	{
		// a failed stream takes no more input
		return;
	}

	if (m_iPos > 0)
	{
		// remove the input that is parsed
		m_sInput.erase(0, m_iPos);
		m_iScanned -= std::min(m_iScanned, m_iPos);
		m_iPos      = 0;
	}

	m_sInput += sInput;

} // Add

//-----------------------------------------------------------------------------
bool KServerSentEventParser::NextLine(KStringView& sLine)
//-----------------------------------------------------------------------------
{
	if (HasError())
	{
		return false;
	}

	KStringView sInput(m_sInput);
	sInput.remove_prefix(m_iPos);

	if (m_bSkipLF && !sInput.empty())
	{
		// the LF of a CRLF that the input had split
		if (sInput.front() == '\n')
		{
			sInput.remove_prefix(1);
			++m_iPos;
		}

		m_bSkipLF = false;
	}

	if (m_bAtStart)
	{
		if (sInput.size() < sByteOrderMark.size() && sByteOrderMark.starts_with(sInput))
		{
			// this may become a byte order mark
			return false;
		}

		if (sInput.remove_prefix(sByteOrderMark))
		{
			m_iPos += sByteOrderMark.size();
		}

		m_bAtStart = false;
	}

	// what was searched in an earlier call is not searched again
	auto iEnd = sInput.find_first_of(detail::kLineBreaksSet, std::max(m_iScanned, m_iPos) - m_iPos);

	// the length of the line, or of what is there of a line that is not complete
	if (std::min(iEnd, sInput.size()) > m_iMaxLineLength)
	{
		// give the memory back before the error is set, which may throw
		KString().swap(m_sInput);
		m_iPos     = 0;
		m_iScanned = 0;
		m_sData.clear();
		m_sEvent.clear();

		SetError(kFormat("line of the event stream is longer than {} bytes", m_iMaxLineLength));

		return false;
	}

	if (iEnd == KStringView::npos)
	{
		// the line is not complete
		m_iScanned = m_sInput.size();
		return false;
	}

	sLine = sInput.substr(0, iEnd);

	auto iNext = iEnd + 1;

	if (sInput[iEnd] == '\r')
	{
		if (iNext == sInput.size())
		{
			// the LF of a CRLF may follow with the next input
			m_bSkipLF = true;
		}
		else if (sInput[iNext] == '\n')
		{
			++iNext;
		}
	}

	m_iPos    += iNext;
	m_iScanned = m_iPos;

	return true;

} // NextLine

//-----------------------------------------------------------------------------
void KServerSentEventParser::AddField(KStringView sLine)
//-----------------------------------------------------------------------------
{
	auto iColon = sLine.find(':');
	auto sField = sLine.substr(0, iColon);

	KStringView sValue;

	if (iColon != KStringView::npos)
	{
		sValue = sLine.substr(iColon + 1);
		// one space after the colon is not part of the value
		sValue.remove_prefix(" ");
	}

	if (sField == "data")
	{
		m_sData += sValue;
		m_sData += '\n';
	}
	else if (sField == "event")
	{
		m_sEvent = sValue;
	}
	else if (sField == "id")
	{
		if (!sValue.contains('\0'))
		{
			m_sID = sValue;
		}
	}
	else if (sField == "retry")
	{
		// only a value of ASCII digits counts, and one that a KDuration can hold
		std::int64_t iRetry { 0 };

		if (kFromDigits(iRetry, sValue) && iRetry <= KDuration::max().milliseconds().count())
		{
			m_Retry     = chrono::milliseconds(iRetry);
			m_bHasRetry = true;
		}
	}

	// all other fields are ignored

} // AddField

//-----------------------------------------------------------------------------
bool KServerSentEventParser::Next(KServerSentEvent& Event)
//-----------------------------------------------------------------------------
{
	KStringView sLine;

	while (NextLine(sLine))
	{
		if (sLine.empty())
		{
			// the empty line ends an event
			m_sLastEventID = m_sID;

			if (m_sData.empty())
			{
				// without data this is no event
				m_sEvent.clear();
				continue;
			}

			// the last line of the data has no line break
			m_sData.remove_suffix(1);

			Event.sData  = std::move(m_sData);
			Event.sEvent = std::move(m_sEvent);
			Event.sID    = m_sLastEventID;

			m_sData.clear();
			m_sEvent.clear();

			return true;
		}

		if (sLine.front() == ':')
		{
			// a comment
			continue;
		}

		AddField(sLine);
	}

	return false;

} // Next

//-----------------------------------------------------------------------------
void KServerSentEventParser::clear()
//-----------------------------------------------------------------------------
{
	ClearError();
	m_sInput.clear();
	m_iPos      = 0;
	m_iScanned  = 0;
	m_sData.clear();
	m_sEvent.clear();
	m_sID.clear();
	m_sLastEventID.clear();
	m_Retry     = KDuration::zero();
	m_bHasRetry = false;
	m_bAtStart  = true;
	m_bSkipLF   = false;

} // clear

#ifdef DEKAF2_REPEAT_CONSTEXPR_VARIABLE
constexpr std::size_t KServerSentEventParser::DefaultMaxLineLength;
#endif

DEKAF2_NAMESPACE_END
