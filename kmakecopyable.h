/*
 //
 // DEKAF(tm): Lighter, Faster, Smarter (tm)
 //
 // Copyright (c) 2021, Ridgeware, Inc.
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

namespace dekaf2 {

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// A class that makes a non-copy-constructible type copy-constructible by assigning
/// the default constructed value on copy. Requires that type is default constructible..
template<typename NonCopyable,
         typename std::enable_if<std::is_default_constructible<NonCopyable>::value, int>::type = 0>
class KMakeCopyable
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	/// default constructor
	KMakeCopyable() = default;

	/// call default constructor for copy construction
	KMakeCopyable(const KMakeCopyable&) : KMakeCopyable() {};

	/// Construction with any arguments the NonCopyable type permits
	// make sure this does not cover the copy constructor by requesting an args count
	// of != 1
	template<class... Args,
		typename std::enable_if<
			sizeof...(Args) != 1, int
		>::type = 0
	>
	KMakeCopyable(Args&&... args)
	: m_Value(std::forward<Args>(args)...)
	{
	}

	/// Construction of the NonCopyable type with any single argument.
	// make sure this does not cover the copy constructor by requesting the single
	// arg being of a different type than KMakeCopyable
	template<class Arg,
		typename std::enable_if<
			!std::is_same<
				typename std::decay<Arg>::type, KMakeCopyable
			>::value, int
		>::type = 0
	>
	KMakeCopyable(Arg&& arg)
	: m_Value(std::forward<Arg>(arg))
	{
	}

	KMakeCopyable& operator=(const KMakeCopyable&) { m_Value = NonCopyable();    return *this; };
	KMakeCopyable& operator=(NonCopyable&& other) { m_Value = std::move(other); return *this; };

	/// explicitly get the value type
	const NonCopyable& get() const { return m_Value; }
	/// explicitly get the value type
	      NonCopyable& get()       { return m_Value; }

	/// implicitly get the value type
	operator const NonCopyable&() const { return get(); }
	/// implicitly get the value type
	operator       NonCopyable&()       { return get(); }

//----------
private:
//----------

	NonCopyable m_Value;

}; // KMakeCopyable

} // end of namespace dekaf2
