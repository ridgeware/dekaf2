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

#include "kstringview.h"
#include "kstring.h"

#ifdef DEKAF2_IS_WINDOWS
	// Windows has a DELETE macro somewhere which interferes with
	// dekaf2::KHTTPMethod::DELETE (macros are evil!)
	#ifdef DELETE
		#undef DELETE
	#endif
#endif

namespace dekaf2 {

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KHTTPMethod
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	//-----------------------------------------------------------------------------
	KHTTPMethod()
	//-----------------------------------------------------------------------------
	    : m_method(GET)
	{}

	//-----------------------------------------------------------------------------
	KHTTPMethod(const KHTTPMethod&) = default;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	KHTTPMethod(KHTTPMethod&&) = default;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	KHTTPMethod(KString sStr)
	//-----------------------------------------------------------------------------
	: m_method(std::move(sStr))
	{}

	//-----------------------------------------------------------------------------
	KHTTPMethod(KStringView sv)
	//-----------------------------------------------------------------------------
	: KHTTPMethod(KString(sv))
	{}

	//-----------------------------------------------------------------------------
	KHTTPMethod(KStringViewZ sv)
	//-----------------------------------------------------------------------------
	: KHTTPMethod(KString(sv))
	{}

	//-----------------------------------------------------------------------------
	KHTTPMethod(const char* sv)
	//-----------------------------------------------------------------------------
	: KHTTPMethod(KString(sv))
	{}

	//-----------------------------------------------------------------------------
	const KString& Serialize() const
	//-----------------------------------------------------------------------------
	{
		return m_method;
	}

	//-----------------------------------------------------------------------------
	operator const KString&() const
	//-----------------------------------------------------------------------------
	{
		return Serialize();
	}

	//-----------------------------------------------------------------------------
	bool empty() const
	//-----------------------------------------------------------------------------
	{
		return m_method.empty();
	}

	//-----------------------------------------------------------------------------
	KHTTPMethod& operator=(const KHTTPMethod&) = default;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	KHTTPMethod& operator=(KHTTPMethod&&) = default;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	bool operator==(const KHTTPMethod& other) const
	//-----------------------------------------------------------------------------
	{
		return m_method == other.m_method;
	}

	//-----------------------------------------------------------------------------
	bool operator!=(const KHTTPMethod& other) const
	//-----------------------------------------------------------------------------
	{
		return m_method != other.m_method;
	}

	static constexpr KStringViewZ GET     = "GET";
	static constexpr KStringViewZ HEAD    = "HEAD";
	static constexpr KStringViewZ POST    = "POST";
	static constexpr KStringViewZ PUT     = "PUT";
	static constexpr KStringViewZ DELETE  = "DELETE";
	static constexpr KStringViewZ OPTIONS = "OPTIONS";
	static constexpr KStringViewZ PATCH   = "PATCH";
	static constexpr KStringViewZ CONNECT = "CONNECT";

	static constexpr KStringViewZ REQUEST_METHODS = "GET,HEAD,POST,PUT,DELETE,OPTIONS,PATCH,CONNECT";

//------
private:
//------

	KString m_method { GET };

}; // KHTTPMethod

inline bool operator==(const char* left, const KHTTPMethod& right)
{
	return left == right.Serialize();
}

inline bool operator==(const KHTTPMethod& left, const char* right)
{
	return operator==(right, left);
}

inline bool operator!=(const char* left, const KHTTPMethod& right)
{
	return !operator==(left, right);
}

inline bool operator!=(const KHTTPMethod& left, const char* right)
{
	return !operator==(right, left);
}

inline bool operator==(KStringView left, const KHTTPMethod& right)
{
	return left == right.Serialize();
}

inline bool operator==(const KHTTPMethod& left, KStringView right)
{
	return operator==(right, left);
}

inline bool operator!=(KStringView left, const KHTTPMethod& right)
{
	return !operator==(left, right);
}

inline bool operator!=(const KHTTPMethod& left, KStringView right)
{
	return !operator==(right, left);
}

inline bool operator==(const KString& left, const KHTTPMethod& right)
{
	return left == right.Serialize();
}

inline bool operator==(const KHTTPMethod& left, const KString& right)
{
	return operator==(right, left);
}

inline bool operator!=(const KString& left, const KHTTPMethod& right)
{
	return !operator==(left, right);
}

inline bool operator!=(const KHTTPMethod& left, const KString& right)
{
	return !operator==(right, left);
}

} // end of namespace dekaf2
