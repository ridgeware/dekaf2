/*
//
// DEKAF(tm): Lighter, Faster, Smarter (tm)
//
// Copyright (c) 2024, Ridgeware, Inc.
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

#include "khttp_version.h"
#include "klog.h"

DEKAF2_NAMESPACE_BEGIN

//-----------------------------------------------------------------------------
KHTTPVersion KHTTPVersion::Parse(KStringView sVersion)
//-----------------------------------------------------------------------------
{
	if (DEKAF2_LIKELY(sVersion.size() >= KStringView{"HTTP/1"}.size()))
	{
		auto it = sVersion.begin();

		if (*it++ == 'H' && *it++ == 'T' && *it++ == 'T' && *it++ == 'P' && *it++ == '/')
		{
			auto ie    = sVersion.end();
			char major = *it++;

			if (it == ie)
			{
				if (major == '2')
				{
					return KHTTPVersion::http2;
				}
				else if (major == '3')
				{
					return KHTTPVersion::http3;
				}
			}
			else if (major == '1' && *it++ == '.')
			{
				if (it != ie)
				{
					char minor = *it++;

					if (it == ie)
					{
						if (minor == '1')
						{
							return KHTTPVersion::http11;
						}
						else if (minor == '0')
						{
							return KHTTPVersion::http10;
						}
					}
				}
			}
		}
	}

	kDebug(2, "unknown HTTP version: '{}'", sVersion);
	return KHTTPVersion::none;

} // Parse

//-----------------------------------------------------------------------------
KStringViewZ KHTTPVersion::Serialize() const
//-----------------------------------------------------------------------------
{
	switch (m_Version)
	{
		case KHTTPVersion::none:
			break;
		case KHTTPVersion::http10:
			return "HTTP/1.0";
		case KHTTPVersion::http11:
			return "HTTP/1.1";
		case KHTTPVersion::http2:
			return "HTTP/2";
		case KHTTPVersion::http3:
			return "HTTP/3";
	}

	kDebug(2, "no HTTP version set");
	return KStringViewZ{};
}

DEKAF2_NAMESPACE_END
