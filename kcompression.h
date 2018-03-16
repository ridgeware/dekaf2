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

#include <boost/iostreams/filtering_streambuf.hpp>


namespace dekaf2 {

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// KCompressBase gives the interface for all compression algorithms. The
/// framework allows to compress / decompress in and out of strings and streams.
/// It is possible to construct the class without output target, in which case
/// any attempt to compress / decompress will fail until an output is set. As
/// KCompressBase is an Abstract Base Class it can be used as the placeholder
/// for any real compression class derived from it.
class KCompressBase
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
//------
public:
//------

	/// constructs a compressor without target writer - set one with SetOutput
	/// before attempting to write to it
	KCompressBase();
	/// constructs a compressor with a KString as the target
	KCompressBase(KString& sTarget);
	/// constructs a compressor with a std::ostream as the target
	KCompressBase(std::ostream& TargetStream);
	/// constructs a compressor with a KOutStream as the target
	KCompressBase(KOutStream& TargetStream);
	/// copy construction is deleted
	KCompressBase(const KCompressBase&) = delete;
	/// move construction is permitted
	KCompressBase(KCompressBase&&) = default;
	virtual ~KCompressBase();


	/// sets a KString as the target
	bool SetOutput(KString& sTarget);
	/// sets a std::ostream as the target
	bool SetOutput(std::ostream& TargetStream);
	/// sets a KOutStream as the target
	bool SetOutput(KOutStream& TargetStream);

	/// writes a string into the compressor
	bool Write(KStringView sInput);
	/// writes a std::istream into the compressor
	bool Write(std::istream& InputStream);
	/// writes a KInStream into the compressor
	bool Write(KInStream& InputStream);

	/// writes a string into the compressor
	KCompressBase& operator+=(KStringView sInput)
	{
		Write(sInput);
		return *this;
	}

//------
protected:
//------

	using streamfilter = boost::iostreams::filtering_streambuf<boost::iostreams::output>;

	virtual void AddFilter(streamfilter& stream) = 0;

//------
private:
//------

	bool CreateFilter();

	std::ostream* m_TargetStream { nullptr };
	std::unique_ptr<KOStringStream> m_KOStringStream;
	std::unique_ptr<streamfilter> m_Filter;

}; // KCompressBase

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KGZip : public KCompressBase
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
//------
public:
//------

	template<class...Args>
	KGZip(Args&&...args)
	: KCompressBase(std::forward<Args>(args)...)
	{}

//------
protected:
//------

	virtual void AddFilter(streamfilter& stream) override;

}; // KGZip

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KGUnZip : public KCompressBase
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
//------
public:
//------

	template<class...Args>
	KGUnZip(Args&&...args)
	: KCompressBase(std::forward<Args>(args)...)
	{}

//------
protected:
//------

	virtual void AddFilter(streamfilter& stream) override;

}; // KGUnZip

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KBZip2 : public KCompressBase
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	//------
public:
	//------

	template<class...Args>
	KBZip2(Args&&...args)
	: KCompressBase(std::forward<Args>(args)...)
	{}

	//------
protected:
	//------

	virtual void AddFilter(streamfilter& stream) override;

}; // KBZip2

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KBUnZip2 : public KCompressBase
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	//------
public:
	//------

	template<class...Args>
	KBUnZip2(Args&&...args)
	: KCompressBase(std::forward<Args>(args)...)
	{}

	//------
protected:
	//------

	virtual void AddFilter(streamfilter& stream) override;

}; // KBUnZip2

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KZlib : public KCompressBase
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	//------
public:
	//------

	template<class...Args>
	KZlib(Args&&...args)
	: KCompressBase(std::forward<Args>(args)...)
	{}

	//------
protected:
	//------

	virtual void AddFilter(streamfilter& stream) override;

}; // KZlib

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KUnZlib : public KCompressBase
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	//------
public:
	//------

	template<class...Args>
	KUnZlib(Args&&...args)
	: KCompressBase(std::forward<Args>(args)...)
	{}

	//------
protected:
	//------

	virtual void AddFilter(streamfilter& stream) override;

}; // KUnZlib


} // end of namespace dekaf2

