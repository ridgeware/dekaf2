/*
//-----------------------------------------------------------------------------//
//
// DEKAF(tm): Lighter, Faster, Smarter (tm)
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
*/

#include "khttp.h"
#include "kurlencode.h"

namespace dekaf2 {

namespace detail {
namespace http {

constexpr KStringView KMIME::JSON_UTF8;
constexpr KStringView KMIME::HTML_UTF8;
constexpr KStringView KMIME::XML_UTF8;
constexpr KStringView KMIME::SWF;
constexpr KStringView KMIME::WWW_FORM_URLENCODED;
constexpr KStringView KMIME::MULTIPART_FORM_DATA;
constexpr KStringView KMIME::TEXT_PLAIN;
constexpr KStringView KMIME::APPLICATION_BINARY;

}
}

//-----------------------------------------------------------------------------
KHTTP::KHTTP(KConnection& stream, const KURL& url, KMethod method)
//-----------------------------------------------------------------------------
    : m_Stream(stream)
    // we do not know how to properly open the KStream object if it is not
    // open. Therefore we mark it as CLOSED should it not be good()
    , m_State((m_Stream && m_Stream->OutStream().good()) ? State::CONNECTED : State::CLOSED)
{
	if (m_Stream)
	{
		m_Stream->SetReaderEndOfLine('\n');
		m_Stream->SetReaderLeftTrim("");
		m_Stream->SetReaderRightTrim("");
		m_Stream->SetWriterEndOfLine("\r\n");
		Resource(url, method);
	}
	else
	{
		kWarning("constructing on an empty KConnection object");
	}
}

//-----------------------------------------------------------------------------
KHTTP& KHTTP::Resource(const KURL& url, KMethod method)
//-----------------------------------------------------------------------------
{
	m_Method = method;

	if (!url.empty())
	{
		if ((m_State == State::CONNECTED || m_State == State::REQUEST_SENT) && m_Stream)
		{
			m_Stream->Write(method);
			m_Stream->Write(' ');
			if (!url.Path.empty())
			{
				url.Path.Serialize(*m_Stream);
			}
			else
			{
				m_Stream->Write('/');
			}
			url.Query.Serialize(*m_Stream);
			url.Fragment.Serialize(*m_Stream);
			m_Stream->WriteLine(" HTTP/1.1");
			m_State = State::RESOURCE_SET;
			RequestHeader(KHeader::HOST, url.Domain.Serialize());
		}
		else
		{
			kWarning("Bad state - cannot set resource {}", url.Path.Serialize());
		}
	}
	return *this;
}

//-----------------------------------------------------------------------------
KHTTP& KHTTP::RequestHeader(KStringView svName, KStringView svValue)
//-----------------------------------------------------------------------------
{
	if ((m_State == State::RESOURCE_SET || m_State == State::HEADER_SET) && m_Stream)
	{
		m_Stream->Write(svName);
		m_Stream->Write(": ");
		m_Stream->WriteLine(svValue);
		m_State = State::HEADER_SET;
	}
	else
	{
		kWarning("Bad state - cannot set header '{} : {}'", svName, svValue);
	}
	return *this;
}

//-----------------------------------------------------------------------------
bool KHTTP::Request(KStringView svPostData, KStringView svMime)
//-----------------------------------------------------------------------------
{
	if (m_State == State::HEADER_SET && m_Stream)
	{
		if (m_Method == KMethod::POST)
		{
			RequestHeader(KHeader::CONTENT_LENGTH, std::to_string(svPostData.size()));
			RequestHeader(KHeader::CONTENT_TYPE,   svMime.empty() ? KMIME::TEXT_PLAIN : svMime);
		}
		m_Stream->WriteLine();
		if (m_Method == KMethod::POST)
		{
			m_Stream->Write(svPostData);
		}
		m_Stream->Flush();
		m_State = State::REQUEST_SENT;
		return ReadHeader();
	}
	else
	{
		kWarning("Bad state - cannot send request");
	}
	return false;
}

//-----------------------------------------------------------------------------
bool KHTTP::ReadHeader()
//-----------------------------------------------------------------------------
{
	if (m_State == State::REQUEST_SENT && m_Stream)
	{
		KString sLine;
		while (m_Stream->ReadLine(sLine))
		{
			m_ResponseHeader.Parse(sLine);
			if (m_ResponseHeader.HeaderComplete())
			{
				m_bTEChunked = false;
				// find the content length
				KStringView sv(m_ResponseHeader.Get(KHeader::content_length));
				if (sv.empty())
				{
					KStringView svTE = m_ResponseHeader.Get(KHeader::transfer_encoding);
					if (svTE == "chunked")
					{
						m_bTEChunked = true;
					}
				}
				KString s(sv); // TODO create conversions for KStringView
				m_iRemainingContentSize = kToULong(s);
				m_State = State::HEADER_PARSED;
				return true;
			}
		}
	}
	else
	{
		kWarning("Bad state - cannot read headers");
	}
	return false;
}

//-----------------------------------------------------------------------------
inline bool KHTTP::GetNextChunkSize()
//-----------------------------------------------------------------------------
{
	if (m_bTEChunked && m_iRemainingContentSize == 0)
	{
		KString sLine;
		bool bGood = m_Stream->ReadLine(sLine);
		if (bGood)
		{
			kTrim(sLine);
			try {
				if (sLine.empty())
				{
					return false;
				}
				auto len = std::strtoul(sLine.c_str(), nullptr, 16);
				m_iRemainingContentSize = len;
				return true;
			} catch (const std::exception e) {
				kException(e);
				return false;
			}
		}
		return false;
	}
	return true;
}

//-----------------------------------------------------------------------------
inline void KHTTP::CheckForChunkEnd()
//-----------------------------------------------------------------------------
{
	if (m_bTEChunked && m_iRemainingContentSize == 0)
	{
		if (m_Stream->Read() == 13)
		{
			m_Stream->Read();
		}
	}
}

//-----------------------------------------------------------------------------
/// Stream into outstream
size_t KHTTP::Read(KOutStream& stream, size_t len)
//-----------------------------------------------------------------------------
{
	if (m_State == State::HEADER_PARSED && m_Stream)
	{
		size_t tlen{0};
		auto rlen{len};
		while (len && rlen)
		{
			if (GetNextChunkSize())
			{
				rlen = std::min(len, size());
				rlen = m_Stream->Read(stream, rlen);
				m_iRemainingContentSize -= rlen;
				len  -= rlen;
				tlen += rlen;
				CheckForChunkEnd();
			}
		}
		return tlen;
	}
	else
	{
		kWarning("Bad state - cannot read data");
		return 0;
	}
}

//-----------------------------------------------------------------------------
/// Append to sBuffer
size_t KHTTP::Read(KString& sBuffer, size_t len)
//-----------------------------------------------------------------------------
{
	if (m_State == State::HEADER_PARSED && m_Stream)
	{
		size_t tlen{0};
		auto rlen{len};
		while (len && rlen)
		{
			if (GetNextChunkSize())
			{
				rlen = std::min(len, size());
				len = m_Stream->Read(sBuffer, rlen);
				m_iRemainingContentSize -= rlen;
				len  -= rlen;
				tlen += rlen;
				CheckForChunkEnd();
			}
		}
		return tlen;
	}
	else
	{
		kWarning("Bad state - cannot read data");
		return 0;
	}
}

//-----------------------------------------------------------------------------
/// Read one line into sBuffer, including EOL
bool KHTTP::ReadLine(KString& sBuffer)
//-----------------------------------------------------------------------------
{
	if (m_State == State::HEADER_PARSED && m_Stream)
	{
		bool bGood = false;
		GetNextChunkSize();
		if (m_iRemainingContentSize)
		{
			// TODO this will fail with chunked transfer if the newline is in a new transfer block..
			bGood = m_Stream->ReadLine(sBuffer);
			if (bGood)
			{
				auto len = sBuffer.size();
				m_iRemainingContentSize -= len;
			}
			else
			{
				m_iRemainingContentSize = 0;
			}
		}
		return bGood;
	}
	else
	{
		kWarning("Bad state - cannot read data");
		sBuffer.clear();
		return false;
	}
}

} // end of namespace dekaf2
