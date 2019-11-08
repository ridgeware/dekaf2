/*
//
// DEKAF(tm): Lighter, Faster, Smarter(tm)
//
// Copyright (c) 2019, Ridgeware, Inc.
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

/// @file kversion.h
/// Helper class for version number parsing and comparison

#pragma once

#include "kstringview.h"
#include "kstring.h"
#include <vector>

namespace dekaf2 {

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Helper class for version number parsing and comparison
class KVersion
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

	static constexpr KStringView sDefaultSeparators = ".,-:/|\\";

//----------
public:
//----------

	using value_type     = uint32_t;
	using storage_type   = std::vector<value_type>;
	using size_type      = storage_type::size_type;
	using iterator       = storage_type::iterator;
	using const_iterator = storage_type::const_iterator;

	/// ctor for empty version
	KVersion() = default;

	/// construct from a string
	explicit KVersion(KStringView sVersion, KStringView sSeparators = sDefaultSeparators)
	{
		Parse(sVersion, sSeparators);
	}

	/// construct from a string
	explicit KVersion(const KString& sVersion, KStringView sSeparators = sDefaultSeparators)
	{
		Parse(sVersion, sSeparators);
	}

	/// construct from char*
	KVersion(const char* sVersion, KStringView sSeparators = sDefaultSeparators)
	{
		Parse(sVersion, sSeparators);
	}

	/// construct from a list of ints
	KVersion(std::initializer_list<value_type> il)
	: m_Version(std::move(il))
	{
	}

	/// reset version
	void clear()
	{
		m_Version.clear();
	}

	/// is this class empty?
	bool empty() const
	{
		return m_Version.empty();
	}

	/// return count of elements
	size_type size() const
	{
		return m_Version.size();
	}

	/// get version value at position iPos
	value_type operator[](size_type iPos) const;

	/// set version value at position iPos, auto expands version store if pos exceeds current scheme
	value_type& operator[](size_type iPos);

	const_iterator begin() const
	{
		return m_Version.begin();
	}

	const_iterator end() const
	{
		return m_Version.end();
	}

	iterator begin()
	{
		return m_Version.begin();
	}

	iterator end()
	{
		return m_Version.end();
	}

	/// parse version string
	bool Parse(KStringView sVersion, KStringView sSeparators = sDefaultSeparators);

	/// generate version string
	KString Serialize(KString::value_type chSeparator = '.') const;

	/// remove trailing zeroes
	KVersion& Trim();

//----------
private:
//----------

	storage_type m_Version;

}; // KVersion

extern bool operator==(const KVersion& left, const KVersion& right);
extern bool operator<(const KVersion& left, const KVersion& right);

inline bool operator!=(const KVersion& left, const KVersion& right)
{
	return !operator==(left, right);
}

inline bool operator>(const KVersion& left, const KVersion& right)
{
	return operator<(right, left);
}

inline bool operator<=(const KVersion& left, const KVersion& right)
{
	return !operator<(right, left);
}

inline bool operator>=(const KVersion& left, const KVersion& right)
{
	return !operator<(left, right);
}

} // end of namespace dekaf2

