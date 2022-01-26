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

#pragma once

/// @file kcompression.h
/// Compression framework

#include "kconfiguration.h"
#include "kstream.h"
#include "kinstringstream.h"
#include "koutstringstream.h"

#include <boost/iostreams/filtering_stream.hpp>

namespace dekaf2 {

namespace detail {

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC KCompressionBase
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
//------
public:
//------

	using compressor   = boost::iostreams::filtering_ostream;
	using uncompressor = boost::iostreams::filtering_istream;

	enum COMPRESSION
	{
		NONE,
		GZIP,
		BZIP2,
		ZLIB,
#ifdef DEKAF2_HAS_LIBLZMA
		LZMA,
#endif
#ifdef DEKAF2_HAS_LIBZSTD
		ZSTD,
#endif
#ifdef DEKAF2_HAS_LIBBROTLI
		BROTLI,
#endif
		AUTO
	};

	/// return the COMPRESSION enum for a given compression name or suffix
	static COMPRESSION FromString(KStringView sCompression);
	/// return the compression name for a COMPRESSION enum
	static KStringView ToString(COMPRESSION comp);
	/// return the COMPRESSION enum from a filename suffix (isolates suffix)
	static COMPRESSION GetCompressionMethodFromFilename(KStringView sFilename);
	/// return the default extension for a COMPRESSION enum
	static KStringView DefaultExtension(COMPRESSION comp);
	/// translate a percent compression level into a step level with iMax as the maximum for 100%
	static uint16_t ScaleLevel(uint16_t iLevel, uint16_t iMax);

};

} // end of namespace detail

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// KCompressOStream is a std::ostream which allows to compress in and out of strings and streams.
/// It is possible to construct the class without output target, in which case
/// any attempt to compress will fail until an output is set.
class DEKAF2_PUBLIC KCompressOStream : public detail::KCompressionBase, public detail::KCompressionBase::compressor
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
//------
public:
//------

	/// constructs a compressor without target writer - set one with SetOutput
	/// before attempting to write to it
	KCompressOStream() = default;
	/// constructs a compressor with a KString as the target
	/// @param sTarget string to write the compressed data into
	/// @param compression one of the compression methods: GZIP, ZLIB, BZIP2, ZSTD, LZMA, BROTLI
	/// @param iLevel sets the compression level from 1..100%, 0 = default
	/// @param iMultiThreading (only for ZSTD compression): 0 = #CPU (default), n = n parallel threads
	/// @param iDataSize hint for file size, may be defaulted at npos
	KCompressOStream(KString& sTarget, COMPRESSION compression, uint16_t iLevel = 0, uint16_t iMultiThreading = 0, std::size_t iDataSize = npos)
	{
		open(sTarget, compression, iLevel, iMultiThreading, iDataSize);
	}
	/// constructs a compressor with a KOutStream as the target
	/// @param TargetStream stream to write the compressed data into
	/// @param compression one of the compression methods: GZIP, ZLIB, BZIP2, ZSTD, LZMA, BROTLI
	/// @param iLevel sets the compression level from 1..100%, 0 = default
	/// @param iMultiThreading (only for ZSTD compression): 0 = #CPU (default), n = n parallel threads
	/// @param iDataSize hint for file size, may be defaulted at npos
	KCompressOStream(KOutStream& TargetStream, COMPRESSION compression, uint16_t iLevel = 0, uint16_t iMultiThreading = 0, std::size_t iDataSize = npos)
	{
		open(TargetStream, compression, iLevel, iMultiThreading, iDataSize);
	}
	/// constructs a compressor with a file as the target, compression is deduced by the file's suffix, multithreading is #CPU
	/// @param sTarget name of a file to write the compressed data into
	/// @param iLevel sets the compression level from 1..100%, 0 = default
	/// @param iMultiThreading (only for ZSTD compression): 0 = #CPU (default), n = n parallel threads
	/// @param iDataSize hint for file size, may be defaulted at npos
	KCompressOStream(KString& sTarget, uint16_t iLevel = 0, uint16_t iMultiThreading = 0, std::size_t iDataSize = npos)
	{
		open_file(sTarget, AUTO, iLevel, iMultiThreading, iDataSize);
	}
	/// copy construction is deleted
	KCompressOStream(const KCompressOStream&) = delete;
	/// move construction is deleted
	KCompressOStream(KCompressOStream&&) = delete;
	~KCompressOStream()
	{
		close();
	}

	/// sets a KString as the target
	/// @param sTarget string to write the compressed data into
	/// @param compression one of the compression methods: GZIP, ZLIB, BZIP2, ZSTD, LZMA, BROTLI
	/// @param iLevel sets the compression level from 1..100%, 0 = default
	/// @param iMultiThreading (only for ZSTD compression): 0 = #CPU (default), n = n parallel threads
	bool open(KString& sTarget, COMPRESSION compression, uint16_t iLevel = 0, uint16_t iMultiThreading = 0, std::size_t iDataSize = npos);
	/// sets a KOutStream as the target
	/// @param TargetStream stream to write the compressed data into
	/// @param compression one of the compression methods: GZIP, ZLIB, BZIP2, ZSTD, LZMA, BROTLI
	/// @param iLevel sets the compression level from 1..100%, 0 = default
	/// @param iMultiThreading (only for ZSTD compression): 0 = #CPU (default), n = n parallel threads
	/// @param iDataSize hint for file size, may be defaulted at npos
	bool open(KOutStream& TargetStream, COMPRESSION compression, uint16_t iLevel = 0, uint16_t iMultiThreading = 0, std::size_t iDataSize = npos);
	/// creates a file and sets it as the target
	/// @param sTarget name of a file to write the compressed data into
	/// @param compression one of the compression methods: GZIP, ZLIB, BZIP2, ZSTD, LZMA, BROTLI
	/// @param iLevel sets the compression level from 1..100%, 0 = default
	/// @param iMultiThreading (only for ZSTD compression): 0 = #CPU (default), n = n parallel threads
	/// @param iDataSize hint for file size - if default the file size will be checked by open_file if important for the compression algorithm
	bool open_file(KStringViewZ sOutFile, COMPRESSION compression = AUTO, uint16_t iLevel = 0, uint16_t iMultiThreading = 0, std::size_t iDataSize = npos);

	/// closes the output stream, calls finalizers of
	/// encoders (also done by destructor)
	void close();

//------
private:
//------

	/// Create the compression filter
	/// @param compression one of the compression methods: GZIP, ZLIB, BZIP2, ZSTD, LZMA, BROTLI
	/// @param iLevel sets the compression level from 1..100%, 0 = default
	/// @param iMultiThreading (only for ZSTD compression): 0 = #CPU (default), 1 = off, n = n parallel threads
	bool CreateFilter(COMPRESSION compression, uint16_t iLevel = 0, uint16_t iMultiThreading = 0, std::size_t iDataSize = npos);

	KOutStream* m_TargetStream { nullptr };
	std::unique_ptr<KOutStream> m_KOutStream;

}; // KCompressOStream


//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// KUnCompressIStream is a std::istream that allows to decompress in and out of strings and streams.
/// It is possible to construct the class without input source, in which case
/// any attempt to decompress will fail until an input is set.
class DEKAF2_PUBLIC KUnCompressIStream : public detail::KCompressionBase, public detail::KCompressionBase::uncompressor
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
//------
public:
//------

	/// constructs an uncompressor without target writer - set one with open()
	/// before attempting to read from it
	KUnCompressIStream() = default;
	/// constructs an uncompressor with a KStringView as the source
	/// @param sSource string to read the compressed data from
	/// @param compression one of the compression methods: GZIP, ZLIB, BZIP2, ZSTD, LZMA, BROTLI
	KUnCompressIStream(KStringView sSource, COMPRESSION compression)
	{
		open(sSource, compression);
	}
	/// constructs an uncompressor with a KInStream as the source
	/// @param SourceStream stream to read the compressed data from
	/// @param compression one of the compression methods: GZIP, ZLIB, BZIP2, ZSTD, LZMA, BROTLI
	KUnCompressIStream(KInStream& SourceStream, COMPRESSION compression)
	{
		open(SourceStream, compression);
	}
	/// constructs an uncompressor with a file as the source, compression type is deduced by the file's suffix
	/// @param sInFile name of a file to read the compressed data from
	KUnCompressIStream(KStringViewZ sInFile)
	{
		open_file(sInFile, AUTO);
	}
	/// copy construction is deleted
	KUnCompressIStream(const KUnCompressIStream&) = delete;
	/// move construction is deleted
	KUnCompressIStream(KUnCompressIStream&&) = delete;
	~KUnCompressIStream()
	{
		close();
	}

	/// sets a KStringView as the source
	bool open(KStringView sSource, COMPRESSION compression);
	/// sets a KInStream as the source
	bool open(KInStream& SourceStream, COMPRESSION compression);
	/// opens a file
	bool open_file(KStringViewZ sInFile, COMPRESSION compression = AUTO);

	/// closes the input stream, calls finalizers of
	/// decoders (also done by destructor)
	void close();

//------
private:
//------

	/// Create the uncompression filter
	/// @param compression one of the compression methods: GZIP, ZLIB, BZIP2, ZSTD, LZMA, BROTLI
	bool CreateFilter(COMPRESSION compression);

	KInStream* m_SourceStream { nullptr };
	std::unique_ptr<KInStream> m_KInStream;

}; // KUnCompressIStream


// the reader / writer typedefs for KCompress / KUnCompress

using KCompress = KWriter<KCompressOStream>;
using KUnCompress = KReader<KUnCompressIStream>;

namespace detail {

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
template<KCompressionBase::COMPRESSION compression>
class DEKAF2_PUBLIC KOneComp : public KCompress
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
//------
public:
//------

	template<typename Target>
	KOneComp(Target&& target, uint16_t iLevel = 0, uint16_t iMultiThreading = 0, std::size_t iDataSize = npos)
	: KCompress(std::forward<Target>(target), compression, iLevel, iMultiThreading, iDataSize)
	{
	}

	template<typename Target>
	bool open(Target&& target, uint16_t iLevel = 0, uint16_t iMultiThreading = 0, std::size_t iDataSize = npos)
	{
		return KCompress::open(std::forward<Target>(target), compression, iLevel, iMultiThreading, iDataSize);
	}

	bool open_file(KStringViewZ sOutFile, uint16_t iLevel = 0, uint16_t iMultiThreading = 0, std::size_t iDataSize = npos)
	{
		return KCompress::open_file(sOutFile, compression, iLevel, iMultiThreading, iDataSize);
	}

}; // KOneComp

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
template<KCompressionBase::COMPRESSION compression>
class DEKAF2_PUBLIC KOneUnComp : public KUnCompress
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
//------
public:
//------

	template<typename Target>
	KOneUnComp(Target&& target)
	: KUnCompress(std::forward<Target>(target), compression)
	{
	}

	template<typename Target>
	bool open(Target&& target)
	{
		return KUnCompress::open(std::forward<Target>(target), compression);
	}

	bool open_file(KStringViewZ sInFile)
	{
		return KUnCompress::open_file(sInFile, compression);
	}

}; // KOneUnComp

} // of namespace detail

using KGZip     = detail::KOneComp<detail::KCompressionBase::GZIP>;
using KUnGZip   = detail::KOneUnComp<detail::KCompressionBase::GZIP>;
using KBZip2    = detail::KOneComp<detail::KCompressionBase::BZIP2>;
using KUnBZip2  = detail::KOneUnComp<detail::KCompressionBase::BZIP2>;
using KZlib     = detail::KOneComp<detail::KCompressionBase::ZLIB>;
using KUnZlib   = detail::KOneUnComp<detail::KCompressionBase::ZLIB>;
#ifdef DEKAF2_HAS_LIBLZMA
using KLZMA     = detail::KOneComp<detail::KCompressionBase::LZMA>;
using KUnLZMA   = detail::KOneUnComp<detail::KCompressionBase::LZMA>;
#endif
#ifdef DEKAF2_HAS_LIBZSTD
using KZSTD     = detail::KOneComp<detail::KCompressionBase::ZSTD>;
using KUnZSTD   = detail::KOneUnComp<detail::KCompressionBase::ZSTD>;
#endif
#ifdef DEKAF2_HAS_LIBBROTLI
using KBrotli   = detail::KOneComp<detail::KCompressionBase::BROTLI>;
using KUnBrotli = detail::KOneUnComp<detail::KCompressionBase::BROTLI>;
#endif

} // end of namespace dekaf2

