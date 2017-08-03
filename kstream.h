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


//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// The general bidirectional stream abstraction for dekaf2. Can be constructed around any
/// std::iostream.
template<class IOStream>
class KReaderWriter
        : public IOStream
        , public KBasicReader
        , public KBasicWriter
{
	using base_type = IOStream;
	using self_type = KReaderWriter<IOStream>;
	using reader_type = KBasicReader;
	using writer_type = KBasicWriter;

//-------
public:
//-------

	//-----------------------------------------------------------------------------
	/// move construct a KReaderWriter
	KReaderWriter(self_type&& other) noexcept
	//-----------------------------------------------------------------------------
	    : base_type(std::move(other))
	    , reader_type(std::move(other))
	    , writer_type(std::move(other))
	{}

	//-----------------------------------------------------------------------------
	/// copy construct a KReaderWriter
	KReaderWriter(self_type& other)
	//-----------------------------------------------------------------------------
	    : base_type(other)
	    , reader_type(other)
	    , writer_type(other)
	{}

	//-----------------------------------------------------------------------------
	// semi-perfect forwarding - currently needed as std::iostream does not yet
	// support string_views as arguments
	template<class... Args>
	KReaderWriter(KStringView sv, Args&&... args)
	    : base_type(std::string(sv), std::forward<Args>(args)...)
	    , reader_type(static_cast<std::istream&>(*this))
	    , writer_type(static_cast<std::ostream&>(*this))
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
	    , reader_type(static_cast<std::istream&>(*this))
	    , writer_type(static_cast<std::ostream&>(*this))
	//-----------------------------------------------------------------------------
	{
		static_assert(std::is_base_of<std::iostream, IOStream>::value,
		              "KReaderWriter cannot be derived from a non-std::iostream class");
	}

	//-----------------------------------------------------------------------------
	/// copy assignment
	self_type& operator=(const self_type& other)
	//-----------------------------------------------------------------------------
	{
		base_type::operator=(other);
		reader_type::operator=(other);
		writer_type::operator=(other);
	}

	//-----------------------------------------------------------------------------
	/// move assignment
	self_type& operator=(self_type&& other)
	//-----------------------------------------------------------------------------
	{
		base_type::operator=(std::move(other));
		reader_type::operator=(std::move(other));
		writer_type::operator=(std::move(other));
	}

};


/// File stream based on std::fstream
using KFile           = KReaderWriter<std::fstream>;

/// String writer based on std::stringstream
using KStringStream   = KReaderWriter<std::stringstream>;

} // end of namespace dekaf2

