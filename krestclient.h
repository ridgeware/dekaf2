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
#include "kwebclient.h"
#include "kjson.h"

using namespace dekaf2;

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KRestClient : private KWebClient
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	using base_type = KWebClient;

	KRestClient(KURL URL, bool bVerifyCerts = false);

	KString Request (KStringView sPath, KStringView sVerb, KStringView sBody, KMIME mime);

	KString Get (KStringView sPath, KStringView sBody = KStringView{}, KMIME mime = {})
	{
		return Request(sPath, KHTTPMethod::GET, sBody, mime);
	}

	KString Post (KStringView sPath, KStringView sBody, KMIME mime)
	{
		return Request(sPath, KHTTPMethod::POST, sBody, mime);
	}

	KString Put (KStringView sPath, KStringView sBody, KMIME mime)
	{
		return Request(sPath, KHTTPMethod::PUT, sBody, mime);
	}

	KString Patch (KStringView sPath, KStringView sBody, KMIME mime)
	{
		return Request(sPath, KHTTPMethod::PATCH, sBody, mime);
	}

	KString Delete (KStringView sPath)
	{
		return Request(sPath, KHTTPMethod::DELETE, KStringView{}, KMIME{});
	}

	using base_type::Good;
	using base_type::HttpSuccess;
	using base_type::HttpFailure;
	using base_type::GetStatusCode;
	using base_type::Error;
	using base_type::Authentication;
	using base_type::BasicAuthentication;
	using base_type::DigestAuthentication;
	using base_type::ClearAuthentication;
	using base_type::SetRequestHeader;
	using base_type::SetTimeout;
	using base_type::SetProxy;
	using base_type::AutoConfigureProxy;
	using base_type::AllowRedirects;
	using base_type::AllowCompression;
	using base_type::VerifyCerts;

//----------
private:
//----------

	KURL m_URL;

}; // KRestClient

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KJsonRestClient : public KRestClient
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	using ErrorCallback = std::function<KStringView(const KJSON&)>;

	KJsonRestClient(KURL URL, bool bVerifyCerts = false, ErrorCallback ecb = nullptr)
	: KRestClient(std::move(URL), bVerifyCerts)
	, m_ErrorCallback(ecb)
	{
	}

	void SetErrorCallback(ErrorCallback ecb)
	{
		m_ErrorCallback = ecb;
	}

	KJSON Request (KStringView sPath, KStringView sVerb, const KJSON& json);

	KJSON Get (KStringView sPath, const KJSON& json = KJSON{})
	{
		return Request(sPath, KHTTPMethod::GET, json);
	}

	KJSON Post (KStringView sPath, const KJSON& json)
	{
		return Request(sPath, KHTTPMethod::POST, json);
	}

	KJSON Put (KStringView sPath, const KJSON& json)
	{
		return Request(sPath, KHTTPMethod::PUT, json);
	}

	KJSON Patch (KStringView sPath, const KJSON& json)
	{
		return Request(sPath, KHTTPMethod::PATCH, json);
	}

	KJSON Delete (KStringView sPath)
	{
		return Request(sPath, KHTTPMethod::DELETE, KJSON{});
	}

//----------
private:
//----------

	ErrorCallback m_ErrorCallback;

}; // KJsonRestClient

