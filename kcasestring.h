/*
//-----------------------------------------------------------------------------//
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
/// compares case insensitive, trims strings before compare
int kCaseCompareTrim(KStringView left, KStringView right, KStringView svTrim = " \t\r\n\b");
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// tests trimmed strings for case insensitive equality
bool kCaseEqualTrim(KStringView left, KStringView right, KStringView svTrim = " \t\r\n\b");
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
/// compares case insensitive, trims strings before compare,
/// assuming right string trimmed and in lowercase
int kCaseCompareTrimLeft(KStringView left, KStringView right, KStringView svTrim = " \t\r\n\b");
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// tests trimmed strings for case insensitive equality
/// assuming right string trimmed and in lowercase
bool kCaseEqualTrimLeft(KStringView left, KStringView right, KStringView svTrim = " \t\r\n\b");
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// calculates a hash for case insensitive string
std::size_t kCalcCaseHash(KStringView);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// calculates a hash for case insensitive trimmed string
std::size_t kCalcCaseHashTrim(KStringView, KStringView svTrim = " \t\r\n\b");
//-----------------------------------------------------------------------------



namespace detail {
namespace casestring {

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
struct TrimWhiteSpaces
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	static constexpr KStringView svTrimLeft  {" \t\n\r\b"};
	static constexpr KStringView svTrimRight {" \t\n\r\b"};
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
template<typename Trimming>
class KCaseStringViewBase : public KStringView
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	//-----------------------------------------------------------------------------
	template<typename ...Args>
	KCaseStringViewBase(Args&& ...args)
	//-----------------------------------------------------------------------------
	    : KStringView(std::forward<Args>(args)...)
	{}

	//-----------------------------------------------------------------------------
	int compare(KStringView other)
	//-----------------------------------------------------------------------------
	{
		return kCaseCompareTrim<Trimming, detail::casestring::NoTrim>(*this, other);
	}

	//-----------------------------------------------------------------------------
	int compare(KCaseStringViewBase other)
	//-----------------------------------------------------------------------------
	{
		return kCaseCompareTrim<Trimming, decltype(other)::Trimming>(*this, other);
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
template<typename TrimmingLeft>
inline bool operator==(KCaseStringViewBase<TrimmingLeft> left, const char* right)
//-----------------------------------------------------------------------------
{
	return kCaseEqualTrim<TrimmingLeft, detail::casestring::NoTrim>(left, right);
}

//-----------------------------------------------------------------------------
template<typename TrimmingRight>
inline bool operator==(const char* left, const KCaseStringViewBase<TrimmingRight> right)
//-----------------------------------------------------------------------------
{
	return right == left;
}

//-----------------------------------------------------------------------------
template<typename TrimmingLeft>
inline bool operator==(KCaseStringViewBase<TrimmingLeft> left, KStringView right)
//-----------------------------------------------------------------------------
{
	return kCaseEqualTrim<TrimmingLeft, detail::casestring::NoTrim>(left, right);
}

//-----------------------------------------------------------------------------
template<typename TrimmingRight>
inline bool operator==(KStringView left, const KCaseStringViewBase<TrimmingRight> right)
//-----------------------------------------------------------------------------
{
	return right == left;
}

//-----------------------------------------------------------------------------
template<typename TrimmingLeft>
inline bool operator==(KCaseStringViewBase<TrimmingLeft> left, KString right)
//-----------------------------------------------------------------------------
{
	return kCaseEqualTrim<TrimmingLeft, detail::casestring::NoTrim>(left, right.ToView());
}

//-----------------------------------------------------------------------------
template<typename TrimmingRight>
inline bool operator==(KString left, const KCaseStringViewBase<TrimmingRight> right)
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
template<typename TrimmingLeft>
inline bool operator!=(KCaseStringViewBase<TrimmingLeft> left, const char* right)
//-----------------------------------------------------------------------------
{
	return !(left == right);
}

//-----------------------------------------------------------------------------
template<typename TrimmingRight>
inline bool operator!=(const char* left, const KCaseStringViewBase<TrimmingRight> right)
//-----------------------------------------------------------------------------
{
	return right != left;
}

//-----------------------------------------------------------------------------
template<typename TrimmingLeft>
inline bool operator!=(KCaseStringViewBase<TrimmingLeft> left, KStringView right)
//-----------------------------------------------------------------------------
{
	return !(left == right);
}

//-----------------------------------------------------------------------------
template<typename TrimmingRight>
inline bool operator!=(KStringView left, const KCaseStringViewBase<TrimmingRight> right)
//-----------------------------------------------------------------------------
{
	return right != left;
}

//-----------------------------------------------------------------------------
template<typename TrimmingLeft>
inline bool operator!=(KCaseStringViewBase<TrimmingLeft> left, KString right)
//-----------------------------------------------------------------------------
{
	return !(left == right);
}

//-----------------------------------------------------------------------------
template<typename TrimmingRight>
inline bool operator!=(KString left, const KCaseStringViewBase<TrimmingRight> right)
//-----------------------------------------------------------------------------
{
	return right == left;
}


//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// A string class that compares and hashes case insensitive and trimmed
template<typename Trimming>
class KCaseStringBase : public KString
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	//-----------------------------------------------------------------------------
	template<typename ...Args>
	KCaseStringBase(Args&& ...args)
	//-----------------------------------------------------------------------------
	    : KString(std::forward<Args>(args)...)
	{}

	//-----------------------------------------------------------------------------
	int compare(KString other)
	//-----------------------------------------------------------------------------
	{
		return ToView().compare(other);
	}

	//-----------------------------------------------------------------------------
	KCaseStringViewBase<Trimming> ToView()
	//-----------------------------------------------------------------------------
	{
		return KCaseStringViewBase<Trimming>(this->data(), this->size());
	}

};

//-----------------------------------------------------------------------------
template<typename TrimmingLeft, typename TrimmingRight>
inline bool operator==(KCaseStringBase<TrimmingLeft> left, KCaseStringBase<TrimmingRight> right)
//-----------------------------------------------------------------------------
{
	return kCaseEqualTrim(left.ToView(), right.ToView());
}

//-----------------------------------------------------------------------------
template<typename TrimmingLeft>
inline bool operator==(KCaseStringBase<TrimmingLeft> left, const char* right)
//-----------------------------------------------------------------------------
{
	return kCaseEqualTrim(left.ToView(), right);
}

//-----------------------------------------------------------------------------
template<typename TrimmingRight>
inline bool operator==(const char* left, const KCaseStringBase<TrimmingRight> right)
//-----------------------------------------------------------------------------
{
	return right == left;
}

//-----------------------------------------------------------------------------
template<typename TrimmingLeft>
inline bool operator==(KCaseStringBase<TrimmingLeft> left, KStringView right)
//-----------------------------------------------------------------------------
{
	return kCaseEqualTrim(left.ToView(), right);
}

//-----------------------------------------------------------------------------
template<typename TrimmingRight>
inline bool operator==(KStringView left, const KCaseStringBase<TrimmingRight> right)
//-----------------------------------------------------------------------------
{
	return right == left;
}

//-----------------------------------------------------------------------------
template<typename TrimmingLeft>
inline bool operator==(KCaseStringBase<TrimmingLeft> left, KString right)
//-----------------------------------------------------------------------------
{
	return kCaseEqualTrim(left.ToView(), right.ToView());
}

//-----------------------------------------------------------------------------
template<typename TrimmingRight>
inline bool operator==(KString left, const KCaseStringBase<TrimmingRight> right)
//-----------------------------------------------------------------------------
{
	return right == left;
}

//-----------------------------------------------------------------------------
template<typename TrimmingLeft, typename TrimmingRight>
inline bool operator!=(KCaseStringBase<TrimmingLeft> left, KCaseStringBase<TrimmingRight> right)
//-----------------------------------------------------------------------------
{
	return !(left == right);
}

//-----------------------------------------------------------------------------
template<typename TrimmingLeft>
inline bool operator!=(KCaseStringBase<TrimmingLeft> left, const char* right)
//-----------------------------------------------------------------------------
{
	return !(left == right);
}

//-----------------------------------------------------------------------------
template<typename TrimmingRight>
inline bool operator!=(const char* left, const KCaseStringBase<TrimmingRight> right)
//-----------------------------------------------------------------------------
{
	return right != left;
}

//-----------------------------------------------------------------------------
template<typename TrimmingLeft>
inline bool operator!=(KCaseStringBase<TrimmingLeft> left, KStringView right)
//-----------------------------------------------------------------------------
{
	return !(left == right);
}

//-----------------------------------------------------------------------------
template<typename TrimmingRight>
inline bool operator!=(KStringView left, const KCaseStringBase<TrimmingRight> right)
//-----------------------------------------------------------------------------
{
	return right != left;
}

//-----------------------------------------------------------------------------
template<typename TrimmingLeft>
inline bool operator!=(KCaseStringBase<TrimmingLeft> left, KString right)
//-----------------------------------------------------------------------------
{
	return !(left == right);
}

//-----------------------------------------------------------------------------
template<typename TrimmingRight>
inline bool operator!=(KString left, const KCaseStringBase<TrimmingRight> right)
//-----------------------------------------------------------------------------
{
	return right == left;
}


using KCaseStringView     = KCaseStringViewBase<detail::casestring::NoTrim>;
using KCaseString         = KCaseStringBase<detail::casestring::NoTrim>;
using KCaseTrimStringView = KCaseStringViewBase<detail::casestring::TrimWhiteSpaces>;
using KCaseTrimString     = KCaseStringBase<detail::casestring::TrimWhiteSpaces>;

} // end of namespace dekaf2


namespace std
{
	/// provide a std::hash for KString
	template<typename Trimming> struct hash<dekaf2::KCaseStringBase<Trimming>>
	{
		typedef dekaf2::KCaseStringBase<Trimming> argument_type;
		typedef std::size_t result_type;
		result_type operator()(argument_type const& s) const noexcept
		{
			return dekaf2::kCalcCaseHashTrim<Trimming>(s);
		}
	};

} // end of namespace std

#include <boost/functional/hash.hpp>

namespace boost
{
	/// provide a boost::hash for KString
	template<typename Trimming> struct hash<dekaf2::KCaseStringBase<Trimming>> : public std::unary_function<dekaf2::KCaseStringBase<Trimming>, std::size_t>
	{
		typedef dekaf2::KCaseStringBase<Trimming> argument_type;
		typedef std::size_t result_type;
		result_type operator()(argument_type const& s) const noexcept
		{
			return dekaf2::kCalcCaseHashTrim<Trimming>(s);
		}
	};

} // end of namespace boost


