
#include <algorithm>
#include "kstringview.h"

namespace dekaf2
{

#ifndef DEKAF2_HAS_CPP_17

KStringView::size_type KStringView::find_first_of(self_type sv, size_type pos) const noexcept
{
	if (sv.size() == 1)
	{
		return find(sv[0]);
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

KStringView::size_type KStringView::find_last_of(self_type sv, size_type pos) const noexcept
{
	if (sv.size() == 1)
	{
		return rfind(sv[0]);
	}
	else
	{
		auto it = std::find_first_of(rbegin() + pos, rend(), sv.rbegin(), sv.rend());
		if (it == rend())
		{
			return KStringView::npos;
		}
		else
		{
			return static_cast<size_type>(it - rbegin());
		}
	}
}

KStringView::size_type KStringView::find_first_not_of(self_type sv, size_type pos) const noexcept
{
	if (sv.size() == 1)
	{
		return find_first_not_of(sv[0]);
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

KStringView::size_type KStringView::find_last_not_of(self_type sv, size_type pos) const noexcept
{
	if (sv.size() == 1)
	{
		return find_last_not_of(sv[0]);
	}
	else
	{
		auto it = std::find_if_not(rbegin() + pos, rend(),
								   [&sv](KStringView::value_type ch)
								   { return memchr(sv.data(), ch, sv.size()) != nullptr; });
		if (it == rend())
		{
			return KStringView::npos;
		}
		else
		{
			return static_cast<size_type>(it - rbegin());
		}
	}
}

KStringView::size_type KStringView::find_first_not_of(value_type ch_p, size_type pos) const noexcept
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

KStringView::size_type KStringView::find_last_not_of(value_type ch_p, size_type pos) const noexcept
{
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

