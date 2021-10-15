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

#include "kstring.h"
#include "kstringutils.h"
#include "bits/khash.h"

namespace dekaf2 {

//-----------------------------------------------------------------------------
/// compares case insensitive
int kCaseCompare(KStringView left, KStringView right);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// tests for case insensitive equality
bool kCaseEqual(KStringView left, KStringView right);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// tests for case insensitive equality at begin of left string
bool kCaseBeginsWith(KStringView left, KStringView right);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// tests for case insensitive equality at end of left string
bool kCaseEndsWith(KStringView left, KStringView right);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// compares case insensitive, trims strings before compare
int kCaseCompareTrim(KStringView left, KStringView right, KStringView svTrim = detail::kASCIISpaces);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// tests trimmed strings for case insensitive equality
bool kCaseEqualTrim(KStringView left, KStringView right, KStringView svTrim = detail::kASCIISpaces);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// compares case insensitive, assuming right string in lowercase
int kCaseCompareLeft(KStringView left, KStringView right);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// tests for case insensitive equality, assuming right string in lowercase
bool kCaseEqualLeft(KStringView left, KStringView right);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// tests for case insensitive equality at begin of left string, assuming right string in lowercase
/// (despite the name this tests for left beginning with right)
bool kCaseBeginsWithLeft(KStringView left, KStringView right);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// tests for case insensitive equality at end of left string, assuming right string in lowercase
/// (despite the name this tests for left ending with right)
bool kCaseEndsWithLeft(KStringView left, KStringView right);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// compares case insensitive, trims strings before compare,
/// assuming right string trimmed and in lowercase
int kCaseCompareTrimLeft(KStringView left, KStringView right, KStringView svTrim = detail::kASCIISpaces);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// tests trimmed strings for case insensitive equality
/// assuming right string trimmed and in lowercase
bool kCaseEqualTrimLeft(KStringView left, KStringView right, KStringView svTrim = detail::kASCIISpaces);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// calculates a hash for case insensitive string
DEKAF2_CONSTEXPR_14
std::size_t kCalcCaseHash(KStringView sv)
//-----------------------------------------------------------------------------
{
	return kCaseHash(sv.data(), sv.size());
}

//-----------------------------------------------------------------------------
/// calculates a hash for case insensitive trimmed string
std::size_t kCalcCaseHashTrim(KStringView, KStringView svTrim = detail::kASCIISpaces);
//-----------------------------------------------------------------------------



namespace detail {
namespace casestring {

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
struct TrimWhiteSpaces
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	static constexpr KStringView svTrimLeft  { detail::kASCIISpaces };
	static constexpr KStringView svTrimRight { detail::kASCIISpaces };
};

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
struct NoTrim
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

	return kCaseCompare(left, right);
}

//-----------------------------------------------------------------------------
/// tests for equality on trimmed strings
template<typename TrimmingLeft, typename TrimmingRight>
bool kCaseEqualTrim(KStringView left, KStringView right)
//-----------------------------------------------------------------------------
{
	left  = detail::casestring::Trim<TrimmingLeft>(left);
	right = detail::casestring::Trim<TrimmingRight>(right);

	return kCaseEqual(left, right);
}

//-----------------------------------------------------------------------------
/// calculates a hash for the trimmed string
template<typename Trimming>
std::size_t kCalcCaseHashTrim(KStringView sv)
//-----------------------------------------------------------------------------
{
	sv = detail::casestring::Trim<Trimming>(sv);
	return kCalcCaseHash(sv);
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
		 typename std::enable_if<std::is_convertible<const T&, KStringView>::value == true, int>::type = 0>
inline bool operator==(KCaseStringViewBase<TrimmingLeft> left, const T& right)
//-----------------------------------------------------------------------------
{
	return kCaseEqualTrim<TrimmingLeft, detail::casestring::NoTrim>(left, right);
}

//-----------------------------------------------------------------------------
template<typename TrimmingRight, typename T,
         typename std::enable_if<std::is_convertible<const T&, KStringView>::value == true, int>::type = 0>
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
         typename std::enable_if<std::is_convertible<const T&, KStringView>::value == true, int>::type = 0>
inline bool operator!=(KCaseStringViewBase<TrimmingLeft> left, const T& right)
//-----------------------------------------------------------------------------
{
	return !(left == right);
}

//-----------------------------------------------------------------------------
template<typename TrimmingRight, typename T,
         typename std::enable_if<std::is_convertible<const T&, KStringView>::value == true, int>::type = 0>
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
         typename std::enable_if<std::is_convertible<const T&, KStringView>::value == true, int>::type = 0>
inline bool operator==(const KCaseStringBase<TrimmingLeft>& left, const T& right)
//-----------------------------------------------------------------------------
{
	return kCaseEqualTrim<TrimmingLeft, detail::casestring::NoTrim>(left.ToView(), right);
}

//-----------------------------------------------------------------------------
template<typename TrimmingRight, typename T,
         typename std::enable_if<std::is_convertible<const T&, KStringView>::value == true, int>::type = 0>
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
         typename std::enable_if<std::is_convertible<const T&, KStringView>::value == true, int>::type = 0>
inline bool operator!=(const KCaseStringBase<TrimmingLeft>& left, const T& right)
//-----------------------------------------------------------------------------
{
	return !(left == right);
}

//-----------------------------------------------------------------------------
template<typename TrimmingRight, typename T,
         typename std::enable_if<std::is_convertible<const T&, KStringView>::value == true, int>::type = 0>
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

} // end of namespace dekaf2


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

#if defined(DEKAF2_NO_GCC) || DEKAF2_GCC_VERSION_MAJOR > 6 
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

#if defined(DEKAF2_NO_GCC) || DEKAF2_GCC_VERSION_MAJOR > 6 
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

#include <boost/functional/hash.hpp>

namespace boost
{
	/// provide a boost::hash for KCaseStringViewBase
#if (BOOST_VERSION < 106400)
	template<typename Trimming> struct hash<dekaf2::KCaseStringViewBase<Trimming>> : public std::unary_function<dekaf2::KCaseStringBase<Trimming>, std::size_t>
#else
	template<typename Trimming> struct hash<dekaf2::KCaseStringViewBase<Trimming>>
#endif
	{
		std::size_t operator()(dekaf2::KStringView sv) const noexcept
		{
			return dekaf2::kCalcCaseHashTrim<Trimming>(sv);
		}
	};

	/// provide a boost::hash for KCaseStringBase
#if (BOOST_VERSION < 106400)
	template<typename Trimming> struct hash<dekaf2::KCaseStringBase<Trimming>> : public std::unary_function<dekaf2::KCaseStringBase<Trimming>, std::size_t>
#else
	template<typename Trimming> struct hash<dekaf2::KCaseStringBase<Trimming>>
#endif
	{
		std::size_t operator()(dekaf2::KStringView sv) const noexcept
		{
			return dekaf2::kCalcCaseHashTrim<Trimming>(sv);
		}
	};

} // end of namespace boost

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


