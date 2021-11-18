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

	using compressor = boost::iostreams::filtering_ostream;
	using uncompressor = boost::iostreams::filtering_istream;

	enum COMPRESSION
	{
		NONE,
		GZIP,
		BZIP2,
		ZLIB,
		AUTO
	};

	static COMPRESSION GetCompressionMethodFromFilename(KStringView sFilename);

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
	KCompressOStream(KString& sTarget, COMPRESSION compression)
	{
		open(sTarget, compression);
	}
	/// constructs a compressor with a KOutStream as the target
	KCompressOStream(KOutStream& TargetStream, COMPRESSION compression)
	{
		open(TargetStream, compression);
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
	bool open(KString& sTarget, COMPRESSION compression);
	/// sets a KOutStream as the target
	bool open(KOutStream& TargetStream, COMPRESSION compression);

	/// closes the output stream, calls finalizers of
	/// encoders (also done by destructor)
	void close();

//------
private:
//------

	bool CreateFilter(COMPRESSION compression);

	KOutStream* m_TargetStream { nullptr };
	std::unique_ptr<KOutStringStream> m_KOutStringStream;

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
	KUnCompressIStream(KStringView sSource, COMPRESSION compression)
	{
		open(sSource, compression);
	}
	/// constructs an uncompressor with a KInStream as the source
	KUnCompressIStream(KInStream& SourceStream, COMPRESSION compression)
	{
		open(SourceStream, compression);
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

	/// closes the input stream, calls finalizers of
	/// decoders (also done by destructor)
	void close();

//------
private:
//------

	bool CreateFilter(COMPRESSION compression);

	KInStream* m_SourceStream { nullptr };
	std::unique_ptr<KInStringStream> m_KInStringStream;

}; // KUnCompressIStream


// the reader / writer typedefs for KCompress / KUnCompress

using KCompress = KWriter<KCompressOStream>;
using KUnCompress = KReader<KUnCompressIStream>;


//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// GZIP compressor
class DEKAF2_PUBLIC KGZip : public KCompress
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
//------
public:
//------

	template<class... Args>
	KGZip(Args&&... args)
	: KCompress(std::forward<Args>(args)..., GZIP)
	{
	}

	template<class... Args>
	bool open(Args&&... args)
	{
		return KCompress::open(std::forward<Args>(args)..., GZIP);
	}

}; // KGZip

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// GZIP uncompressor
class DEKAF2_PUBLIC KUnGZip : public KUnCompress
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
//------
public:
//------

	template<class... Args>
	KUnGZip(Args&&... args)
	: KUnCompress(std::forward<Args>(args)..., GZIP)
	{
	}

	template<class... Args>
	bool open(Args&&... args)
	{
		return KUnCompress::open(std::forward<Args>(args)..., GZIP);
	}

}; // KUnGZip

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// BZIP2 compressor
class DEKAF2_PUBLIC KBZip2 : public KCompress
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
//------
public:
//------

	template<class... Args>
	KBZip2(Args&&... args)
	: KCompress(std::forward<Args>(args)..., BZIP2)
	{
	}

	template<class... Args>
	bool open(Args&&... args)
	{
		return KCompress::open(std::forward<Args>(args)..., BZIP2);
	}


}; // KBZip2

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// BZIP2 uncompressor
class DEKAF2_PUBLIC KUnBZip2 : public KUnCompress
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
//------
public:
//------

	template<class... Args>
	KUnBZip2(Args&&... args)
	: KUnCompress(std::forward<Args>(args)..., BZIP2)
	{
	}

	template<class... Args>
	bool open(Args&&... args)
	{
		return KUnCompress::open(std::forward<Args>(args)..., BZIP2);
	}

}; // KUnBZip2

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// ZLIB compressor
class DEKAF2_PUBLIC KZlib : public KCompress
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
//------
public:
//------

	template<class... Args>
	KZlib(Args&&... args)
	: KCompress(std::forward<Args>(args)..., ZLIB)
	{
	}

	template<class... Args>
	bool open(Args&&... args)
	{
		return KCompress::open(std::forward<Args>(args)..., ZLIB);
	}

}; // KZlib

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// ZLIB uncompressor
class DEKAF2_PUBLIC KUnZlib : public KUnCompress
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
//------
public:
//------

	template<class... Args>
	KUnZlib(Args&&... args)
	: KUnCompress(std::forward<Args>(args)..., ZLIB)
	{
	}

	template<class... Args>
	bool open(Args&&... args)
	{
		return KUnCompress::open(std::forward<Args>(args)..., ZLIB);
	}

}; // KUnZlib


} // end of namespace dekaf2

