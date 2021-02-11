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

#pragma once

#include "kstring.h"
#include "kstringview.h"
#include "kreader.h"
#include "kwriter.h"
#include "kmime.h"
#include <memory>

/// @file kfileserver.h
/// file server implementation

namespace dekaf2 {

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Simple file server implementation
class KFileServer
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	/// Simple file server implementation
	/// @param bThrow If true, errors lead to KHTTPErrors thrown
	KFileServer(bool bThrow = true)
	: m_bThrow(bThrow)
	{
	}

	/// Prepare for file access. May throw.
	/// @param sDocumentRoot The file system directory that is the root for all requests.
	/// @param sRequest The resource request with the full external path, e.g. http base path.
	/// @param sBaseRoute The base path valid for this request. Will be substracted from sRequest.
	bool Open(KStringView sDocumentRoot, KStringView sRequest, KStringView sBaseRoute);

	/// Returns the file system path as created by Open()
	const KString& GetFileSystemPath() const { return m_sFileSystemPath; }

	/// Returns the file size, or npos if not found.
	std::size_t GetFileSize() const { return m_iFileSize; }

	/// Returns a stream good for reading the resource. May throw.
	std::unique_ptr<KInStream> GetStreamForReading();

	/// Returns a stream good for writing the resource. May throw.
	std::unique_ptr<KOutStream> GetStreamForWriting();

	/// Returns the mime type of the resource. May throw if file not found.
	KMIME GetMIMEType(bool bInspect);

	/// Clears all state (included by Open())
	void clear();

//------
protected:
//------

	KString     m_sFileSystemPath;
	KMIME       m_mime { KMIME::NONE };
	std::size_t m_iFileSize  { npos  };
	bool        m_bThrow     { true  };

}; // KFileServer

} // end of namespace dekaf2
