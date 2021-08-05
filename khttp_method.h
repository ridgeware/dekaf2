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

#include "kstringview.h"
#include "kformat.h"

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
    
    enum Method : uint8_t
    {
        GET = 0,
        HEAD,
        POST,
        PUT,
        DELETE,
        OPTIONS,
        PATCH,
        CONNECT,
        TRACE,
        INVALID
   };

    //-----------------------------------------------------------------------------
    KHTTPMethod(Method method = GET)
    //-----------------------------------------------------------------------------
    : m_method(method)
    {}
    
    //-----------------------------------------------------------------------------
    KHTTPMethod(KStringView sMethod)
    //-----------------------------------------------------------------------------
    : m_method(Parse(sMethod))
    {}
    
    //-----------------------------------------------------------------------------
    KHTTPMethod(const KString& sMethod)
    //-----------------------------------------------------------------------------
    : KHTTPMethod(sMethod.ToView())
    {}
    
    //-----------------------------------------------------------------------------
    KHTTPMethod(const char* sMethod)
    //-----------------------------------------------------------------------------
    : KHTTPMethod(KStringView(sMethod))
    {}
    
    //-----------------------------------------------------------------------------
    KStringViewZ Serialize() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	bool empty() const
	//-----------------------------------------------------------------------------
	{
		return m_method == INVALID;
	}

	//-----------------------------------------------------------------------------
	void clear()
	//-----------------------------------------------------------------------------
	{
		m_method = GET;
	}

	//-----------------------------------------------------------------------------
	operator Method() const { return m_method; }
	//-----------------------------------------------------------------------------

    //-----------------------------------------------------------------------------
    static Method Parse(KStringView sMethod);
    //-----------------------------------------------------------------------------

	static constexpr KStringViewZ REQUEST_METHODS = "GET,HEAD,POST,PUT,DELETE,OPTIONS,PATCH,CONNECT,TRACE";

//------
private:
//------

	Method m_method { GET };

}; // KHTTPMethod

} // end of namespace dekaf2

template <>
struct fmt::formatter<dekaf2::KHTTPMethod> : formatter<string_view>
{
	template <typename FormatContext>
	auto format(const dekaf2::KHTTPMethod& Method, FormatContext& ctx)
	{
		return formatter<string_view>::format(Method.Serialize(), ctx);
	}
};
