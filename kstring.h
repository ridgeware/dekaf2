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
#ifdef DEKAF_HAS_CPP_17
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
	class KString;
}

namespace std
{
	std::istream& getline(std::istream& stream, dekaf2::KString& str);
}

namespace dekaf2
{

#ifdef DEKAF_HAS_CPP_17
using KStringView = std::experimental::string_view;
#else
using KStringView = re2::StringPiece;
#endif

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// dekaf's own string class - a wrapper around std::string
/// which handles most error cases in a benign way
class KString 
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	friend class KStringBuffer;
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
	iterator begin() { return m_rep.begin(); }
	const_iterator begin() const { return m_rep.begin(); }
	iterator end() { return m_rep.end(); }
	const_iterator end() const { return m_rep.end(); }
	reverse_iterator rbegin() { return m_rep.rbegin(); }
	const_reverse_iterator rbegin() const { return m_rep.rbegin(); }
	reverse_iterator rend() { return m_rep.rend(); }
	const_reverse_iterator rend() const { return m_rep.rend(); }
	const_iterator cbegin() const noexcept { return m_rep.cbegin(); }
	const_iterator cend() const noexcept { return m_rep.cend(); }
	const_reverse_iterator crbegin() const noexcept { return m_rep.crbegin(); }
	const_reverse_iterator crend() const noexcept { return m_rep.crend(); }

	size_type size() const { return m_rep.size(); }
	size_type length() const { return m_rep.length(); }
	size_type max_size() const { return m_rep.max_size(); }
	void resize(size_type n, value_type c) { m_rep.resize(n, c); }
	void resize(size_type n) { m_rep.resize(n); }
	size_type capacity() const {return m_rep.capacity(); }
	void reserve(size_type res_arg = 0) { m_rep.reserve(res_arg); }
	void clear() { m_rep.clear(); }
	bool empty() const { return m_rep.empty(); }
	void shrink_to_fit() { m_rep.shrink_to_fit(); }
	
	const_reference operator[] (size_type pos) const { return m_rep[pos]; }
	reference operator[](size_type pos) { return m_rep[pos]; }
	const_reference at(size_type n) const { return m_rep.at(n); }
	reference at(size_type n) { return m_rep.at(n); }
	const_reference back() const { return m_rep.back(); }
	reference back() { return m_rep.back(); }
	const_reference front() const { return m_rep.front(); }
	reference front() { return m_rep.front(); }

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
	explicit KString (KStringView sv) : m_rep(sv.data(), sv.size()) {}

	//operator+=
	KString& operator+= (const KString& str){ m_rep += str.m_rep; return *this; }
	KString& operator+= (const value_type ch){ m_rep += ch; return *this; }
	KString& operator+= (const value_type *s){ if (s) m_rep += s; return *this; }
	KString& operator+= (std::initializer_list<value_type> il) { m_rep += il; return *this; }
	KString& operator+= (KStringView sv) { m_rep.append(sv.data(), sv.size()); return *this; }

	//operator=
	KString& operator= (const KString& str) { m_rep = str.m_rep; }
	KString& operator= (const value_type* pszStr) { if (pszStr) m_rep = pszStr; return *this; }
	KString& operator= (const value_type c) { m_rep = c; return *this; }
	KString& operator= (const string_type& sStr) { m_rep = sStr; return *this; }
	KString& operator= (KString&& str) noexcept { m_rep = std::move(str.m_rep); return *this; }
	KString& operator= (string_type&& sStr) noexcept { m_rep = std::move(sStr); return *this; }
	KString& operator= (std::initializer_list<value_type> il) { m_rep = il; return *this; }
	KString& operator= (KStringView sv) { m_rep.assign(sv.data(), sv.size()); return *this; }

	//std methods
	KString& append(const KString& str){m_rep.append(str.m_rep); return *this; } 
	KString& append(const KString& str, size_type pos, size_type n = npos){m_rep.append(str.m_rep, pos, n); return *this; }
	KString& append(const value_type* str, size_type n) {m_rep.append(str, n); return *this; }
	KString& append(const value_type* str) {m_rep.append(str); return *this; }
	KString& append(size_type n, value_type ch){m_rep.append(n, ch); return *this; }
	template<class _InputIterator>
		KString& append(_InputIterator first, _InputIterator last) {m_rep.append(first, last); return *this; }
	KString& append(std::initializer_list<value_type> il) { m_rep.append(il); return *this; }
	KString& append(KStringView sv) { m_rep.append(sv.data(), sv.size()); return *this; }

	KString& push_back(const value_type chPushBack) { m_rep.push_back(chPushBack); return *this; }
	void pop_back() { m_rep.pop_back(); }

	KString& assign(const KString& str) {m_rep.assign(str.m_rep); return *this; }
	KString& assign(const KString& str, size_type pos, size_type n = npos) {m_rep.assign(str.m_rep, pos, n); return *this;}
	KString& assign(const value_type* s, size_type n) {m_rep.assign(s, n); return *this; } 
	KString& assign(const value_type* str) {m_rep.assign(str); return *this;}
	KString& assign(size_type n, value_type ch) {m_rep.assign(n, ch); return *this;}
	template<class _InputIterator>
		KString& assign(_InputIterator first, _InputIterator last) {m_rep.assign(first, last); return *this; }
	KString& assign(std::initializer_list<value_type> il) { m_rep.assign(il); return *this; }
	KString& assign(KString&& str) { m_rep.assign(std::move(str.m_rep)); return *this; }
	KString& assign(KStringView sv) { m_rep.assign(sv.data(), sv.size()); return *this; }

	int compare(const KString& str) const { return m_rep.compare(str.m_rep); }
	int compare(size_type pos, size_type n, const KString& str) const;
	int compare(size_type pos1, size_type n1, const KString& str,  size_type pos2, size_type n2 = npos) const;
	int compare(const value_type* s) const { return m_rep.compare(s ? s : ""); }
	int compare(size_type pos, size_type n1, const value_type* s) const;
	int compare(size_type pos, size_type n1, const value_type* s, size_type n2) const;
	int compare(KStringView sv) const { return m_rep.compare(0, npos, sv.data(), sv.size()); }
	int compare(size_type pos, size_type n1, KStringView sv) const;

	size_type copy(value_type* s, size_type n, size_type pos = 0) const;

	size_type find(const value_type* s, size_type pos, size_type n) const { return m_rep.find(s, pos, n); } 
	size_type find(const KString& str, size_type pos = 0) const { return m_rep.find(str.m_rep, pos); }
	size_type find(const value_type* s, size_type pos = 0) const { return m_rep.find(s, pos); }
	size_type find(value_type c, size_type pos = 0) const { return m_rep.find(c, pos); }
	size_type find(KStringView sv, size_type pos = 0) const { return m_rep.find(sv.data(), pos, sv.size()); }

	size_type rfind(const KString& str, size_type pos = npos) const { return m_rep.rfind(str.m_rep, pos); }
	size_type rfind(const value_type* s, size_type pos, size_type n) const { return m_rep.rfind(s, pos, n); }
	size_type rfind(const value_type* s, size_type pos = npos) const { return m_rep.rfind(s, pos); }
	size_type rfind(value_type c, size_type pos = npos) const { return m_rep.rfind(c, pos); }
	size_type rfind(KStringView sv, size_type pos = npos) const { return m_rep.rfind(sv.data(), pos, sv.size()); }

	size_type find_first_of(const KString& str, size_type pos = 0) const { return m_rep.find_first_of(str.m_rep, pos); }
	size_type find_first_of(const value_type* s, size_type pos, size_type n) const { return m_rep.find_first_of(s, pos, n); }
	size_type find_first_of(const value_type* s, size_type pos = 0) const { return m_rep.find_first_of(s, pos); }
	size_type find_first_of(value_type c, size_type pos = 0) const { return m_rep.find_first_of(c, pos); }
	size_type find_first_of(KStringView sv, size_type pos = 0) const { return m_rep.find_first_of(sv.data(), pos, sv.size()); }

	size_type find_last_of(const KString& str, size_type pos = npos) const { return m_rep.find_last_of(str.m_rep, pos); }
	size_type find_last_of(const value_type* s, size_type pos, size_type n) const { return m_rep.find_last_of(s, pos, n); }
	size_type find_last_of(const value_type* s, size_type pos = npos) const { return m_rep.find_last_of(s, pos); }
	size_type find_last_of(value_type c, size_type pos = npos) const {  return m_rep.find_last_of(c, pos); }
	size_type find_last_of(KStringView sv, size_type pos = npos) const { return m_rep.find_last_of(sv.data(), pos, sv.size()); }

	size_type find_first_not_of(const KString& str, size_type pos = 0) const { return m_rep.find_first_not_of(str.m_rep, pos); }
	size_type find_first_not_of(const value_type* s, size_type pos, size_type n) const { return m_rep.find_first_not_of(s, pos, n); }
	size_type find_first_not_of(const value_type* s, size_type pos = 0) const { return m_rep.find_first_not_of(s, pos); }
	size_type find_first_not_of(value_type c, size_type pos = 0) const { return m_rep.find_first_not_of(c, pos); }
	size_type find_first_not_of(KStringView sv, size_type pos = 0) const { return m_rep.find_first_not_of(sv.data(), pos, sv.size()); }

	size_type find_last_not_of(const KString& str, size_type pos = npos) const { return m_rep.find_last_not_of(str.m_rep, pos); }
	size_type find_last_not_of(const value_type* s, size_type pos, size_type n) const { return m_rep.find_last_not_of(s, pos, n); }
	size_type find_last_not_of(const value_type* s, size_type pos = npos) const { return m_rep.find_last_not_of(s, pos); }
	size_type find_last_not_of(value_type c, size_type pos = npos) const { return m_rep.find_last_not_of(c, pos); }
	size_type find_last_not_of(KStringView sv, size_type pos = npos) const { return m_rep.find_last_not_of(sv.data(), pos, sv.size()); }

	void insert(iterator p, size_type n, value_type c) {m_rep.insert(p, n, c);}
	KString& insert(size_type pos1, const KString& str);
	KString& insert(size_type pos1, const KString& str, size_type pos2, size_type n = npos);
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
	
	KString& replace(size_type pos, size_type n, const KString& str);
	KString& replace(size_type pos1, size_type n1, const KString& str, size_type pos2, size_type n2 = npos);
	KString& replace(size_type pos, size_type n1, const value_type* s, size_type n2);
	KString& replace(size_type pos, size_type n1, const value_type* s);
	KString& replace(size_type pos, size_type n1, size_type n2, value_type c);
	// C++17 wants a const_iterator here, but the COW string implementation in libstdc++ does not have it
	KString& replace(iterator i1, iterator i2, const KString& str);
	// C++17 wants a const_iterator here, but the COW string implementation in libstdc++ does not have it
	KString& replace(iterator i1, iterator i2, const value_type* s, size_type n);
	template<class _InputIterator>
		KString& replace(const_iterator i1, iterator i2, _InputIterator first, _InputIterator last) { m_rep.replace(i1, i2, first, last); return *this; }
	// C++17 wants a const_iterator here, but the COW string implementation in libstdc++ does not have it
	KString& replace(iterator i1, iterator i2, std::initializer_list<value_type> il);
	KString& replace(size_type pos, size_type n, KStringView sv);
	// C++17 wants a const_iterator here, but the COW string implementation in libstdc++ does not have it
	KString& replace(iterator i1, iterator i2, KStringView sv);

	KString substr(size_type pos = 0, size_type n = npos) const;

	void swap(KString& s) { m_rep.swap(s.m_rep); }

	allocator_type get_allocator() const noexcept { return m_rep.get_allocator(); }

	// other methods
	template<class... Args>
	KString& Format(Args&&... args)
	{
		m_rep = kFormat(std::forward<Args>(args)...);
		return *this;
	}

	template<class... Args>
	KString& Printf(Args&&... args)
	{
		m_rep = kPrintf(std::forward<Args>(args)...);
		return *this;
	}

	size_type ReplaceRegex(const KStringView& sRegEx, const KStringView& sReplaceWith, bool bReplaceAll = true);

	size_type Replace(const value_type* pszSearch, const value_type* pszReplaceWith, bool bReplaceAll = false) { return Replace(pszSearch, strlen(pszSearch), pszReplaceWith, strlen(pszReplaceWith), bReplaceAll); }
	size_type Replace(const KString& sSearch, const KString& sReplaceWith, bool bReplaceAll = false) { return Replace(sSearch.data(), sSearch.size(), sReplaceWith.data(), sReplaceWith.size(), bReplaceAll); }
	size_type Replace(const value_type* pszSearch, size_type iSearchLen, const value_type* pszReplaceWith, size_type iReplaceWithLen, bool bReplaceAll = false);

	char*    strdup() const { return ::strdup (c_str()); }
	
	bool     StartsWith(const char* pszStr) const;
	bool     EndsWith(const char* pszStr) const;
	bool     IsEmail() const;
	bool     IsURL() const;
	bool     IsFilePath() const;

	//Compare head and tail of the string. iOffset - where should we shift before comparison
	bool     StartsWith(const KString& sPattern, size_type iFrontOffset = 0, size_type iBackOffset = 0) const;
	bool     EndsWith(const KString& sPattern, size_type iFrontOffset = 0, size_type iBackOffset = 0) const;

	/// changes the string to lowercase
	KString& MakeLower();
	/// changes the string to uppercase
	KString& MakeUpper();

	/// returns a copy of the string in uppercase
	KString  ToUpper() const;
	/// returns a copy of the string in lowercase
	KString  ToLower() const;

	KString  Left(size_type iCount);
	KString  Right(size_type iCount);

	KString& TrimLeft();
	KString& TrimLeft(const value_type chTarget);
	KString& TrimLeft(const value_type* pszTarget);

	KString& TrimRight();
	KString& TrimRight(const value_type chTarget);
	KString& TrimRight(const value_type* pszTarget);

	KString& Trim();
	KString& Trim(const value_type chTarget);
	KString& Trim(const value_type* pszTarget);

	KString& PushBack(const value_type chPushBack) { return push_back(chPushBack); }
	void PopBack() { pop_back(); }
	KString& Append(const value_type* pszAppend) { return append(pszAppend); }

	// Clip removing pszClipAt and everything to its right if found; otherwise do not alter the string
	KString& ClipAt(const value_type* pszClipAt);

	// Clip removing everything to the left of pszClipAtReverse so that pszClipAtReverse becomes the beginning of the string;
	// otherwise do not alter the string
	KString& ClipAtReverse(const value_type* pszClipAtReverse);

	void RemoveIllegalChars(const KString& sIllegalChars);

	// return a pointer of value type
	const value_type* c() const {return c_str();}

	// convert to representation type
	operator string_type&() {return m_rep;}
	operator const string_type&() const {return m_rep;}

	// return the representation type
	const string_type& s() const {return operator const string_type&();}
	string_type& s() {return operator string_type&();}

	operator KStringView() { return m_rep; }
	operator const KStringView() const { return m_rep; }

	bool In (const KString& sHaystack, value_type iDelim=',');

	static bool StartsWith(const KString& input, const KString& pattern);
	static bool StartsWith(const char* pszInput, size_t iInputSize, const char* pszPattern, size_t iPatternSize);
	static bool EndsWith(const char* pszInput, size_t iInputSize, const char* pszPattern, size_t iPatternSize);
	static bool kstrin (const char* sNeedle, const char* sHaystack, char iDelim=',');

//----------
protected:
//----------
	string_type m_rep;

//----------
private:
//----------
	size_type InnerReplace(size_type iIdxStartMatch, size_type iIdxEndMatch, const char* pszReplaceWith);

	friend bool operator==(const KString&, const KString&);
	friend bool operator==(const KString&, const value_type*);
	friend bool operator==(const value_type*, const KString&);
	friend bool operator!=(const KString&, const KString&);
	friend bool operator!=(const KString&, const value_type*);
	friend bool operator!=(const value_type*, const KString&);
	friend bool operator>(const KString&, const KString&);
	friend bool operator>(const KString&, const value_type*);
	friend bool operator>(const value_type*, const KString&);
	friend bool operator<(const KString&, const KString&);
	friend bool operator<(const KString&, const value_type*);
	friend bool operator<(const value_type*, const KString&);
	friend bool operator>=(const KString&, const KString&);
	friend bool operator>=(const KString&, const value_type*);
	friend bool operator>=(const value_type*, const KString&);
	friend bool operator<=(const KString&, const KString&);
	friend bool operator<=(const KString&, const value_type*);
	friend bool operator<=(const value_type*, const KString&);
	friend std::ostream& operator <<(std::ostream& stream, const KString& str);
	friend std::istream& operator >>(std::istream& stream, KString& str);
	friend std::istream& std::getline(std::istream& stream, KString& str);

}; // KString

typedef std::basic_stringstream<KString::value_type> KStringStream;

inline std::ostream& operator <<(std::ostream& stream, const KString& str)
{
	return stream << str.m_rep;
}

inline std::istream& operator >>(std::istream& stream, KString& str)
{
	return stream >> str.m_rep;
}

} // of namespace dekaf2

namespace std
{
	template<> struct hash<dekaf2::KString>
	{
		typedef dekaf2::KString argument_type;
		typedef std::size_t result_type;
		result_type operator()(argument_type const& s) const noexcept
		{
			return std::hash<std::string>{}(s);
		}
	};
}

namespace dekaf2
{

//-----------------------------------------------------------------------------
class KStringBuffer
//-----------------------------------------------------------------------------
{
public:
	enum CB_NEXT_OP {RET_NOOP, RET_REPLACE};
	typedef std::function<
		CB_NEXT_OP(
			const KString& sWorkBuffer,
			std::size_t iPos,
			std::size_t iLen,
			KString& sReplacer)
		> SearchHandler;

	explicit KStringBuffer(KString& sWorkBuffer):m_sWorkBuffer(sWorkBuffer){}

	KString::size_type Replace(const KString& sSearchPattern, const KString& sWith);
	KString::size_type Replace(const KString& sSearchPattern, const SearchHandler& foundHandler);

private:
	class SimpleReplacer;

	KString& m_sWorkBuffer;
};

inline KString operator+(const KString& left, const KString& right)
{
	KString temp(left);
	temp += right;
	return temp;
}

inline KString operator+(const KString& left, const char* right)
{
	KString temp(left);
	temp += right;
	return temp;
}

inline KString operator+(const char* left, const KString& right)
{
	KString temp(left);
	temp += right;
	return temp;
}

inline KString operator+(const KString& left, char right)
{
	KString temp(left);
	temp += right;
	return temp;
}

inline KString operator+(char left, const KString& right)
{
	KString temp;
	temp += left;
	temp += right;
	return temp;
}

// now all operator+() with rvalues
inline KString operator+(KString&& left, KString&& right)
{
	KString temp(std::move(left));
	temp += right;
	return temp;
}

inline KString operator+(KString&& left, const KString& right)
{
	KString temp(std::move(left));
	temp += right;
	return temp;
}

inline KString operator+(const KString& left, KString&& right)
{
	KString temp(left);
	temp.append(std::move(right));
	return temp;
}

inline KString operator+(KString&& left, const char* right)
{
	KString temp(std::move(left));
	temp += right;
	return temp;
}

inline KString operator+(const char* left, KString&& right)
{
	KString temp(left);
	temp.append(std::move(right));
	return temp;
}

inline KString operator+(KString&& left, char right)
{
	KString temp(std::move(left));
	temp += right;
	return temp;
}

inline KString operator+(char left, KString&& right)
{
	KString temp;
	temp += left;
	temp.append(std::move(right));
	return temp;
}

inline bool operator==(const KString& left, const KString& right)
{
	return left.m_rep == right.m_rep;
}

inline bool operator==(const KString& left, const KString::value_type* right)
{
	return left.m_rep == right;
}

inline bool operator==(const KString::value_type* left, const KString& right)
{
	return left == right.m_rep;
}

inline bool operator!=(const KString& left, const KString& right)
{
	return left.m_rep != right.m_rep;
}

inline bool operator!=(const KString& left, const KString::value_type* right)
{
	return left.m_rep != right;
}

inline bool operator!=(const KString::value_type* left, const KString& right)
{
	return left != right.m_rep;
}

inline bool operator>(const KString& left, const KString& right)
{
	return left.m_rep > right.m_rep;
}

inline bool operator>(const KString& left, const KString::value_type* right)
{
	return left.m_rep > right;
}

inline bool operator>(const KString::value_type* left, const KString& right)
{
	return left > right.m_rep;
}

inline bool operator<(const KString& left, const KString& right)
{
	return left.m_rep < right.m_rep;
}

inline bool operator<(const KString::value_type* left, const KString& right)
{
	return left < right.m_rep;
}

inline bool operator<(const KString& left, const KString::value_type* right)
{
	return left.m_rep < right;
}

inline bool operator>=(const KString& left, const KString& right)
{
	return left.m_rep >= right.m_rep;
}

inline bool operator>=(const KString& left, const KString::value_type* right)
{
	return left.m_rep >= right;
}

inline bool operator>=(const KString::value_type* left, const KString& right)
{
	return left >= right.m_rep;
}

inline bool operator<=(const KString& left, const KString& right)
{
	return left.m_rep <= right.m_rep;
}

inline bool operator<=(const KString::value_type* left, const KString& right)
{
	return left <= right.m_rep;
}

inline bool operator<=(const KString& left, const KString::value_type* right)
{
	return left.m_rep <= right;
}


} // end of namespace dekaf2

