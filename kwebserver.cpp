/*
//
// DEKAF(tm): Lighter, Faster, Smarter (tm)
//
// Copyright (c) 2019, Ridgeware, Inc.
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

#include "kwebserver.h"
#include "khttperror.h"
#include "ktime.h"

DEKAF2_NAMESPACE_BEGIN

//-----------------------------------------------------------------------------
uint16_t KWebServer::Serve(KStringView         sDocumentRoot,
                           KStringView         sResourcePath,
                           bool                bHadTrailingSlash,
                           KStringView         sRoute,
                           KHTTPMethod         RequestMethod,
                           const KHTTPHeaders& RequestHeaders,
                           KHTTPHeaders&       ResponseHeaders)
//-----------------------------------------------------------------------------
{
	m_iStatus = KHTTPError::H2xx_OK;

	if (RequestMethod != KHTTPMethod::GET && RequestMethod != KHTTPMethod::HEAD)
	{
		kDebug(1, "invalid method: {}", RequestMethod.Serialize());
		throw KHTTPError { KHTTPError::H4xx_BADMETHOD, kFormat("request method {} not supported for path: {}", RequestMethod.Serialize(), sRoute) };
	}

	this->Open(sDocumentRoot,
	           sResourcePath,
	           sRoute,
	           bHadTrailingSlash);

	if (this->RedirectAsDirectory())
	{
		// redirect
		KString sRedirect = sResourcePath;
		sRedirect += '/';

		ResponseHeaders.Headers.Remove(KHTTPHeader::CONTENT_TYPE);
		ResponseHeaders.Headers.Set(KHTTPHeader::LOCATION, std::move(sRedirect));

		if (RequestMethod == KHTTPMethod::GET || RequestMethod == KHTTPMethod::HEAD)
		{
			throw KHTTPError { KHTTPError::H301_MOVED_PERMANENTLY, "moved permanently" };
		}
		else
		{
			throw KHTTPError { KHTTPError::H308_PERMANENT_REDIRECT, "permanent redirect" };
		}
	}
	else if (this->Exists())
	{
		ResponseHeaders.Headers.Set(KHTTPHeader::CONTENT_TYPE, this->GetMIMEType(true).Serialize());

		auto tIfModifiedSince   = kParseHTTPTimestamp(RequestHeaders.Headers.Get(KHTTPHeader::IF_MODIFIED_SINCE  ));
		auto tIfUnmodifiedSince = kParseHTTPTimestamp(RequestHeaders.Headers.Get(KHTTPHeader::IF_UNMODIFIED_SINCE));
		auto tLastModified      = this->GetFileStat().ModificationTime();

		if (tIfModifiedSince.ok() && tLastModified <= tIfModifiedSince)
		{
			throw KHTTPError { KHTTPError::H304_NOT_MODIFIED, "not modified" };
		}
		else if (tIfUnmodifiedSince.ok() && tLastModified > tIfUnmodifiedSince)
		{
			throw KHTTPError { KHTTPError::H4xx_PRECONDITION_FAILED, "precondition failed" };
		}
		else
		{
			m_iFileSize  = this->GetFileStat().Size();
			m_iFileStart = 0;

			// check for ranges
			auto Ranges = RequestHeaders.GetRanges(m_iFileSize);

			if (!Ranges.empty())
			{
				// if a If-Range header is set with a timestamp, and the resource is of older or
				// same age, the range request is accepted - otherwise the full (newer) document
				// is sent - note: we do not check for etags in the If-Range (we never send them
				// either)
				auto tIfRange = kParseHTTPTimestamp(RequestHeaders.Headers.Get(KHTTPHeader::IF_RANGE));

				if (!tIfRange.ok() || tLastModified <= tIfRange)
				{
					// we currently only support one range per request
					ResponseHeaders.Headers.Set(KHTTPHeader::CONTENT_RANGE, kFormat("bytes {}-{}/{}", Ranges.front().GetStart(), Ranges.front().GetEnd(), m_iFileSize));
					m_iFileStart = Ranges.front().GetStart();
					m_iFileSize  = Ranges.front().GetSize();
					m_iStatus    = KHTTPError::H2xx_PARTIAL_CONTENT;
				}
			}

			ResponseHeaders.Headers.Set(KHTTPHeader::LAST_MODIFIED   , KHTTPHeader::DateToString(tLastModified));
			// announce that we would accept ranges
			ResponseHeaders.Headers.Set(KHTTPHeader::ACCEPT_RANGES   , "bytes");
		}
	}
	else
	{
		// This file does not exist..
		kDebug(1, "Cannot open file: {}", this->GetFileSystemPath());
		ResponseHeaders.Headers.Set(KHTTPHeader::CONTENT_TYPE, KMIME::HTML_UTF8);
		throw KHTTPError { KHTTPError::H4xx_NOTFOUND, "file not found" };
	}

	return m_iStatus;

} // KWebServer::Serve

DEKAF2_NAMESPACE_END
