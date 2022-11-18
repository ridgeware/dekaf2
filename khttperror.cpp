/*
 //
 // DEKAF(tm): Lighter, Faster, Smarter (tm)
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
 */

#include "khttperror.h"
#include "klog.h"

using namespace dekaf2;

//-----------------------------------------------------------------------------
void KHTTPError::clear()
//-----------------------------------------------------------------------------
{
	m_sStatusString.clear();
	KError::clear();

} // clear

//-----------------------------------------------------------------------------
uint16_t KHTTPError::GetHTTPStatusCode() const
//-----------------------------------------------------------------------------
{
	// We make a special exception for status codes 290..292 to
	// differentiate the response for the various request types
	if (value() >= 290 && value() <= 292)
	{
		return (value() == 292) ? 200 : 201;
	}
	else
	{
		return value();
	}

} // GetHTTPStatusCode

//-----------------------------------------------------------------------------
KStringView KHTTPError::GetStatusString(uint16_t iStatusCode)
//-----------------------------------------------------------------------------
{
	switch (iStatusCode)
	{
		// reset
		case 0:                       return KStringView{};

		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		// HTTP 100s: special
		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		case H1xx_CONTINUE:           return "CONTINUE";
		case H1xx_SWITCHING_PROTOCOL: return "SWITCHING PROTOCOL";

		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		// HTTP 200s: ok
		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		case H2xx_OK:                 return "OK";
		case H2xx_CREATED:            return "CREATED";
		case H2xx_ACCEPTED:           return "ACCEPTED";
		case H2xx_NO_CONTENT:         return "NO CONTENT";
		case H2xx_MULTI_STATUS:       return "MULTI-STATUS";
		case H2xx_UPDATED:            return "UPDATED";
		case H2xx_DELETED:            return "DELETED";
		case H2xx_ALREADY:            return "ALREADY DONE";

		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		// HTTP 300s: redirects
		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		case H301_MOVED_PERMANENTLY:  return "MOVED PERMANENTLY";
		case H302_MOVED_TEMPORARILY:  return "MOVED TEMPORARILY";
		case H303_SEE_OTHER:          return "SEE OTHER";
		case H304_NOT_MODIFIED:       return "NOT MODIFIED";
		case H305_USE_PROXY:          return "USE PROXY";
		case H307_TEMPORARY_REDIRECT: return "TEMPORARY REDIRECT";
		case H308_PERMANENT_REDIRECT: return "PERMANENT REDIRECT";

		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		// HTTP 400s: client invocation problems
		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		case H4xx_BADREQUEST:         return "BAD REQUEST";
		case H4xx_FORBIDDEN:          return "FORBIDDEN";
		case H4xx_NOTAUTH:            return "NOT AUTHORIZED";
		case H4xx_NOTFOUND:           return "NOT FOUND";
		case H4xx_BADMETHOD:          return "METHOD NOT ALLOWED";
		case H4xx_CONFLICT:           return "CONFLICT";

		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		// HTTP 500s: server-side problems
		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		case H5xx_ERROR:              return "INTERNAL SERVER ERROR";
		case H5xx_NOTIMPL:            return "NOT IMPLEMENTED";
		case H5xx_UNAVAILABLE:        return "SERVICE UNAVAILABLE";
		case H5xx_FROZEN:             return "SERVICE FROZEN";
		case H5xx_READTIMEOUT:        return "NETWORK READ TIMEOUT ERROR";

		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		// FALL THROUGH: blow up with a 500 error
		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		default:
			kWarning ("BUG: called with code {}", iStatusCode);
			return "INTERNAL SERVER ERROR";
	}

} // GetStatusString

