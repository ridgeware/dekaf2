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

#include "bits/kcppcompat.h"
#include "kconfiguration.h" // for DEKAF2_HAS_LIBLZMA/LIBZSTD ..
#include "kstringview.h"
#include "khttp_header.h"
#include "katomic_object.h"
#include <vector>

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
		ZSTD   = 1 << 0,
#endif
#ifdef DEKAF2_HAS_LIBBROTLI
		BROTLI = 1 << 1,
#endif
		ZLIB   = 1 << 2,
		GZIP   = 1 << 3,
#ifdef DEKAF2_HAS_LIBLZMA
		XZ     = 1 << 4,
		LZMA   = 1 << 7, // do not pick from accept-encoding list
#endif
		BZIP2  = 1 << 5,
		NONE   = 1 << 6,

		ALL    = NONE | BZIP2 | GZIP | ZLIB
#ifdef DEKAF2_HAS_LIBZSTD
		         | ZSTD
#endif
#ifdef DEKAF2_HAS_LIBBROTLI
		         | BROTLI
#endif
#ifdef DEKAF2_HAS_LIBLZMA
		         | XZ    // do not add LZMA to the mask, we do not want to accept it
#endif
	};

	KHTTPCompression() = default;
	/// construct from a comma separated list of compressor names, and pick the best one
	KHTTPCompression(KStringView sCompression)    { Parse(sCompression);            }
	/// construct from KHTTPHeaders, and pick the best compression from the requested ones
	KHTTPCompression(const KHTTPHeaders& Headers) { Parse(Headers);                 }
	/// construct from an explicit compression algorithm
	KHTTPCompression(COMP comp) : m_Compression(comp) {}

	/// assign from a comma separated list of compressor names, and pick the best one
	KHTTPCompression& operator=(KStringView sCompression) { Parse(sCompression); return *this; }

	/// parse comma separated list of compressor names, and pick the best one
	void Parse(KStringView sCompression);
	/// parse from KHTTPHeaders, and pick the best compression from the requested ones
	void Parse(const KHTTPHeaders& Headers);
	/// return selected compression algorithm
	COMP GetCompression() const                   { return m_Compression;           }
	/// set compression algorithm
	void SetCompression(COMP comp)                { m_Compression = comp;           }
	/// return selected compression algorithm as string
	KStringView Serialize() const                 { return ToString(m_Compression); }

	operator COMP() const                         { return m_Compression;           }

	/// returns CSV string with supported HTTP compressors (to be used for ACCEPT_ENCODING)
	static KStringViewZ GetSupportedCompressors();
	/// return best supported HTTP compression from a comma separated list of compressor names
	static COMP         GetBestSupportedCompression(KStringView sCompressors);
	/// return best supported HTTP compression from KHTTPHeaders
	static COMP         GetBestSupportedCompression(const KHTTPHeaders& Headers);
	/// return compression algorithm from name
	static COMP         FromString(KStringView);
	/// return compression name from algorithm
	static KStringView  ToString(COMP comp);
	/// return compression names from algorithms
	static std::vector<KStringView> ToStrings(COMP comp);

	/// set set of permitted compression algorithms
	static void SetPermittedCompressors(COMP Compressors);
	/// set list of permitted compression algorithms from comma separated list of compression names
	static void SetPermittedCompressors(KStringView sCompressors);
	/// return set of permitted compression algorithms
	static COMP GetPermittedCompressors()         { return s_PermittedCompressors;  }

//------
private:
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

	// in theory this should be an atomic type, but the risk of reading the value
	// during an update is very low, and the consequences would only be that a
	// momentarily non-accepted compression is for one time permitted or vice verse
	static COMP                   s_PermittedCompressors;
	static KAtomicObject<KString> s_sPermittedCompressors;

	COMP m_Compression { NONE };

}; // KHTTPCompression

DEKAF2_ENUM_IS_FLAG(KHTTPCompression::COMP)

} // of namespace dekaf2
