/*
//
// DEKAF(tm): Lighter, Faster, Smarter (tm)
//
// Copyright (c) 2017, Ridgeware, Inc.
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

#include "kdefinitions.h"
#include "kstring.h"
#include "kstringutils.h"
#include "kcaseless.h"

DEKAF2_NAMESPACE_BEGIN

namespace detail {
namespace casestring {

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
struct DEKAF2_PUBLIC TrimWhiteSpaces
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	static constexpr KStringView svTrimLeft  { detail::kASCIISpaces };
	static constexpr KStringView svTrimRight { detail::kASCIISpaces };
};

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
struct DEKAF2_PUBLIC NoTrim
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	static constexpr KStringView svTrimLeft  {};
	static constexpr KStringView svTrimRight {};
};

//-----------------------------------------------------------------------------
template<typename Trimming>
inline
KStringView Trim(KStringView sv)
//-----------------------------------------------------------------------------
{
	if (!Trimming::svTrimLeft.empty())
	{
		kTrimLeft(sv, Trimming::svTrimLeft);
	}
	if (!Trimming::svTrimRight.empty())
	{
		kTrimRight(sv, Trimming::svTrimRight);
	}
	return sv;
}

} // end of namespace detail::casestring
} // end of namespace detail

//-----------------------------------------------------------------------------
/// compares trimmed strings
template<typename TrimmingLeft, typename TrimmingRight>
int kCaseCompareTrim(KStringView left, KStringView right)
//-----------------------------------------------------------------------------
{
	left  = detail::casestring::Trim<TrimmingLeft>(left);
	right = detail::casestring::Trim<TrimmingRight>(right);

	return kCaselessCompare(left, right);
}

//-----------------------------------------------------------------------------
/// tests for equality on trimmed strings
template<typename TrimmingLeft, typename TrimmingRight>
bool kCaseEqualTrim(KStringView left, KStringView right)
//-----------------------------------------------------------------------------
{
	left  = detail::casestring::Trim<TrimmingLeft>(left);
	right = detail::casestring::Trim<TrimmingRight>(right);

	return kCaselessEqual(left, right);
}

//-----------------------------------------------------------------------------
/// calculates a hash for the trimmed string
template<typename Trimming>
std::size_t kCalcCaseHashTrim(KStringView sv)
//-----------------------------------------------------------------------------
{
	sv = detail::casestring::Trim<Trimming>(sv);
	return kCalcCaselessHash(sv);
}


//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// A string view class that compares and hashes case insensitive and trimmed
template<typename Trimming = detail::casestring::NoTrim>
class KCaseStringViewBase : public KStringView
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	using KStringView::KStringView;

	//-----------------------------------------------------------------------------
	template<typename TrimmingRight = detail::casestring::NoTrim>
	int compare(KCaseStringViewBase<TrimmingRight> other) const
	//-----------------------------------------------------------------------------
	{
		return kCaseCompareTrim<Trimming, TrimmingRight>(*this, other);
	}

	//-----------------------------------------------------------------------------
	/// nonstandard: output the hash value of instance by calling std::hash() for the type
	std::size_t Hash() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// same as Hash(), added for compatibility with KStringView
	std::size_t CaseHash() const
	//-----------------------------------------------------------------------------
	{
		return Hash();
	}

};

//-----------------------------------------------------------------------------
template<typename TrimmingLeft, typename TrimmingRight>
inline bool operator==(KCaseStringViewBase<TrimmingLeft> left, KCaseStringViewBase<TrimmingRight> right)
//-----------------------------------------------------------------------------
{
	return kCaseEqualTrim<TrimmingLeft, TrimmingRight>(left, right);
}

//-----------------------------------------------------------------------------
template<typename TrimmingLeft, typename T,
		 typename std::enable_if<detail::is_kstringview_assignable<T, true>::value == true, int>::type = 0>
inline bool operator==(KCaseStringViewBase<TrimmingLeft> left, const T& right)
//-----------------------------------------------------------------------------
{
	return kCaseEqualTrim<TrimmingLeft, detail::casestring::NoTrim>(left, right);
}

//-----------------------------------------------------------------------------
template<typename TrimmingRight, typename T,
         typename std::enable_if<detail::is_kstringview_assignable<T, true>::value == true, int>::type = 0>
inline bool operator==(const T& left, const KCaseStringViewBase<TrimmingRight> right)
//-----------------------------------------------------------------------------
{
	return right == left;
}

//-----------------------------------------------------------------------------
template<typename TrimmingLeft, typename TrimmingRight>
inline bool operator!=(KCaseStringViewBase<TrimmingLeft> left, KCaseStringViewBase<TrimmingRight> right)
//-----------------------------------------------------------------------------
{
	return !(left == right);
}

//-----------------------------------------------------------------------------
template<typename TrimmingLeft, typename T,
         typename std::enable_if<detail::is_kstringview_assignable<T, true>::value == true, int>::type = 0>
inline bool operator!=(KCaseStringViewBase<TrimmingLeft> left, const T& right)
//-----------------------------------------------------------------------------
{
	return !(left == right);
}

//-----------------------------------------------------------------------------
template<typename TrimmingRight, typename T,
         typename std::enable_if<detail::is_kstringview_assignable<T, true>::value == true, int>::type = 0>
inline bool operator!=(const T& left, const KCaseStringViewBase<TrimmingRight> right)
//-----------------------------------------------------------------------------
{
	return right != left;
}


//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// A string class that compares and hashes case insensitive and trimmed
template<typename Trimming = detail::casestring::NoTrim>
class KCaseStringBase : public KString
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	using KString::KString;

	//-----------------------------------------------------------------------------
	template<typename TrimmingRight = detail::casestring::NoTrim>
	int compare(const KCaseStringBase<TrimmingRight>& other) const
	//-----------------------------------------------------------------------------
	{
		return ToView().template compare<Trimming, TrimmingRight>(other.ToView());
	}

	//-----------------------------------------------------------------------------
	KCaseStringViewBase<Trimming> ToView() const
	//-----------------------------------------------------------------------------
	{
		return KCaseStringViewBase<Trimming>(this->data(), this->size());
	}

	//-----------------------------------------------------------------------------
	/// nonstandard: output the hash value of instance by calling std::hash() for the type
	std::size_t Hash() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// same as Hash(), added for compatibility with KString
	std::size_t CaseHash() const
	//-----------------------------------------------------------------------------
	{
		return Hash();
	}

};

//-----------------------------------------------------------------------------
template<typename TrimmingLeft, typename TrimmingRight>
inline bool operator==(const KCaseStringBase<TrimmingLeft>& left, const KCaseStringBase<TrimmingRight>& right)
//-----------------------------------------------------------------------------
{
	return kCaseEqualTrim<TrimmingLeft, TrimmingRight>(left.ToView(), right.ToView());
}

//-----------------------------------------------------------------------------
template<typename TrimmingLeft, typename T,
         typename std::enable_if<detail::is_kstringview_assignable<T, true>::value == true, int>::type = 0>
inline bool operator==(const KCaseStringBase<TrimmingLeft>& left, const T& right)
//-----------------------------------------------------------------------------
{
	return kCaseEqualTrim<TrimmingLeft, detail::casestring::NoTrim>(left.ToView(), right);
}

//-----------------------------------------------------------------------------
template<typename TrimmingRight, typename T,
         typename std::enable_if<detail::is_kstringview_assignable<T, true>::value == true, int>::type = 0>
inline bool operator==(const T& left, const KCaseStringBase<TrimmingRight>& right)
//-----------------------------------------------------------------------------
{
	return right == left;
}

//-----------------------------------------------------------------------------
template<typename TrimmingLeft, typename TrimmingRight>
inline bool operator!=(const KCaseStringBase<TrimmingLeft>& left, const KCaseStringBase<TrimmingRight>& right)
//-----------------------------------------------------------------------------
{
	return !(left == right);
}

//-----------------------------------------------------------------------------
template<typename TrimmingLeft, typename T,
         typename std::enable_if<detail::is_kstringview_assignable<T, true>::value == true, int>::type = 0>
inline bool operator!=(const KCaseStringBase<TrimmingLeft>& left, const T& right)
//-----------------------------------------------------------------------------
{
	return !(left == right);
}

//-----------------------------------------------------------------------------
template<typename TrimmingRight, typename T,
         typename std::enable_if<detail::is_kstringview_assignable<T, true>::value == true, int>::type = 0>
inline bool operator!=(const T& left, const KCaseStringBase<TrimmingRight>& right)
//-----------------------------------------------------------------------------
{
	return right != left;
}

extern template class KCaseStringViewBase<detail::casestring::NoTrim>;
extern template class KCaseStringBase<detail::casestring::NoTrim>;
extern template class KCaseStringViewBase<detail::casestring::TrimWhiteSpaces>;
extern template class KCaseStringBase<detail::casestring::TrimWhiteSpaces>;

using KCaseStringView     = KCaseStringViewBase<detail::casestring::NoTrim>;
using KCaseString         = KCaseStringBase<detail::casestring::NoTrim>;
using KCaseTrimStringView = KCaseStringViewBase<detail::casestring::TrimWhiteSpaces>;
using KCaseTrimString     = KCaseStringBase<detail::casestring::TrimWhiteSpaces>;

DEKAF2_NAMESPACE_END


namespace std
{
	/// provide a std::hash for KCaseStringViewBase
	template<typename Trimming> struct hash<dekaf2::KCaseStringViewBase<Trimming>>
	{
		std::size_t operator()(dekaf2::KStringView sv) const noexcept
		{
			return dekaf2::kCalcCaseHashTrim<Trimming>(sv);
		}
	};

	/// provide a std::hash for KCaseStringBase
	template<typename Trimming> struct hash<dekaf2::KCaseStringBase<Trimming>>
	{
		std::size_t operator()(dekaf2::KStringView sv) const noexcept
		{
			return dekaf2::kCalcCaseHashTrim<Trimming>(sv);
		}
	};

	// make sure comparisons work without construction of KCaseStringBase
	template<typename Trimming> struct equal_to<dekaf2::KCaseStringBase<Trimming>>
	{
		using is_transparent = void;

#ifdef DEKAF2_HAS_FULL_CPP_17
		bool operator()(const dekaf2::KCaseStringBase<Trimming>& s1, dekaf2::KStringView s2) const
		{
			return s1 == s2;
		}
		bool operator()(dekaf2::KStringView s1, const dekaf2::KCaseStringBase<Trimming>& s2) const
		{
			return s1 == s2;
		}
#endif
		bool operator()(const dekaf2::KCaseStringBase<Trimming>& s1, const dekaf2::KCaseStringBase<Trimming>& s2) const
		{
			return s1 == s2;
		}
	};

	// make sure comparisons work without construction of KCaseStringBase
	template<typename Trimming> struct less<dekaf2::KCaseStringBase<Trimming>>
	{
		using is_transparent = void;

#ifdef DEKAF2_HAS_FULL_CPP_17
		bool operator()(const dekaf2::KCaseStringBase<Trimming>& s1, dekaf2::KStringView s2) const
		{
			return s1.ToView().template compare<Trimming, dekaf2::detail::casestring::NoTrim>(s2) < 0;
		}
		bool operator()(dekaf2::KStringView s1, const dekaf2::KCaseStringBase<Trimming>& s2) const
		{
			return s2.ToView().template compare<Trimming, dekaf2::detail::casestring::NoTrim>(s1) > 0;
		}
#endif
		bool operator()(const dekaf2::KCaseStringBase<Trimming>& s1, const dekaf2::KCaseStringViewBase<Trimming>& s2) const
		{
			return s1.compare(s2) < 0;
		}
	};

} // end of namespace std

#ifdef BOOST_NO_ARGUMENT_DEPENDENT_LOOKUP
namespace boost {
#else
DEKAF2_NAMESPACE_BEGIN
#endif
	template<typename Trimming>
	std::size_t hash_value(const dekaf2::KCaseStringViewBase<Trimming>& s)
	{
		return dekaf2::kCalcCaseHashTrim<Trimming>(s);
	}

	template<typename Trimming>
	std::size_t hash_value(const dekaf2::KCaseStringBase<Trimming>& s)
	{
		return dekaf2::kCalcCaseHashTrim<Trimming>(s);
	}
#ifdef BOOST_NO_ARGUMENT_DEPENDENT_LOOKUP
}
#else
DEKAF2_NAMESPACE_END
#endif

//----------------------------------------------------------------------
template<typename Trimming>
inline std::size_t dekaf2::KCaseStringViewBase<Trimming>::Hash() const
//----------------------------------------------------------------------
{
	return std::hash<dekaf2::KCaseStringViewBase<Trimming>>()(*this);
}

//----------------------------------------------------------------------
template<typename Trimming>
inline std::size_t dekaf2::KCaseStringBase<Trimming>::Hash() const
//----------------------------------------------------------------------
{
	return std::hash<dekaf2::KCaseStringBase<Trimming>>()(*this);
}

#if DEKAF2_HAS_INCLUDE("kformat.h")

#include "kformat.h"

namespace DEKAF2_FORMAT_NAMESPACE
{

template <typename Trimming>
struct formatter<dekaf2::KCaseStringViewBase<Trimming>> : formatter<string_view>
{
	template <typename FormatContext>
	auto format(const dekaf2::KCaseStringViewBase<Trimming>& String, FormatContext& ctx) const
	{
		return formatter<string_view>::format(string_view(String.data(), String.size()), ctx);
	}
};

template <typename Trimming>
struct formatter<dekaf2::KCaseStringBase<Trimming>> : formatter<string_view>
{
	template <typename FormatContext>
	auto format(const dekaf2::KCaseStringBase<Trimming>& String, FormatContext& ctx) const
	{
		return formatter<string_view>::format(string_view(String.data(), String.size()), ctx);
	}
};

} // end of DEKAF2_FORMAT_NAMESPACE

#endif // of #if DEKAF2_HAS_INCLUDE("kformat.h")
