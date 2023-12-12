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

DEKAF2_NAMESPACE_BEGIN

//-----------------------------------------------------------------------------
KHTTPPath::KHTTPPath(KString _sRoute)
//-----------------------------------------------------------------------------
	: sRoute(std::move(_sRoute))
	, vURLParts(SplitURL(sRoute))
{
	if (!sRoute.empty() && sRoute.front() != '/')
	{
		kWarning("error: route does not start with a slash: {}", sRoute);
	}

} // KHTTPPath

//-----------------------------------------------------------------------------
KStringView KHTTPPath::GetFirstParts(size_t iPartCount) const
//-----------------------------------------------------------------------------
{
	KString sPath;
	KStringView::size_type iLength { 0 };

	if (iPartCount <= vURLParts.size())
	{
		for (const auto& sPart : vURLParts)
		{
			if (!iPartCount++)
			{
				break;
			}

			iLength += sPart.size() + 1;
		}
	}
	else
	{
		kDebug(1, "bad part count requested: have {}, requested {}", vURLParts.size(), iPartCount);
	}

	return KStringView(sRoute.data(), iLength);

} // GetFirstParts

//-----------------------------------------------------------------------------
KHTTPPath::URLParts KHTTPPath::SplitURL(KStringView sURLPath)
//-----------------------------------------------------------------------------
{
	sURLPath.remove_prefix("/");

	return kSplits<URLParts>(sURLPath, '/');

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


KHTTPRewrite::KHTTPRewrite(KStringView _sRegex, KString _sTo)
: RegexFrom(_sRegex)
, sTo(std::move(_sTo))
{
}


DEKAF2_NAMESPACE_END
