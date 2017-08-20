
#include <algorithm>
#include "kstringview.h"

namespace dekaf2
{

#ifndef DEKAF2_HAS_CPP_17

KStringView::size_type KStringView::find_first_of(self_type sv, size_type pos) const noexcept
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

KStringView::size_type KStringView::find_last_of(self_type sv, size_type pos) const noexcept
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

#endif

} // end of namespace dekaf2

