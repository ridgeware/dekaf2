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

#pragma once

#include <vector>
#include "kstring.h"
#include "kstringview.h"
#include "ktimer.h"
#include "kthreadsafe.h"
#include "kregex.h"

/// @file khttpath.h
/// Primitives for HTTP routing

namespace dekaf2 {

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// A HTTP path object, used as base class for all HTTP and REST path / route objects
class DEKAF2_PUBLIC KHTTPPath
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	using URLParts = std::vector<KString>;

	//-----------------------------------------------------------------------------
	/// Construct a HTTP path from a HTTP request path
	/// @param _sRoute the HTTP path, like "/some/path/index.html" or "/documents/*" or "/help"
	KHTTPPath(KString _sRoute);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Returns count of path parts (sections between / )
	size_t PartCount() const { return vURLParts.size(); }
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Returns a string view with the first iPartCount parts of the path
	/// @param iPartCount how many parts of the path to return in a string
	/// @return a stringview on the first parts of the path
	KStringView GetFirstParts(size_t iPartCount) const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// static method to split a URL into parts
	static URLParts SplitURL(KStringView sURLPath);
	//-----------------------------------------------------------------------------

	KString     sRoute;      // e.g. "/some/path/index.html" or "/documents/*" or "/help"
	URLParts    vURLParts;   // vector of the split path parts

}; // KHTTPPath

namespace detail {

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// A HTTP path object that performs some analysis on the path, to accelerate routing
class DEKAF2_PUBLIC KHTTPAnalyzedPath : public KHTTPPath
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	//-----------------------------------------------------------------------------
	/// Construct an analyzed HTTP path
	/// @param _sRoute the HTTP path, like "/some/path/index.html" or "/documents/*" or "/help"
	KHTTPAnalyzedPath(KString _sRoute);
	//-----------------------------------------------------------------------------

	/// a struct with usage and timing statistics of the path / route
	struct Stats
	{
		KDurations  Durations; ///< timing and count
		std::size_t iRxBytes;  ///< received bytes (total)
		std::size_t iTxBytes;  ///< transmitted bytes (total)
	};

	/// A threadsafe object with usage and timing statistics
	mutable KThreadSafe<Stats> Statistics;

	bool bHasWildCardAtEnd    { false };
	bool bHasWildCardFragment { false };

}; // KHTTPAnalyzedPath

} // end of namespace detail

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC KHTTPRewrite
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	KHTTPRewrite(KStringView _sRegex, KString _sTo);

	KRegex  RegexFrom;
	KString sTo;

}; // KHTTPRewrite

} // end of namespace dekaf2
