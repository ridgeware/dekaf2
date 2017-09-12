
#include <algorithm>
#include "kstringview.h"
#include "klog.h"

namespace dekaf2
{

#ifndef DEKAF2_USE_STD_STRING_VIEW_AS_KSTRINGVIEW

//------------------------------------------------------------------------------
KStringView::size_type KStringView::find(self_type search,
                              std::size_t pos) const noexcept
//------------------------------------------------------------------------------
{
	// GLIBC has a performant Boyer-Moore implementation for ::memmem, it
	// outperforms fbstring's simplyfied Boyer-Moore by one magnitude
	// (which means facebook uses it internally as well, as they mention a
	// search performance improvement by a factor of 30, but their code
	// in reality only improves search performance by a factor of 2
	// compared to std::string::find() - it is ::memmem() which brings it
	// to 30)
	if (DEKAF2_UNLIKELY(search.size() == 1))
	{
		// flip to single char search if only one char is in the search argument
		return find(search[0], pos);
	}
	if (DEKAF2_UNLIKELY(pos > size()))
	{
		return npos;
	}
	auto found = static_cast<const char*>(::memmem(data() + pos, size() - pos,
	                                               search.data(), search.size()));
	if (DEKAF2_UNLIKELY(!found))
	{
		return npos;
	}
	else
	{
		return static_cast<KStringView::size_type>(found - data());
	}
}

//-----------------------------------------------------------------------------
KStringView::size_type KStringView::int_find_first_of(self_type sv, size_type pos, bool bNot) const noexcept
//-----------------------------------------------------------------------------
{
	if (pos >= size())
	{
		return npos;
	}

#ifdef DEKAF2_USE_OPTIMIZED_STRING_FIND
	bool table[256];
	std::memset(table, 0, 256);

	for (auto c : sv)
	{
		table[static_cast<unsigned char>(c)] = true;
	}

	auto it = std::find_if(begin() + pos, end(), [&table, bNot](const char c)
	{
		return table[static_cast<unsigned char>(c)] != bNot;
	});
#else
	iterator it;
	if (!bNot)
	{
		it = std::find_first_of(begin() + pos, end(), sv.begin(), sv.end());
	}
	else
	{
	it = std::find_if_not(begin() + pos, end(),
	                      [&sv](KStringView::value_type ch)
	{
	     return memchr(sv.data(), ch, sv.size()) != nullptr;
	});
	}
#endif
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
KStringView::size_type KStringView::int_find_last_of(self_type sv, size_type pos, bool bNot) const noexcept
//-----------------------------------------------------------------------------
{
	if (DEKAF2_UNLIKELY(sv.size() == 1))
	{
		return rfind(sv[0], pos);
	}
	else
	{
		pos = (size() - 1) - std::min(pos, size()-1);

#ifdef DEKAF2_USE_OPTIMIZED_STRING_FIND
		bool table[256];
		std::memset(table, 0, 256);

		for (auto c : sv)
		{
			table[static_cast<unsigned char>(c)] = true;
		}

		auto it = std::find_if(rbegin() + static_cast<difference_type>(pos), rend(),
		                       [&table, bNot](const char c)
		{
			return table[static_cast<unsigned char>(c)] == !bNot;
		});
#else
		reverse_iterator it;
		if (!bNot)
		{
			it = std::find_first_of(rbegin() + static_cast<difference_type>(pos), rend(),
		                            sv.begin(), sv.end());
		}
		else
		{
			it = std::find_if_not(rbegin() + static_cast<difference_type>(pos), rend(),
			                      [&sv](KStringView::value_type ch)
			{
			     return memchr(sv.data(), ch, sv.size()) != nullptr;
			});
		}
#endif
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
KStringView::size_type KStringView::find_first_not_of(value_type ch_p, size_type pos) const noexcept
//-----------------------------------------------------------------------------
{
	if (pos > size())
	{
		pos = size();
	}

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

