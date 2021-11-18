/*
//
// DEKAF(tm): Lighter, Faster, Smarter (tm)
//
// Copyright (c) 2018, Ridgeware, Inc.
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

/// @file kstringviewz.h
/// string view implementation with a trailing zero

#include "kcppcompat.h"
#include "../kstringview.h"
#include "../kutf8.h"
#include "khash.h"

namespace dekaf2 {

class KStringViewZ;

inline namespace literals {

	/// provide a string literal for KStringViewZ
	constexpr dekaf2::KStringViewZ operator"" _ksz(const char *data, std::size_t size);

} // namespace literals

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// A string_view type with a guaranteed trailing zero that can be used on
/// any C API
#ifndef DEKAF2_IS_WINDOWS
class DEKAF2_PUBLIC DEKAF2_GSL_POINTER() KStringViewZ : private KStringView
#else
class DEKAF2_PUBLIC KStringViewZ : private KStringView
#endif
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	using self_type              = KStringViewZ;
	using self                   = KStringViewZ;
	using base_type              = KStringView;
	using size_type              = base_type::size_type;
	using iterator               = base_type::iterator;
	using const_iterator         = base_type::iterator;
	using reverse_iterator       = base_type::reverse_iterator;
	using const_reverse_iterator = base_type::const_reverse_iterator;
	using value_type             = base_type::value_type;
	using difference_type        = base_type::difference_type;
	using pointer                = base_type::pointer;
	using const_pointer          = base_type::const_pointer;
	using reference              = base_type::reference;
	using const_reference        = base_type::const_reference;
	using traits_type            = base_type::traits_type;

	using base_type::npos;

	DEKAF2_CONSTEXPR_14 KStringViewZ(const self_type&) noexcept = default;
	DEKAF2_CONSTEXPR_14 KStringViewZ& operator=(const self_type&) noexcept = default;

//----------
private:
//----------

	//-----------------------------------------------------------------------------
	// This is a private constructor. We do not trust external sources to
	// have a 0 following the string, but internally we guarantee this for
	// functions like ToView() and substr()
	constexpr
	KStringViewZ(const char* s, size_type count) noexcept
	//-----------------------------------------------------------------------------
	: base_type { s, count }
	{}

	// make the literal operator a friend, so that it can access the private constructor
	friend constexpr KStringViewZ literals::operator "" _ksz (const char *data, std::size_t size);

//----------
public:
//----------

	//-----------------------------------------------------------------------------
	constexpr
	KStringViewZ() noexcept
	//-----------------------------------------------------------------------------
	: base_type { &s_empty, 0 }
	{
	}

	//-----------------------------------------------------------------------------
	constexpr
	KStringViewZ(const char* s) noexcept
	//-----------------------------------------------------------------------------
	: base_type { s }
	{}

	//-----------------------------------------------------------------------------
	KStringViewZ(const std::string& str) noexcept
	//-----------------------------------------------------------------------------
	: base_type { str }
	{}

#ifdef DEKAF2_HAS_STD_STRING_VIEW
	//-----------------------------------------------------------------------------
	KStringViewZ(const sv::string_view& str) = delete;
	//-----------------------------------------------------------------------------
#endif

	//-----------------------------------------------------------------------------
	KStringViewZ(const KString& str) noexcept
	//-----------------------------------------------------------------------------
	: base_type { str }
	{}

	//-----------------------------------------------------------------------------
	// no construction from KStringView, which has no trailing 0
	// this overrides the otherwise implicit construction
	// KStringView > (temp)KString > KStringViewZ
	/// Construction of KStringViewZ from KStringView is not allowed
	KStringViewZ(KStringView sv) = delete;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	self& operator=(KStringView other) = delete;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	DEKAF2_CONSTEXPR_14
	self& operator=(const char* other)
	//-----------------------------------------------------------------------------
	{
		*this = self_type(other);
		return *this;
	}

	//-----------------------------------------------------------------------------
	self& operator=(const KString& other)
	//-----------------------------------------------------------------------------
	{
		*this = self_type(other);
		return *this;
	}

	//-----------------------------------------------------------------------------
	self& operator=(const std::string& other)
	//-----------------------------------------------------------------------------
	{
		*this = self_type(other);
		return *this;
	}

#ifdef DEKAF2_HAS_STD_STRING_VIEW
	//-----------------------------------------------------------------------------
	self& operator=(sv::string_view other) = delete;
	//-----------------------------------------------------------------------------
#endif

	using base_type::begin;
	using base_type::cbegin;
	using base_type::end;
	using base_type::cend;
	using base_type::rbegin;
	using base_type::crbegin;
	using base_type::rend;
	using base_type::crend;
	using base_type::operator[];
	using base_type::at;
	using base_type::front;
	using base_type::back;
	using base_type::data;
	using base_type::size;
	using base_type::length;
	using base_type::max_size;
	using base_type::empty;
	using base_type::remove_prefix;
	using base_type::swap;
	using base_type::copy;
	using base_type::compare;
	using base_type::find;
	using base_type::rfind;
	using base_type::starts_with;
	using base_type::ends_with;
	using base_type::contains;

#if defined(DEKAF2_USE_OPTIMIZED_STRING_FIND) && defined(__GLIBC__)
	// we have a super fast implementation for these signatures in GLIBC, let
	// them supersede base_type's version
	size_type find_first_of(KStringView search, size_type pos = 0) const;
	size_type find_first_not_of(KStringView search, size_type pos = 0) const;
#endif
	
	using base_type::find_first_of;
	using base_type::find_last_of;
	using base_type::find_first_not_of;
	using base_type::find_last_not_of;
	
	// non standard
	using base_type::Hash;
	using base_type::CaseHash;
	using base_type::clear;
	using base_type::StartsWith;
	using base_type::EndsWith;
	using base_type::Contains;
	using base_type::ToUpper;
	using base_type::ToLower;
	using base_type::ToUpperLocale;
	using base_type::ToLowerLocale;
	using base_type::ToUpperASCII;
	using base_type::ToLowerASCII;
	using base_type::MatchRegex;
	using base_type::MatchRegexGroups;
	using base_type::Left;
	using base_type::LeftUTF8;
	using base_type::operator bool;
	using base_type::operator fmt::string_view;

	//-----------------------------------------------------------------------------
	// nonstandard
	/// returns substring starting at iStart until end of string
	constexpr
	self_type Mid(size_type iStart) const noexcept
	//-----------------------------------------------------------------------------
	{
		return ToView(iStart);
	}

	//-----------------------------------------------------------------------------
	// nonstandard
	/// returns substring starting at iStart with size iCount
	constexpr
	base_type Mid(size_type iStart, size_type iCount) const noexcept
	//-----------------------------------------------------------------------------
	{
		return ToView(iStart, iCount);
	}

	//-----------------------------------------------------------------------------
	// nonstandard
	/// returns rightmost iCount chars of string
	self_type Right(size_type iCount) const noexcept;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	// nonstandard
	/// returns substring starting at codepoint iStart until end of string
	DEKAF2_CONSTEXPR_14
	self_type MidUTF8(size_type iStart) const noexcept
	//-----------------------------------------------------------------------------
	{
		auto it = Unicode::LeftUTF8(begin(), end(), iStart);
#ifndef _MSC_VER
		return self_type(it, end() - it);
#else
		return self_type(data() + (it - begin()), end() - it);
#endif
	}

	//-----------------------------------------------------------------------------
	// nonstandard
	/// returns substring starting at codepoint iStart with size iCount (in codepoints)
	DEKAF2_CONSTEXPR_14
	base_type MidUTF8(size_type iStart, size_type iCount) const noexcept
	//-----------------------------------------------------------------------------
	{
		return base_type::MidUTF8(iStart, iCount);
	}

	//-----------------------------------------------------------------------------
	// nonstandard
	/// returns rightmost iCount UTF8 codepoints of string
	DEKAF2_CONSTEXPR_14
	self_type RightUTF8(size_type iCount) const noexcept
	//-----------------------------------------------------------------------------
	{
		auto it = Unicode::RightUTF8(begin(), end(), iCount);
#ifndef _MSC_VER
		return self_type(it, end() - it);
#else
		return self_type(data() + (it - begin()), end() - it);
#endif
	}

	//-----------------------------------------------------------------------------
	// nonstandard
	/// removes white space from the left of the string
	self& TrimLeft();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	// nonstandard
	/// removes chTrim from the left of the string
	self& TrimLeft(value_type chTrim);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	// nonstandard
	/// removes any character in sTrim from the left of the string
	self& TrimLeft(KStringView sTrim);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	// nonstandard
	/// Clip removing everything to the left of sClipAtReverse so that sClipAtReverse becomes the beginning of the string;
	/// otherwise do not alter the string
	bool ClipAtReverse(KStringView sClipAtReverse);
	//-----------------------------------------------------------------------------

	using base_type::Split;
	using base_type::Bool;
	using base_type::Int16;
	using base_type::UInt16;
	using base_type::Int32;
	using base_type::UInt32;
	using base_type::Int64;
	using base_type::UInt64;
#ifdef DEKAF2_HAS_INT128
	using base_type::Int128;
	using base_type::UInt128;
#endif
	using base_type::Float;
	using base_type::Double;
	using base_type::In;

	//-----------------------------------------------------------------------------
	/// returns a sub-view of the current view from pos to end of view
	DEKAF2_CONSTEXPR_14
	self_type ToView(size_type pos = 0) const noexcept
	//-----------------------------------------------------------------------------
	{
		const auto iSize = size();

		if (pos > iSize)
		{
#ifndef NDEBUG
			Warn(DEKAF2_FUNCTION_NAME, "pos exceeds size");
#endif
			pos = iSize;
		}
		return self_type(data() + pos, iSize - pos);
	}

	//----------------------------------------------------------------------
	/// returns a sub-view of the current view from pos of view with size n,
	/// return type is a KStringView as the trailing zero got lost
	DEKAF2_CONSTEXPR_14
	base_type ToView(size_type pos, size_type n) const noexcept
	//----------------------------------------------------------------------
	{
		const auto iSize = size();

		if (pos > iSize)
		{
			// do not warn
			pos = iSize;
		}

		if (n > iSize)
		{
			n = iSize - pos;
		}
		else if (pos + n > iSize)
		{
			n = iSize - pos;
		}

		return KStringView(data() + pos, n);

	} // ToView

	//-----------------------------------------------------------------------------
	/// returns a C style char array with trailing zero - the reason this class
	/// exists
	constexpr
	const value_type* c_str() const noexcept
	//-----------------------------------------------------------------------------
	{
		return data();
	}

	// not using base_type::substr;
	// but we can implement two versions, one returning self_type, the other base_type

	//-----------------------------------------------------------------------------
	/// returns a sub-view of the current view from pos of view with size n,
	/// return type is a KStringView as the trailing zero got lost
	DEKAF2_CONSTEXPR_14
	base_type substr(size_type pos, size_type count) const
	//-----------------------------------------------------------------------------
	{
		return base_type::substr(pos, count);
	}

	//-----------------------------------------------------------------------------
	/// returns a sub-view of the current view from pos to end of view
	DEKAF2_CONSTEXPR_14
	self_type substr(size_type pos) const
	//-----------------------------------------------------------------------------
	{
		const auto iSize = size();

		if (DEKAF2_LIKELY(pos < iSize))
		{
			return { data() + pos, iSize - pos };
		}

		return {};
	}

	// not using base_type::erase;
	// but we can implement a version that permits prefix erase

	//-----------------------------------------------------------------------------
	/// nonstandard: emulate erase if range is at begin, otherwise does
	/// nothing (but emits a warning to the debug log)
	self& erase(size_type pos = 0, size_type n = npos);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// nonstandard: emulate erase if position is at begin, otherwise does
	/// nothing (but emits a warning to the debug log)
	iterator erase(const_iterator position);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// nonstandard: emulate erase if range is at begin, otherwise does
	/// nothing (but emits a warning to the debug log)
	iterator erase(const_iterator first, const_iterator last);
	//-----------------------------------------------------------------------------

	// not using base_type::remove_suffix;

private:

	static constexpr value_type s_empty = '\0';

}; // KStringViewZ

using KStringViewZPair = std::pair<KStringViewZ, KStringViewZ>;

inline namespace literals {

	/// provide a string literal for KStringViewZ
	constexpr dekaf2::KStringViewZ operator"" _ksz(const char *data, std::size_t size)
	{
		// we can access a private constructor of KStringViewZ because we are a friend..
		return { data, size };
	}

} // namespace literals

} // end of namespace dekaf2

namespace std
{
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// provide a std::hash for KStringViewZ
	template<>
	struct hash<dekaf2::KStringViewZ>
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		DEKAF2_CONSTEXPR_14 std::size_t operator()(dekaf2::KStringView s) const noexcept
		{
			return dekaf2::kHash(s.data(), s.size());
		}

		DEKAF2_CONSTEXPR_14 std::size_t operator()(const char* s) const noexcept
		{
			return dekaf2::kHash(s);
		}
	};

} // namespace std

namespace boost
{
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// provide a boost::hash for KStringViewZ
#if (BOOST_VERSION < 106400)
	template<>
	struct hash<dekaf2::KStringViewZ> : public std::unary_function<dekaf2::KStringViewZ, std::size_t>
#else
	template<>
	struct hash<dekaf2::KStringViewZ>
#endif
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		DEKAF2_CONSTEXPR_14 std::size_t operator()(dekaf2::KStringView s) const noexcept
		{
			return dekaf2::kHash(s.data(), s.size());
		}

		DEKAF2_CONSTEXPR_14 std::size_t operator()(const char* s) const noexcept
		{
			return dekaf2::kHash(s);
		}
	};

} // namespace boost


