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
/// Simple file server implementation. This class keeps state about ONE file at a time when starting a new
/// request with Open(). If you want concurrent access from multiple threads on the same pool of files,
/// then use one instance of KFileServer per thread.
/// The main purpose of this implementation is to maximise security, and detect invalid requests.
class DEKAF2_PUBLIC KFileServer
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	/// Simple file server implementation - errors lead to KHTTPErrors thrown
	/// @param jConfig json configuration, used properties: indexfile, styles, addtohead, addtobodytop, addtobodybottom
	/// where index is the file name to serve when resource is a directory, per default index.html
	/// styles is the style sheet to use instead of the default style sheet. Either a URL/path (ending with .css)
	/// or plain CSS definitions. Defaults to the empty string, forcing the default style sheet.
	KFileServer(const KJSON& jConfig);

	/// Prepare for file access. May throw.
	/// @param sDocumentRoot The file system directory that contains all served files.
	/// @param sRequest The normalized resource request with the full external path,
	/// but stripped by the base route prefix and an eventual trailing slash.
	/// @param sRoute The base path valid for this request. Will be substracted from sRequest.
	/// @param bHadTrailingSlash did the original request have a trailing slash, so that we can search for the index file?
	/// @param bCreateAdHocIndex if the path is a directory, and there is no index.html,
	/// should an automatic index be created?
	/// @param bWithUpload if the path is a directory, should uploads be allowed?
	bool Open(KStringView sDocumentRoot,
	          KStringView sRequest,
	          KStringView sRoute,
	          bool        bHadTrailingSlash,
	          bool        bCreateAdHocIndex,
			  bool        bWithUpload);

	/// Checks if the requested file exists
	DEKAF2_NODISCARD
	bool Exists() const { return m_FileStat.IsFile(); }

	/// Checks if the requested file is a directory
	DEKAF2_NODISCARD
	bool IsDirectory() const { return m_FileStat.IsDirectory(); }

	/// Checks if the requested file is a directory, but the original request did not have a slash at the end.
	/// This is needed for HTTP redirects, as we cannot simply append a index.html in that case to the
	/// directory name to serve the index, but have to make sure the client sees the slash after the directory..
	DEKAF2_NODISCARD
	bool RedirectAsDirectory() const { return m_bReDirectory; }

	/// Returns the file system path as created by Open()
	DEKAF2_NODISCARD
	const KString& GetFileSystemPath() const { return m_sFileSystemPath; }

	/// Returns KFileStat information about a file
	DEKAF2_NODISCARD
	const KFileStat& GetFileStat() const { return m_FileStat; }

	/// Returns a stream good for reading the resource. May throw.
	/// @param iFromPos positions the read position of the stream to the given value, defaults to 0/start
	DEKAF2_NODISCARD
	std::unique_ptr<KInStream> GetStreamForReading(std::size_t iFromPos = 0);

	/// Returns a stream good for writing the resource. May throw.
	/// @param iToPos positions the write position of the stream to the given value, defaults to 0/start
	DEKAF2_NODISCARD
	std::unique_ptr<KOutStream> GetStreamForWriting(std::size_t iToPos = 0);

	/// Returns the mime type of the resource. May throw if file not found.
	DEKAF2_NODISCARD
	const KMIME& GetMIMEType(bool bInspect);

	/// Returns true if the last request generated an ad-hoc index file
	DEKAF2_NODISCARD
	bool IsAdHocIndex() const { return m_bIsAdHocIndex; }

	/// Returns ad-hoc generated index file
	DEKAF2_NODISCARD
	const KString& GetAdHocIndex() const { return m_sIndex; }

	/// Clears all state (included by Open())
	void clear();

//------
protected:
//------

	void GenerateAdHocIndexFile(KStringView sDirectory, bool bWithIndex, bool bWithDelete);

	const KJSON& m_jConfig;
	KString      m_sDirIndexFile;
	KString      m_sFileSystemPath;
	KString      m_sIndex;
	KMIME        m_mime    { KMIME::NONE };
	KFileStat    m_FileStat;
	bool         m_bReDirectory  { false };
	bool         m_bIsAdHocIndex { false };

}; // KFileServer

DEKAF2_NAMESPACE_END
