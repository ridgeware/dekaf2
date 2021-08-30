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

#pragma once

/// @file khttperror.h
/// provides HTTP exception / error class


#include "kstring.h"
#include "kstringview.h"
#include "kexception.h"

namespace dekaf2 {

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// A HTTP error to throw or to return
class KHTTPError : public KError
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	enum
	{
		H2xx_OK                 = 200,  // each new code needs logic in GetStatusString()
		H2xx_CREATED            = 201,
		H2xx_ACCEPTED           = 202,
		H2xx_NO_CONTENT         = 204,
		H2xx_UPDATED            = 290,
		H2xx_DELETED            = 291,
		H2xx_ALREADY            = 292,

		H301_MOVED_PERMANENTLY  = 301,
		H302_MOVED_TEMPORARILY  = 302,
		H303_SEE_OTHER          = 303,
		H304_NOT_MODIFIED       = 304,
		H305_USE_PROXY          = 305,
		H307_TEMPORARY_REDIRECT = 307,
		H308_PERMANENT_REDIRECT = 308,

		H4xx_BADREQUEST         = 400,
		H4xx_NOTAUTH            = 401,
		H4xx_FORBIDDEN          = 403,
		H4xx_NOTFOUND           = 404,
		H4xx_BADMETHOD          = 405,
		H4xx_CONFLICT           = 409,

		H5xx_ERROR              = 500,
		H5xx_NOTIMPL            = 501,
		H5xx_UNAVAILABLE        = 503,
		H5xx_READTIMEOUT        = 598
	};

	KHTTPError() = default;
	
	template<class S1>
	KHTTPError(uint16_t _iStatusCode, S1&& _sError)
	: KError(std::forward<S1>(_sError), _iStatusCode)
	{
		m_sStatusString = GetStatusString(_iStatusCode);
	}

	template<class S1, class S2>
	KHTTPError(uint16_t _iStatusCode, S1&& _sStatusString, S2&& _sError)
	: KError(std::forward<S2>(_sError), _iStatusCode)
	, m_sStatusString(std::forward<S1>(_sStatusString))
	{
	}

	/// clears state (and resets status code to 0)
	void clear();

	/// our internal status code that makes a difference for
	/// UPDATED / DELETED / ALREADY DONE, but maps those
	/// onto OK / CREATED
	uint16_t GetRawStatusCode() const
	{
		return value();
	}

	/// maps UPDATED / DELETED / ALREADY DONE onto OK / CREATED,
	/// returns all other verbatim
	uint16_t GetHTTPStatusCode() const;

	/// returns the fixed status string
	const KString& GetHTTPStatusString() const
	{
		return m_sStatusString;
	}

	/// returns true when state is an error
	operator bool() const
	{
		return value() != 0 && (value() >= 300 || value() < 200);
	}

	/// gets the status string depending on the status code
	static KStringView GetStatusString(uint16_t iStatusCode);

//----------
protected:
//----------

	KString m_sStatusString;

}; // KHTTPError

} // end of namespace dekaf2
