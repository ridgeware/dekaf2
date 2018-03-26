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
#include "kchunkedtransfer.h"
#include "kstringstream.h"

#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filter/zlib.hpp>

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

	m_Connection->SetTimeout(m_Timeout);

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
	if (AlreadyConnected(url))
	{
		return true;
	}

	return Connect(KConnection::Create(url, false, bVerifyCerts, true));

} // Connect

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
	if (m_Connection)
	{
		m_Connection->SetTimeout(iSeconds);
	}
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
	else if ((m_State != State::CONNECTED && m_State != State::CONTENT_READ))
	{
		return SetError(kFormat("bad state - cannot set resource {}", url.Path.Serialize()));
	}
	else if (!m_Connection)
	{
		return SetError("no stream");
	}
	else if (!m_Connection->Stream().KOutStream::Good())
	{
		return SetError(m_Connection->Error());
	}

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
		return SetError(kFormat("bad state - cannot set header '{} : {}'", svName, svValue));
	}
	else if (!m_Connection)
	{
		return SetError("no stream");
	}
	else if (!m_Connection->Stream().KOutStream::Good())
	{
		return SetError(m_Connection->Error());
	}

	KStream& Stream = m_Connection->Stream();
	Stream.Write(svName);
	Stream.Write(": ");
	Stream.WriteLine(svValue);
	m_State = State::HEADER_SET;

	return Stream.KOutStream::Good();
}

//-----------------------------------------------------------------------------
bool KHTTPClient::Request(KStringView svPostData, KStringView svMime)
//-----------------------------------------------------------------------------
{
	if (m_State != State::HEADER_SET)
	{
		SetError(kFormat("bad state - cannot send request"));
	}
	else if (!m_Connection)
	{
		return SetError("no stream");
	}
	else if (!m_Connection->Stream().KOutStream::Good())
	{
		return SetError(m_Connection->Error());
	}

	KStream& Stream = m_Connection->Stream();

	if (m_Method == KHTTPMethod::POST)
	{
		RequestHeader(KHTTPHeader::CONTENT_LENGTH, KString::to_string(svPostData.size()));
		RequestHeader(KHTTPHeader::CONTENT_TYPE,   svMime.empty() ? KMIME::TEXT_PLAIN : svMime);
	}

	RequestHeader(KHTTPHeader::ACCEPT_ENCODING, "gzip");

	Stream.WriteLine();

	if (m_Method == KHTTPMethod::POST)
	{
		kDebug(2, "sending {} bytes of POST data", svPostData.size());
		Stream.Write(svPostData);
	}

	if (!Stream.Flush().Good())
	{
		return SetError("write error");
	}

	m_State = State::REQUEST_SENT;

	return ReadHeader();
}

//-----------------------------------------------------------------------------
bool KHTTPClient::ReadHeader()
//-----------------------------------------------------------------------------
{
	if (m_State != State::REQUEST_SENT)
	{
		SetError("bad state - cannot read headers");
	}
	else if (!m_Connection)
	{
		return SetError("no stream");
	}
	else if (!m_Connection->Stream().KOutStream::Good())
	{
		return SetError(m_Connection->Error());
	}

	KStream& Stream = m_Connection->Stream();

	if (!m_ResponseHeader.Parse(Stream))
	{
		SetError(m_ResponseHeader.Error());
		return false;
	}

	// find the content length
	KStringView sRemainingContentSize = m_ResponseHeader.Get(KHTTPHeader::content_length);
	size_t      iRemainingContentSize = sRemainingContentSize.UInt64();

	bool bTEChunked          = m_ResponseHeader.Get(KHTTPHeader::transfer_encoding) == "chunked";
	KStringView sCompression = m_ResponseHeader.Get(KHTTPHeader::content_encoding);

	// start setting up the input filter queue
	m_Filter = std::make_unique<boost::iostreams::filtering_istream>();

	if (sCompression == "gzip" || sCompression == "x-gzip")
	{
		kDebug(2, "using {} decompression", sCompression);
		m_Filter->push(boost::iostreams::gzip_decompressor());
	}
	else if (sCompression == "deflate")
	{
		kDebug(2, "using zlib / {} decompression", sCompression);
		m_Filter->push(boost::iostreams::zlib_decompressor());
	}

	kDebug(2, "content transfer: {}", bTEChunked ? "chunked" : "plain");
	kDebug(2, "content length:   {}", iRemainingContentSize);

	// we use the chunked reader also in the unchunked case -
	// it protects us from reading more than content length bytes
	// into the buffered iostreams
	KChunkedSource Source(Stream, bTEChunked, sRemainingContentSize.empty() ? -1 : iRemainingContentSize);

	// and finally add our source stream to the filtering_istream
	m_Filter->push(Source);

	m_State = State::HEADER_PARSED;

	return true;

} // ReadHeader


//-----------------------------------------------------------------------------
/// Stream into outstream
size_t KHTTPClient::Read(KOutStream& stream, size_t len)
//-----------------------------------------------------------------------------
{
	if (m_State != State::HEADER_PARSED)
	{
		SetError("bad state - cannot read data");
		return 0;
	}
	else if (!m_Filter)
	{
		SetError("no stream");
		return 0;
	}
	else if (!m_Filter->good())
	{
		SetError(m_Connection->Error());
		return 0;
	}

	if (len == KString::npos)
	{
		// read until eof
		// ignore len, copy full stream
		stream.OutStream() << m_Filter->rdbuf();
	}
	{
		KInStream istream(*m_Filter);
		stream.Write(istream, len);
	}

	if (m_Filter->eof())
	{
		m_State = State::CONTENT_READ;
	}

	return len;

} // Read

//-----------------------------------------------------------------------------
/// Append to sBuffer
size_t KHTTPClient::Read(KString& sBuffer, size_t len)
//-----------------------------------------------------------------------------
{
	if (m_State != State::HEADER_PARSED)
	{
		SetError("bad state - cannot read data");
		return 0;
	}
	else if (!m_Filter)
	{
		SetError("no stream");
		return 0;
	}
	else if (!m_Filter->good())
	{
		SetError(m_Connection->Error());
		return 0;
	}

	if (len == KString::npos)
	{
		// read until eof
		KOStringStream stream(sBuffer);
		stream << m_Filter->rdbuf();
	}
	else
	{
		KInStream istream(*m_Filter);
		istream.Read(sBuffer, len);
	}

	if (m_Filter->eof())
	{
		m_State = State::CONTENT_READ;
	}

	return sBuffer.size();

} // Read

//-----------------------------------------------------------------------------
/// Read one line into sBuffer, including EOL
bool KHTTPClient::ReadLine(KString& sBuffer)
//-----------------------------------------------------------------------------
{
	sBuffer.clear();

	if (m_State != State::HEADER_PARSED)
	{
		return SetError("bad state - cannot read data");
	}
	else if (!m_Filter)
	{
		SetError("no stream");
		return false;
	}
	else if (m_Filter->eof())
	{
		m_State = State::CONTENT_READ;
		return false;
	}
	else if (!m_Filter->good())
	{
		SetError(m_Connection->Error());
		return false;
	}

	KInStream istream(*m_Filter);
	if (!istream.ReadLine(sBuffer))
	{
		SetError(m_Connection->Error());
		return false;
	}

	if (m_Filter->eof())
	{
		m_State = State::CONTENT_READ;
	}

	return true;

} // ReadLine

//-----------------------------------------------------------------------------
KString KHTTPClient::Get(const KURL& URL)
//-----------------------------------------------------------------------------
{
	KString sBuffer;

	if (Connect(URL))
	{
		if (Resource(URL, KHTTPMethod::GET))
		{
			if (Request())
			{
				Read(sBuffer);
			}
		}
	}

	return sBuffer;

} // Get

//-----------------------------------------------------------------------------
KString KHTTPClient::Post(const KURL& URL, KStringView svPostData, KStringView svMime)
//-----------------------------------------------------------------------------
{
	KString sBuffer;

	if (Connect(URL))
	{
		if (Resource(URL, KHTTPMethod::POST))
		{
			if (Request(svPostData, svMime))
			{
				Read(sBuffer);
			}
		}
	}

	return sBuffer;

} // Post

//-----------------------------------------------------------------------------
bool KHTTPClient::AlreadyConnected(const KURL& URL) const
//-----------------------------------------------------------------------------
{
	if (!m_Connection)
	{
		return false;
	}

	return URL == m_Connection->EndPoint();

} // AlreadyConnected

//-----------------------------------------------------------------------------
bool KHTTPClient::SetError(KStringView sError) const
//-----------------------------------------------------------------------------
{
	kDebug(1, "{}", sError);
	m_sError = sError;
	return false;

} // SetError


//-----------------------------------------------------------------------------
KString kHTTPGet(const KURL& URL)
//-----------------------------------------------------------------------------
{
	KHTTPClient HTTP;
	return HTTP.Get(URL);

} // kHTTPGet

//-----------------------------------------------------------------------------
KString kHTTPPost(const KURL& URL, KStringView svPostData, KStringView svMime)
//-----------------------------------------------------------------------------
{
	KHTTPClient HTTP;
	return HTTP.Post(URL, svPostData, svMime);

} // kHTTPPost


} // end of namespace dekaf2
