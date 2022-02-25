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

#include "khttpcompression.h"

namespace dekaf2 {

//-----------------------------------------------------------------------------
KHTTPCompression::COMP KHTTPCompression::FromString(KStringView sCompression)
//-----------------------------------------------------------------------------
{
	switch (sCompression.Hash())
	{
#ifdef DEKAF2_HAS_LIBZSTD
		case "zstd"_hash:
			return ZSTD;
#endif
#ifdef DEKAF2_HAS_LIBBROTLI
		case "br"_hash:
			return BROTLI;
#endif
#ifdef DEKAF2_HAS_LIBLZMA
		case "xz"_hash:
			return XZ;

		case "lzma"_hash:
			return LZMA;
#endif
		case "deflate"_hash:
			return ZLIB;

		case "gzip"_hash:
		case "x-gzip"_hash:
			return GZIP;

		case "bzip2"_hash:
			return BZIP2;

		default:
			return NONE;
	}

	return NONE;

} // FromString

//-----------------------------------------------------------------------------
void KHTTPCompression::Parse(KStringView sCompression)
//-----------------------------------------------------------------------------
{
	m_Compression = FromString(sCompression);

	if (m_Compression == NONE && sCompression.contains(','))
	{
		m_Compression = GetBestSupportedCompression(sCompression);
	}

} // Parse

//-----------------------------------------------------------------------------
void KHTTPCompression::Parse(const KHTTPHeaders& Headers)
//-----------------------------------------------------------------------------
{
	Parse(Headers.Headers.Get(KHTTPHeader::CONTENT_ENCODING));

} // Parse

//-----------------------------------------------------------------------------
KHTTPCompression::COMP KHTTPCompression::GetBestSupportedCompression(KStringView sCompressors)
//-----------------------------------------------------------------------------
{
	COMP Compression = NONE;

	// check the client's request headers for accepted compression encodings
	const auto Compressors = sCompressors.Split(",;");

	for (const auto sCompressor : Compressors)
	{
		auto NewComp = FromString(sCompressor);

		if (NewComp < Compression && (NewComp & s_PermittedCompressors) > 0)
		{
			Compression = NewComp;
		}
	}

	return Compression;

} // GetBestSupportedCompression

//-----------------------------------------------------------------------------
KHTTPCompression::COMP KHTTPCompression::GetBestSupportedCompression(const KHTTPHeaders& Headers)
//-----------------------------------------------------------------------------
{
	return GetBestSupportedCompression(Headers.Headers.Get(KHTTPHeader::ACCEPT_ENCODING));

} // GetBestSupportedCompression

//-----------------------------------------------------------------------------
KStringView KHTTPCompression::ToString(COMP comp)
//-----------------------------------------------------------------------------
{
	switch (comp)
	{
		case NONE:
		case ALL:
			return ""_ksv;

		case ZLIB:
			return "deflate"_ksv;

		case GZIP:
			return "gzip"_ksv;

		case BZIP2:
			return "bzip2"_ksv;

#ifdef DEKAF2_HAS_LIBZSTD
		case ZSTD:
			return "zstd"_ksv;
#endif
#ifdef DEKAF2_HAS_LIBBROTLI
		case BROTLI:
			return "br"_ksv;
#endif
#ifdef DEKAF2_HAS_LIBLZMA
		case XZ:
			return "xz"_ksv;

		case LZMA:
			return "lzma"_ksv;
#endif
	}

	return ""_ksv;

} // ToString

//-----------------------------------------------------------------------------
void KHTTPCompression::SetPermittedCompressors(KStringView sCompressors)
//-----------------------------------------------------------------------------
{
	COMP Compressors { COMP::NONE };

	for (auto& sCompressor : sCompressors.Split(",;"))
	{
		Compressors |= FromString(sCompressor);
	}

	SetPermittedCompressors(Compressors);

} // SetPermittedCompressors

KHTTPCompression::COMP KHTTPCompression::s_PermittedCompressors { COMP::ALL };

} // of namespace dekaf2
