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

#pragma once

#include "kdefinitions.h"
#include "kstringview.h"
#include "kformat.h"

DEKAF2_NAMESPACE_BEGIN

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// which HTTP version is used/requested/permitted?
class DEKAF2_PUBLIC KHTTPVersion
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	enum Version : uint8_t
	{
		none     = 0,      ///< unknown / not set
		http10   = 1 << 0, ///< HTTP/1.0
		http11   = 1 << 1, ///< HTTP/1.1
		http2    = 1 << 2, ///< HTTP/2
		http3    = 1 << 3  ///< HTTP/3
	};

	KHTTPVersion(Version Version = none) : m_Version(Version) {}
	KHTTPVersion(KStringView sVersion)   : m_Version(Parse(sVersion)) {}

	KStringViewZ   Serialize() const;
	      operator Version()   const { return m_Version;                  }
	void           clear()           { m_Version = Version::none;         }
	bool           empty()     const { return m_Version == Version::none; }

	static KHTTPVersion Parse(KStringView sVersion);

//------
private:
//------

	Version m_Version { Version::none };

}; // KHTTPVersion

DEKAF2_ENUM_IS_FLAG(KHTTPVersion::Version)

DEKAF2_NAMESPACE_END

namespace DEKAF2_FORMAT_NAMESPACE
{

template <>
struct formatter<DEKAF2_PREFIX KHTTPVersion::Version> : formatter<string_view>
{
	template <typename FormatContext>
	auto format(const DEKAF2_PREFIX KHTTPVersion::Version& Version, FormatContext& ctx) const
	{
		return formatter<string_view>::format(DEKAF2_PREFIX KHTTPVersion(Version).Serialize(), ctx);
	}
};

template <>
struct formatter<DEKAF2_PREFIX KHTTPVersion> : formatter<string_view>
{
	template <typename FormatContext>
	auto format(const DEKAF2_PREFIX KHTTPVersion& Version, FormatContext& ctx) const
	{
		return formatter<string_view>::format(Version.Serialize(), ctx);
	}
};

} // end of namespace DEKAF2_FORMAT_NAMESPACE
