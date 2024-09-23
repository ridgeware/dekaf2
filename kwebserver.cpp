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
void KWebServer::Check
(
	KStringView        sDocumentRoot,
	KStringView         sResourcePath,
	bool                bHadTrailingSlash,
	bool                bCreateAdHocIndex,
	bool                bWithUpload,
	KStringView         sRoute,
	KHTTPMethod         RequestMethod,
	const KHTTPHeaders& RequestHeaders,
	KHTTPHeaders&       ResponseHeaders,
	const CheckMethod&  RouteCheck
)
//-----------------------------------------------------------------------------
{
	m_iStatus         = KHTTPError::H2xx_OK;
	m_bIsValid        = false;
	bool bIsGetOrHead = RequestMethod == KHTTPMethod::GET  || RequestMethod == KHTTPMethod::HEAD;
	bool bIsPostOrPut = RequestMethod == KHTTPMethod::POST || RequestMethod == KHTTPMethod::PUT;
	bool bIsDelete    = RequestMethod == KHTTPMethod::DELETE;

	if (!bIsGetOrHead && (!bWithUpload || !(bIsDelete || bIsPostOrPut)))
	{
		kDebug(1, "invalid method: {}", RequestMethod.Serialize());
		throw KHTTPError { KHTTPError::H4xx_BADMETHOD, kFormat("request method {} not supported for path: {}", RequestMethod, sRoute) };
	}

	if (bWithUpload && bIsPostOrPut)
	{
		// remove the index.html for POST requests, this may have
		// been the result of an explicit ask for index.html instead
		// of the directory only
		if (sResourcePath.remove_suffix("/index.html"))
		{
			bHadTrailingSlash = true;
		}
	}

	this->Open
	(
		sDocumentRoot,
		sResourcePath,
		sRoute,
		bHadTrailingSlash,
		bCreateAdHocIndex && bIsGetOrHead,
		bWithUpload       && bIsGetOrHead
	);

	if (this->RedirectAsDirectory())
	{
		// redirect
		KString sRedirect = sResourcePath;
		sRedirect += '/';

		ResponseHeaders.Headers.Remove (KHTTPHeader::CONTENT_TYPE);
		ResponseHeaders.Headers.Set    (KHTTPHeader::LOCATION, std::move(sRedirect));

		if (bIsGetOrHead)
		{
			throw KHTTPError { KHTTPError::H301_MOVED_PERMANENTLY, "moved permanently" };
		}
		else
		{
			throw KHTTPError { KHTTPError::H308_PERMANENT_REDIRECT, "permanent redirect" };
		}
	}

	if (bIsPostOrPut)
	{
		// file may exist or not..
	}
	else if (this->Exists())
	{
		if (RequestMethod == KHTTPMethod::GET)
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

				ResponseHeaders.Headers.Set(KHTTPHeader::LAST_MODIFIED, KHTTPHeader::DateToString(tLastModified));
				// announce that we would accept ranges
				ResponseHeaders.Headers.Set(KHTTPHeader::ACCEPT_RANGES, "bytes");
			}
		}
	}
	else
	{
		// This file does not exist.. now check if we should better return
		// a REST error code, or an HTTP server error code. A REST error code
		// makes sense when this path was defined in a REST context, but with
		// a different method. We may end up here in the Web server mode if
		// the web server is sort of a catch all at the end of the routes..
		// (which, for security and ambiguity, should really better be avoided)

		if (RouteCheck && RouteCheck(RequestMethod, sResourcePath))
		{
			kDebug (2, "request method {} not supported for path: {}", RequestMethod.Serialize(), sResourcePath);
			throw KHTTPError { KHTTPError::H4xx_BADMETHOD, kFormat("request method {} not supported for path: {}", RequestMethod, sResourcePath) };
		}
		else
		{
			kDebug(1, "file does not exist: {}", this->GetFileSystemPath());

			if (bIsGetOrHead)
			{
				ResponseHeaders.Headers.Set(KHTTPHeader::CONTENT_TYPE, KMIME::HTML_UTF8);
			}

			throw KHTTPError { KHTTPError::H4xx_NOTFOUND, kFormat("file not found: {}", sResourcePath) };
		}
	}

	m_bIsValid = true;

} // KWebServer::Check

//-----------------------------------------------------------------------------
KHTTPMethod KWebServer::Serve
(
	KStringView         sDocumentRoot,
	KStringView         sResourcePath,
	bool                bHadTrailingSlash,
	bool                bCreateAdHocIndex,
	bool                bWithUpload,
	KStringView         sRoute,
	KHTTPMethod         RequestMethod,
	const KHTTPHeaders& RequestHeaders,
	KHTTPHeaders&       ResponseHeaders,
	const CheckMethod&  RouteCheck
)
//-----------------------------------------------------------------------------
{
	Check
	(
		sDocumentRoot,
		sResourcePath,
		bHadTrailingSlash,
		bCreateAdHocIndex,
		bWithUpload,
		sRoute,
		RequestMethod,
		RequestHeaders,
		ResponseHeaders,
		RouteCheck
	);

	if (!IsValid())
	{
		// this should have been thrown already, probably more precisely
		throw KHTTPError { KHTTPError::H4xx_BADREQUEST, "bad request" };
	}

	switch (RequestMethod)
	{
		case KHTTPMethod::HEAD:
		case KHTTPMethod::GET:
			break;

		case KHTTPMethod::POST:
		{
			if (!m_InputStream)
			{
				throw KHTTPError { KHTTPError::H5xx_ERROR, "no input stream"};
			}

			// check if we have a boundary string in the content-type header
			KStringView sBoundary = RequestHeaders.Headers.Get(KHTTPHeader::CONTENT_TYPE);

			auto pos = sBoundary.find(';');

			if (pos != KStringView::npos)
			{
				sBoundary.remove_prefix(pos + 1);
				sBoundary.Trim();

				if (sBoundary.remove_prefix("boundary="))
				{
					if (sBoundary.remove_prefix('"'))
					{
						sBoundary.remove_suffix('"');
					}
					else if (sBoundary.remove_prefix('\''))
					{
						sBoundary.remove_suffix('\'');
					}
				}
				else
				{
					sBoundary.clear();
				}
			}
			else
			{
				sBoundary.clear();
			}

			bool bShowFileIndexAgain { false };

			KMIME Mime = RequestHeaders.ContentType();

			if (!sBoundary.empty() || Mime.Serialize().starts_with("multipart/"))
			{
				if (!bHadTrailingSlash)
				{
					// we would not know where to write the post data to if this was not a request
					// on an existing directory path (with a slash at the end)
					throw KHTTPError { KHTTPError::H4xx_BADREQUEST, kFormat("upload path must have a trailing slash: {}", sResourcePath) };
				}

				// this is a multipart encoded request
				KMIMEReceiveMultiPartFormData Receiver(m_TempDir.Name(), sBoundary);

				if (!Receiver.ReadFromStream(*m_InputStream))
				{
					throw KHTTPError { KHTTPError::H4xx_BADREQUEST, Receiver.Error() };
				}

				for (auto& File : Receiver.GetFiles())
				{
					auto sFrom = kFormat("{}{}{}", m_TempDir.Name(), kDirSep, File.GetFilename());

					// move the files from the temp location into the upload folder
					if (File.GetCompleted())
					{
						auto sTo   = kFormat("{}{}{}{}", sDocumentRoot, sResourcePath, kDirSep, File.GetFilename());
						kMove(sFrom, sTo);
					}
					else
					{
						kDebug(2, "skipping incomplete upload file: {}", File.GetFilename());
						kRemoveFile(sFrom);
					}
				}

				if (!bHadTrailingSlash)
				{
					// this is probably never used, but we keep it for completeness:
					// remove last path component (the filename)
					sResourcePath     = kDirname(sResourcePath);
					bHadTrailingSlash = true;
				}

				bShowFileIndexAgain = true;
			}
			else if (bHadTrailingSlash)
			{
				// we may have a create/delete file/directory request in HTTP POST logic, issued from
				// a web browser and not from a REST client
				KString sBody;
				kUrlDecode(m_InputStream->ReadAll(), sBody);

				if (sBody.remove_prefix("createDir="))
				{
					if (sBody.empty())
					{
						throw KHTTPError { KHTTPError::H4xx_BADREQUEST, "missing directory name" };
					}

					auto sCreateDir = kFormat("{}{}{}",
					                          sResourcePath,
					                          kDirSep,
					                          kMakeSafePathname(sBody, false)
					                          );

					if (kCreateDir(kFormat("{}{}", sDocumentRoot, sCreateDir)))
					{
						kDebug(2, "created directory: {}", sCreateDir);
					}

					bShowFileIndexAgain = true;
				}
				else if (sBody.remove_prefix("deleteFile="))
				{
					if (sBody.empty())
					{
						throw KHTTPError { KHTTPError::H4xx_BADREQUEST, "missing file name" };
					}

					auto sRemoveFile = kFormat("{}{}{}",
					                           sResourcePath,
					                           kDirSep,
					                           kMakeSafeFilename(sBody, false)
					                           );

					if (kRemoveFile(kFormat("{}{}", sDocumentRoot, sRemoveFile)))
					{
						kDebug(2, "removed file: {}", sRemoveFile);
					}

					bShowFileIndexAgain = true;
				}
				else if (sBody.remove_prefix("deleteDir="))
				{
					if (sBody.empty())
					{
						throw KHTTPError { KHTTPError::H4xx_BADREQUEST, "missing directory name" };
					}

					auto sRemoveDir = kFormat("{}{}{}",
					                          sResourcePath,
					                          kDirSep,
					                          kMakeSafePathname(sBody, false)
					                          );

					if (kRemoveDir(kFormat("{}{}", sDocumentRoot, sRemoveDir)))
					{
						kDebug(2, "removed directory: {}", sRemoveDir);
					}

					bShowFileIndexAgain = true;
				}
			}

			if (bShowFileIndexAgain)
			{
				// show the new list of files:
				// change the POST into a GET request
				RequestMethod = KHTTPMethod::GET;
				// and check the files again
				Check(
					sDocumentRoot,
					sResourcePath,
					bHadTrailingSlash,
					bCreateAdHocIndex,
					bWithUpload,
					sRoute,
					RequestMethod,
					RequestHeaders,
					ResponseHeaders,
					RouteCheck
				);
				// exit the switch here, the POST was successful
				break;
			}
			// else just fall through into PUT..
		}
		// fallthrough into the PUT case, the POST was not a multipart encoding
		DEKAF2_FALLTHROUGH;

		case KHTTPMethod::PUT:
			RequestMethod = KHTTPMethod::PUT;

			if (!m_InputStream)
			{
				throw KHTTPError { KHTTPError::H5xx_ERROR, "no input stream"};
			}

			if (bHadTrailingSlash == false)
			{
				// this is a plain PUT (or POST) of data, the file name is taken
				// from the last part of the URL
				auto sName = kMakeSafeFilename(kBasename(sResourcePath));

				if (sName.empty())
				{
					throw KHTTPError { KHTTPError::H4xx_BADREQUEST, "missing target name in URL" };
				}

				auto sFrom = kFormat("{}{}{}", m_TempDir.Name(), kDirSep, sName);
				auto sTo   = kFormat("{}{}{}", sDocumentRoot, kDirSep, sName);

				KOutFile OutFile(sFrom);

				if (!OutFile.is_open())
				{
					throw KHTTPError { KHTTPError::H4xx_BADREQUEST, "cannot open file" };
				}

				if (!OutFile.Write(*m_InputStream).Good())
				{
					throw KHTTPError { KHTTPError::H5xx_ERROR, "cannot write file" };
				}

				OutFile.close();

				kDebug(2, "received {} for file: {}", kFormBytes(kFileSize(sFrom)), sName);

				kMove(sFrom, sTo);
			}
			else
			{
				throw KHTTPError { KHTTPError::H4xx_BADREQUEST, "missing target name in URL" };
			}
			// this is a REST request, do not return the index file but simply return
			break;

		case KHTTPMethod::DELETE:
		{
			auto sName = kFormat("{}{}{}", sDocumentRoot, kDirSep, sResourcePath);

			kDebug(2, "deleting file: {}", sResourcePath);

			if (!kRemove(sName, KFileTypes::ALL))
			{
				throw KHTTPError { KHTTPError::H5xx_ERROR, "cannot remove file" };
			}

			break;
		}

		default:
			throw KHTTPError { KHTTPError::H4xx_BADREQUEST, "method not supported" };
	}

	return RequestMethod;

} // Serve

DEKAF2_NAMESPACE_END
