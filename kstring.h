/*
//
// DEKAF(tm): Lighter, Faster, Smarter(tm)
//
// Copyright (c) 2000-2017, Ridgeware, Inc.
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

#pragma once

#include "kcppcompat.h"
#include <string>
#ifdef DEKAF2_HAS_CPP_17
#include <experimental/string_view>
#else // prepare to use re2's StringPiece as string_view
#include <re2/re2.h>
#endif
#include <vector>
#include <cstdarg>
#include <cstring>
#include <cctype>
#include <istream>
#include "kformat.h"


namespace dekaf2
{

#ifdef DEKAF2_HAS_CPP_17
using KStringView = std::experimental::string_view;
#else
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// an extended StringPiece with the methods of
/// C++17's std::string_view
class KStringView : public re2::StringPiece
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
public:
	KStringView() {}
	KStringView(const std::string& str) : StringPiece(str) {}
	KStringView(const char* str) : StringPiece(str) {}
	KStringView(const char* str, size_type len) : StringPiece(str, len) {}

// TODO implement find_first_of, find_last_of family of methods
};

#endif

bool kStartsWith(KStringView sInput, KStringView sPattern);
bool kEndsWith(KStringView sInput, KStringView sPattern);

bool kStrIn (const char* sNeedle, const char* sHaystack, char iDelim=',');

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// dekaf's own string class - a wrapper around std::string
/// which handles most error cases in a benign way
class KString 
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	typedef std::string string_type;

//----------
public:
//----------
	typedef string_type::traits_type            traits_type;
	typedef string_type::value_type             value_type;
	typedef string_type::allocator_type         allocator_type;
	typedef string_type::size_type              size_type;
	typedef string_type::difference_type        difference_type;
	typedef string_type::reference              reference;
	typedef string_type::const_reference        const_reference;
	typedef string_type::pointer                pointer;
	typedef string_type::const_pointer          const_pointer;
	typedef string_type::iterator               iterator;
	typedef string_type::const_iterator         const_iterator;
	typedef string_type::const_reverse_iterator const_reverse_iterator;
	typedef string_type::reverse_iterator       reverse_iterator;

	static const size_type npos = string_type::npos;

	//Iterators
	inline iterator begin() { return m_rep.begin(); }
	inline const_iterator begin() const { return m_rep.begin(); }
	inline iterator end() { return m_rep.end(); }
	inline const_iterator end() const { return m_rep.end(); }
	inline reverse_iterator rbegin() { return m_rep.rbegin(); }
	inline const_reverse_iterator rbegin() const { return m_rep.rbegin(); }
	inline reverse_iterator rend() { return m_rep.rend(); }
	inline const_reverse_iterator rend() const { return m_rep.rend(); }
	inline const_iterator cbegin() const noexcept { return m_rep.cbegin(); }
	inline const_iterator cend() const noexcept { return m_rep.cend(); }
	inline const_reverse_iterator crbegin() const noexcept { return m_rep.crbegin(); }
	inline const_reverse_iterator crend() const noexcept { return m_rep.crend(); }

	inline size_type size() const { return m_rep.size(); }
	inline size_type length() const { return m_rep.length(); }
	inline size_type max_size() const { return m_rep.max_size(); }
	inline void resize(size_type n, value_type c) { m_rep.resize(n, c); }
	inline void resize(size_type n) { m_rep.resize(n); }
	inline size_type capacity() const { return m_rep.capacity(); }
	inline void reserve(size_type res_arg = 0) { m_rep.reserve(res_arg); }
	inline void clear() { m_rep.clear(); }
	inline bool empty() const { return m_rep.empty(); }
	inline void shrink_to_fit() { m_rep.shrink_to_fit(); }
	
	inline const_reference operator[] (size_type pos) const { return m_rep[pos]; }
	inline reference operator[](size_type pos) { return m_rep[pos]; }
	inline const_reference at(size_type n) const { return m_rep.at(n); }
	inline reference at(size_type n) { return m_rep.at(n); }
	inline const_reference back() const { return m_rep.back(); }
	inline reference back() { return m_rep.back(); }
	inline const_reference front() const { return m_rep.front(); }
	inline reference front() { return m_rep.front(); }

	//Constructors
	KString () {}
	KString (const KString& str) : m_rep(str.m_rep){}
	KString (const KString& str, size_type pos, size_type len = npos) : m_rep(str.m_rep, (pos > str.size()) ? str.size() : pos, len){}
	KString (const string_type& sStr) : m_rep(sStr){}
	KString (size_type iCount, value_type ch) : m_rep(iCount, ch){}
	KString (const value_type* s) : m_rep(s?s:""){}
	KString (const value_type* pszString, size_type iCount) : m_rep(pszString?pszString:"", pszString?iCount:0){}
	KString (const value_type* pszString, size_type iRoff, size_type iCount) : m_rep(pszString?pszString:"", pszString?iRoff:0, pszString?iCount:0){}
	template<class _InputIterator>
		KString (_InputIterator first, _InputIterator last) : m_rep(first, last) {}
	KString (KString&& str) noexcept : m_rep(std::move(str.m_rep)){}
	KString (string_type&& sStr) noexcept : m_rep(std::move(sStr)){}
	KString (std::initializer_list<value_type> il) : m_rep(il) {}
	KString (KStringView sv) : m_rep(sv.data(), sv.size()) {}

	//operator+=
	KString& operator+= (const KString& str){ m_rep += str.m_rep; return *this; }
	KString& operator+= (const string_type& str){ m_rep += str; return *this; }
	KString& operator+= (const value_type ch){ m_rep += ch; return *this; }
	KString& operator+= (const value_type *s){ if (s) m_rep += s; return *this; }
	KString& operator+= (std::initializer_list<value_type> il) { m_rep += il; return *this; }
	KString& operator+= (KStringView sv) { m_rep.append(sv.data(), sv.size()); return *this; }

	//operator=
	KString& operator= (const KString& str) { m_rep = str.m_rep; return *this; }
	KString& operator= (const string_type& sStr) { m_rep = sStr; return *this; }
	KString& operator= (const value_type* pszStr) { if (pszStr) m_rep = pszStr; return *this; }
	KString& operator= (const value_type c) { m_rep = c; return *this; }
	KString& operator= (KString&& str) noexcept { m_rep = std::move(str.m_rep); return *this; }
	KString& operator= (string_type&& sStr) noexcept { m_rep = std::move(sStr); return *this; }
	KString& operator= (std::initializer_list<value_type> il) { m_rep = il; return *this; }
	KString& operator= (KStringView sv) { m_rep.assign(sv.data(), sv.size()); return *this; }

	//std methods
	KString& append(const KString& str){ m_rep.append(str.m_rep); return *this; }
	KString& append(const KString& str, size_type pos, size_type n = npos) { return append(str.m_rep, pos, n); }
	KString& append(const string_type& str){ m_rep.append(str); return *this; }
	KString& append(const string_type& str, size_type pos, size_type n = npos);
	KString& append(const value_type* str) { m_rep.append(str); return *this; }
	KString& append(const value_type* str, size_type n) { m_rep.append(str, n); return *this; }
	KString& append(size_type n, value_type ch){ m_rep.append(n, ch); return *this; }
	template<class _InputIterator>
		KString& append(_InputIterator first, _InputIterator last) { m_rep.append(first, last); return *this; }
	KString& append(std::initializer_list<value_type> il) { m_rep.append(il); return *this; }
	KString& append(KStringView sv) { m_rep.append(sv.data(), sv.size()); return *this; }

	KString& push_back(const value_type chPushBack) { m_rep.push_back(chPushBack); return *this; }
	void pop_back() { m_rep.pop_back(); }

	KString& assign(const KString& str) { m_rep.assign(str.m_rep); return *this; }
	KString& assign(const KString& str, size_type pos, size_type n = npos) { return assign(str.m_rep, pos, n); }
	KString& assign(const string_type& str) { m_rep.assign(str); return *this; }
	KString& assign(const string_type& str, size_type pos, size_type n = npos);
	KString& assign(const value_type* s, size_type n) { m_rep.assign(s, n); return *this; }
	KString& assign(const value_type* str) { m_rep.assign(str); return *this;}
	KString& assign(size_type n, value_type ch) { m_rep.assign(n, ch); return *this;}
	template<class _InputIterator>
		KString& assign(_InputIterator first, _InputIterator last) { m_rep.assign(first, last); return *this; }
	KString& assign(std::initializer_list<value_type> il) { m_rep.assign(il); return *this; }
	KString& assign(KString&& str) { m_rep.assign(std::move(str.m_rep)); return *this; }
	KString& assign(KStringView sv) { m_rep.assign(sv.data(), sv.size()); return *this; }

	int compare(const KString& str) const { return m_rep.compare(str.m_rep); }
	int compare(size_type pos, size_type n, const KString& str) const { return compare(pos, n, str.m_rep); }
	int compare(size_type pos1, size_type n1, const KString& str, size_type pos2, size_type n2 = npos) const { return compare(pos1, n1, str.m_rep, pos2, n2); }
	int compare(const string_type& str) const { return m_rep.compare(str); }
	int compare(size_type pos, size_type n, const string_type& str) const;
	int compare(size_type pos1, size_type n1, const string_type& str, size_type pos2, size_type n2 = npos) const;
	int compare(const value_type* s) const { return m_rep.compare(s ? s : ""); }
	int compare(size_type pos, size_type n1, const value_type* s) const;
	int compare(size_type pos, size_type n1, const value_type* s, size_type n2) const;
	int compare(KStringView sv) const { return m_rep.compare(0, npos, sv.data(), sv.size()); }
	int compare(size_type pos, size_type n1, KStringView sv) const;

	size_type copy(value_type* s, size_type n, size_type pos = 0) const;

	size_type find(const KString& str, size_type pos = 0) const { return m_rep.find(str.m_rep, pos); }
	size_type find(const string_type& str, size_type pos = 0) const { return m_rep.find(str, pos); }
	size_type find(const value_type* s, size_type pos = 0) const { return m_rep.find(s, pos); }
	size_type find(const value_type* s, size_type pos, size_type n) const { return m_rep.find(s, pos, n); }
	size_type find(value_type c, size_type pos = 0) const { return m_rep.find(c, pos); }
	size_type find(KStringView sv, size_type pos = 0) const { return m_rep.find(sv.data(), pos, sv.size()); }

	size_type rfind(const KString& str, size_type pos = npos) const { return m_rep.rfind(str.m_rep, pos); }
	size_type rfind(const string_type& str, size_type pos = npos) const { return m_rep.rfind(str, pos); }
	size_type rfind(const value_type* s, size_type pos = npos) const { return m_rep.rfind(s, pos); }
	size_type rfind(const value_type* s, size_type pos, size_type n) const { return m_rep.rfind(s, pos, n); }
	size_type rfind(value_type c, size_type pos = npos) const { return m_rep.rfind(c, pos); }
	size_type rfind(KStringView sv, size_type pos = npos) const { return m_rep.rfind(sv.data(), pos, sv.size()); }

	size_type find_first_of(const KString& str, size_type pos = 0) const { return m_rep.find_first_of(str.m_rep, pos); }
	size_type find_first_of(const string_type& str, size_type pos = 0) const { return m_rep.find_first_of(str, pos); }
	size_type find_first_of(const value_type* s, size_type pos = 0) const { return m_rep.find_first_of(s, pos); }
	size_type find_first_of(const value_type* s, size_type pos, size_type n) const { return m_rep.find_first_of(s, pos, n); }
	size_type find_first_of(value_type c, size_type pos = 0) const { return m_rep.find_first_of(c, pos); }
	size_type find_first_of(KStringView sv, size_type pos = 0) const { return m_rep.find_first_of(sv.data(), pos, sv.size()); }

	size_type find_last_of(const KString& str, size_type pos = npos) const { return m_rep.find_last_of(str.m_rep, pos); }
	size_type find_last_of(const string_type& str, size_type pos = npos) const { return m_rep.find_last_of(str, pos); }
	size_type find_last_of(const value_type* s, size_type pos = npos) const { return m_rep.find_last_of(s, pos); }
	size_type find_last_of(const value_type* s, size_type pos, size_type n) const { return m_rep.find_last_of(s, pos, n); }
	size_type find_last_of(value_type c, size_type pos = npos) const {  return m_rep.find_last_of(c, pos); }
	size_type find_last_of(KStringView sv, size_type pos = npos) const { return m_rep.find_last_of(sv.data(), pos, sv.size()); }

	size_type find_first_not_of(const KString& str, size_type pos = 0) const { return m_rep.find_first_not_of(str.m_rep, pos); }
	size_type find_first_not_of(const string_type& str, size_type pos = 0) const { return m_rep.find_first_not_of(str, pos); }
	size_type find_first_not_of(const value_type* s, size_type pos = 0) const { return m_rep.find_first_not_of(s, pos); }
	size_type find_first_not_of(const value_type* s, size_type pos, size_type n) const { return m_rep.find_first_not_of(s, pos, n); }
	size_type find_first_not_of(value_type c, size_type pos = 0) const { return m_rep.find_first_not_of(c, pos); }
	size_type find_first_not_of(KStringView sv, size_type pos = 0) const { return m_rep.find_first_not_of(sv.data(), pos, sv.size()); }

	size_type find_last_not_of(const KString& str, size_type pos = npos) const { return m_rep.find_last_not_of(str.m_rep, pos); }
	size_type find_last_not_of(const string_type& str, size_type pos = npos) const { return m_rep.find_last_not_of(str, pos); }
	size_type find_last_not_of(const value_type* s, size_type pos = npos) const { return m_rep.find_last_not_of(s, pos); }
	size_type find_last_not_of(const value_type* s, size_type pos, size_type n) const { return m_rep.find_last_not_of(s, pos, n); }
	size_type find_last_not_of(value_type c, size_type pos = npos) const { return m_rep.find_last_not_of(c, pos); }
	size_type find_last_not_of(KStringView sv, size_type pos = npos) const { return m_rep.find_last_not_of(sv.data(), pos, sv.size()); }

	void insert(iterator p, size_type n, value_type c) { m_rep.insert(p, n, c); }
	KString& insert(size_type pos, const KString& str) { return insert(pos, str.m_rep); }
	KString& insert(size_type pos1, const KString& str, size_type pos2, size_type n = npos) { return insert(pos1, str.m_rep, pos2, n); }
	KString& insert(size_type pos, const string_type& str);
	KString& insert(size_type pos1, const string_type& str, size_type pos2, size_type n = npos);
	KString& insert(size_type pos, const value_type* s, size_type n);
	KString& insert(size_type pos, const value_type* s);
	KString& insert(size_type pos, size_type n, value_type c);
	// C++17 wants a const_iterator here, but the COW string implementation in libstdc++ does not have it
	iterator insert(iterator it, value_type c);
	template<class _InputIterator>
		void insert(const_iterator it, _InputIterator beg, _InputIterator end) { m_rep.insert(it, beg, end); }
	// should be const_iterator with C++11, but is not supported by libstdc++
	KString& insert (iterator it, std::initializer_list<value_type> il);
	KString& insert(size_type pos, KStringView sv);
	
	const value_type* c_str() const noexcept { return m_rep.c_str(); }
	const value_type* data() const noexcept { return m_rep.data(); }
	// C++17 supports non-const data(), but gcc does not yet know it..
	value_type* data() noexcept { return &m_rep[0]; }

	KString& erase(size_type pos = 0, size_type n = npos);
	// C++17 wants a const_iterator here, but the COW string implementation in libstdc++ does not have it
	iterator erase(iterator position);
	// C++17 wants a const_iterator here, but the COW string implementation in libstdc++ does not have it
	iterator erase(iterator first, iterator last);
	
	KString& replace(size_type pos, size_type n, const KString& str) { return replace(pos, n, str.m_rep); }
	KString& replace(size_type pos1, size_type n1, const KString& str, size_type pos2, size_type n2 = npos) { return replace(pos1, n1, str.m_rep, pos2, n2); }
	KString& replace(size_type pos, size_type n, const string_type& str);
	KString& replace(size_type pos1, size_type n1, const string_type& str, size_type pos2, size_type n2 = npos);
	KString& replace(size_type pos, size_type n1, const value_type* s, size_type n2);
	KString& replace(size_type pos, size_type n1, const value_type* s);
	KString& replace(size_type pos, size_type n1, size_type n2, value_type c);
	// C++17 wants a const_iterator here, but the COW string implementation in libstdc++ does not have it
	KString& replace(iterator i1, iterator i2, const KString& str) { return replace(i1, i2, str.m_rep); }
	// C++17 wants a const_iterator here, but the COW string implementation in libstdc++ does not have it
	KString& replace(iterator i1, iterator i2, const string_type& str);
	// C++17 wants a const_iterator here, but the COW string implementation in libstdc++ does not have it
	KString& replace(iterator i1, iterator i2, const value_type* s, size_type n);
	template<class _InputIterator>
	KString& replace(const_iterator i1, iterator i2, _InputIterator first, _InputIterator last)
	{
		try {
			m_rep.replace(i1, i2, first, last);
		} catch (std::exception& e) {
			log_exception(e, "replace");
		}
		return *this;
	}
	// C++17 wants a const_iterator here, but the COW string implementation in libstdc++ does not have it
	KString& replace(iterator i1, iterator i2, std::initializer_list<value_type> il);
	KString& replace(size_type pos, size_type n, KStringView sv);
	// C++17 wants a const_iterator here, but the COW string implementation in libstdc++ does not have it
	KString& replace(iterator i1, iterator i2, KStringView sv);

	KString substr(size_type pos = 0, size_type n = npos) const;

	inline void swap(KString& s) { m_rep.swap(s.m_rep); }

	inline allocator_type get_allocator() const noexcept { return m_rep.get_allocator(); }

	// other methods
	/// print arguments with fmt::format
	template<class... Args>
	KString& Format(Args&&... args)
	{
		m_rep = kFormat(std::forward<Args>(args)...);
		return *this;
	}

	/// print arguments with fmt::printf
	template<class... Args>
	KString& Printf(Args&&... args)
	{
		m_rep = kPrintf(std::forward<Args>(args)...);
		return *this;
	}

	/// replace with regular expression
	size_type ReplaceRegex(KStringView sRegEx, KStringView sReplaceWith, bool bReplaceAll = true);

	/// replace one part of the string with another string
	size_type Replace(KStringView sSearch, KStringView sReplace, bool bReplaceAll = false);

	/// does the string start with sPattern?
	bool StartsWith(KStringView sPattern) const { return kStartsWith(*this, sPattern); }

	/// does the string end with sPattern?
	bool EndsWith(KStringView sPattern) const { return kEndsWith(*this, sPattern); }

	bool IsEmail() const;
	bool IsURL() const;
	bool IsFilePath() const;

	/// changes the string to lowercase
	KString& MakeLower();

	/// changes the string to uppercase
	KString& MakeUpper();

	/// returns a copy of the string in uppercase
	KString ToUpper() const;

	/// returns a copy of the string in lowercase
	KString ToLower() const;

	/// returns leftmost iCount chars of string
	KStringView Left(size_type iCount);

	/// returns rightmost iCount chars of string
	KStringView Right(size_type iCount);

	/// pads string at the left up to iWidth size with chPad
	KString& PadLeft(size_t iWidth, value_type chPad = ' ');

	/// pads string at the right up to iWidth size with chPad
	KString& PadRight(size_t iWidth, value_type chPad = ' ');

	KString& TrimLeft();
	KString& TrimLeft(value_type chTarget);
	KString& TrimLeft(KStringView sTarget);

	KString& TrimRight();
	KString& TrimRight(value_type chTarget);
	KString& TrimRight(KStringView sTarget);

	KString& Trim();
	KString& Trim(value_type chTarget);
	KString& Trim(KStringView sTarget);

	/// Clip removing pszClipAt and everything to its right if found; otherwise do not alter the string
	KString& ClipAt(KStringView sClipAt);

	/// Clip removing everything to the left of pszClipAtReverse so that pszClipAtReverse becomes the beginning of the string;
	/// otherwise do not alter the string
	KString& ClipAtReverse(KStringView sClipAtReverse);

	void RemoveIllegalChars(const KString& sIllegalChars);

	/// return a pointer of value type
	const value_type* c() const { return c_str(); }

	/// convert to representation type
	operator string_type&() { return m_rep; }
	operator const string_type&() const { return m_rep; }

	/// return the representation type
	const string_type& s() const { return operator const string_type&(); }
	string_type& s() { return operator string_type&(); }

	KStringView sv() { return m_rep; }
	operator KStringView() const { return m_rep; }

	/// is string one of the values in sHaystack, delimited by iDelim?
	bool In (KStringView sHaystack, value_type iDelim=',');


//----------
protected:
//----------
	static void log_exception(std::exception& e, KStringView sWhere);

	string_type m_rep;

}; // KString

// typedef std::basic_stringstream<KString::value_type> KStringStream;

inline std::ostream& operator <<(std::ostream& stream, const KString& str)
{
	return stream << str.s();
}

inline std::istream& operator >>(std::istream& stream, KString& str)
{
	return stream >> str.s();
}

inline KString operator+(KStringView left, KStringView right)
{
	KString temp;
	temp.reserve(left.size() + right.size());
	temp += left;
	temp += right;
	return temp;
}

inline KString operator+(const KString::value_type* left, KStringView right)
{
	KStringView sv(left);
	return sv + right;
}

inline KString operator+(KStringView left, KString::value_type right)
{
	KString temp(left);
	temp.reserve(left.size() + 1);
	temp += right;
	return temp;
}

inline KString operator+(KString::value_type left, KStringView right)
{
	KString temp;
	temp.reserve(right.size() + 1);
	temp += left;
	temp += right;
	return temp;
}

// now all operator+() with rvalues (only make sense for the rvalue as the left param)
inline KString operator+(KString&& left, KStringView right)
{
	KString temp(std::move(left));
	temp += right;
	return temp;
}

inline KString operator+(KString&& left, KString::value_type right)
{
	KString temp(std::move(left));
	temp += right;
	return temp;
}

// KStringView includes comparison for KString
inline bool operator==(KStringView left, KStringView right)
{
#ifdef DEKAF2_HAS_CPP_17
	return std::experimental::operator==(left, right);
#else
	return re2::operator==(left, right);
#endif
}

inline bool operator!=(KStringView left, KStringView right)
{
#ifdef DEKAF2_HAS_CPP_17
	return std::experimental::operator!=(left, right);
#else
	return re2::operator!=(left, right);
#endif
}

inline bool operator<(KStringView left, KStringView right)
{
#ifdef DEKAF2_HAS_CPP_17
	return std::experimental::operator<(left, right);
#else
	return re2::operator<(left, right);
#endif
}

inline bool operator<=(KStringView left, KStringView right)
{
#ifdef DEKAF2_HAS_CPP_17
	return std::experimental::operator<=(left, right);
#else
	return re2::operator<=(left, right);
#endif
}

inline bool operator>(KStringView left, KStringView right)
{
#ifdef DEKAF2_HAS_CPP_17
	return std::experimental::operator>(left, right);
#else
	return re2::operator>(left, right);
#endif
}

inline bool operator>=(KStringView left, KStringView right)
{
#ifdef DEKAF2_HAS_CPP_17
	return std::experimental::operator>=(left, right);
#else
	return re2::operator>=(left, right);
#endif
}

} // end of namespace dekaf2

namespace std
{
	std::istream& getline(std::istream& stream, dekaf2::KString& str);

	template<> struct hash<dekaf2::KString>
	{
		typedef dekaf2::KString argument_type;
		typedef std::size_t result_type;
		result_type operator()(argument_type const& s) const noexcept
		{
			return std::hash<std::string>{}(s);
		}
	};

} // end of namespace std

