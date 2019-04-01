/*
//
// DEKAF(tm): Lighter, Faster, Smarter (tm)
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
*/

#pragma once

/// @file kstream.h
/// bidirectional streams

#include "kreader.h"
#include "kwriter.h"

namespace dekaf2
{

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// The generalized bidirectional stream abstraction for dekaf2
class KStream : public KInStream, public KOutStream
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	using self_type   = KStream;
	using reader_type = KInStream;
	using writer_type = KOutStream;

//-------
public:
//-------

	//-----------------------------------------------------------------------------
	/// value construct a KStream
	KStream(std::iostream& Stream)
	//-----------------------------------------------------------------------------
	    : reader_type(Stream)
	    , writer_type(Stream)
	{}

	//-----------------------------------------------------------------------------
	/// value construct a KStream from separate in- and out-streams
	KStream(std::istream& InStream, std::ostream& OutStream)
	//-----------------------------------------------------------------------------
	    : reader_type(InStream)
	    , writer_type(OutStream)
	{}

	//-----------------------------------------------------------------------------
	/// move construct a KStream
	KStream(self_type&& other) = default;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// copy construction is deleted, as with std::iostream
	KStream(self_type& other) = delete;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// dtor
	virtual ~KStream();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// assignment operator is deleted, as with std::iostream
	self_type& operator=(const self_type& other) = delete;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// move operator
	self_type& operator=(self_type&& other) = default;
	//-----------------------------------------------------------------------------

};

extern KStream KInOut;

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// The templatized bidirectional stream abstraction for dekaf2. Can be constructed
/// around any std::iostream.
template<class IOStream>
class KReaderWriter
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
        : public IOStream
        , public KStream
{
	using base_type = IOStream;
	using self_type = KReaderWriter<IOStream>;
	using k_rw_type = KStream;

	static_assert(std::is_base_of<std::iostream, IOStream>::value,
	              "KReaderWriter cannot be derived from a non-std::iostream class");

//-------
public:
//-------

	//-----------------------------------------------------------------------------
	/// perfect forwarding ctor (forwards all arguments to the iostream)
	template<class... Args>
	KReaderWriter(Args&&... args)
	    : base_type(std::forward<Args>(args)...)
	    , k_rw_type(static_cast<base_type&>(*this))
	//-----------------------------------------------------------------------------
	{
	}

	//-----------------------------------------------------------------------------
	/// move construct a KReaderWriter
	KReaderWriter(self_type&& other) = default;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// copy constructor is deleted - std::iostream's is, too
	KReaderWriter(const self_type& other) = delete;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// dtor
	virtual ~KReaderWriter()
	//-----------------------------------------------------------------------------
	{
	}

	//-----------------------------------------------------------------------------
	/// copy assignment is deleted - std::iostring's is, too
	self_type& operator=(const self_type& other) = delete;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// move assignment
	self_type& operator=(self_type&& other) = default;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	// this one is necessary because ios_base has a symbol named end .. (for seeking)
	const_iterator end()
	//-----------------------------------------------------------------------------
	 {
		return KInStream::end();
	 }

};

extern template class KReaderWriter<std::fstream>;

/// File stream based on std::fstream
// std::fstream does not understand KString and KStringView, so let's help it
class KFile : public KReaderWriter<std::fstream>
{
public:

	using base_type = KReaderWriter<std::fstream>;

	//-----------------------------------------------------------------------------
	KFile(KString str, ios_base::openmode mode = ios_base::in | ios_base::out)
	: base_type(kToFilesystemPath(str), mode | ios_base::binary)
	//-----------------------------------------------------------------------------
	{
	}

	//-----------------------------------------------------------------------------
	KFile(KStringViewZ sz, ios_base::openmode mode = ios_base::in | ios_base::out)
	: base_type(kToFilesystemPath(sz), mode | ios_base::binary)
	//-----------------------------------------------------------------------------
	{
	}

	//-----------------------------------------------------------------------------
	KFile(KStringView sv, ios_base::openmode mode = ios_base::in | ios_base::out)
	: KFile(KString(sv), mode | ios_base::binary)
	//-----------------------------------------------------------------------------
	{
	}

#ifndef _MSC_VER
	using base_type::base_type;
#else
	// MSC has issues with perfect forwarding of KReaderWriter and does not catch the
	// KStringView ctor above if we forward all base class constructors
	// therefore we need to declare a few more constructors here
	KFile() = default;
	KFile(KFile&&) = default;
	KFile(const std::string& s, ios_base::openmode mode = ios_base::out)
	: base_type(kToFilesystemPath(s), mode | ios_base::binary) {}
	KFile(const char* sz, ios_base::openmode mode = ios_base::out)
	: base_type(kToFilesystemPath(KStringViewZ(sz)), mode | ios_base::binary) {}
	void open(const std::string& s, ios_base::openmode mode = ios_base::out)
	{ base_type::open(kToFilesystemPath(s), mode | ios_base::binary); }
	// that open is the most bizarre.. MS added an open with an additional int parm,
	// but did not document what it does - we just drop it, but have to support it
	// for Windows compatibility
	void open(const char* sz, ios_base::openmode mode = ios_base::out, int = 0)
	{ base_type::open(kToFilesystemPath(KStringViewZ(sz)), mode | ios_base::binary); }
#endif

	//-----------------------------------------------------------------------------
	void open(const KString& str, ios_base::openmode mode = ios_base::in | ios_base::out)
	//-----------------------------------------------------------------------------
	{
		base_type::open(kToFilesystemPath(str), mode | ios_base::binary);
	}

	//-----------------------------------------------------------------------------
	void open(const KStringViewZ sz, ios_base::openmode mode = ios_base::in | ios_base::out)
	//-----------------------------------------------------------------------------
	{
		base_type::open(kToFilesystemPath(sz), mode | ios_base::binary);
	}

	//-----------------------------------------------------------------------------
	void open(const KStringView sv, ios_base::openmode mode = ios_base::in | ios_base::out)
	//-----------------------------------------------------------------------------
	{
		open(KString(sv), mode | ios_base::binary);
	}

	using base_type::open;

};

} // end of namespace dekaf2

