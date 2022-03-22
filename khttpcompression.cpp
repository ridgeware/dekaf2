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
void KHTTPCompression::Parse(KStringView sCompression, bool bSingleValue)
//-----------------------------------------------------------------------------
{
	m_Compression = FromString(sCompression);

	if (m_Compression == NONE && !bSingleValue && sCompression.contains(','))
	{
		m_Compression = GetBestSupportedCompressor(sCompression);
	}

} // Parse

//-----------------------------------------------------------------------------
void KHTTPCompression::Parse(const KHTTPHeaders& Headers)
//-----------------------------------------------------------------------------
{
	// parse the content-encoding header, and accept only a single value, not a list..
	KStringView sHeader = Headers.Headers.Get(KHTTPHeader::CONTENT_ENCODING);
	// we remove some white space if existing
	sHeader.Trim(" \t");
	Parse(sHeader, true);

} // Parse

//-----------------------------------------------------------------------------
KHTTPCompression::COMP KHTTPCompression::GetBestSupportedCompressor(KStringView sCompressors)
//-----------------------------------------------------------------------------
{
	COMP Compression = NONE;

	// check the client's request headers for accepted compression encodings
	const auto Compressors = sCompressors.Split(",");

	for (auto sCompressor : Compressors)
	{
		// we want to remove the quality value, but are not interested in
		// the actual quality .. we have our own ranking
		KHTTPHeader::GetQualityValue(sCompressor, true);

		auto NewComp = FromString(sCompressor);

		if (NewComp < Compression && (NewComp & s_PermittedCompressors) == NewComp)
		{
			Compression = NewComp;
		}
	}

	return Compression;

} // GetBestSupportedCompressor

//-----------------------------------------------------------------------------
KHTTPCompression::COMP KHTTPCompression::GetBestSupportedCompressor(const KHTTPHeaders& Headers)
//-----------------------------------------------------------------------------
{
	return GetBestSupportedCompressor(Headers.Headers.Get(KHTTPHeader::ACCEPT_ENCODING));

} // GetBestSupportedCompressor

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
std::vector<KStringView> KHTTPCompression::ToStrings(COMP comp)
//-----------------------------------------------------------------------------
{
	std::vector<KStringView> Result;

	using inttype = std::underlying_type<COMP>::type;

	inttype mask = 1;

	for (;;)
	{
		auto iMasked = comp & mask;

		if (iMasked != 0 && iMasked != COMP::NONE)
		{
			Result.push_back(ToString(static_cast<COMP>(iMasked)));
		}

		if (mask >= COMP::ALL)
		{
			break;
		}

		mask <<= 1;
	}

	return Result;

} // ToStrings

//-----------------------------------------------------------------------------
KStringViewZ KHTTPCompression::GetCompressors()
//-----------------------------------------------------------------------------
{
	return (s_PermittedCompressors == COMP::ALL) ? GetImplementedCompressors() : s_sPermittedCompressors.getRef().ToView();

} // GetCompressors

//-----------------------------------------------------------------------------
void KHTTPCompression::SetPermittedCompressors(COMP Compressors)
//-----------------------------------------------------------------------------
{
	if ((Compressors & COMP::ALL) == COMP::ALL)
	{
		// make sure to really only set COMP::ALL flags, not more - that makes
		// it easier to test for the ALL case
		Compressors = COMP::ALL;
		// no need to reset the compressors string - it is not used for ALL
		kDebug(2, "compression reset to ALL: {}", GetImplementedCompressors());
	}
	else
	{
		// build and set the new compressors string
		s_sPermittedCompressors.reset(kJoined(ToStrings(Compressors), ","));
		kDebug(2, "compression set to subset: {}", s_sPermittedCompressors.getRef());
	}

	s_PermittedCompressors = Compressors;

} // SetPermittedCompressors

//-----------------------------------------------------------------------------
void KHTTPCompression::SetPermittedCompressors(KStringView sCompressors)
//-----------------------------------------------------------------------------
{
	COMP Compressors { COMP::NONE };

	for (auto& sCompressor : sCompressors.Split(",;|"))
	{
		Compressors |= FromString(sCompressor);
	}

	SetPermittedCompressors(Compressors);

} // SetPermittedCompressors

KHTTPCompression::COMP KHTTPCompression::s_PermittedCompressors { COMP::ALL };
KAtomicObject<KString> KHTTPCompression::s_sPermittedCompressors { KHTTPCompression::s_sSupportedCompressors };

#ifdef DEKAF2_REPEAT_CONSTEXPR_VARIABLE
constexpr KStringViewZ KHTTPCompression::s_sSupportedCompressors;
#endif

} // of namespace dekaf2
