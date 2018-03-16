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
class KCompressBase
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
//------
public:
//------

	KCompressBase(KString& sTarget);
	KCompressBase(KOutStream& TargetStream);
	virtual ~KCompressBase();

	bool Write(KStringView sInput);
	bool Write(KInStream& InputStream);

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

	void CreateFilter();

	std::ostream* m_TargetStream;
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

