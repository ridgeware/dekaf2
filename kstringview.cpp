
#include <algorithm>
#include "kstringview.h"

namespace dekaf2
{

#ifndef DEKAF2_USE_STD_STRING_VIEW_AS_KSTRINGVIEW

//-----------------------------------------------------------------------------
KStringView::size_type KStringView::rfind(value_type ch, size_type pos) const noexcept
//-----------------------------------------------------------------------------
{
	if (size() > 0)
	{
#if (DEKAF2_GCC_VERSION > 40600)
		// memrchr is supported since glibc v2.2. gcc 4.6 should satisfy that.
		if (pos != npos)
		{
			++pos;
		}
		pos = std::min(pos, size());
		const value_type* base  = data();
		const value_type* found = static_cast<const value_type*>(memrchr(base, ch, pos));
		if (found)
		{
			return static_cast<size_type>(found - base);
		}
#else
		// windows has no memrchr()
		pos = std::min(pos, size()-1);
		const value_type* base  = data();
		const value_type* found = base + pos;
		for (; found >= base; --found)
		{
			if (*found == ch)
			{
				return static_cast<size_type>(found - base);
			}
		}
#endif
	}
	return npos;
}

//-----------------------------------------------------------------------------
KStringView::size_type KStringView::find_first_of(self_type sv, size_type pos) const noexcept
//-----------------------------------------------------------------------------
{
	if (sv.size() == 1)
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
	if (sv.size() == 1)
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
	if (sv.size() == 1)
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
	if (sv.size() == 1)
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
#endif

} // end of namespace dekaf2

