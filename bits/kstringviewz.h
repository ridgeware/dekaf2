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

#include "../kstringview.h"

namespace dekaf2 {

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KStringViewZ : private KStringView
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

	constexpr KStringViewZ(const self_type&) noexcept = default;
	constexpr KStringViewZ& operator=(const self_type&) noexcept = default;

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
	constexpr
	KStringViewZ& operator=(KStringView other) = delete;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	constexpr
	KStringViewZ& operator=(const char* other)
	//-----------------------------------------------------------------------------
	{
		*this = self_type(other);
		return *this;
	}

	//-----------------------------------------------------------------------------
	KStringViewZ& operator=(const KString& other)
	//-----------------------------------------------------------------------------
	{
		*this = self_type(other);
		return *this;
	}

	//-----------------------------------------------------------------------------
	KStringViewZ& operator=(const std::string& other)
	//-----------------------------------------------------------------------------
	{
		*this = self_type(other);
		return *this;
	}

#ifdef DEKAF2_HAS_STD_STRING_VIEW
	//-----------------------------------------------------------------------------
	KStringViewZ& operator=(sv::string_view other) = delete;
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

#if defined(DEKAF2_USE_OPTIMIZED_STRING_FIND) && !defined(DEKAF2_NO_GCC)
	// we have a super fast implementation for these signatures in GLIBC, let
	// them superseede base_type's version
	size_type find_first_of(KStringView sv, size_type pos = 0) const;
	size_type find_first_not_of(KStringView sv, size_type pos = 0) const;
#endif
	
	using base_type::find_first_of;
	using base_type::find_last_of;
	using base_type::find_first_not_of;
	using base_type::find_last_not_of;
	
	// non standard
	using base_type::Hash;
	using base_type::ToRange;
	using base_type::clear;
	using base_type::StartsWith;
	using base_type::EndsWith;
	using base_type::Contains;
	using base_type::ToUpper;
	using base_type::ToLower;
	using base_type::ToUpperLocale;
	using base_type::ToLowerLocale;
	using base_type::MatchRegex;
	using base_type::MatchRegexGroups;
	using base_type::Left;
	using base_type::operator bool;

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
	KStringView Mid(size_type iStart, size_type iCount) const noexcept
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
	using base_type::SplitPairs;
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
	constexpr
	self_type ToView(size_type pos = 0) const noexcept
	//-----------------------------------------------------------------------------
	{
		if (pos > size())
		{
			Warn("pos exceeds size");
			pos = size();
		}
		return self_type(data() + pos, size() - pos);
	}

	//----------------------------------------------------------------------
	constexpr
	base_type ToView(size_type pos, size_type n) const noexcept
	//----------------------------------------------------------------------
	{
		if (pos > size())
		{
			Warn("pos exceeds size");
			pos = size();
		}
		if (n > size())
		{
			n = size() - pos;
		}
		else if (pos + n > size())
		{
			n = size() - pos;
		}
		return KStringView(data() + pos, n);

	} // ToView
	//-----------------------------------------------------------------------------
	constexpr
	const value_type* c_str() const noexcept
	//-----------------------------------------------------------------------------
	{
		return data();
	}

	// not using base_type::substr;
	// but we can implement two versions, one returning self_type, the other base_type

	//-----------------------------------------------------------------------------
	constexpr
	base_type substr(size_type pos, size_type count) const
	//-----------------------------------------------------------------------------
	{
		return base_type::substr(pos, count);
	}

	//-----------------------------------------------------------------------------
	constexpr
	self_type substr(size_type pos) const
	//-----------------------------------------------------------------------------
	{
		if (pos < size())
		{
			return { data() + pos, size() - pos };
		}
		else
		{
			return {};
		}
	}

	// not using base_type::erase;
	// but we can implement a version that permits prefix erase

	//-----------------------------------------------------------------------------
	/// nonstandard: emulate erase if range is at begin
	self& erase(size_type pos = 0, size_type n = npos)
	//-----------------------------------------------------------------------------
	{
		if (pos)
		{
			Warn("impossible to erase past the begin in a KStringViewZ");
		}
		else
		{
			n = std::min(n, size());
			unchecked_remove_prefix(n);
		}
		return *this;
	}

	//-----------------------------------------------------------------------------
	/// nonstandard: emulate erase if position is at begin
	iterator erase(const_iterator position)
	//-----------------------------------------------------------------------------
	{
		if (position != begin())
		{
			Warn("impossible to erase past the begin in a KStringViewZ");
			return end();
		}
		else
		{
			erase(static_cast<size_type>(position - begin()), 1);
			return begin();
		}
	}

	//-----------------------------------------------------------------------------
	/// nonstandard: emulate erase if range is at begin
	iterator erase(const_iterator first, const_iterator last)
	//-----------------------------------------------------------------------------
	{
		if (first != begin())
		{
			Warn("impossible to erase past the begin in a KStringViewZ");
			return end();
		}
		else
		{
			erase(static_cast<size_type>(first - begin()), static_cast<size_type>(last - first));
			return begin();
		}
	}

	// not using base_type::remove_suffix;

	//-----------------------------------------------------------------------------
	constexpr
	bool operator==(KStringViewZ other)
	//-----------------------------------------------------------------------------
	{
		return KStringView::operator==(other);
	}

	//-----------------------------------------------------------------------------
	constexpr
	bool operator!=(KStringViewZ other)
	//-----------------------------------------------------------------------------
	{
		return KStringView::operator!=(other);
	}

	//-----------------------------------------------------------------------------
	constexpr
	bool operator==(KStringView other)
	//-----------------------------------------------------------------------------
	{
		return KStringView::operator==(other);
	}

	//-----------------------------------------------------------------------------
	constexpr
	bool operator!=(KStringView other)
	//-----------------------------------------------------------------------------
	{
		return KStringView::operator!=(other);
	}

	//-----------------------------------------------------------------------------
	bool operator==(const KString& other)
	//-----------------------------------------------------------------------------
	{
		return KStringView::operator==(other);
	}

	//-----------------------------------------------------------------------------
	bool operator!=(const KString& other)
	//-----------------------------------------------------------------------------
	{
		return KStringView::operator!=(other);
	}

private:

	static constexpr value_type s_empty = '\0';

}; // KStringViewZ

using KStringViewZPair = std::pair<KStringViewZ, KStringViewZ>;


//-----------------------------------------------------------------------------
constexpr
inline bool operator==(const char* left, KStringViewZ right)
//-----------------------------------------------------------------------------
{
	return right.operator==(KStringView(left));
}

//-----------------------------------------------------------------------------
constexpr
inline bool operator==(KStringViewZ left, const char* right)
//-----------------------------------------------------------------------------
{
	return left.operator==(KStringView(right));
}

//-----------------------------------------------------------------------------
constexpr
inline bool operator!=(const char* left, KStringViewZ right)
//-----------------------------------------------------------------------------
{
	return right.operator!=(KStringView(left));
}

//-----------------------------------------------------------------------------
constexpr
inline bool operator!=(KStringViewZ left, const char* right)
//-----------------------------------------------------------------------------
{
	return left.operator!=(KStringView(right));
}

//-----------------------------------------------------------------------------
inline bool operator==(const std::string& left, KStringViewZ right)
//-----------------------------------------------------------------------------
{
	return right.operator==(KStringView(left));
}

//-----------------------------------------------------------------------------
inline bool operator==(KStringViewZ left, const std::string& right)
//-----------------------------------------------------------------------------
{
	return left.operator==(KStringView(right));
}

//-----------------------------------------------------------------------------
inline bool operator!=(const std::string& left, KStringViewZ right)
//-----------------------------------------------------------------------------
{
	return right.operator!=(KStringView(left));
}

//-----------------------------------------------------------------------------
inline bool operator!=(KStringViewZ left, const std::string& right)
//-----------------------------------------------------------------------------
{
	return left.operator!=(KStringView(right));
}

#ifdef DEKAF2_HAS_STD_STRING_VIEW
//-----------------------------------------------------------------------------
constexpr
inline bool operator==(sv::string_view left, KStringViewZ right)
//-----------------------------------------------------------------------------
{
	return right.operator==(KStringView(left));
}

//-----------------------------------------------------------------------------
constexpr
inline bool operator==(KStringViewZ left, sv::string_view right)
//-----------------------------------------------------------------------------
{
	return left.operator==(KStringView(right));
}

//-----------------------------------------------------------------------------
constexpr
inline bool operator!=(sv::string_view left, KStringViewZ right)
//-----------------------------------------------------------------------------
{
	return right.operator!=(KStringView(left));
}

//-----------------------------------------------------------------------------
constexpr
inline bool operator!=(KStringViewZ left, sv::string_view right)
//-----------------------------------------------------------------------------
{
	return left.operator!=(KStringView(right));
}
#endif



inline namespace literals {

	/// provide a string literal for KStringViewZ
	constexpr dekaf2::KStringViewZ operator"" _ksz(const char *data, std::size_t size)
	{
		// literals are always 0-terminated, therefore we can construct via a char*
		// (and strlen will be called as a compile-time constant expression..)
		return {data};
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
		typedef dekaf2::KStringViewZ argument_type;
		typedef std::size_t result_type;
		constexpr result_type operator()(argument_type s) const
		{
			return dekaf2::hash_bytes_FNV(s.data(), s.size());
		}
	};

} // namespace std

namespace boost
{
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// provide a boost::hash for KStringViewZ
	template<>
	struct hash<dekaf2::KStringViewZ>
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		typedef dekaf2::KStringViewZ argument_type;
		typedef std::size_t result_type;
		constexpr result_type operator()(argument_type s) const
		{
			return dekaf2::hash_bytes_FNV(s.data(), s.size());
		}
	};

} // namespace boost


