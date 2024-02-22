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

#include "kdefinitions.h"
#include "kstring.h"
#include "kstringview.h"
#include "kreader.h"
#include "kwriter.h"
#include "kmime.h"
#include "kfilesystem.h"
#include <memory>

/// @file kfileserver.h
/// file server implementation

DEKAF2_NAMESPACE_BEGIN

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Simple file server implementation
class DEKAF2_PUBLIC KFileServer
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	/// Simple file server implementation
	/// @param bThrow If true, errors lead to KHTTPErrors thrown. Default true.
	/// @param sDirIndexFile file name to serve when resource is a directory, per default index.html
	KFileServer(bool bThrow = true, KString sDirIndexFile = "index.html")
	: m_sDirIndexFile(std::move(sDirIndexFile))
	, m_bThrow(bThrow)
	{
	}

	/// Prepare for file access. May throw.
	/// @param sDocumentRoot The file system directory that contains all served files.
	/// @param sRequest The normalized resource request with the full external path,
	/// but stripped by the base route prefix and an eventual trailing slash.
	/// @param sRoute The base path valid for this request. Will be substracted from sRequest.
	/// @param bHadLeadingSlash did the original request have a leading slash, so that we can search for the index file?
	bool Open(KStringView sDocumentRoot,
			  KStringView sRequest,
			  KStringView sRoute,
			  bool        bHadLeadingSlash);

	/// Checks if the requested file exists
	bool Exists() const { return m_FileStat.IsFile(); }

	/// Checks if the requested file is a directory
	bool IsDirectory() const { return m_FileStat.IsDirectory(); }

	/// Checks if the requested file is a directory, but the original request did not have a slash at the end.
	/// This is needed for HTTP redirects, as we cannot simply append a index.html in that case to the
	/// directory name to serve the index, but have to make sure the client sees the slash after the directory..
	bool RedirectAsDirectory() const { return m_bReDirectory; }

	/// Returns the file system path as created by Open()
	const KString& GetFileSystemPath() const { return m_sFileSystemPath; }

	/// Returns KFileStat information about a file
	const KFileStat& GetFileStat() const { return m_FileStat; }

	/// Returns a stream good for reading the resource. May throw.
	/// @param iFromPos positions the read position of the stream to the given value, defaults to 0/start
	std::unique_ptr<KInStream> GetStreamForReading(std::size_t iFromPos = 0);

	/// Returns a stream good for writing the resource. May throw.
	/// @param iToPos positions the write position of the stream to the given value, defaults to 0/start
	std::unique_ptr<KOutStream> GetStreamForWriting(std::size_t iToPos = 0);

	/// Returns the mime type of the resource. May throw if file not found.
	const KMIME& GetMIMEType(bool bInspect);

	/// Clears all state (included by Open())
	void clear();

//------
protected:
//------

	KString     m_sDirIndexFile;
	KString     m_sFileSystemPath;
	KMIME       m_mime   { KMIME::NONE };
	KFileStat   m_FileStat;
	bool        m_bThrow       { true  };
	bool        m_bReDirectory { false };

}; // KFileServer

DEKAF2_NAMESPACE_END
