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
	*this = KHTTPError { 0, "", "" };

} // clear

//-----------------------------------------------------------------------------
uint16_t KHTTPError::GetHTTPStatusCode() const
//-----------------------------------------------------------------------------
{
	// We make a special exception for status codes 290..292 to
	// differentiate the response for the various request types
	if (iStatusCode >= 290 && iStatusCode <= 292)
	{
		return (iStatusCode == 292) ? 200 : 201;
	}
	else
	{
		return iStatusCode;
	}

} // GetHTTPStatusCode

//-----------------------------------------------------------------------------
void KHTTPError::SetStatusString()
//-----------------------------------------------------------------------------
{
	switch (iStatusCode)
	{
		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		// HTTP 400s: client invocation problems
		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		case H4xx_BADREQUEST:   sStatusString = "BAD REQUEST";            break;
		case H4xx_FORBIDDEN:    sStatusString = "FORBIDDEN";              break;
		case H4xx_NOTAUTH:      sStatusString = "NOT AUTHORIZED";         break;
		case H4xx_NOTFOUND:     sStatusString = "NOT FOUND";              break;
		case H4xx_CONFLICT:     sStatusString = "CONFLICT";               break;

		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		// HTTP 500s: server-side problems
		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		case H5xx_ERROR:        sStatusString = "INTERNAL SERVER ERROR";  break;
		case H5xx_NOTIMPL:      sStatusString = "NOT IMPLEMENTED";        break;

		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		// FALL THROUGH: blow up with a 500 error
		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		default:                sStatusString = "INTERNAL SERVER ERROR";
			kWarning ("BUG: called with code {}", iStatusCode);
			break;
	}

} // SetStatusString

