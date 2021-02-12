/*
//
// DEKAF(tm): Lighter, Faster, Smarter (tm)
//
// Copyright (c) 2021, Ridgeware, Inc.
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

#include "kfileserver.h"
#include "khttperror.h"

namespace dekaf2 {

//-----------------------------------------------------------------------------
bool KFileServer::Open(KStringView sDocumentRoot, KStringView sRequest, KStringView sBaseRoute)
//-----------------------------------------------------------------------------
{
	clear();

	if (!sRequest.remove_prefix(sBaseRoute) || sRequest.front() != '/')
	{
		kDebug(1, "invalid document path (internal error): {}", sRequest);

		if (m_bThrow)
		{
			throw KHTTPError { KHTTPError::H5xx_ERROR, "invalid path" };
		}
		else
		{
			return false;
		}
	}

	sRequest.remove_prefix(1); // the leading slash

	if (!sRequest.empty() && !kIsSafePathname(sRequest))
	{
		kDebug(1, "invalid document path: {}", sRequest);

		if (m_bThrow)
		{
			throw KHTTPError { KHTTPError::H4xx_BADREQUEST, "invalid path" };
		}
		else
		{
			return false;
		}
	}

	m_sFileSystemPath = sDocumentRoot;
	m_sFileSystemPath += kDirSep;
	m_sFileSystemPath += sRequest;

	m_iFileSize       = kFileSize(m_sFileSystemPath);

	return true;

} // Open

//-----------------------------------------------------------------------------
std::unique_ptr<KInStream> KFileServer::GetStreamForReading()
//-----------------------------------------------------------------------------
{
	std::unique_ptr<KInFile> Stream;

	if (m_iFileSize != npos)
	{
		Stream = std::make_unique<KInFile>(m_sFileSystemPath);
	}

	if (!Stream || !Stream->is_open() || !Stream->Good())
	{
		kDebug(1, "Cannot open file: {}", m_sFileSystemPath);

		if (m_bThrow)
		{
			throw KHTTPError { KHTTPError::H4xx_NOTFOUND, "file not found" };
		}
		else
		{
			Stream.reset();
		}
	}

	return Stream;

} // GetStreamForReading

//-----------------------------------------------------------------------------
std::unique_ptr<KOutStream> KFileServer::GetStreamForWriting()
//-----------------------------------------------------------------------------
{
	std::unique_ptr<KOutFile> Stream;

	Stream = std::make_unique<KOutFile>(m_sFileSystemPath);

	if (!Stream || !Stream->is_open() || !Stream->Good())
	{
		kDebug(1, "Cannot open file: {}", m_sFileSystemPath);

		if (m_bThrow)
		{
			throw KHTTPError { KHTTPError::H4xx_NOTFOUND, "cannot open file" };
		}
		else
		{
			Stream.reset();
		}
	}

	return Stream;

} // GetStreamForWriting

//-----------------------------------------------------------------------------
KMIME KFileServer::GetMIMEType(bool bInspect)
//-----------------------------------------------------------------------------
{
	if (m_mime == KMIME::NONE)
	{
		if (m_iFileSize != npos)
		{
			m_mime = KMIME::CreateByExtension(m_sFileSystemPath);

			if (m_mime == KMIME::NONE)
			{
				if (bInspect)
				{
					m_mime = KMIME::CreateByInspection(m_sFileSystemPath, KMIME::BINARY);
				}
				else
				{
					m_mime = KMIME::BINARY;
				}
			}
		}
		else
		{
			if (m_bThrow)
			{
				throw KHTTPError { KHTTPError::H4xx_NOTFOUND, "file not found" };
			}
		}
	}
	
	return m_mime;

} // GetMIMEType

//-----------------------------------------------------------------------------
void KFileServer::clear()
//-----------------------------------------------------------------------------
{
	m_sFileSystemPath.clear();
	m_mime      = KMIME::NONE;
	m_iFileSize = npos;

} // clear

static_assert(std::is_nothrow_move_constructible<KFileServer>::value,
			  "KFileServer is intended to be nothrow move constructible, but is not!");

} // end of namespace dekaf2
