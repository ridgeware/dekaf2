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

DEKAF2_NAMESPACE_BEGIN

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// The generalized bidirectional stream abstraction for dekaf2
class DEKAF2_PUBLIC KStream : public KInStream, public KOutStream
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
	/// value construct a KStream from separate in- and out-streams
	KStream(KInStream& InStream, KOutStream& OutStream)
	//-----------------------------------------------------------------------------
	: reader_type(InStream)
	, writer_type(OutStream)
	{}

	//-----------------------------------------------------------------------------
	/// value construct a KStream from separate in- and out-streams
	template<class In, class Out>
	KStream(KReader<In>& InStream, KWriter<Out>& OutStream)
	//-----------------------------------------------------------------------------
	: reader_type(InStream.InStream())
	, writer_type(OutStream.OutStream())
	{}

	//-----------------------------------------------------------------------------
	/// copy construction
	KStream(const self_type& other) = default;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// move construct a KStream
	KStream(self_type&& other) = default;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// dtor
	virtual ~KStream();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// assignment operator
	self_type& operator=(const self_type& other) = default;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// move operator
	self_type& operator=(self_type&& other) = default;
	//-----------------------------------------------------------------------------

};

extern KStream KInOut;

//-----------------------------------------------------------------------------
/// return a std::iostream object that reads and writes from/to nothing, but is valid
std::iostream& kGetNullIOStream();
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// return a KStream object that reads and writes from/to nothing, but is valid
KStream& kGetNullStream();
//-----------------------------------------------------------------------------

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// The templatized bidirectional stream abstraction for dekaf2. Can be constructed
/// around any std::iostream.
template<class IOStream>
class DEKAF2_PUBLIC KReaderWriter
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
	// make sure this does not cover the copy or move constructor by requesting an
	// args count of != 1
	template<class... Args,
		typename std::enable_if<
			sizeof...(Args) != 1, int
		>::type = 0
	>
	KReaderWriter(Args&&... args)
	    : base_type(std::forward<Args>(args)...)
	    , k_rw_type(static_cast<base_type&>(*this))
	//-----------------------------------------------------------------------------
	{
	}

	//-----------------------------------------------------------------------------
	// make sure this does not cover the copy or move constructor by requesting the
	// single arg being of a different type than self_type
	template<class Arg,
		typename std::enable_if<
			!std::is_same<
				typename std::decay<Arg>::type, self_type
			>::value, int
		>::type = 0
	>
	KReaderWriter(Arg&& arg)
	    : base_type(std::forward<Arg>(arg))
	    , k_rw_type(static_cast<base_type&>(*this))
	//-----------------------------------------------------------------------------
	{
	}

	//-----------------------------------------------------------------------------
	/// copy constructor is deleted - std::iostream's is, too
	KReaderWriter(const self_type& other) = delete;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	// depending on the iostream type, move construction is allowed
	template<typename T = IOStream, typename std::enable_if<std::is_move_constructible<T>::value == true, int>::type = 0>
	KReaderWriter(KReaderWriter&& other)
	    : base_type(std::move(other))
	    , k_rw_type(std::move(other))
	//-----------------------------------------------------------------------------
	{
		// make sure the pointers in the rw_type point to the moved object
		SetInStream(*this);
		SetOutStream(*this);
	}

	//-----------------------------------------------------------------------------
	// depending on the iostream type, move construction is not allowed
	template<typename T = IOStream, typename std::enable_if<std::is_move_constructible<T>::value == false, int>::type = 0>
	KReaderWriter(KReaderWriter&& other) = delete;
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
	/// move assignment is not allowed
	self_type& operator=(self_type&& other) = delete;
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

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// File stream based on std::fstream
// std::fstream does not understand KString and KStringView, so let's help it
class DEKAF2_PUBLIC KFile : public KReaderWriter<std::fstream>
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
public:

	using base_type = KReaderWriter<std::fstream>;

	//-----------------------------------------------------------------------------
	KFile() = default;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	KFile(KStringViewZ sFilename, ios_base::openmode mode = ios_base::in | ios_base::out)
	    : base_type(kToFilesystemPath(sFilename), mode | ios_base::binary)
	//-----------------------------------------------------------------------------
	{
	}

	//-----------------------------------------------------------------------------
	// depending on the iostream type, move construction is allowed
	template<typename T = base_type, typename std::enable_if<std::is_move_constructible<T>::value == true, int>::type = 0>
	KFile(KFile&& other)
	    : base_type(std::move(other))
	//-----------------------------------------------------------------------------
	{
	}

	//-----------------------------------------------------------------------------
	// depending on the iostream type, move construction is not allowed
	template<typename T = base_type, typename std::enable_if<std::is_move_constructible<T>::value == false, int>::type = 0>
	KFile(KFile&& other) = delete;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	void open(KStringViewZ sFilename, ios_base::openmode mode = ios_base::in | ios_base::out)
	//-----------------------------------------------------------------------------
	{
		base_type::open(kToFilesystemPath(sFilename), mode | ios_base::binary);
	}

};

DEKAF2_NAMESPACE_END

