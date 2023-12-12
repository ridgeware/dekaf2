
/*
 //
 // DEKAF(tm): Lighter, Faster, Smarter (tm)
 //
 // Copyright (c) 2018, Ridgeware, Inc.
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

#include "klambdastream.h"
#include "klog.h"
#include "ksystem.h"
#include "khttp_header.h"
#include "kurlencode.h"

DEKAF2_NAMESPACE_BEGIN

//-----------------------------------------------------------------------------
std::streamsize KLambdaInStream::StreamReader(void* sBuffer, std::streamsize iCount, void* stream_)
//-----------------------------------------------------------------------------
{
	// we do not need to loop the reader, as the streambuf requests bytes in blocks
	// and calls underflow() if more are expected

	if (stream_)
	{
		auto stream   = static_cast<KLambdaInStream::Stream*>(stream_);
		char* sOutBuf = static_cast<char*>(sBuffer);
		auto iRemain  = iCount;

		if (!stream->sHeader.empty())
		{
			// copy from the header buffer into the stream buffer
			auto iCopy = std::min(static_cast<KStringView::size_type>(iRemain), stream->sHeader.size());
			std::memcpy(sOutBuf, stream->sHeader.data(), iCopy);
			stream->sHeader.remove_prefix(iCopy);

			return iCopy;
		}

		// when the prepared header is empty this stream is done..
	}

	return 0;

} // StreamReader

//-----------------------------------------------------------------------------
bool KLambdaInStream::CreateHeader()
//-----------------------------------------------------------------------------
{
	m_Stream.sHeader.clear();

	KInStream stream(*m_Stream.istream);

	KString sArg;
	KString sPostData;
	KString sPath;
	KString sQuery;
	KString sMethod;
	KString sHeaders;

	while (stream.ReadLine(sArg))
	{
		if (sArg.empty() || sArg.front() == '#')
		{
			continue;
		}
		
		KStringView sValue;
		KStringView sKey;

		auto iColon = sArg.find(':');

		if (DEKAF2_LIKELY(iColon != KString::npos))
		{
			sValue = sArg.ToView(iColon + 1, KString::npos).Trim();
			sKey   = sArg.ToView(0, iColon);
		}

		if (sKey == "resource")
		{
			// m_sResource = sRHS; -- ignore
		}
		else if (sKey == "path")
		{
			sPath.clear();
			kUrlEncode(kUrlDecode<KString>(sValue), sPath, URIPart::Path);
		}
		else if (sKey == "httpMethod")
		{
			sMethod = sValue;
		}
		else if (sKey == "headers")
		{
			iColon = sValue.find(':');
			if (DEKAF2_LIKELY(iColon != KStringView::npos))
			{
				auto sSecond = sValue.ToView(iColon + 1).Trim();
				auto sFirst  = sValue.ToView(0, iColon);
				sHeaders += sFirst;
				sHeaders += ": ";
				sHeaders += sSecond;
				sHeaders += "\r\n";
			}
		}
		else if (sKey == "queryStringParameters")
		{
			iColon = sValue.find(':');
			if (DEKAF2_LIKELY(iColon != KStringView::npos))
			{
				auto sSecond = sValue.ToView(iColon + 1).Trim();
				auto sFirst  = sValue.ToView(0, iColon);
				if (!sQuery.empty())
				{
					sQuery += '&';
				}
				// this looks stupid at first (decoding, then encoding), but think of cases
				// where spaces had not been encoded (which would break the request header
				// later) or where percent encoding had already taken place (both are seen
				// in real data..)
				kUrlEncode(kUrlDecode<KString>(sFirst), sQuery, URIPart::Query);
				sQuery += '=';
				kUrlEncode(kUrlDecode<KString>(sSecond), sQuery, URIPart::Query);
			}
		}
		else if (sKey == "requestContext")
		{
			//m_RequestContext.Add (sFirst, sSecond); -- ignore
		}
		else if (sKey == "pathParameters")
		{
			//m_sPathParameters = sRHS; -- ignore
		}
		else if (sKey == "stageVariables")
		{
			//m_sStageVariables = sRHS; -- ignore
		}
		else if (sKey == "body")
		{
			sPostData = sValue;
		}
		else if (sKey == "isBase64Encoded")
		{
			sHeaders += "X-IsBase64Encoded: true\r\n";
		}
	}

	m_sHeader = sMethod;
	m_sHeader += ' ';
	m_sHeader += sPath;
	if (!sQuery.empty())
	{
		m_sHeader += '?';
		m_sHeader += sQuery;
	}
	m_sHeader += " HTTP/1.0\r\n";

	m_sHeader += sHeaders;

	if (!sPostData.empty())
	{
		m_sHeader += "Content-length: ";
		m_sHeader += KString::to_string(sPostData.size());
		m_sHeader += "\r\n";
	}

	// final end of header line
	m_sHeader += "\r\n";

	// if any, add the post data
	m_sHeader += sPostData;

	// point string view into the constructed header
	m_Stream.sHeader = m_sHeader;

	return true;

} // CreateHeader

//-----------------------------------------------------------------------------
KLambdaInStream::KLambdaInStream(std::istream& istream)
//-----------------------------------------------------------------------------
    : base_type(&m_StreamBuf)
{
	m_Stream.istream = &istream;

	CreateHeader();

} // ctor

DEKAF2_NAMESPACE_END

