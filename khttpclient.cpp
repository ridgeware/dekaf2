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

namespace dekaf2 {


//-----------------------------------------------------------------------------
void KHTTPClient::clear()
//-----------------------------------------------------------------------------
{
	Request.clear();
	Response.clear();
	m_sError.clear();
	m_bRequestCompression = true;
}

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
KHTTPClient::KHTTPClient(std::unique_ptr<KConnection> stream)
//-----------------------------------------------------------------------------
{
	Connect(std::move(stream));

} // Ctor

//-----------------------------------------------------------------------------
bool KHTTPClient::Connect(std::unique_ptr<KConnection> Connection)
//-----------------------------------------------------------------------------
{
	SetError(KStringView{});

	m_Connection = std::move(Connection);

	if (!m_Connection)
	{
		return SetError("KConnection is invalid");
	}

	if (!m_Connection->Good())
	{
		return SetError(m_Connection->Error());
	}

	m_Connection->SetTimeout(m_Timeout);

	(*m_Connection)->SetReaderEndOfLine('\n');
	(*m_Connection)->SetReaderLeftTrim("");
	(*m_Connection)->SetReaderRightTrim("\r\n");
	(*m_Connection)->SetWriterEndOfLine("\r\n");

	Response.SetInputStream(m_Connection->Stream());
	Request.SetOutputStream(m_Connection->Stream());

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

	return Connect(KConnection::Create(url, false, bVerifyCerts));

} // Connect

//-----------------------------------------------------------------------------
bool KHTTPClient::Disconnect()
//-----------------------------------------------------------------------------
{
	if (!m_Connection)
	{
		return SetError("no connection to disconnect");
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

} // SetTimeout

//-----------------------------------------------------------------------------
bool KHTTPClient::Resource(const KURL& url, KHTTPMethod method)
//-----------------------------------------------------------------------------
{
	if (url.empty())
	{
		return SetError("URL is empty");
	};

	Request.Resource = url;

	// we do not want to send any eventual protocol and domain parts of the URL in the request
	Request.Resource.Protocol.clear();
	Request.Resource.User.clear();
	Request.Resource.Password.clear();
	Request.Resource.Domain.clear();
	Request.Resource.Port.clear();

	Request.Method = method;
	Request.sHTTPVersion = "HTTP/1.1";

	// make sure we always have a valid path set
	if (Request.Resource.Path.empty())
	{
		Request.Resource.Path.set("/");
	}

	if (!url.Domain.empty())
	{
		// set the host header so that it overwrites a previously set one
		RequestHeader(KHTTPHeaders::HOST, url.Domain.Serialize(), true);
	}

	return true;

} // Resource

//-----------------------------------------------------------------------------
bool KHTTPClient::RequestHeader(KStringView svName, KStringView svValue, bool bOverwrite)
//-----------------------------------------------------------------------------
{
	if (bOverwrite)
	{
		Request.Headers.Set(svName, svValue);
	}
	else
	{
		Request.Headers.Add(svName, svValue);
	}

	return true;

} // RequestHeader

//-----------------------------------------------------------------------------
bool KHTTPClient::Parse()
//-----------------------------------------------------------------------------
{
	Request.close();

	if (!Response.Parse())
	{
		if (!Response.Error().empty())
		{
			SetError(Response.Error());
		}
		else
		{
			SetError(m_Connection->Error());
		}
		return false;
	}

	return true;

} // Parse

//-----------------------------------------------------------------------------
bool KHTTPClient::Serialize()
//-----------------------------------------------------------------------------
{
	if (!Request.Serialize())
	{
		SetError(Request.Error());
		return false;
	}

	return true;

} // Serialize

//-----------------------------------------------------------------------------
bool KHTTPClient::SendRequest(KStringView svPostData, KMIME Mime)
//-----------------------------------------------------------------------------
{
	Response.clear();

	if (Request.Resource.empty())
	{
		return SetError("no resource");
	}

	if (!m_Connection || !m_Connection->Good())
	{
		return SetError("no stream");
	}

	if (   Request.Method != KHTTPMethod::GET
		&& Request.Method != KHTTPMethod::HEAD
		&& Request.Method != KHTTPMethod::OPTIONS)
	{
		RequestHeader(KHTTPHeaders::CONTENT_LENGTH, KString::to_string(svPostData.size()));

		if (!svPostData.empty())
		{
			RequestHeader(KHTTPHeaders::CONTENT_TYPE, Mime);
		}
	}
	else
	{
		if (!svPostData.empty())
		{
			kDebug(1, "cannot send body data with {} request, data removed", Request.Method.Serialize())
			svPostData.clear();
		}
	}

	if (m_bRequestCompression)
	{
		RequestHeader(KHTTPHeaders::ACCEPT_ENCODING, "gzip");
	}

	if (!Request.Serialize())
	{
		return SetError(Request.Error());
	}

	if (!svPostData.empty())
	{
		kDebug(2, "sending {} bytes of {} data", svPostData.size(), Mime);
		Request.Write(svPostData);
		// We only need to flush if we have content data, as Request.Serialize()
		// already flushes after the headers are written.
		// Request.close() closes the output transformations, which flushes their
		// pipeline into the output stream, and then calls flush() on the output
		// stream.
		Request.close();
	}

	if (!m_Connection->Good())
	{
		return SetError("write error");
	}

	return ReadHeader();
}

//-----------------------------------------------------------------------------
bool KHTTPClient::ReadHeader()
//-----------------------------------------------------------------------------
{
	if (!Response.Parse())
	{
		SetError(Response.Error());
		return false;
	}

	return true;

} // ReadHeader

//-----------------------------------------------------------------------------
KString KHTTPClient::Get(const KURL& URL)
//-----------------------------------------------------------------------------
{
	KString sBuffer;

	if (Connect(URL))
	{
		if (Resource(URL, KHTTPMethod::GET))
		{
			if (SendRequest())
			{
				Read(sBuffer);
			}
		}
	}

	return sBuffer;

} // Get

//-----------------------------------------------------------------------------
KString KHTTPClient::Options(const KURL& URL)
//-----------------------------------------------------------------------------
{
	KString sBuffer;

	if (Connect(URL))
	{
		if (Resource(URL, KHTTPMethod::OPTIONS))
		{
			if (SendRequest())
			{
				Read(sBuffer);
			}
		}
	}

	return sBuffer;

} // Options

//-----------------------------------------------------------------------------
bool KHTTPClient::Head(const KURL& URL)
//-----------------------------------------------------------------------------
{
	KString sBuffer;
	
	if (Connect(URL))
	{
		if (Resource(URL, KHTTPMethod::HEAD))
		{
			if (SendRequest())
			{
				return Response.Good();
			}
		}
	}

	return false;

} // Head

//-----------------------------------------------------------------------------
KString KHTTPClient::Post(const KURL& URL, KStringView svRequestBody, KMIME Mime)
//-----------------------------------------------------------------------------
{
	KString sBuffer;

	if (Connect(URL))
	{
		if (Resource(URL, KHTTPMethod::POST))
		{
			if (SendRequest(svRequestBody, Mime))
			{
				Read(sBuffer);
			}
		}
	}

	return sBuffer;

} // Post

//-----------------------------------------------------------------------------
KString KHTTPClient::Delete(const KURL& URL, KStringView svRequestBody, KMIME Mime)
//-----------------------------------------------------------------------------
{
	KString sBuffer;

	if (Connect(URL))
	{
		if (Resource(URL, KHTTPMethod::DELETE))
		{
			if (SendRequest(svRequestBody, Mime))
			{
				Read(sBuffer);
			}
		}
	}

	return sBuffer;

} // Delete

//-----------------------------------------------------------------------------
bool KHTTPClient::Put(const KURL& URL, KStringView svRequestBody, KMIME Mime)
//-----------------------------------------------------------------------------
{
	if (Connect(URL))
	{
		if (Resource(URL, KHTTPMethod::PUT))
		{
			if (SendRequest(svRequestBody, Mime))
			{
				return Response.Good();
			}
		}
	}

	return false;

} // Put

//-----------------------------------------------------------------------------
bool KHTTPClient::Patch(const KURL& URL, KStringView svRequestBody, KMIME Mime)
//-----------------------------------------------------------------------------
{
	if (Connect(URL))
	{
		if (Resource(URL, KHTTPMethod::PATCH))
		{
			if (SendRequest(svRequestBody, Mime))
			{
				return Response.Good();
			}
		}
	}

	return false;

} // Patch

//-----------------------------------------------------------------------------
bool KHTTPClient::AlreadyConnected(const KURL& URL) const
//-----------------------------------------------------------------------------
{
	if (!m_Connection || !m_Connection->Good())
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
bool kHTTPHead(const KURL& URL)
//-----------------------------------------------------------------------------
{
	KHTTPClient HTTP;
	return HTTP.Head(URL);

} // kHTTPHead

//-----------------------------------------------------------------------------
KString kHTTPPost(const KURL& URL, KStringView svPostData, KStringView svMime)
//-----------------------------------------------------------------------------
{
	KHTTPClient HTTP;
	return HTTP.Post(URL, svPostData, svMime);

} // kHTTPPost


} // end of namespace dekaf2
