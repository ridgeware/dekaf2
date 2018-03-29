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
KHTTPClient::KHTTPClient(KConnection&& stream, const KURL& url, KHTTPMethod method)
//-----------------------------------------------------------------------------
{
	if (Connect(std::move(stream)))
	{
		Resource(url, method);
	}

} // Ctor

//-----------------------------------------------------------------------------
bool KHTTPClient::Connect(KConnection&& Connection)
//-----------------------------------------------------------------------------
{
	SetError(KStringView{});

	m_Connection = std::move(Connection);

	if (!m_Connection)
	{
		return SetError(m_Connection.Error());
	}

	m_Connection.SetTimeout(m_Timeout);

	m_Connection->SetReaderEndOfLine('\n');
	m_Connection->SetReaderLeftTrim("");
	m_Connection->SetReaderRightTrim("\r\n");
	m_Connection->SetWriterEndOfLine("\r\n");

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
	m_Connection.Disconnect();

	return true;

} // Disconnect

//-----------------------------------------------------------------------------
void KHTTPClient::SetTimeout(long iSeconds)
//-----------------------------------------------------------------------------
{
	m_Timeout = iSeconds;
	m_Connection.SetTimeout(iSeconds);

} // SetTimeout

//-----------------------------------------------------------------------------
bool KHTTPClient::Resource(const KURL& url, KHTTPMethod method)
//-----------------------------------------------------------------------------
{
	if (url.empty())
	{
		return SetError("URL is empty");
	};

	m_Request.Resource() = url;
	m_Request.Method() = method;
	m_Request.HTTPVersion() = "HTTP/1.1";

	return RequestHeader(KHTTPHeader::HOST, url.Domain.Serialize());

} // Resource

//-----------------------------------------------------------------------------
bool KHTTPClient::RequestHeader(KStringView svName, KStringView svValue)
//-----------------------------------------------------------------------------
{
	m_Request->Add(svName, svValue);

	return true;

} // RequestHeader

//-----------------------------------------------------------------------------
bool KHTTPClient::Request(KStringView svPostData, KStringView svMime)
//-----------------------------------------------------------------------------
{
	if (m_Request.Resource().empty())
	{
		return SetError("no resource");
	}
	else if (!m_Connection)
	{
		return SetError("no stream");
	}

	if (m_Request.Method() == KHTTPMethod::POST)
	{
		RequestHeader(KHTTPHeader::CONTENT_LENGTH, KString::to_string(svPostData.size()));
		RequestHeader(KHTTPHeader::CONTENT_TYPE,   svMime.empty() ? KMIME::TEXT_PLAIN : svMime);
	}

	if (m_bRequestCompression)
	{
		RequestHeader(KHTTPHeader::ACCEPT_ENCODING, "gzip");
	}

	if (!m_Request.Serialize(m_Connection.Stream()))
	{
		return SetError(m_Request.Error());
	}

	if (m_Request.Method() == KHTTPMethod::POST)
	{
		kDebug(2, "sending {} bytes of POST data", svPostData.size());
		m_Request.Write(m_Connection.Stream(), svPostData);
	}

	m_Connection->Flush();

	if (!m_Connection.Good())
	{
		return SetError("write error");
	}

	return ReadHeader();
}

//-----------------------------------------------------------------------------
bool KHTTPClient::ReadHeader()
//-----------------------------------------------------------------------------
{
	if (!m_Response.Parse(*m_Connection))
	{
		SetError(m_Response.Error());
		return false;
	}

	return true;

} // ReadHeader

//-----------------------------------------------------------------------------
/// Stream into outstream
size_t KHTTPClient::Read(KOutStream& stream, size_t len)
//-----------------------------------------------------------------------------
{
	m_Response.Read(m_Connection.Stream(), stream, len);

	return len;

} // Read

//-----------------------------------------------------------------------------
/// Append to sBuffer
size_t KHTTPClient::Read(KString& sBuffer, size_t len)
//-----------------------------------------------------------------------------
{
	m_Response.Read(m_Connection.Stream(), sBuffer, len);

	return sBuffer.size();

} // Read

//-----------------------------------------------------------------------------
/// Read one line into sBuffer, including EOL
bool KHTTPClient::ReadLine(KString& sBuffer)
//-----------------------------------------------------------------------------
{
	sBuffer.clear();

	if (!m_Response.ReadLine(m_Connection.Stream(), sBuffer))
	{
		SetError(m_Connection.Error());
		return false;
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

	return URL == m_Connection.EndPoint();

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
