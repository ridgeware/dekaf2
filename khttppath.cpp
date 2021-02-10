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

#include "khttppath.h"
#include "ksplit.h"
#include "klog.h"

namespace dekaf2 {

//-----------------------------------------------------------------------------
KHTTPPath::KHTTPPath(KString _sRoute)
//-----------------------------------------------------------------------------
	: sRoute(std::move(_sRoute))
{
	if (!sRoute.empty() && sRoute.front() != '/')
	{
		kWarning("error: route does not start with a slash: {}", sRoute);
	}

	SplitURL(vURLParts, sRoute);

} // KHTTPPath

//-----------------------------------------------------------------------------
size_t KHTTPPath::SplitURL(URLParts& Parts, KStringView sURLPath)
//-----------------------------------------------------------------------------
{
	Parts.clear();
	sURLPath.remove_prefix("/");
	return kSplit(Parts, sURLPath, "/", "");

} // SplitURL

namespace detail {

//-----------------------------------------------------------------------------
KHTTPAnalyzedPath::KHTTPAnalyzedPath(KString _sRoute)
//-----------------------------------------------------------------------------
	: KHTTPPath(std::move(_sRoute))
{
	size_t iCount { 0 };

	for (auto& it : vURLParts)
	{
		++iCount;

		if (it == "*")
		{
			if (iCount == vURLParts.size())
			{
				bHasWildCardAtEnd = true;

				if (!sRoute.remove_suffix("/*"))
				{
					kWarning("cannot remove suffix '/*' from '{}'", sRoute);
				}
			}
			else
			{
				bHasWildCardFragment = true;
			}
			break;
		}
	}
	
} // KHTTPAnalyzedPath

} // end of namespace detail

} // end of namespace dekaf2
