/*
//
// DEKAF(tm): Lighter, Faster, Smarter (tm)
//
// Copyright (c) 2026, Ridgeware, Inc.
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

/// @file kwebdav.h
/// WebDAV Class 2 implementation for file-based network storage

#include "kdefinitions.h"
#include "kstring.h"
#include "kstringview.h"
#include "kfilesystem.h"
#include "ktime.h"
#include "krestserver.h"
#include "kwebserverpermissions.h"

DEKAF2_NAMESPACE_BEGIN

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// WebDAV Class 2 implementation for file-based network storage.
/// Handles PROPFIND, PROPPATCH, MKCOL, COPY, MOVE, DELETE, OPTIONS, LOCK, and UNLOCK methods.
/// Standard HTTP methods (GET, HEAD, PUT, POST) are delegated to KWebServer.
/// @note Symlinks inside the document root are followed by design. Ensure that the document
/// root and its contents do not contain symlinks pointing outside the intended directory tree.
class DEKAF2_PUBLIC KWebDAV
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	/// Serve a WebDAV-specific request (PROPFIND, PROPPATCH, MKCOL, COPY, MOVE, DELETE, OPTIONS, LOCK, UNLOCK).
	/// Throws KHTTPError on failure.
	/// @param HTTP the REST server instance
	/// @param sDocumentRoot the filesystem root path
	/// @param sRequestPath the matched request path
	/// @param sRoute the route prefix to strip from request path
	/// @param Permissions the permission set for access control
	/// @param sUser the authenticated user name (may be empty)
	static void Serve(KRESTServer& HTTP,
	                  KStringView  sDocumentRoot,
	                  KStringView  sRequestPath,
	                  KStringView  sRoute,
	                  const KWebServerPermissions& Permissions,
	                  KStringView  sUser);

	/// generate an ETag string from file stat info
	DEKAF2_NODISCARD
	static KString GenerateETag(const KFileStat& Stat);

	/// resolve a request path to a filesystem path, validates path safety
	/// @param sDocumentRoot the filesystem root path
	/// @param sRequestPath the matched request path
	/// @param sRoute the route prefix to strip
	/// @returns the resolved filesystem path
	DEKAF2_NODISCARD
	static KString ResolveFilesystemPath(KStringView sDocumentRoot, KStringView sRequestPath, KStringView sRoute);

//------
private:
//------

	static void Propfind  (KRESTServer& HTTP, KStringView sDocumentRoot, KStringView sRequestPath, KStringView sRoute);
	static void Proppatch (KRESTServer& HTTP, KStringView sDocumentRoot, KStringView sRequestPath, KStringView sRoute);
	static void Mkcol     (KRESTServer& HTTP, KStringView sDocumentRoot, KStringView sRequestPath, KStringView sRoute);
	static void CopyOrMove(KRESTServer& HTTP, KStringView sDocumentRoot, KStringView sRequestPath, KStringView sRoute,
	                       const KWebServerPermissions& Permissions, KStringView sUser, bool bIsMove);
	static void Delete    (KRESTServer& HTTP, KStringView sDocumentRoot, KStringView sRequestPath, KStringView sRoute);
	static void Options   (KRESTServer& HTTP);
	static void Lock      (KRESTServer& HTTP, KStringView sDocumentRoot, KStringView sRequestPath, KStringView sRoute);
	static void Unlock    (KRESTServer& HTTP);

}; // KWebDAV

DEKAF2_NAMESPACE_END
