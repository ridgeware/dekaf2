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
	KHTTPClient::KHTTPClient(const KURL& url, KHTTPMethod method, bool bVerifyCerts)
//-----------------------------------------------------------------------------
{
	if (Connect(url, bVerifyCerts))
	{
		Resource(url, method);
	}

} // Ctor

//-----------------------------------------------------------------------------
KHTTPClient::KHTTPClient(KStringView sUrl, KHTTPMethod method, bool bVerifyCerts)
//-----------------------------------------------------------------------------
	: KHTTPClient(KURL(sUrl), method, bVerifyCerts)
{
} // Ctor

//-----------------------------------------------------------------------------
KHTTPClient::KHTTPClient(std::unique_ptr<KConnection> stream, const KURL& url, KHTTPMethod method)
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
	SetError(KStringView{});

	m_State = State::CLOSED;

	m_Connection = std::move(Connection);

	if (!m_Connection)
	{
		return SetError("No Stream");
	}

	KStream& Stream = m_Connection->Stream();

	if (!Stream.KOutStream::Good())
	{
		return SetError(m_Connection->Error());
	}

	Stream.SetReaderEndOfLine('\n');
	Stream.SetReaderLeftTrim("");
	Stream.SetReaderRightTrim("\r\n");
	Stream.SetWriterEndOfLine("\r\n");

	m_State = State::CONNECTED;

	return true;

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

	if (!m_Connection)
	{
		return false;
	}

	m_Connection.reset();

	return true;

} // Disconnect

//-----------------------------------------------------------------------------
void KHTTPClient::SetTimeout(long iSeconds)
//-----------------------------------------------------------------------------
{
	m_Timeout = iSeconds;
	m_Connection->SetTimeout(iSeconds);
}

//-----------------------------------------------------------------------------
bool KHTTPClient::Resource(const KURL& url, KHTTPMethod method)
//-----------------------------------------------------------------------------
{
	m_Method = method;

	if (url.empty())
	{
		return SetError("URL is empty");
	}
	else if ((m_State != State::CONNECTED && m_State != State::REQUEST_SENT))
	{
		return SetError(kFormat("Bad state - cannot set resource {}", url.Path.Serialize()));
	}
	else if (!m_Connection)
	{
		return SetError("no stream");
	}
	else if (!m_Connection->Stream().KOutStream::Good())
	{
		return SetError(m_Connection->Error());
	}

	m_Connection->ExpiresFromNow(m_Timeout);

	KStream& Stream = m_Connection->Stream();

	Stream.Write(method.Serialize());
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

	return RequestHeader(KHTTPHeader::HOST, url.Domain.Serialize());

} // Resource

//-----------------------------------------------------------------------------
bool KHTTPClient::RequestHeader(KStringView svName, KStringView svValue)
//-----------------------------------------------------------------------------
{
	if ((m_State != State::RESOURCE_SET && m_State != State::HEADER_SET))
	{
		return SetError(kFormat("Bad state - cannot set header '{} : {}'", svName, svValue));
	}
	else if (!m_Connection)
	{
		return SetError("no stream");
	}
	else if (!m_Connection->Stream().KOutStream::Good())
	{
		return SetError(m_Connection->Error());
	}

	m_Connection->ExpiresFromNow(m_Timeout);

	KStream& Stream = m_Connection->Stream();
	Stream.Write(svName);
	Stream.Write(": ");
	Stream.WriteLine(svValue);
	m_State = State::HEADER_SET;

	return true;
}

//-----------------------------------------------------------------------------
bool KHTTPClient::Request(KStringView svPostData, KStringView svMime)
//-----------------------------------------------------------------------------
{
	if (m_State != State::HEADER_SET)
	{
		SetError(kFormat("Bad state - cannot send request"));
	}
	else if (!m_Connection)
	{
		return SetError("no stream");
	}
	else if (!m_Connection->Stream().KOutStream::Good())
	{
		return SetError(m_Connection->Error());
	}

	m_Connection->ExpiresFromNow(m_Timeout);

	KStream& Stream = m_Connection->Stream();

	if (m_Method == KHTTPMethod::POST)
	{
		RequestHeader(KHTTPHeader::CONTENT_LENGTH, KString::to_string(svPostData.size()));
		RequestHeader(KHTTPHeader::CONTENT_TYPE,   svMime.empty() ? KMIME::TEXT_PLAIN : svMime);
	}

	Stream.WriteLine();

	if (m_Method == KHTTPMethod::POST)
	{
		Stream.Write(svPostData);
	}

	Stream.Flush();
	m_State = State::REQUEST_SENT;

	return ReadHeader();
}

//-----------------------------------------------------------------------------
bool KHTTPClient::ReadHeader()
//-----------------------------------------------------------------------------
{
	if (m_State != State::REQUEST_SENT)
	{
		SetError("Bad state - cannot read headers");
	}
	else if (!m_Connection)
	{
		return SetError("no stream");
	}
	else if (!m_Connection->Stream().KOutStream::Good())
	{
		return SetError(m_Connection->Error());
	}

	m_Connection->ExpiresFromNow(m_Timeout);

	KStream& Stream = m_Connection->Stream();

	if (!m_ResponseHeader.Parse(Stream))
	{
		SetError(m_ResponseHeader.Error());
		return false;
	}

	m_bTEChunked = false;

	// find the content length
	KStringView sv(m_ResponseHeader.Get(KHTTPHeader::content_length));

	if (sv.empty())
	{
		KStringView svTE = m_ResponseHeader.Get(KHTTPHeader::transfer_encoding);
		if (svTE == "chunked")
		{
			m_bTEChunked = true;
			m_bReceivedFinalChunk = false;
		}
	}

	m_iRemainingContentSize = sv.UInt64();

	m_State = State::HEADER_PARSED;

	return true;

} // ReadHeader

//-----------------------------------------------------------------------------
inline bool KHTTPClient::GetNextChunkSize()
//-----------------------------------------------------------------------------
{
	m_Connection->ExpiresFromNow(m_Timeout);
	
	if (!m_bTEChunked)
	{
		return true;
	}
	else if (m_iRemainingContentSize != 0)
	{
		return true;
	}
	else if (m_bReceivedFinalChunk)
	{
		return false;
	}

	KStream& Stream = m_Connection->Stream();

	KString sLine;

	if (!Stream.ReadLine(sLine))
	{
		return SetError(m_Connection->Error());
	}

	try
	{
		kTrim(sLine);

		if (sLine.empty())
		{
			return false;
		}

		auto len = std::strtoul(sLine.c_str(), nullptr, 16);

		m_iRemainingContentSize = len;

		if (!m_iRemainingContentSize)
		{
			m_bReceivedFinalChunk = true;
			return false;
		}

		return true;
	}
	catch (const std::exception& e)
	{
		return SetError(e.what());
	}

	return false;

} // GetNextChunkSize

//-----------------------------------------------------------------------------
inline void KHTTPClient::CheckForChunkEnd()
//-----------------------------------------------------------------------------
{
	if (m_bTEChunked && m_iRemainingContentSize == 0)
	{
		m_Connection->ExpiresFromNow(m_Timeout);
		KStream& Stream = m_Connection->Stream();
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
	if (m_State != State::HEADER_PARSED)
	{
		SetError("Bad state - cannot read data");
		return 0;
	}
	else if (!m_Connection)
	{
		SetError("no stream");
		return 0;
	}
	else if (!m_Connection->Stream().KInStream::Good())
	{
		SetError(m_Connection->Error());
		return 0;
	}

	KStream& Stream = m_Connection->Stream();
	size_t tlen{0};

	// if this is a chunked transfer we loop until we read len bytes
	while (len && GetNextChunkSize())
	{
		// we touch the expiry timer in GetNextChunkSize() already
		auto wanted = std::min(len, size());

		if (wanted == 0)
		{
			break;
		}

		auto received = Stream.Read(stream, wanted);

		m_iRemainingContentSize -= received;
		len -= received;
		tlen += received;

		if (received < wanted)
		{
			SetError(m_Connection->GetStreamError());
			break;
		}

		if (!m_bTEChunked)
		{
			break;
		}

		CheckForChunkEnd();
	}

	return tlen;

} // Read

//-----------------------------------------------------------------------------
/// Append to sBuffer
size_t KHTTPClient::Read(KString& sBuffer, size_t len)
//-----------------------------------------------------------------------------
{
	if (m_State != State::HEADER_PARSED)
	{
		SetError("Bad state - cannot read data");
		return 0;
	}
	else if (!m_Connection)
	{
		SetError("no stream");
		return 0;
	}
	else if (!m_Connection->Stream().KInStream::Good())
	{
		SetError(m_Connection->Error());
		return 0;
	}

	KStream& Stream = m_Connection->Stream();
	size_t tlen{0};

	// if this is a chunked transfer we loop until we read len bytes
	while (len && GetNextChunkSize())
	{
		// we touch the expiry timer in GetNextChunkSize() already
		auto wanted = std::min(len, size());

		if (wanted == 0)
		{
			break;
		}

		auto received = Stream.Read(sBuffer, wanted);

		m_iRemainingContentSize -= received;
		len -= received;
		tlen += received;

		if (received < wanted)
		{
			SetError(m_Connection->GetStreamError());
			break;
		}

		if (!m_bTEChunked)
		{
			break;
		}

		CheckForChunkEnd();
	}

	return tlen;

} // Read

//-----------------------------------------------------------------------------
/// Read one line into sBuffer, including EOL
bool KHTTPClient::ReadLine(KString& sBuffer)
//-----------------------------------------------------------------------------
{
	sBuffer.clear();

	if (m_State != State::HEADER_PARSED)
	{
		return SetError("Bad state - cannot read data");
	}
	else if (!m_Connection)
	{
		return SetError("no stream");
	}
	else if (!m_Connection->Stream().KInStream::Good())
	{
		return SetError(m_Connection->Error());
	}

	// we touch the expiry timer in GetNextChunkSize() already
	GetNextChunkSize();

	if (!m_iRemainingContentSize)
	{
		return false;
	}

	KStream& Stream = m_Connection->Stream();

	// TODO this will fail with chunked transfer if the newline is in a new transfer block..
	if (!Stream.ReadLine(sBuffer))
	{
		SetError(m_Connection->GetStreamError());
		m_iRemainingContentSize = 0;
		return false;
	}

	auto len = sBuffer.size();
	m_iRemainingContentSize -= len;

	return true;

} // ReadLine

} // end of namespace dekaf2
