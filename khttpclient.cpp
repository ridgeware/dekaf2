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

#include "khttpclient.h"
#include "kmime.h"
#include "kurlencode.h"

namespace dekaf2 {

//-----------------------------------------------------------------------------
	KHTTPClient::KHTTPClient(const KURL& url, KMethod method, bool bVerifyCerts)
//-----------------------------------------------------------------------------
{
	if (Connect(url, bVerifyCerts))
	{
		Resource(url, method);
	}

} // Ctor

//-----------------------------------------------------------------------------
KHTTPClient::KHTTPClient(KStringView sUrl, KMethod method, bool bVerifyCerts)
//-----------------------------------------------------------------------------
	: KHTTPClient(KURL(sUrl), method, bVerifyCerts)
{
} // Ctor

//-----------------------------------------------------------------------------
KHTTPClient::KHTTPClient(std::unique_ptr<KConnection> stream, const KURL& url, KMethod method)
//-----------------------------------------------------------------------------
{
	if (Connect(std::move(stream)))
	{
		Resource(url, method);
	}

} // Ctor

//-----------------------------------------------------------------------------
bool KHTTPClient::Connect(std::unique_ptr<KConnection> Connection)
//-----------------------------------------------------------------------------
{
	m_Stream = std::move(Connection);

	if (m_Stream)
	{
		KStream& Stream = m_Stream->Stream();
		m_State = Stream.OutStream().good() ? State::CONNECTED : State::CLOSED;
		Stream.SetReaderEndOfLine('\n');
		Stream.SetReaderLeftTrim("");
		Stream.SetReaderRightTrim("");
		Stream.SetWriterEndOfLine("\r\n");
	}
	else
	{
		m_State = State::CLOSED;
	}

	return m_State == State::CONNECTED;

} // Connect

//-----------------------------------------------------------------------------
bool KHTTPClient::Connect(const KURL& url, bool bVerifyCerts)
//-----------------------------------------------------------------------------
{
	return Connect(KConnection::Create(url, bVerifyCerts));

} // Connect

//-----------------------------------------------------------------------------
bool KHTTPClient::Connect(KStringView sUrl, bool bVerifyCerts)
//-----------------------------------------------------------------------------
{
	return Connect(KURL(sUrl), bVerifyCerts);
}

//-----------------------------------------------------------------------------
bool KHTTPClient::Disconnect()
//-----------------------------------------------------------------------------
{
	m_State = State::CLOSED;

	if (!m_Stream)
	{
		return false;
	}

	m_Stream.reset();

	return true;

} // Disconnect

//-----------------------------------------------------------------------------
void KHTTPClient::SetTimeout(long iSeconds)
//-----------------------------------------------------------------------------
{
	m_Timeout = iSeconds;
	m_Stream->SetTimeout(iSeconds);
}

//-----------------------------------------------------------------------------
KHTTPClient& KHTTPClient::Resource(const KURL& url, KMethod method)
//-----------------------------------------------------------------------------
{
	m_Method = method;

	if (!url.empty())
	{
		if ((m_State == State::CONNECTED || m_State == State::REQUEST_SENT) && m_Stream)
		{
			m_Stream->ExpiresFromNow(m_Timeout);
			KStream& Stream = m_Stream->Stream();
			Stream.Write(method);
			Stream.Write(' ');
			if (!url.Path.empty())
			{
				url.Path.Serialize(Stream);
			}
			else
			{
				Stream.Write('/');
			}
			url.Query.Serialize(Stream);
			url.Fragment.Serialize(Stream);
			Stream.WriteLine(" HTTP/1.1");
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
KHTTPClient& KHTTPClient::RequestHeader(KStringView svName, KStringView svValue)
//-----------------------------------------------------------------------------
{
	if ((m_State == State::RESOURCE_SET || m_State == State::HEADER_SET) && m_Stream)
	{
		m_Stream->ExpiresFromNow(m_Timeout);
		KStream& Stream = m_Stream->Stream();
		Stream.Write(svName);
		Stream.Write(": ");
		Stream.WriteLine(svValue);
		m_State = State::HEADER_SET;
	}
	else
	{
		kWarning("Bad state - cannot set header '{} : {}'", svName, svValue);
	}
	return *this;
}

//-----------------------------------------------------------------------------
bool KHTTPClient::Request(KStringView svPostData, KStringView svMime)
//-----------------------------------------------------------------------------
{
	if (m_State == State::HEADER_SET && m_Stream)
	{
		m_Stream->ExpiresFromNow(m_Timeout);
		KStream& Stream = m_Stream->Stream();
		if (m_Method == KMethod::POST)
		{
			RequestHeader(KHeader::CONTENT_LENGTH, KString::to_string(svPostData.size()));
			RequestHeader(KHeader::CONTENT_TYPE,   svMime.empty() ? KMIME::TEXT_PLAIN : svMime);
		}
		Stream.WriteLine();
		if (m_Method == KMethod::POST)
		{
			Stream.Write(svPostData);
		}
		Stream.Flush();
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
bool KHTTPClient::ReadHeader()
//-----------------------------------------------------------------------------
{
	if (m_State == State::REQUEST_SENT && m_Stream)
	{
		m_Stream->ExpiresFromNow(m_Timeout);
		KStream& Stream = m_Stream->Stream();
		KString sLine;
		while (Stream.ReadLine(sLine))
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
				m_iRemainingContentSize = sv.UInt64();
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
inline bool KHTTPClient::GetNextChunkSize()
//-----------------------------------------------------------------------------
{
	m_Stream->ExpiresFromNow(m_Timeout);
	if (m_bTEChunked && m_iRemainingContentSize == 0)
	{
		KStream& Stream = m_Stream->Stream();
		KString sLine;
		bool bGood = Stream.ReadLine(sLine);
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
inline void KHTTPClient::CheckForChunkEnd()
//-----------------------------------------------------------------------------
{
	if (m_bTEChunked && m_iRemainingContentSize == 0)
	{
		m_Stream->ExpiresFromNow(m_Timeout);
		KStream& Stream = m_Stream->Stream();
		if (Stream.Read() == 13)
		{
			Stream.Read();
		}
	}
}

//-----------------------------------------------------------------------------
/// Stream into outstream
size_t KHTTPClient::Read(KOutStream& stream, size_t len)
//-----------------------------------------------------------------------------
{
	if (m_State == State::HEADER_PARSED && m_Stream)
	{
		KStream& Stream = m_Stream->Stream();
		size_t tlen{0};
		while (len)
		{
			// we touch the expiry timer in GetNextChunkSize() already
			if (GetNextChunkSize())
			{
				auto rxlen = std::min(len, size());
				rxlen = Stream.Read(stream, rxlen);
				if (!rxlen)
				{
					break;
				}
				m_iRemainingContentSize -= rxlen;
				len  -= rxlen;
				tlen += rxlen;
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
size_t KHTTPClient::Read(KString& sBuffer, size_t len)
//-----------------------------------------------------------------------------
{
	if (m_State == State::HEADER_PARSED && m_Stream)
	{
		KStream& Stream = m_Stream->Stream();
		size_t tlen{0};
		while (len)
		{
			// we touch the expiry timer in GetNextChunkSize() already
			if (GetNextChunkSize())
			{
				auto rxlen = std::min(len, size());
				rxlen = Stream.Read(sBuffer, rxlen);
				if (!rxlen)
				{
					break;
				}
				m_iRemainingContentSize -= rxlen;
				len -= rxlen;
				tlen += rxlen;
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
bool KHTTPClient::ReadLine(KString& sBuffer)
//-----------------------------------------------------------------------------
{
	if (m_State == State::HEADER_PARSED && m_Stream)
	{
		bool bGood = false;
		// we touch the expiry timer in GetNextChunkSize() already
		GetNextChunkSize();
		if (m_iRemainingContentSize)
		{
			KStream& Stream = m_Stream->Stream();
			// TODO this will fail with chunked transfer if the newline is in a new transfer block..
			bGood = Stream.ReadLine(sBuffer);
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
