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

#include "khttp_method.h"

namespace dekaf2 {

//-----------------------------------------------------------------------------
KHTTPMethod::Method KHTTPMethod::Parse(KStringView sMethod)
//-----------------------------------------------------------------------------
{
    Method method;
    
    switch (sMethod.Hash())
    {
        case "GET"_hash:
            method = GET;
            break;
        case "HEAD"_hash:
            method = HEAD;
            break;
        case "POST"_hash:
            method = POST;
            break;
        case "PUT"_hash:
            method = PUT;
            break;
        case "DELETE"_hash:
            method = DELETE;
            break;
        case "OPTIONS"_hash:
            method = OPTIONS;
            break;
        case "PATCH"_hash:
            method = PATCH;
            break;
        case "CONNECT"_hash:
            method = CONNECT;
            break;
        case "TRACE"_hash:
            method = TRACE;
            break;
        default:
            method = INVALID;
            break;
    }
    
    return method;
    
} // Method

//-----------------------------------------------------------------------------
KStringViewZ KHTTPMethod::Serialize() const
//-----------------------------------------------------------------------------
{
    switch (m_method)
    {
        case INVALID:
            return "";
        case GET:
            return "GET";
        case HEAD:
            return "HEAD";
        case POST:
            return "POST";
        case PUT:
            return "PUT";
        case DELETE:
            return "DELETE";
        case OPTIONS:
            return "OPTIONS";
        case PATCH:
            return "PATCH";
        case CONNECT:
            return "CONNECT";
        case TRACE:
            return "TRACE";
    }
    
    // gcc is stupid..
    return "";
    
} // Serialize

static_assert(std::is_nothrow_move_constructible<KHTTPMethod>::value,
			  "KHTTPMethod is intended to be nothrow move constructible, but is not!");
	
} // end of namespace dekaf2

