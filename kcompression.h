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
#include "kstringstream.h"

#include <boost/iostreams/filtering_stream.hpp>


namespace dekaf2 {

namespace detail {

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KCompressionBase
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
//------
public:
//------

	enum COMPRESSION
	{
		NONE,
		GZIP,
		BZIP2,
		ZLIB,
	};

	/// set compression method (only once..)
	void SetCompressor(COMPRESSION compression)
	{
		if (m_compression == NONE)
		{
			m_compression = compression;
		}
	}

//------
protected:
//------

	COMPRESSION m_compression { NONE };

};

}

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// KCompress gives the interface for all compression algorithms. The
/// framework allows to compress in and out of strings and streams.
/// It is possible to construct the class without output target, in which case
/// any attempt to compress will fail until an output is set.
class KCompress : public detail::KCompressionBase
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
//------
public:
//------

	/// constructs a compressor without target writer - set one with SetOutput
	/// before attempting to write to it
	KCompress() = default;
	/// constructs a compressor with a KString as the target
	KCompress(KString& sTarget)
	{
		Open(sTarget);
	}
	/// constructs a compressor with a std::ostream as the target
	KCompress(std::ostream& TargetStream)
	{
		Open(TargetStream);
	}
	/// constructs a compressor with a KOutStream as the target
	KCompress(KOutStream& TargetStream)
	{
		Open(TargetStream);
	}
	/// copy construction is deleted
	KCompress(const KCompress&) = delete;
	/// move construction is permitted
	KCompress(KCompress&&) = default;
	~KCompress()
	{
		Close();
	}

	/// sets a KString as the target
	bool Open(KString& sTarget);
	/// sets a std::ostream as the target
	bool Open(std::ostream& TargetStream);
	/// sets a KOutStream as the target
	bool Open(KOutStream& TargetStream);

	/// writes a string into the compressor
	bool Write(KStringView sInput);
	/// writes a std::istream into the compressor
	bool Write(std::istream& InputStream);
	/// writes a KInStream into the compressor
	bool Write(KInStream& InputStream);

	/// closes the output stream, calls finalizers of
	/// encoders (also done by destructor)
	void Close();

	/// writes a string into the compressor
	KCompress& operator+=(KStringView sInput)
	{
		Write(sInput);
		return *this;
	}

//------
private:
//------

	using streamfilter = boost::iostreams::filtering_ostream;

	bool CreateFilter();

	std::ostream* m_TargetStream { nullptr };
	std::unique_ptr<KOStringStream> m_KOStringStream;
	std::unique_ptr<streamfilter> m_Filter;

}; // KCompress

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// KUnCompress gives the interface for all uncompression algorithms. The
/// framework allows to decompress in and out of strings and streams.
/// It is possible to construct the class without output target, in which case
/// any attempt to decompress will fail until an output is set.
class KUnCompress : public detail::KCompressionBase
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
//------
public:
//------

	/// constructs an uncompressor without target writer - set one with SetOutput
	/// before attempting to write to it
	KUnCompress() = default;
	/// constructs an uncompressor with a KStringView as the source
	KUnCompress(KStringView sSource)
	{
		Open(sSource);
	}
	/// constructs an uncompressor with a std::istream as the source
	KUnCompress(std::istream& SourceStream)
	{
		Open(SourceStream);
	}
	/// constructs an uncompressor with a KInStream as the source
	KUnCompress(KInStream& SourceStream)
	{
		Open(SourceStream);
	}
	/// copy construction is deleted
	KUnCompress(const KUnCompress&) = delete;
	/// move construction is permitted
	KUnCompress(KUnCompress&&) = default;
	~KUnCompress()
	{
		Close();
	}

	/// sets a KStringView as the source
	bool Open(KStringView sSource);
	/// sets a std::istream as the source
	bool Open(std::istream& SourceStream);
	/// sets a KInStream as the source
	bool Open(KInStream& SourceStream);

	/// reads a string from the uncompressor
	size_t Read(KString& sOutput, size_t len = KString::npos);
	/// reads a std::ostream from the uncompressor
	size_t Read(std::ostream& OutputStream, size_t len = KString::npos);
	/// reads a KOutStream from the uncompressor
	size_t Read(KOutStream& OutputStream, size_t len = KString::npos);

	/// closes the input stream, calls finalizers of
	/// decoders (also done by destructor)
	void Close();

//------
private:
//------

	using streamfilter = boost::iostreams::filtering_istream;

	bool CreateFilter();

	std::istream* m_SourceStream { nullptr };
	std::unique_ptr<KIStringStream> m_KIStringStream;
	std::unique_ptr<streamfilter> m_Filter;

}; // KUnCompress

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KGZip : public KCompress
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
//------
public:
//------

	template<class... Args>
	KGZip(Args&&... args)
	: KCompress(std::forward<Args>(args)...)
	{
		SetCompressor(GZIP);
	}

}; // KGZip

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KGUnZip : public KUnCompress
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
//------
public:
//------

	template<class... Args>
	KGUnZip(Args&&... args)
	: KUnCompress(std::forward<Args>(args)...)
	{
		SetCompressor(GZIP);
	}

}; // KGUnZip

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KBZip2 : public KCompress
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
//------
public:
//------

	template<class... Args>
	KBZip2(Args&&... args)
	: KCompress(std::forward<Args>(args)...)
	{
		SetCompressor(BZIP2);
	}

}; // KBZip2

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KBUnZip2 : public KUnCompress
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
//------
public:
//------

	template<class... Args>
	KBUnZip2(Args&&... args)
	: KUnCompress(std::forward<Args>(args)...)
	{
		SetCompressor(BZIP2);
	}

}; // KBUnZip2

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KZlib : public KCompress
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
//------
public:
//------

	template<class... Args>
	KZlib(Args&&... args)
	: KCompress(std::forward<Args>(args)...)
	{
		SetCompressor(ZLIB);
	}

}; // KZlib

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KUnZlib : public KUnCompress
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
//------
public:
//------

	template<class... Args>
	KUnZlib(Args&&... args)
	: KUnCompress(std::forward<Args>(args)...)
	{
		SetCompressor(ZLIB);
	}

}; // KUnZlib


} // end of namespace dekaf2

