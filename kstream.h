/*
//-----------------------------------------------------------------------------//
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

#include "kreader.h"
#include "kwriter.h"


namespace dekaf2
{


/// The generalized bidirectional stream abstraction for dekaf2
class KStream : public KInStream, public KOutStream
{
	using self_type   = KStream;
	using reader_type = KInStream;
	using writer_type = KOutStream;

//-------
public:
//-------

	//-----------------------------------------------------------------------------
	/// move construct a KReaderWriter
	KStream(self_type&& other) noexcept
	//-----------------------------------------------------------------------------
		: reader_type(std::move(other))
		, writer_type(std::move(other))
	{}

	//-----------------------------------------------------------------------------
	/// copy construction is deleted, as with std::iostream
	KStream(self_type& other) = delete;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// value construct a KStream
	KStream(std::iostream& Stream)
	//-----------------------------------------------------------------------------
	    : reader_type(Stream)
	    , writer_type(Stream)
	{}

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
	self_type& operator=(self_type&& other)
	//-----------------------------------------------------------------------------
	{
		reader_type::operator=(std::move(other));
		writer_type::operator=(std::move(other));
		return *this;
	}

};

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// The templatized bidirectional stream abstraction for dekaf2. Can be constructed around any
/// std::iostream.
template<class IOStream>
class KReaderWriter
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
        : public IOStream
        , public KStream
{
	using base_type = IOStream;
	using self_type = KReaderWriter<IOStream>;
	using k_rw_type = KStream;

//-------
public:
//-------

	//-----------------------------------------------------------------------------
	/// move construct a KReaderWriter
	KReaderWriter(self_type&& other) noexcept
	//-----------------------------------------------------------------------------
	    : base_type(std::move(other))
	    , k_rw_type(std::move(other))
	{}

	//-----------------------------------------------------------------------------
	/// copy constructor is deleted - std::iostreams is, too
	KReaderWriter(self_type& other) = delete;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	// semi-perfect forwarding - currently needed as std::iostream does not yet
	// support string_views as arguments
	template<class... Args>
	KReaderWriter(KStringView sv, Args&&... args)
	    : base_type(std::string(sv), std::forward<Args>(args)...)
	    , k_rw_type(*this)
	//-----------------------------------------------------------------------------
	{
		static_assert(std::is_base_of<std::iostream, IOStream>::value,
		              "KReaderWriter cannot be derived from a non-std::iostream class");
	}

	//-----------------------------------------------------------------------------
	// perfect forwarding
	template<class... Args>
	KReaderWriter(Args&&... args)
	    : base_type(std::forward<Args>(args)...)
	    , k_rw_type(static_cast<std::iostream&>(*this))
	//-----------------------------------------------------------------------------
	{
		static_assert(std::is_base_of<std::iostream, IOStream>::value,
		              "KReaderWriter cannot be derived from a non-std::iostream class");
	}

	//-----------------------------------------------------------------------------
	/// copy assignment is deleted - std::iostring's is, too
	self_type& operator=(const self_type& other) = delete;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// move assignment
	self_type& operator=(self_type&& other)
	//-----------------------------------------------------------------------------
	{
		base_type::operator=(std::move(other));
		k_rw_type::operator=(std::move(other));
	}

};


/// File stream based on std::fstream
using KFile           = KReaderWriter<std::fstream>;

/// String writer based on std::stringstream
using KStringStream   = KReaderWriter<std::stringstream>;

} // end of namespace dekaf2

