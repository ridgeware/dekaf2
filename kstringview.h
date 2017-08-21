
#pragma once

#include <functional>
#include <boost/functional/hash.hpp>
#include "bits/kcppcompat.h"


#if defined(DEKAF2_HAS_CPP_17) and !defined(DEKAF2_FORCE_STRINGPIECE_AS_STRINGVIEW)
 #define DEKAF2_USE_STD_STRING_VIEW_AS_KSTRINGVIEW
#endif

#ifdef DEKAF2_USE_STD_STRING_VIEW_AS_KSTRINGVIEW

 // experimental/string_view is missing a declaration of string_view::npos ..
 // therefore we currently prefer the re2/stringpiece implementation
 // which additionally adds more protection against range overflows as it
 // does not throw

 #include <experimental/string_view>

#else

 // prepare to use re2's StringPiece as string_view

 #include <re2/stringpiece.h>
 #include "khash.h"

#endif

namespace dekaf2
{

#ifdef DEKAF2_USE_STD_STRING_VIEW_AS_KSTRINGVIEW

using KStringView = std::experimental::string_view;

#else

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// an extended StringPiece with the methods of
/// C++17's std::string_view
class KStringView : public re2::StringPiece
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

	using self_type = KStringView;
	using base_type = re2::StringPiece;

//----------
public:
//----------

	//-----------------------------------------------------------------------------
	KStringView()
	//-----------------------------------------------------------------------------
	{
	}

	//-----------------------------------------------------------------------------
	KStringView(const std::string& str)
	//-----------------------------------------------------------------------------
		: StringPiece(str)
	{
	}

	//-----------------------------------------------------------------------------
	KStringView(const char* str)
	//-----------------------------------------------------------------------------
		: StringPiece(str)
	{
	}

	//-----------------------------------------------------------------------------
	KStringView(const char* str, size_type len)
	//-----------------------------------------------------------------------------
		 : StringPiece(str, len)
	{
	}

	//-----------------------------------------------------------------------------
	inline const_iterator cbegin() const
	//-----------------------------------------------------------------------------
	{
		return begin();
	}

	//-----------------------------------------------------------------------------
	inline const_iterator cend() const
	//-----------------------------------------------------------------------------
	{
		return end();
	}

	//-----------------------------------------------------------------------------
	inline const_reverse_iterator crbegin() const
	//-----------------------------------------------------------------------------
	{
		return rbegin();
	}

	//-----------------------------------------------------------------------------
	inline const_reverse_iterator crend() const
	//-----------------------------------------------------------------------------
	{
		return rend();
	}

	//-----------------------------------------------------------------------------
	inline const_reference at(size_type index) const
	//-----------------------------------------------------------------------------
	{
		if (index < size()) return operator[](index);
		else return s_0ch;
	}

	//-----------------------------------------------------------------------------
	inline const_reference front() const
	//-----------------------------------------------------------------------------
	{
		if (empty()) return s_0ch;
		else return operator[](0);
	}

	//-----------------------------------------------------------------------------
	inline const_reference back() const
	//-----------------------------------------------------------------------------
	{
		if (empty()) return s_0ch;
		else return operator[](size() - 1);
	}

	//-----------------------------------------------------------------------------
	static size_type max_size() noexcept
	//-----------------------------------------------------------------------------
	{
		return std::string().max_size();
	}

	//-----------------------------------------------------------------------------
	// the rfind(value_type, size_type) implementation of StringPiece is erroneous
	// - if given npos for pos it does not search - therefore we implement our own
	size_type rfind(value_type ch, size_type pos = npos) const noexcept;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	inline size_type rfind(self_type sv, size_type pos = npos) const noexcept
	//-----------------------------------------------------------------------------
	{
		return base_type::rfind(sv, pos);
	}

	//-----------------------------------------------------------------------------
	size_type find_first_of(self_type sv, size_type pos = 0) const noexcept;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	inline size_type find_first_of(const value_type* s, size_type pos) const noexcept
	//-----------------------------------------------------------------------------
	{
		return find_first_of(KStringView(s), pos);
	}

	//-----------------------------------------------------------------------------
	inline size_type find_first_of(const value_type* s, size_type pos, size_type count) const noexcept
	//-----------------------------------------------------------------------------
	{
		return find_first_of(KStringView(s, count), pos);
	}

	//-----------------------------------------------------------------------------
	inline size_type find_first_of(value_type ch, size_type pos = 0) const noexcept
	//-----------------------------------------------------------------------------
	{
		return find(ch, pos);
	}

	//-----------------------------------------------------------------------------
	size_type find_last_of(self_type sv, size_type pos = npos) const noexcept;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	inline size_type find_last_of(const value_type* s, size_type pos) const noexcept
	//-----------------------------------------------------------------------------
	{
		return find_first_of(KStringView(s), pos);
	}

	//-----------------------------------------------------------------------------
	inline size_type find_last_of(const value_type* s, size_type pos, size_type count) const noexcept
	//-----------------------------------------------------------------------------
	{
		return find_last_of(KStringView(s, count), pos);
	}

	//-----------------------------------------------------------------------------
	inline size_type find_last_of(value_type ch, size_type pos = npos) const noexcept
	//-----------------------------------------------------------------------------
	{
		return rfind(ch, pos);
	}

	//-----------------------------------------------------------------------------
	size_type find_first_not_of(self_type sv, size_type pos = 0) const noexcept;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	inline size_type find_first_not_of(const value_type* s, size_type pos) const noexcept
	//-----------------------------------------------------------------------------
	{
		return find_first_not_of(KStringView(s), pos);
	}

	//-----------------------------------------------------------------------------
	inline size_type find_first_not_of(const value_type* s, size_type pos, size_type count) const noexcept
	//-----------------------------------------------------------------------------
	{
		return find_first_not_of(KStringView(s, count), pos);
	}

	//-----------------------------------------------------------------------------
	size_type find_first_not_of(value_type ch, size_type pos = 0) const noexcept;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	size_type find_last_not_of(self_type sv, size_type pos = npos) const noexcept;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	inline size_type find_last_not_of(const value_type* s, size_type pos) const noexcept
	//-----------------------------------------------------------------------------
	{
		return find_last_not_of(KStringView(s), pos);
	}

	//-----------------------------------------------------------------------------
	inline size_type find_last_not_of(const value_type* s, size_type pos, size_type count) const noexcept
	//-----------------------------------------------------------------------------
	{
		return find_last_not_of(KStringView(s, count), pos);
	}

	//-----------------------------------------------------------------------------
	size_type find_last_not_of(value_type ch, size_type pos = npos) const noexcept;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	void swap(KStringView& other) noexcept
	//-----------------------------------------------------------------------------
	{
		// StringPiece has no swap(), and the data members are private,
		// but we can access them at no additional cost through member functions.
		StringPiece::const_pointer data = other.data();
		StringPiece::size_type size = other.size();
		other.set(this->data(), this->size());
		this->set(data, size);
	}

//----------
private:
//----------

	static const value_type s_0ch = '\0';

}; // KStringView

#endif

// KStringView includes comparison for KString

//-----------------------------------------------------------------------------
inline bool operator==(KStringView left, KStringView right)
//-----------------------------------------------------------------------------
{
	KStringView::size_type len = left.size();
	if (len != right.size())
	{
		return false;
	}
	return left.data() == right.data()
	        || len == 0
	        || memcmp(left.data(), right.data(), len) == 0;
}

//-----------------------------------------------------------------------------
inline bool operator!=(KStringView left, KStringView right)
//-----------------------------------------------------------------------------
{
	return !(left == right);
}

//-----------------------------------------------------------------------------
inline bool operator<(KStringView left, KStringView right)
//-----------------------------------------------------------------------------
{
	KStringView::size_type min_size = std::min(left.size(), right.size());
	int r = min_size == 0 ? 0 : memcmp(left.data(), right.data(), min_size);
	return (r < 0) || (r == 0 && left.size() < right.size());
}

//-----------------------------------------------------------------------------
inline bool operator>(KStringView left, KStringView right)
//-----------------------------------------------------------------------------
{
	return right < left;
}

//-----------------------------------------------------------------------------
inline bool operator<=(KStringView left, KStringView right)
//-----------------------------------------------------------------------------
{
	return !(left > right);
}

//-----------------------------------------------------------------------------
inline bool operator>=(KStringView left, KStringView right)
//-----------------------------------------------------------------------------
{
	return !(left < right);
}

} // end of namespace dekaf2

#ifndef DEKAF2_USE_STD_STRING_VIEW_AS_KSTRINGVIEW
namespace std
{
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	// add a hash function for KStringView
	template<>
	struct hash<dekaf2::KStringView>
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		typedef dekaf2::KStringView argument_type;
		typedef std::size_t result_type;
		result_type operator()(argument_type s) const
		{
			return dekaf2::hash_bytes_FNV(s.data(), s.size());
		}
	};

}
#endif

namespace boost
{
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	// add a hash function for KStringView
	template<>
	struct hash<dekaf2::KStringView>
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		typedef dekaf2::KStringView argument_type;
		typedef std::size_t result_type;
		result_type operator()(argument_type s) const
		{
#if defined(DEKAF2_USE_STD_STRING_VIEW_AS_KSTRINGVIEW)
			return std::hash<dekaf2::KStringView>{}({s.data(), s.size()});
#else
			return dekaf2::hash_bytes_FNV(s.data(), s.size());
#endif
		}
	};

}

