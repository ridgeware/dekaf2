
#pragma once

#include <functional>
#include <boost/functional/hash.hpp>
#include "bits/kcppcompat.h"

#ifdef DEKAF2_HAS_CPP_17
#include <experimental/string_view>
#else // prepare to use re2's StringPiece as string_view
#include <re2/stringpiece.h>
#include "khash.h"
#endif

namespace dekaf2
{

#ifdef DEKAF2_HAS_CPP_17

using KStringView = std::experimental::string_view;

#else

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// an extended StringPiece with the methods of
/// C++17's std::string_view
class KStringView : public re2::StringPiece
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

	using self_type = KStringView;

//----------
public:
//----------

	KStringView() {}
	KStringView(const std::string& str) : StringPiece(str) {}
	KStringView(const char* str) : StringPiece(str) {}
	KStringView(const char* str, size_type len) : StringPiece(str, len) {}

	inline const_iterator cbegin() const { return begin(); }
	inline const_iterator cend() const { return end(); }
	inline const_reverse_iterator crbegin() const { return rbegin(); }
	inline const_reverse_iterator crend() const { return rend(); }

	const_reference at(size_type index) const
	{
		if (index < size())
		{
			return operator[](index);
		}
		else
		{
			return s_0ch;
		}
	}

	inline const_reference front() const
	{
		if (empty()) return s_0ch;
		else return operator[](0);
	}

	inline const_reference back() const
	{
		if (empty()) return s_0ch;
		else return operator[](size() - 1);
	}

	static size_type max_size() noexcept
	{
		return std::string().max_size();
	}

	size_type find_first_of(self_type sv, size_type pos = 0) const noexcept;

	inline size_type find_first_of(const value_type* s, size_type pos) const noexcept
	{
		return find_first_of(KStringView(s), pos);
	}

	inline size_type find_first_of(const value_type* s, size_type pos, size_type count) const noexcept
	{
		return find_first_of(KStringView(s, count), pos);
	}

	inline size_type find_first_of(value_type ch, size_type pos = 0) const noexcept
	{
		return find(ch, pos);
	}

	size_type find_last_of(self_type sv, size_type pos = 0) const noexcept;

	inline size_type find_last_of(const value_type* s, size_type pos) const noexcept
	{
		return find_first_of(KStringView(s), pos);
	}

	inline size_type find_last_of(const value_type* s, size_type pos, size_type count) const noexcept
	{
		return find_last_of(KStringView(s, count), pos);
	}

	inline size_type find_last_of(value_type ch, size_type pos = 0) const noexcept
	{
		return rfind(ch, pos);
	}

	void swap(KStringView& other) noexcept
	{
		// StringPiece has no swap(), and the data members are private,
		// but we can access them at no additional cost through member functions.
		StringPiece::const_pointer data = other.data();
		StringPiece::size_type size = other.size();
		other.set(this->data(), this->size());
		this->set(data, size);
	}

	// TODO implement find_first_not_of, find_last_not_of methods

//----------
private:
//----------

	static const value_type s_0ch = '\0';

}; // KStringView

#endif

// KStringView includes comparison for KString

//-----------------------------------------------------------------------------
inline bool operator==(const KStringView& left, const KStringView& right)
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
inline bool operator!=(const KStringView& left, const KStringView& right)
//-----------------------------------------------------------------------------
{
	return !(left == right);
}

//-----------------------------------------------------------------------------
inline bool operator<(const KStringView& left, const KStringView& right)
//-----------------------------------------------------------------------------
{
	KStringView::size_type min_size = std::min(left.size(), right.size());
	int r = min_size == 0 ? 0 : memcmp(left.data(), right.data(), min_size);
	return (r < 0) || (r == 0 && left.size() < right.size());
}

//-----------------------------------------------------------------------------
inline bool operator>(const KStringView& left, const KStringView& right)
//-----------------------------------------------------------------------------
{
	return right < left;
}

//-----------------------------------------------------------------------------
inline bool operator<=(const KStringView& left, const KStringView& right)
//-----------------------------------------------------------------------------
{
	return !(left > right);
}

//-----------------------------------------------------------------------------
inline bool operator>=(const KStringView& left, const KStringView& right)
//-----------------------------------------------------------------------------
{
	return !(left < right);
}

} // end of namespace dekaf2

#ifndef DEKAF2_HAS_CPP_17
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
		result_type operator()(const argument_type& s) const
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
		result_type operator()(const argument_type& s) const
		{
#ifdef DEKAF2_HAS_CPP_17
			return std::hash<dekaf2::KStringView>{}({s.data(), s.size()});
#else
			return dekaf2::hash_bytes_FNV(s.data(), s.size());
#endif
		}
	};

}

