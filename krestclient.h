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

namespace dekaf2 {

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KRestClient : private KWebClient
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	using base_type = KWebClient;
	using self = KRestClient;

	KRestClient(KURL URL, bool bVerifyCerts = false);

	KString Request (KStringView sBody = KStringView{}, KMIME mime = {});

	self& Verb     (KString sVerb);
	self& Path     (KString sPath);
	self& SetQuery (url::KQuery Query);
	self& AddQuery (url::KQuery Query);
	self& AddQuery (KString sName, KString sValue);
	self& Get      (KString sPath)   { return Path(sPath).Verb(KHTTPMethod::GET    ); }
	self& Post     (KString sPath)   { return Path(sPath).Verb(KHTTPMethod::POST   ); }
	self& Put      (KString sPath)   { return Path(sPath).Verb(KHTTPMethod::PUT    ); }
	self& Patch    (KString sPath)   { return Path(sPath).Verb(KHTTPMethod::PATCH  ); }
	self& Delete   (KString sPath)   { return Path(sPath).Verb(KHTTPMethod::DELETE ); }

	void clear();

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
protected:
//----------

	KURL m_URL;
	KString m_sVerb;
	KString m_sPath;
	url::KQuery m_Query;

}; // KRestClient

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KJsonRestClient : public KRestClient
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	using ErrorCallback = std::function<KStringView(const KJSON&)>;
	using self = KJsonRestClient;
	using base = KRestClient;

	KJsonRestClient(KURL URL, bool bVerifyCerts = false, ErrorCallback ecb = nullptr)
	: KRestClient(std::move(URL), bVerifyCerts)
	, m_ErrorCallback(ecb)
	{
	}

	self& SetErrorCallback(ErrorCallback ecb)
	{
		m_ErrorCallback = ecb;
		return *this;
	}

	KJSON Request (const KJSON& json);

	self& Verb     (KString sVerb)     { base::Verb(sVerb);             return *this; }
	self& Path     (KString sPath)     { base::Path(sPath);             return *this; }
	self& SetQuery (url::KQuery Query) { base::SetQuery(Query);         return *this; }
	self& AddQuery (url::KQuery Query) { base::AddQuery(Query);         return *this; }
	self& AddQuery (KString sName, KString sValue)
	                                   { base::AddQuery(sName, sValue); return *this; }
	self& Get      (KString sPath)     { base::Get(sPath);              return *this; }
	self& Post     (KString sPath)     { base::Post(sPath);             return *this; }
	self& Put      (KString sPath)     { base::Put(sPath);              return *this; }
	self& Patch    (KString sPath)     { base::Patch(sPath);            return *this; }
	self& Delete   (KString sPath)     { base::Delete(sPath);           return *this; }

//----------
private:
//----------

	ErrorCallback m_ErrorCallback;

}; // KJsonRestClient

} // end of namespace dekaf2
