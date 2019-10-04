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

#include "kstring.h"
#include "kstringview.h"
#include "kexception.h"

namespace dekaf2 {

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
// an error to throw
class KHTTPError : public KException
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	enum
	{
		H2xx_OK         = 200,  // each new code needs logic in SetStatusString()
		H2xx_CREATED    = 201,
		H2xx_UPDATED    = 290,
		H2xx_DELETED    = 291,
		H2xx_ALREADY    = 292,

		H4xx_BADREQUEST = 400,
		H4xx_NOTAUTH    = 401,
		H4xx_FORBIDDEN  = 403,
		H4xx_NOTFOUND   = 404,
		H4xx_BADMETHOD  = 405,
		H4xx_CONFLICT   = 409,

		H5xx_ERROR      = 500,
		H5xx_NOTIMPL    = 501,
		H5xx_READTIMEOUT= 598
	};

	KHTTPError() = default;
	
	template<class S1>
	KHTTPError(uint16_t _iStatusCode, S1&& _sError)
	: KException(std::forward<S1>(_sError))
	, iStatusCode(_iStatusCode)
	{
		SetStatusString();
	}

	template<class S1, class S2>
	KHTTPError(uint16_t _iStatusCode, S1&& _sStatusString, S2&& _sError)
	: KException(std::forward<S2>(_sError))
	, sStatusString(std::forward<S1>(_sStatusString))
	, iStatusCode(_iStatusCode)
	{
	}

	void clear();

	uint16_t GetRawStatusCode() const
	{
		return iStatusCode;
	}

	uint16_t GetHTTPStatusCode() const;

	const KString& GetHTTPStatusString() const
	{
		return sStatusString;
	}

//----------
protected:
//----------

	void SetStatusString();

	KString  sStatusString;
	uint16_t iStatusCode { 0 };

}; // KHTTPError

} // end of namespace dekaf2
