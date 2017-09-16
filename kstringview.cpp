

#include "kstringview.h"
#include "klog.h"
#include "dekaf2.h"


namespace dekaf2 {

const KStringView::size_type KStringView::npos;
const KStringView::value_type KStringView::s_0ch = '\0';

//-----------------------------------------------------------------------------
size_t kFind(
        KStringView haystack,
        KStringView needle,
        size_t pos)
//-----------------------------------------------------------------------------
{
	if (DEKAF2_UNLIKELY(needle.size() == 1))
	{
		// flip to single char search if only one char is in the search argument
		return kFind(haystack, needle[0], pos);
	}

	if (DEKAF2_UNLIKELY(pos > haystack.size()))
	{
		return KStringView::npos;
	}

	auto found = static_cast<const char*>(::memmem(haystack.data() + pos,
	                                               haystack.size() - pos,
	                                               needle.data(),
	                                               needle.size()));

	if (DEKAF2_UNLIKELY(!found))
	{
		return KStringView::npos;
	}
	else
	{
		return static_cast<size_t>(found - haystack.data());
	}

}

//-----------------------------------------------------------------------------
size_t kRFind(
        KStringView haystack,
        KStringView needle,
        size_t pos)
//-----------------------------------------------------------------------------
{
	if (DEKAF2_UNLIKELY(needle.size() == 1))
	{
		return kRFind(haystack, needle[0], pos);
	}

	if (DEKAF2_LIKELY(needle.size() <= haystack.size()))
	{
		pos = std::min(haystack.size() - needle.size(), pos);

		for(;;)
		{
			auto found = static_cast<const char*>(::memrchr(haystack.data(),
			                                                needle[0],
			                                                pos+1));
			if (!found)
			{
				break;
			}

			pos = static_cast<size_t>(found - haystack.data());

			if (std::memcmp(haystack.data() + pos + 1,
			                needle.data() + 1,
			                needle.size() - 1) == 0)
			{
				return pos;
			}

			--pos;
		}
	}

	return KStringView::npos;
}

namespace detail { namespace stringview {

//-----------------------------------------------------------------------------
size_t kFindFirstOfBool(
        KStringView haystack,
        KStringView needle,
        size_t pos,
        bool bNot)
//-----------------------------------------------------------------------------
{
	if (DEKAF2_UNLIKELY(needle.size() == 1))
	{
		return bNot ? kRFind(haystack, needle[0], pos)
		            : kFind(haystack, needle[0], pos);
	}

	if (DEKAF2_UNLIKELY(pos >= haystack.size()))
	{
		return KStringView::npos;
	}

#ifdef __x86_64__xx
	static bool has_sse42 = Dekaf().GetCpuId().sse42();
	if (DEKAF2_LIKELY(has_sse42))
	{

	}
#endif

#ifdef DEKAF2_USE_FOLLY_STRINGPIECE_AS_KSTRINGVIEW
	if (!bNot)
	{
		if (DEKAF2_UNLIKELY(pos > 0))
		{
			haystack.remove_prefix(pos);
		}

		return folly::qfind_first_of(haystack.ToRange(),
		                             needle.ToRange())
		        + pos;
	}
#endif

 	bool table[256];
	std::memset(table, false, 256);

	for (auto c : needle)
	{
		table[static_cast<unsigned char>(c)] = true;
	}

	auto it = std::find_if(haystack.begin() + pos,
	                       haystack.end(),
	                       [&table, bNot](const char c)
	{
		return table[static_cast<unsigned char>(c)] != bNot;
	});

	if (it == haystack.end())
	{
		return KStringView::npos;
	}
	else
	{
		return static_cast<size_t>(it - haystack.begin());
	}
}

//-----------------------------------------------------------------------------
size_t kFindLastOfBool(
        KStringView haystack,
        KStringView needle,
        size_t pos,
        bool bNot)
//-----------------------------------------------------------------------------
{
	if (DEKAF2_UNLIKELY(needle.size() == 1))
	{
		return kRFind(haystack, needle[0], pos);
	}

	if (DEKAF2_UNLIKELY(haystack.empty()))
	{
		return KStringView::npos;
	}

	pos = (haystack.size() - 1) - std::min(pos, haystack.size()-1);

	bool table[256];
	std::memset(table, false, 256);

	for (auto c : needle)
	{
		table[static_cast<unsigned char>(c)] = true;
	}

	auto it = std::find_if(haystack.rbegin() +
						   static_cast<typename KStringView::difference_type>(pos),
						   haystack.rend(),
						   [&table, bNot](const char c)
	{
		return table[static_cast<unsigned char>(c)] != bNot;
	});

	if (it == haystack.rend())
	{
		return KStringView::npos;
	}
	else
	{
		return static_cast<size_t>((it.base() - 1) - haystack.begin());
	}
}

} } // end of namespace detail::stringview

//-----------------------------------------------------------------------------
KStringView::size_type KStringView::copy(iterator dest, size_type count, size_type pos) const
//-----------------------------------------------------------------------------
{
	if (DEKAF2_UNLIKELY(pos > size()))
	{
		kWarning("attempt to copy from past the end of string view of size {}: pos {}",
		         size(), pos);

		pos = size();
	}

	count = std::min(size() - pos, count);

	return static_cast<size_type>(std::copy(const_cast<char*>(begin() + pos),
	                                        const_cast<char*>(begin() + pos + count),
	                                        const_cast<char*>(dest))
	                              - dest);
}

//-----------------------------------------------------------------------------
/// nonstandard: emulate erase if range is at begin or end
KStringView::self_type& KStringView::erase(size_type pos, size_type n)
//-----------------------------------------------------------------------------
{
	if (DEKAF2_UNLIKELY(pos >= size()))
	{
			kWarning("attempt to erase past end of string view of size {}: pos {}, n {}",
			         size(), pos, n);
			pos = size();
	}

	n = std::min(n, size() - pos);

#ifdef DEKAF2_USE_FOLLY_STRINGPIECE_AS_KSTRINGVIEW

	try {

		m_rep.erase(begin()+pos, begin()+pos+n);

	} catch (const std::exception& ex) {

			kException(ex);

	}

#else

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

#endif

	return *this;

}


} // end of namespace dekaf2

