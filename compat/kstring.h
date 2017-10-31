/*
//
// DEKAF(tm): Lighter, Faster, Smarter(tm)
//
// Copyright (c) 2000-2017, Ridgeware, Inc.
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

#include "kconfiguration.h"

#ifdef DEKAF1_INCLUDE_PATH

#include "../kstring.h"
#include DEKAF2_stringify(DEKAF1_INCLUDE_PATH/kstring.h)

namespace dekaf2 {
namespace compat {

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KString : public dekaf2::KString
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	//----------------------------------------------------------------------
	KString(const ::KString& old)
	//----------------------------------------------------------------------
	    : dekaf2::KString(old.c_str(), old.size())
	{
	}

	//----------------------------------------------------------------------
	KString(::KString& old)
	//----------------------------------------------------------------------
	    : dekaf2::KString(old.c_str(), old.size())
	{
	}

#ifndef DEKAF2_USE_FOLLY_STRING_AS_KSTRING
	//----------------------------------------------------------------------
	KString(::KString&& old)
	//----------------------------------------------------------------------
	    : dekaf2::KString(std::move(old))
	{
	}
#endif

	//----------------------------------------------------------------------
	template<class...Args>
	KString(Args&&...args)
	//----------------------------------------------------------------------
	    : dekaf2::KString(std::forward<Args>(args)...)
	{
	}

	//----------------------------------------------------------------------
	/// print arguments with fmt::printf..
	// actually we switch the printing format specifier to a sprintf - like
	// as the previous KString used such
	template<class... Args>
	KString& Format(Args&&... args)
	//----------------------------------------------------------------------
	{
		m_rep = kPrintf(std::forward<Args>(args)...);
		return *this;
	}

};

} // end of namespace compat
} // end of namespace dekaf2

namespace std
{
	/// provide a std::hash for dekaf2::compat::KString
	template<> struct hash<dekaf2::compat::KString>
	{
		typedef dekaf2::compat::KString argument_type;
		typedef std::size_t result_type;
		result_type operator()(argument_type const& s) const noexcept
		{
#ifdef DEKAF2_USE_FBSTRING_AS_KSTRING
			return std::hash<dekaf2::KString::string_type>{}(s);
#else
			return std::hash<dekaf2::KString::string_type>{}(s.ToStdString());
#endif
		}
	};

} // end of namespace std

#include <boost/functional/hash.hpp>

namespace boost
{
	/// provide a boost::hash for dekaf2::compat::KString
	template<> struct hash<dekaf2::compat::KString> : public std::unary_function<dekaf2::compat::KString, std::size_t>
	{
		typedef dekaf2::compat::KString argument_type;
		typedef std::size_t result_type;
		result_type operator()(argument_type const& s) const noexcept
		{
#ifdef DEKAF2_USE_FBSTRING_AS_KSTRING
			// reuse the std::hash, as it knows fbstring already
			return std::hash<dekaf2::KString::string_type>{}(s);
#else
			return std::hash<dekaf2::KString::string_type>{}(s.ToStdString());
#endif
		}
	};

} // end of namespace boost

#endif // of DEKAF1_INCLUDE_PATH
