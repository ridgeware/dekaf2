/*
 //
 // DEKAF(tm): Lighter, Faster, Smarter(tm)
 //
 // Copyright (c) 2017, Ridgeware, Inc.
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
 //
 */

#include "kcompression.h"
#include "klog.h"
#include "kfilesystem.h"
#ifdef DEKAF2_HAS_LIBZSTD
#include "ksystem.h" // for kGetCPUCount()
#endif
#include "bits/kiostreams_filters.h"

namespace dekaf2 {

//-----------------------------------------------------------------------------
detail::KCompressionBase::COMPRESSION detail::KCompressionBase::FromString(KStringView sCompression)
//-----------------------------------------------------------------------------
{
	switch (sCompression.Hash())
	{
		case "gz"_hash:
		case "gzip"_hash:
		case "tgz"_hash:
			return GZIP;

		case "bz2"_hash:
		case "bzip2"_hash:
		case "tbz2"_hash:
		case "tbz"_hash:
			return BZIP2;

		case "zlib"_hash:
		case "z"_hash:
		case "tz"_hash:
			return ZLIB;

#ifdef DEKAF2_HAS_LIBLZMA
		case "xz"_hash:   // this is lzma v2, the native format of our LZMA implementation
		case "lzma"_hash: // lzma v1 should be uncompressed by LZMA v2, TBC
			return LZMA;
#endif

#ifdef DEKAF2_HAS_LIBZSTD
		case "zstd"_hash:
		case "zst"_hash:
		case "lz4"_hash: // zstd can uncompress lz4 inputs
			return ZSTD;
#endif

#ifdef DEKAF2_HAS_LIBBROTLI
		case "br"_hash:
		case "brotli"_hash:
			return BROTLI;
#endif
	}

	return NONE;

} // FromString

//-----------------------------------------------------------------------------
KStringView detail::KCompressionBase::ToString(COMPRESSION comp)
//-----------------------------------------------------------------------------
{
	switch (comp)
	{
		case AUTO:
		case NONE:
			return ""_ksv;

		case ZLIB:
			return "zlib"_ksv;

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
		case LZMA:
			return "lzma"_ksv;
#endif
	}

	return ""_ksv;

} // ToString

//-----------------------------------------------------------------------------
KStringView detail::KCompressionBase::DefaultExtension(COMPRESSION comp)
//-----------------------------------------------------------------------------
{
	switch (comp)
	{
		case AUTO:
		case NONE:
			return ""_ksv;

		case ZLIB:
			return "z"_ksv;

		case GZIP:
			return "gz"_ksv;

		case BZIP2:
			return "bz2"_ksv;

#ifdef DEKAF2_HAS_LIBZSTD
		case ZSTD:
			return "zst"_ksv;
#endif
#ifdef DEKAF2_HAS_LIBBROTLI
		case BROTLI:
			return "br"_ksv;
#endif
#ifdef DEKAF2_HAS_LIBLZMA
		case LZMA:
			return "xz"_ksv;
#endif
	}

	return ""_ksv;

} // DefaultExtension

//-----------------------------------------------------------------------------
detail::KCompressionBase::COMPRESSION detail::KCompressionBase::GetCompressionMethodFromFilename(KStringView sFilename)
//-----------------------------------------------------------------------------
{
	return FromString(kExtension(sFilename).ToLowerASCII());

} // GetCompressionMethodFromFilename

//-----------------------------------------------------------------------------
uint16_t detail::KCompressionBase::ScaleLevel(uint16_t iLevel, uint16_t iMax)
//-----------------------------------------------------------------------------
{
	iLevel = (iLevel > 100) ? 100 : iLevel;
	uint16_t iScaled = iMax * iLevel / 100;
	iScaled += (iScaled) ? 0 : 1;
	return iScaled;

} // ScaleLevel

//-----------------------------------------------------------------------------
bool KCompressOStream::open(KOutStream& TargetStream, COMPRESSION compression, uint16_t iLevel, uint16_t iMultiThreading, std::size_t iDataSize)
//-----------------------------------------------------------------------------
{
	m_TargetStream = &TargetStream;
	return CreateFilter(compression, iLevel, iMultiThreading, iDataSize);

} // Open

//-----------------------------------------------------------------------------
bool KCompressOStream::open(KStringRef& sTarget, COMPRESSION compression, uint16_t iLevel, uint16_t iMultiThreading, std::size_t iDataSize)
//-----------------------------------------------------------------------------
{
	m_KOutStream = std::make_unique<KOutStringStream>(sTarget);
	return open(*m_KOutStream, compression, iLevel, iMultiThreading, iDataSize);

} // open

//-----------------------------------------------------------------------------
bool KCompressOStream::open_file(KStringViewZ sOutFile, COMPRESSION compression, uint16_t iLevel, uint16_t iMultiThreading, std::size_t iDataSize)
//-----------------------------------------------------------------------------
{
	m_KOutStream = std::make_unique<KOutFile>(sOutFile, std::ios::trunc);
	compression  = (compression == AUTO) ? GetCompressionMethodFromFilename(sOutFile) : compression;
	return open(*m_KOutStream, compression, iLevel, iMultiThreading, iDataSize);

} // open

//-----------------------------------------------------------------------------
bool KCompressOStream::CreateFilter(COMPRESSION compression, uint16_t iLevel, uint16_t iMultiThreading, std::size_t iDataSize)
//-----------------------------------------------------------------------------
{
	compressor::reset();

	if (!m_TargetStream)
	{
		kWarning("no output stream");
		return false;
	}

	switch (compression)
	{
		case AUTO:
		case NONE:
			break;

		case GZIP:
		{
			auto iScaled = iLevel ? ScaleLevel(iLevel, boost::iostreams::gzip::best_compression) : boost::iostreams::gzip::default_compression;
			kDebug(2, "setting {} compression to level {}", "gzip", iScaled);
			compressor::push(boost::iostreams::gzip_compressor(boost::iostreams::gzip_params(iScaled)));
		}
		break;

		case BZIP2:
		{
			auto iScaled = iLevel ? ScaleLevel(iLevel, 9) : boost::iostreams::bzip2::default_block_size;
			kDebug(2, "setting {} compression to level {}", "bzip2", iScaled);
			compressor::push(boost::iostreams::bzip2_compressor(iScaled));
		}
		break;

		case ZLIB:
		{
			auto iScaled = iLevel ? ScaleLevel(iLevel, boost::iostreams::zlib::best_compression) : boost::iostreams::zlib::default_compression;
			kDebug(2, "setting {} compression to level {}", "zlib", iScaled);
			compressor::push(boost::iostreams::zlib_compressor(boost::iostreams::zlib_params(iScaled)));
		}
		break;

#ifdef DEKAF2_HAS_LIBLZMA
		case LZMA:
		{
			auto iScaled = iLevel ? ScaleLevel(iLevel, lzmaiostreams::lzma::best_compression) : lzmaiostreams::lzma::default_compression;
			kDebug(2, "setting {} compression to level {}", "lzma", iScaled);
			compressor::push(lzmaiostreams::lzma_compressor(lzmaiostreams::lzma_params(iScaled)));
		}
		break;
#endif

#ifdef DEKAF2_HAS_LIBZSTD
		case ZSTD:
		{
			auto iScaled = iLevel ? ScaleLevel(iLevel, dekaf2::iostreams::zstd::best_compression) : dekaf2::iostreams::zstd::default_compression;
			kDebug(2, "setting {} compression to level {}", "zstd", iScaled);
			auto iCPUCount = kGetCPUCount();

			if (iMultiThreading == 0 || iMultiThreading > iCPUCount)
			{
				iMultiThreading = iCPUCount;
			}

			if (iDataSize != npos)
			{
				// each thread works on at least 512k, therefore do not spawn more than
				// iDataSize / 512k threads..
				// for sizes < 512k run in the main thread (=0)
				auto iMaxThreads = iDataSize / (512 * 1024);

				if (iMaxThreads < iMultiThreading)
				{
					iMultiThreading = iMaxThreads;
				}
			}
			kDebug(2, "setting zstd compression to {} parallel threads", iMultiThreading);
			compressor::push(dekaf2::iostreams::zstd_compressor(dekaf2::iostreams::zstd_params(iScaled, iMultiThreading)));
		}
		break;
#endif

#ifdef DEKAF2_HAS_LIBBROTLI
		case BROTLI:
		{
			auto iScaled = iLevel ? ScaleLevel(iLevel, dekaf2::iostreams::brotli::best_compression) : dekaf2::iostreams::brotli::default_compression;
			kDebug(2, "setting {} to level {}", "brotli", iScaled);
			compressor::push(dekaf2::iostreams::brotli_compressor(dekaf2::iostreams::brotli_params(iScaled)));
		}
		break;
#endif
	}

	compressor::push(m_TargetStream->ostream());

	return true;

} // CreateFilter

//-----------------------------------------------------------------------------
void KCompressOStream::close()
//-----------------------------------------------------------------------------
{
	compressor::reset();
	m_KOutStream.reset();

} // close

//-----------------------------------------------------------------------------
bool KUnCompressIStream::open(KInStream& SourceStream, COMPRESSION compression)
//-----------------------------------------------------------------------------
{
	m_SourceStream = &SourceStream;
	return CreateFilter(compression);

} // open

//-----------------------------------------------------------------------------
bool KUnCompressIStream::open(KStringView sSource, COMPRESSION compression)
//-----------------------------------------------------------------------------
{
	m_KInStream = std::make_unique<KInStringStream>(sSource);
	return open(*m_KInStream, compression);

} // open

//-----------------------------------------------------------------------------
bool KUnCompressIStream::open_file(KStringViewZ sInFile, COMPRESSION compression)
//-----------------------------------------------------------------------------
{
	m_KInStream = std::make_unique<KInFile>(sInFile);
	compression = (compression == AUTO) ? GetCompressionMethodFromFilename(sInFile) : compression;
	return open(*m_KInStream, compression);

} // open_file

//-----------------------------------------------------------------------------
bool KUnCompressIStream::CreateFilter(COMPRESSION compression)
//-----------------------------------------------------------------------------
{
	uncompressor::reset();

	if (!m_SourceStream)
	{
		kWarning("no input stream");
		return false;
	}

	switch (compression)
	{
		case AUTO:
		case NONE:
			break;

		case GZIP:
			uncompressor::push(boost::iostreams::gzip_decompressor());
			break;

		case BZIP2:
			uncompressor::push(boost::iostreams::bzip2_decompressor());
			break;

		case ZLIB:
			uncompressor::push(boost::iostreams::zlib_decompressor());
			break;

#ifdef DEKAF2_HAS_LIBLZMA
		case LZMA:
			uncompressor::push(lzmaiostreams::lzma_decompressor());
			break;
#endif

#ifdef DEKAF2_HAS_LIBZSTD
		case ZSTD:
			uncompressor::push(dekaf2::iostreams::zstd_decompressor());
			break;
#endif

#ifdef DEKAF2_HAS_LIBBROTLI
		case BROTLI:
			uncompressor::push(dekaf2::iostreams::brotli_decompressor());
			break;
#endif
	}

	uncompressor::push(m_SourceStream->istream());

	return true;

} // CreateFilter

//-----------------------------------------------------------------------------
void KUnCompressIStream::close()
//-----------------------------------------------------------------------------
{
	uncompressor::reset();
	m_KInStream.reset();

} // close

} // end of namespace dekaf2


