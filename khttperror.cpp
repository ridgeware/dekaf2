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
void KHTTPError::SetStatusString()
//-----------------------------------------------------------------------------
{
	switch (value())
	{
		// reset
		case 0:                 m_sStatusString.clear();                    break;

		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		// HTTP 200s: ok
		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		case H2xx_OK:           m_sStatusString = "OK";                     break;
		case H2xx_CREATED:      m_sStatusString = "CREATED";                break;
		case H2xx_NO_CONTENT:   m_sStatusString = "NO CONTENT";             break;
		case H2xx_UPDATED:      m_sStatusString = "UPDATED";                break;
		case H2xx_DELETED:      m_sStatusString = "DELETED";                break;
		case H2xx_ALREADY:      m_sStatusString = "ALREADY DONE";           break;

		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		// HTTP 300s: redirects
		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		case H301_MOVED_PERMANENTLY:  m_sStatusString = "MOVED PERMANENTLY";  break;
		case H302_MOVED_TEMPORARILY:  m_sStatusString = "MOVED TEMPORARILY";  break;
		case H303_SEE_OTHER:          m_sStatusString = "SEE OTHER";          break;
		case H304_NOT_MODIFIED:       m_sStatusString = "NOT MODIFIED";       break;
		case H305_USE_PROXY:          m_sStatusString = "USE PROXY";          break;
		case H307_TEMPORARY_REDIRECT: m_sStatusString = "TEMPORARY REDIRECT"; break;
		case H308_PERMANENT_REDIRECT: m_sStatusString = "PERMANENT REDIRECT"; break;

		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		// HTTP 400s: client invocation problems
		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		case H4xx_BADREQUEST:   m_sStatusString = "BAD REQUEST";            break;
		case H4xx_FORBIDDEN:    m_sStatusString = "FORBIDDEN";              break;
		case H4xx_NOTAUTH:      m_sStatusString = "NOT AUTHORIZED";         break;
		case H4xx_NOTFOUND:     m_sStatusString = "NOT FOUND";              break;
		case H4xx_BADMETHOD:    m_sStatusString = "METHOD NOT ALLOWED";     break;
		case H4xx_CONFLICT:     m_sStatusString = "CONFLICT";               break;

		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		// HTTP 500s: server-side problems
		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		case H5xx_ERROR:        m_sStatusString = "INTERNAL SERVER ERROR";      break;
		case H5xx_NOTIMPL:      m_sStatusString = "NOT IMPLEMENTED";            break;
		case H5xx_UNAVAILABLE:  m_sStatusString = "SERVICE UNAVAILABLE";        break;
		case H5xx_READTIMEOUT:  m_sStatusString = "NETWORK READ TIMEOUT ERROR"; break;

		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		// FALL THROUGH: blow up with a 500 error
		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		default:                m_sStatusString = "INTERNAL SERVER ERROR";
			kWarning ("BUG: called with code {}", value());
			break;
	}

} // SetStatusString

