/*
 //
 // DEKAF(tm): Lighter, Faster, Smarter (tm)
 //
 // Copyright (c) 2022, Ridgeware, Inc.
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

#include "kconfiguration.h" // for DEKAF2_HAS_LIBLZMA/LIBZSTD ..
#include "kstringview.h"
#include "khttp_header.h"

namespace dekaf2 {

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC KHTTPCompression
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	enum COMP
	{
#ifdef DEKAF2_HAS_LIBZSTD
		ZSTD   = 1,
#endif
#ifdef DEKAF2_HAS_LIBBROTLI
		BROTLI = 2,
#endif
		ZLIB   = 3,
		GZIP   = 4,
#ifdef DEKAF2_HAS_LIBLZMA
		XZ     = 5,
		LZMA   = 8, // do not pick from accept-encoding list
#endif
		BZIP2  = 6,
		NONE   = 7
	};

	KHTTPCompression() = default;
	KHTTPCompression(KStringView sCompression)    { Parse(sCompression);            }
	KHTTPCompression(const KHTTPHeaders& Headers) { Parse(Headers);                 }
	KHTTPCompression(COMP comp) : m_Compression(comp) {}

	KHTTPCompression& operator=(KStringView sCompression) { Parse(sCompression); return *this; }

	void Parse(KStringView sCompression);
	void Parse(const KHTTPHeaders& Headers);
	COMP GetCompression() const                   { return m_Compression;           }
	void SetCompression(COMP comp)                { m_Compression = comp;           }
	KStringView Serialize() const                 { return ToString(m_Compression); }

	operator COMP() const                         { return m_Compression;           }

	/// Returns CSV string with supported HTTP compressors (to be used for ACCEPT_ENCODING)
	constexpr
	static KStringViewZ GetSupportedCompressors() { return s_sSupportedCompressors; }
	static COMP         GetBestSupportedCompression(KStringView sCompressors);
	static COMP         GetBestSupportedCompression(const KHTTPHeaders& Headers);
	static COMP         FromString(KStringView);
	static KStringView  ToString(COMP comp);

//------
protected:
//------

	static constexpr KStringViewZ s_sSupportedCompressors =
#ifdef DEKAF2_HAS_LIBZSTD
															"zstd, "
#endif
#ifdef DEKAF2_HAS_LIBBROTLI
															"br, "
#endif
															"gzip, deflate, "
#ifdef DEKAF2_HAS_LIBLZMA
															"xz, lzma, "
#endif
															"bzip2";

	COMP m_Compression { NONE };

}; // KHTTPCompression

} // of namespace dekaf2
