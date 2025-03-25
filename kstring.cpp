/*
//
// DEKAF(tm): Lighter, Faster, Smarter(tm)
//
// Copyright (c) 2000-2015, Ridgeware, Inc.
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
//
*/

#include "kstring.h"
#include "kstringutils.h"
#include "klog.h"
#include "kregex.h"
#include "kutf.h"
#include "kctype.h"

DEKAF2_NAMESPACE_BEGIN

#ifdef DEKAF2_REPEAT_CONSTEXPR_VARIABLE

constexpr KString::size_type KString::npos;
constexpr KString::value_type KString::s_0ch;

#endif

KString::value_type KString::s_0ch_v { '\0' };

//------------------------------------------------------------------------------
void KString::log_exception(const std::exception& e, const char* sWhere)
//------------------------------------------------------------------------------
{
#ifdef DEKAF2_WITH_KLOG
	KLog::getInstance().Exception(e, sWhere);
#else
	detail::IntException_uNuSuAlNaMe(e, sWhere);
#endif
}

//------------------------------------------------------------------------------
void KString::resize_uninitialized(size_type n)
//------------------------------------------------------------------------------
{
	kResizeUninitialized(*this, n);
}

//------------------------------------------------------------------------------
// the method to which (nearly) all replaces reduce
KString& KString::replace(size_type pos, size_type n, KStringView sv)
//------------------------------------------------------------------------------
{
	DEKAF2_TRY_EXCEPTION
	m_rep.replace(pos, n, sv.data(), sv.size());
	DEKAF2_LOG_EXCEPTION
	return *this;
}

//------------------------------------------------------------------------------
KString& KString::replace(iterator i1, iterator i2, KStringView sv)
//------------------------------------------------------------------------------
{
	// we turn this into an indexed replace because
	// the std::string iterator replace does not test for
	// iterator out of range and segfaults if out of range..
	auto pos = static_cast<size_type>(i1 - begin());
	auto n = static_cast<size_type>(i2 - i1);
	return replace (pos, n, sv);
}

//------------------------------------------------------------------------------
KString& KString::replace(size_type pos, size_type n1, size_type n2, value_type c)
//------------------------------------------------------------------------------
{
	DEKAF2_TRY_EXCEPTION
	m_rep.replace(pos, n1, n2, c);
	DEKAF2_LOG_EXCEPTION
	return *this;
}

//------------------------------------------------------------------------------
KString& KString::replace(iterator i1, iterator i2, std::initializer_list<value_type> il)
//------------------------------------------------------------------------------
{
	DEKAF2_TRY_EXCEPTION
	// we turn this into an indexed replace because
	// the std::string iterator replace does not test for
	// iterator out of range and segfaults if out of range..
	auto pos = static_cast<size_type>(i1 - begin());
	auto n = static_cast<size_type>(i2 - i1);
	m_rep.replace(pos, n, il);
	DEKAF2_LOG_EXCEPTION
	return *this;
}

//------------------------------------------------------------------------------
KString KString::substr(size_type pos, size_type n) const &
//------------------------------------------------------------------------------
{
	return DEKAF2_UNLIKELY(pos > size())
		? KString()
		: KString(data() + pos, std::min(n, size() - pos));
}

//------------------------------------------------------------------------------
KString KString::substr(size_type pos, size_type n/*=npos*/) &&
//------------------------------------------------------------------------------
{
	if (DEKAF2_UNLIKELY(pos >= size()))
	{
		clear();
	}
	else
	{
		erase(0, pos);

		if (n < size() - pos)
		{
			resize(n);
		}
	}

	return std::move(*this);
}

//------------------------------------------------------------------------------
KString::size_type KString::copy(value_type* s, size_type n, size_type pos) const
//------------------------------------------------------------------------------
{
	DEKAF2_TRY_EXCEPTION
	return m_rep.copy(s, n, pos);
	DEKAF2_LOG_EXCEPTION
	return 0;
}

//------------------------------------------------------------------------------
KString& KString::insert(size_type pos, KStringView sv)
//------------------------------------------------------------------------------
{
	DEKAF2_TRY_EXCEPTION
	m_rep.insert(pos, sv.data(), sv.size());
	DEKAF2_LOG_EXCEPTION
	return *this;
}

//------------------------------------------------------------------------------
KString& KString::insert(size_type pos, size_type n, value_type c)
//------------------------------------------------------------------------------
{
	DEKAF2_TRY_EXCEPTION
	m_rep.insert(pos, n, c);
	DEKAF2_LOG_EXCEPTION
	return *this;
}

//------------------------------------------------------------------------------
KString::iterator KString::insert(iterator it, value_type c)
//------------------------------------------------------------------------------
{
	// we turn this into an indexed insert because
	// the std::string iterator insert does not test for
	// iterator out of range and segfaults if out of range..
	auto pos = static_cast<size_type>(it - begin());
	// std::string::insert(pos, ..) only throws when pos > size()
	if (DEKAF2_LIKELY(pos <= size()))
	{
		m_rep.insert(pos, 1, c);
		return it;
	}
	else
	{
		kWarning("iterator out of range");
		return end();
	}
}

//------------------------------------------------------------------------------
KString::iterator KString::insert(iterator it, std::initializer_list<value_type> il)
//------------------------------------------------------------------------------
{
	// this check is not part of the standard, and there is no index version of
	// the insert of an initializer list, therefore we check explicitly
	if (DEKAF2_UNLIKELY((it < begin() || it > end())))
	{
		kWarning("iterator out of range");
		return end();
	}

#if defined(DEKAF2_GLIBCXX_VERSION_MAJOR) && \
	(DEKAF2_GLIBCXX_VERSION_MAJOR < 10)
	// older versions of libstdc++ do not return an iterator, but void
	// see https://gcc.gnu.org/bugzilla/show_bug.cgi?id=83328
	//
	// workaround:
	// find index, insert, and calculate an iterator to that index (might point
	// to somewhere else because of reallocs)
	//
	auto pos = static_cast<size_type>(it - begin());
	m_rep.insert(it, il);
	return begin() + pos;
#else
	return m_rep.insert(it, il);
#endif
}

//------------------------------------------------------------------------------
KString& KString::erase(size_type pos, size_type n)
//------------------------------------------------------------------------------
{
	DEKAF2_TRY_EXCEPTION
	m_rep.erase(pos, n);
	DEKAF2_LOG_EXCEPTION
	return *this;
}

//------------------------------------------------------------------------------
KString::iterator KString::erase(iterator position)
//------------------------------------------------------------------------------
{
	// we turn this into an indexed erase because
	// the std::string iterator erase does not test for
	// iterator out of range and segfaults if out of range..
	auto pos = static_cast<size_type>(position - begin());

	if (DEKAF2_LIKELY(pos <= size()))
	{
		m_rep.erase(pos, 1);
		return position;
	}
	else
	{
		kWarning("iterator out of range");
		return end();
	}
}

//------------------------------------------------------------------------------
KString::iterator KString::erase(iterator first, iterator last)
//------------------------------------------------------------------------------
{
	// we turn this into an indexed erase because
	// the std::string iterator erase does not test for
	// iterator out of range and segfaults if out of range..
	auto pos = static_cast<size_type>(first - begin());

	if (DEKAF2_LIKELY(pos <= size()))
	{
		auto n = static_cast<size_type>(last - first);
		m_rep.erase(pos, n);
		return first;
	}
	else
	{
		kWarning("iterator out of range");
		return end();
	}
}

//----------------------------------------------------------------------
// we keep a KString version of kReplace, because it uses an unchecked subscript access
KString::size_type KString::Replace(
		value_type chSearch,
		value_type chReplace,
		size_type pos,
		bool bReplaceAll)
//----------------------------------------------------------------------
{
	size_type iReplaced{0};

	while ((pos = find(chSearch, pos)) != npos)
	{
		m_rep[pos] = chReplace;
		++pos;
		++iReplaced;

		if (!bReplaceAll)
		{
			break;
		}
	}

	return iReplaced;

} // Replace

//----------------------------------------------------------------------
KString::size_type KString::ReplaceRegex(const KStringView sRegEx, const KStringView sReplaceWith, bool bReplaceAll)
//----------------------------------------------------------------------
{
	return KRegex::Replace(m_rep, sRegEx, sReplaceWith, bReplaceAll);
}

//----------------------------------------------------------------------
KStringViewZ KString::ToView(size_type pos) const
//----------------------------------------------------------------------
{
	KStringViewZ sv(*this);
	sv.remove_prefix(pos);
	return sv;

} // ToView

//----------------------------------------------------------------------
KStringView KString::ToView(size_type pos, size_type n) const
//----------------------------------------------------------------------
{
	if (DEKAF2_UNLIKELY(pos > size()))
	{
		kDebug (1, "pos ({}) exceeds size ({})", pos, size());
		pos = size();
	}
	
	if (DEKAF2_UNLIKELY(n > size() || pos + n > size()))
	{
		n = size() - pos;
	}

	return KStringView(data() + pos, n);

} // ToView

/*
 for the following case folds, the utf8 sizes of
 upper and lower codepoint are not the same -
 therefore we cannot do a case change in place..

 size 2 != size 1, for fold from İ to i (toupper)
 size 2 != size 1, for fold from ı to I (tolower)
 size 2 != size 1, for fold from ſ to S (tolower)
 size 2 != size 3, for fold from Ⱥ to ⱥ (toupper)
 size 2 != size 3, for fold from Ⱦ to ⱦ (toupper)
 size 2 != size 3, for fold from ȿ to Ȿ (tolower)
 size 2 != size 3, for fold from ɐ to Ɐ (tolower)
 size 2 != size 3, for fold from ɑ to Ɑ (tolower)
 size 2 != size 3, for fold from ɒ to Ɒ (tolower)
 size 2 != size 3, for fold from ɜ to Ɜ (tolower)
 size 2 != size 3, for fold from ɡ to Ɡ (tolower)
 size 2 != size 3, for fold from ɥ to Ɥ (tolower)
 size 2 != size 3, for fold from ɦ to Ɦ (tolower)
 size 2 != size 3, for fold from ɫ to Ɫ (tolower)
 size 2 != size 3, for fold from ɬ to Ɬ (tolower)
 size 2 != size 3, for fold from ɱ to Ɱ (tolower)
 size 2 != size 3, for fold from ɽ to Ɽ (tolower)
 size 2 != size 3, for fold from ʂ to Ʂ (tolower)
 size 2 != size 3, for fold from ʇ to Ʇ (tolower)
 size 2 != size 3, for fold from ʝ to Ʝ (tolower)
 size 2 != size 3, for fold from ʞ to Ʞ (tolower)
 size 3 != size 2, for fold from ᲀ to В (tolower)
 size 3 != size 2, for fold from ᲁ to Д (tolower)
 size 3 != size 2, for fold from ᲂ to О (tolower)
 size 3 != size 2, for fold from ᲃ to С (tolower)
 size 3 != size 2, for fold from ᲅ to Т (tolower)
 size 3 != size 2, for fold from ᲆ to Ъ (tolower)
 size 3 != size 2, for fold from ᲇ to Ѣ (tolower)
 size 3 != size 2, for fold from ẞ to ß (toupper)
 size 3 != size 2, for fold from ι to Ι (tolower)
 size 3 != size 2, for fold from Ω to ω (toupper)
 size 3 != size 1, for fold from K to k (toupper)
 size 3 != size 2, for fold from Å to å (toupper)
 */

//----------------------------------------------------------------------
KString& KString::PadLeft(size_t iWidth, value_type chPad) &
//----------------------------------------------------------------------
{
	kPadLeft(m_rep, iWidth, chPad);
	return *this;
}

//----------------------------------------------------------------------
KString& KString::PadRight(size_t iWidth, value_type chPad) &
//----------------------------------------------------------------------
{
	kPadRight(m_rep, iWidth, chPad);
	return *this;
}

//----------------------------------------------------------------------
KString& KString::TrimLeft() &
//----------------------------------------------------------------------
{
	kTrimLeft(*this);
	return *this;
}

//----------------------------------------------------------------------
KString& KString::TrimLeft(value_type chTrim) &
//----------------------------------------------------------------------
{
	kTrimLeft(*this, [chTrim](value_type ch){ return ch == chTrim; } );
	return *this;
}

//----------------------------------------------------------------------
KString& KString::TrimLeft(KStringView sTrim) &
//----------------------------------------------------------------------
{
	kTrimLeft(*this, sTrim);
	return *this;
}

//----------------------------------------------------------------------
KString& KString::TrimRight() &
//----------------------------------------------------------------------
{
	kTrimRight(*this);
	return *this;
}

//----------------------------------------------------------------------
KString& KString::TrimRight(value_type chTrim) &
//----------------------------------------------------------------------
{
	kTrimRight(*this, [chTrim](value_type ch){ return ch == chTrim; } );
	return *this;
}

//----------------------------------------------------------------------
KString& KString::TrimRight(KStringView sTrim) &
//----------------------------------------------------------------------
{
	kTrimRight(*this, sTrim);
	return *this;
}

//----------------------------------------------------------------------
KString& KString::Trim() &
//----------------------------------------------------------------------
{
	kTrim(*this);
	return *this;
}

//----------------------------------------------------------------------
KString& KString::Trim(value_type chTrim) &
//----------------------------------------------------------------------
{
	kTrim(*this, [chTrim](value_type ch){ return ch == chTrim; } );
	return *this;
}

//----------------------------------------------------------------------
KString& KString::Trim(KStringView sTrim) &
//----------------------------------------------------------------------
{
	kTrim(*this, sTrim);
	return *this;
}

//----------------------------------------------------------------------
KString& KString::Collapse() &
//----------------------------------------------------------------------
{
	return kCollapse(*this);
	return *this;
}

//----------------------------------------------------------------------
KString& KString::Collapse(KStringView svCollapse, value_type chTo) &
//----------------------------------------------------------------------
{
	kCollapse(*this, svCollapse, chTo);
	return *this;
}

//----------------------------------------------------------------------
KString& KString::CollapseAndTrim() &
//----------------------------------------------------------------------
{
	return kCollapseAndTrim(*this);
	return *this;
}

//----------------------------------------------------------------------
KString& KString::CollapseAndTrim(KStringView svCollapse, value_type chTo) &
//----------------------------------------------------------------------
{
	kCollapseAndTrim(*this, svCollapse, chTo);
	return *this;
}

//----------------------------------------------------------------------
bool KString::ClipAt(KStringView sClipAt)
//----------------------------------------------------------------------
{
	size_type pos = find(sClipAt);
	if (pos != npos)
	{
		erase(pos);
		return (true); // we modified the string
	}
	return (false); // we did not modify the string

} // ClipAt

//----------------------------------------------------------------------
bool KString::ClipAtReverse(KStringView sClipAtReverse)
//----------------------------------------------------------------------
{
	erase(0, find(sClipAtReverse));
	return true;

} // ClipAtReverse

#if DEKAF2_IS_GCC
#pragma GCC diagnostic push
#ifdef DEKAF2_HAS_WARN_STRINGOP_OVERFLOW
#pragma GCC diagnostic ignored "-Wstringop-overflow"
#endif
#endif
//----------------------------------------------------------------------
KString::size_type KString::RemoveChars(KStringView sChars)
//----------------------------------------------------------------------
{
	auto iOrigLen = size();
	auto pos      = iOrigLen;

	for (;;)
	{
		pos = find_last_of(sChars, pos);

		if (DEKAF2_UNLIKELY(pos >= size()))
		{
			break;
		}

		m_rep.erase(pos, 1);
	}

	return iOrigLen - size();

} // RemoveChars
#if DEKAF2_IS_GCC
#pragma GCC diagnostic pop
#endif

//-----------------------------------------------------------------------------
float KString::Float() const noexcept
//-----------------------------------------------------------------------------
{
	return std::strtof(c_str(), nullptr);
}

//-----------------------------------------------------------------------------
double KString::Double() const noexcept
//-----------------------------------------------------------------------------
{
	return std::strtod(c_str(), nullptr);
}

//-----------------------------------------------------------------------------
KString KString::to_string(float f)
//-----------------------------------------------------------------------------
{
	return kFormat("{}", f);
}

//-----------------------------------------------------------------------------
KString KString::to_string(double f)
//-----------------------------------------------------------------------------
{
	return kFormat("{}", f);
}

//-----------------------------------------------------------------------------
KString KString::signed_to_string(int64_t i, uint16_t iBase, bool bZeroPad, bool bUppercase)
//-----------------------------------------------------------------------------
{
	return kSignedToString<KString>(i, iBase, bZeroPad, bUppercase);
}

//-----------------------------------------------------------------------------
KString KString::unsigned_to_string(uint64_t i, uint16_t iBase, bool bZeroPad, bool bUppercase)
//-----------------------------------------------------------------------------
{
	return kUnsignedToString<KString>(i, iBase, bZeroPad, bUppercase);
}

static_assert(std::is_nothrow_move_constructible<KString>::value,
			  "KString is intended to be nothrow move constructible, but is not!");

DEKAF2_NAMESPACE_END


//----------------------------------------------------------------------
std::istream& std::getline(std::istream& stream, DEKAF2_PREFIX KString& str)
//----------------------------------------------------------------------
{
	return std::getline(stream, str.str());
}

//----------------------------------------------------------------------
std::istream& std::getline(std::istream& stream, DEKAF2_PREFIX KString& str, DEKAF2_PREFIX KString::value_type delimiter)
//----------------------------------------------------------------------
{
	return std::getline(stream, str.str(), delimiter);
}

