
#include <algorithm>
#include "kstringview.h"
#include "klog.h"

namespace dekaf2
{

#ifndef DEKAF2_USE_STD_STRING_VIEW_AS_KSTRINGVIEW

//-----------------------------------------------------------------------------
KStringView::size_type KStringView::find_first_of(self_type sv, size_type pos) const noexcept
//-----------------------------------------------------------------------------
{
	if (DEKAF2_UNLIKELY(sv.size() == 1))
	{
		return find(sv[0], pos);
	}
	else
	{
		auto it = std::find_first_of(begin() + pos, end(), sv.begin(), sv.end());
		if (it == end())
		{
			return KStringView::npos;
		}
		else
		{
			return static_cast<size_type>(it - begin());
		}
	}
}

//-----------------------------------------------------------------------------
KStringView::size_type KStringView::find_last_of(self_type sv, size_type pos) const noexcept
//-----------------------------------------------------------------------------
{
	if (DEKAF2_UNLIKELY(sv.size() == 1))
	{
		return rfind(sv[0], pos);
	}
	else
	{
		pos = (size() - 1) - std::min(pos, size()-1);
		auto it = std::find_first_of(rbegin() + static_cast<difference_type>(pos), rend(),
		                             sv.begin(), sv.end());
		if (it == rend())
		{
			return KStringView::npos;
		}
		else
		{
			return static_cast<size_type>((it.base() - 1) - begin());
		}
	}
}

//-----------------------------------------------------------------------------
KStringView::size_type KStringView::find_first_not_of(self_type sv, size_type pos) const noexcept
//-----------------------------------------------------------------------------
{
	if (DEKAF2_UNLIKELY(sv.size() == 1))
	{
		return find_first_not_of(sv[0], pos);
	}
	else
	{
		auto it = std::find_if_not(begin() + pos, end(),
								   [&sv](KStringView::value_type ch)
								   { return memchr(sv.data(), ch, sv.size()) != nullptr; });
		if (it == end())
		{
			return KStringView::npos;
		}
		else
		{
			return static_cast<size_type>(it - begin());
		}
	}
}

//-----------------------------------------------------------------------------
KStringView::size_type KStringView::find_last_not_of(self_type sv, size_type pos) const noexcept
//-----------------------------------------------------------------------------
{
	if (DEKAF2_UNLIKELY(sv.size() == 1))
	{
		return find_last_not_of(sv[0], pos);
	}
	else
	{
		if (pos >= size()) // this includes npos
		{
			pos = 0;
		}
		else
		{
			pos = size() - pos;
		}
		auto it = std::find_if_not(rbegin() + pos, rend(),
								   [&sv](KStringView::value_type ch)
								   { return memchr(sv.data(), ch, sv.size()) != nullptr; });
		if (it == rend())
		{
			return KStringView::npos;
		}
		else
		{
			return static_cast<size_type>(it.base() - begin());
		}
	}
}

//-----------------------------------------------------------------------------
KStringView::size_type KStringView::find_first_not_of(value_type ch_p, size_type pos) const noexcept
//-----------------------------------------------------------------------------
{
	auto it = std::find_if_not(begin() + pos, end(),
	                           [ch_p](KStringView::value_type ch)
	                           { return ch_p == ch; });
	if (it == end())
	{
		return KStringView::npos;
	}
	else
	{
		return static_cast<size_type>(it - begin());
	}
}

//-----------------------------------------------------------------------------
KStringView::size_type KStringView::find_last_not_of(value_type ch_p, size_type pos) const noexcept
//-----------------------------------------------------------------------------
{
	if (pos >= size()) // this includes npos
	{
		pos = 0;
	}
	else
	{
		pos = size() - pos;
	}
	auto it = std::find_if_not(rbegin() + pos, rend(),
	                           [ch_p](KStringView::value_type ch)
	                           { return ch_p == ch; });
	if (it == rend())
	{
		return KStringView::npos;
	}
	else
	{
		return static_cast<size_type>(it - rbegin());
	}
}

//-----------------------------------------------------------------------------
// non-standard: emulate erase if range is at begin or end
KStringView::self_type& KStringView::erase(size_type pos, size_type n)
//-----------------------------------------------------------------------------
{
	if (DEKAF2_UNLIKELY(pos >= size()))
	{
		kWarning("attempt to erase past end of string view of size {}: pos {}, n {}",
		         size(), pos, n);
	}
	else {

		if (n == npos)
		{
			n = size() - pos;
		}
		else if (DEKAF2_UNLIKELY(n > size()))
		{
			kWarning("impossible to remove {} chars at pos {} in a string view of size {}",
					 n, pos, size());

			// manipulate arguments such that the result is empty
			pos = 0;
			n = size();
		}

		if (pos == 0)
		{
			remove_prefix(n);
		}
		else if (pos + n == size())
		{
			remove_suffix(n);
		}
		else
		{
			kWarning("impossible to remove {} chars at pos {} in a string view of size {}",
					 n, pos, size());
		}

	}

	return *this;
}

#endif

} // end of namespace dekaf2

